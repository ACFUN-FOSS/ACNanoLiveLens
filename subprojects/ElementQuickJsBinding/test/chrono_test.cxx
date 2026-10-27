#include "test_common.hxx"

#include <chrono>
#include <cstdint>
#include <functional>
#include <print>
#include <string>
#include <vector>

// 时间点与时间长度 ↔ 脚本
//
// 这是绑定层的结构性转换（见 ElementQuickJsBinding/metapp_chrono_metatypes.hxx）：
// 任何 std::chrono::time_point 在脚本里都是 Date，任何 duration 都是毫秒数，
// 既不需要注册，也不需要给类型加辅助访问器。
//
// 重点覆盖「成员变量」这条路径：属性 getter 读到的成员是**引用语义**的 Variant，
// 转换必须照样走翻译，而不是去构造孪生体——孪生体对时间点是不可能的，曾经因此
// 只能在 C++ 侧额外加 timestampMs()/timestampText() 之类的方法绕开。

struct ChronoHolder
{
	std::string name;
	// system_clock 的周期未必是毫秒（MSVC 上是 100ns），转换要能处理任意周期。
	std::chrono::system_clock::time_point bornAt;
	std::chrono::milliseconds timeout{ 500 };
	std::chrono::duration<double> accuracy{ 0.25 };
	// const 成员：属性只读，但读出来仍然必须是 Date。
	const std::chrono::system_clock::time_point frozenAt{ std::chrono::milliseconds{ 1700000000000 } };

	std::chrono::system_clock::time_point echo(std::chrono::system_clock::time_point at) {
		return at;
	}
	std::chrono::milliseconds measure(std::chrono::milliseconds duration) {
		return duration;
	}
};

template<>
struct metapp::DeclareMetaType<ChronoHolder> : metapp::DeclareMetaTypeBase<ChronoHolder>
{
	static const metapp::MetaClass *getMetaClass() {
		static const metapp::MetaClass metaClass{
			metapp::getMetaType<ChronoHolder>(),
			[](metapp::MetaClass &mc) {
				mc.registerType<ChronoHolder>("ChronoHolder");
				mc.registerVariable("name", &ChronoHolder::name);
				mc.registerVariable("bornAt", &ChronoHolder::bornAt);
				mc.registerVariable("timeout", &ChronoHolder::timeout);
				mc.registerVariable("accuracy", &ChronoHolder::accuracy);
				mc.registerVariable("frozenAt", &ChronoHolder::frozenAt);
				mc.registerCallable("echo", &ChronoHolder::echo);
				mc.registerCallable("measure", &ChronoHolder::measure);
			}
		};
		return &metaClass;
	}
};

// 是不是一个真正的 Date：比较原型对象，等价于脚本里的 `value instanceof Date`。
static bool jsIsDate(qjs::Context &ctx, JSValue value) {
	if (!JS_IsObject(value))
		return false;

	const JSValue dateCtor = JS_GetPropertyStr(ctx.ctx, ctx.global().v, "Date");
	const JSValue datePrototype = JS_GetPropertyStr(ctx.ctx, dateCtor, "prototype");
	const JSValue valuePrototype = JS_GetPrototype(ctx.ctx, value);

	const bool isDate = JS_IsObject(valuePrototype)
		&& JS_VALUE_GET_PTR(valuePrototype) == JS_VALUE_GET_PTR(datePrototype);

	JS_FreeValue(ctx.ctx, valuePrototype);
	JS_FreeValue(ctx.ctx, datePrototype);
	JS_FreeValue(ctx.ctx, dateCtor);
	return isDate;
}

int main() {
	std::setvbuf(stdout, nullptr, _IONBF, 0);

	qjs::Runtime rt;
	qjs::Context ctx{ rt };
	initJs(rt, ctx);

	Binding binding{ rt, ctx };
	binding.regTypeStatic<ChronoHolder>();

	using TimePoint = std::chrono::system_clock::time_point;
	const auto timePoint = TimePoint{ std::chrono::milliseconds{ 1700000000123 } };

	ChronoHolder holder{ .name = "nimshab", .bornAt = timePoint };
	std::function<ChronoHolder()> getHolder = [&holder]() { return holder; };
	binding.regGlobalFunction("getHolder", getHolder);

	// C++ 回调拿到的时间点（JsHook 用的就是这条路径：形参按值声明的 std::function）。
	std::vector<std::function<void(TimePoint)>> timeCallbacks;
	std::function<int(std::function<void(TimePoint)>)> addTimeCallback =
		[&](std::function<void(TimePoint)> callback) -> int {
			timeCallbacks.push_back(std::move(callback));
			return static_cast<int>(timeCallbacks.size());
		};
	binding.regTypeStatic<std::function<void(TimePoint)>>();
	binding.regGlobalFunction("addTimeCallback", addTimeCallback);

	// -----------------------------------------------------------------
	// 纯值：翻译的两个方向
	// -----------------------------------------------------------------
	{
		const auto jsDate = binding.cpp2JSAuto(metapp::Variant{ timePoint });
		if (JS_IsException(jsDate) || !jsIsDate(ctx, jsDate)) {
			js_std_dump_error(ctx.ctx);
			std::println(stderr, "FAIL: 时间点没有翻译成 JS 的 Date 对象。");
			return 1;
		}

		const auto back = binding.js2CppAs(*metapp::getMetaType<TimePoint>(), jsDate);
		const auto backTimePoint = back.get<TimePoint>();
		JS_FreeValue(ctx.ctx, jsDate);
		if (backTimePoint != timePoint) {
			std::println(stderr, "FAIL: 时间点未能往返（round trip）。");
			return 1;
		}
		std::println("时间点 ↔ Date 往返成功（{} ms）",
			std::chrono::duration_cast<std::chrono::milliseconds>(timePoint.time_since_epoch()).count());
	}

	{
		// 时间长度翻译为毫秒数；亚毫秒精度（1.5 ms）不能被截断。
		const auto jsNumber = binding.cpp2JSAuto(metapp::Variant{ std::chrono::microseconds{ 1500 } });
		if (!JS_IsNumber(jsNumber)) {
			std::println(stderr, "FAIL: 时间长度没有翻译成 JS 的 number。");
			return 1;
		}
		double milliseconds = 0;
		JS_ToFloat64(ctx.ctx, &milliseconds, jsNumber);
		JS_FreeValue(ctx.ctx, jsNumber);
		if (milliseconds != 1.5) {
			std::println(stderr, "FAIL: 亚毫秒精度丢失，得到 {} ms，期望 1.5 ms。", milliseconds);
			return 1;
		}

		// 反向：number → duration，按声明类型（这里是 milliseconds）还原。
		const auto jsMs = binding.cpp2JSAutoStatic(std::int64_t{ 2500 });
		const auto back = binding.js2CppAs(*metapp::getMetaType<std::chrono::milliseconds>(), jsMs);
		JS_FreeValue(ctx.ctx, jsMs);
		if (back.get<std::chrono::milliseconds>() != std::chrono::milliseconds{ 2500 }) {
			std::println(stderr, "FAIL: 毫秒数未能还原为 duration。");
			return 1;
		}
		std::println("时间长度 ↔ 毫秒数往返成功（含 1.5 ms 的亚毫秒精度）");
	}

	// -----------------------------------------------------------------
	// 成员变量、方法参数与返回值（脚本侧）
	// -----------------------------------------------------------------
	try_eval_module(ctx, rt, R"(
		import * as std from 'std';

		const fail = [];
		const check = (condition, message) => {
			if (!condition)
				fail.push(message);
		};

		const h = getHolder();

		// 成员变量读出来是 Date / number，而不是孪生体或枚举之类的东西。
		check(h.bornAt instanceof Date, "bornAt 不是 Date");
		check(h.bornAt.getTime() === 1700000000123, "bornAt 的毫秒数不对：" + h.bornAt.getTime());
		check(h.timeout === 500, "timeout 不是 500：" + h.timeout);
		check(h.accuracy === 250, "accuracy 不是 250 ms：" + h.accuracy);

		// const 成员也是只读的 Date。
		check(h.frozenAt instanceof Date, "frozenAt 不是 Date");
		check(h.frozenAt.getTime() === 1700000000000, "frozenAt 的毫秒数不对");

		// 写回：Date → time_point。
		h.bornAt = new Date(1234567890123);
		check(h.bornAt.getTime() === 1234567890123, "写回 bornAt 失败");
		// 写回：number → duration。
		h.timeout = 1500;
		check(h.timeout === 1500, "写回 timeout 失败");

		// 参数与返回值。
		check(h.echo(new Date(42)).getTime() === 42, "echo 的参数/返回值转换失败");
		check(h.measure(2500) === 2500, "measure 的参数/返回值转换失败");

		// const 成员拒绝写入。
		try {
			h.frozenAt = new Date(1);
			fail.push("frozenAt 竟然可以写入");
		} catch (e) { }

		// 时间点不接受非 Date 的值。
		try {
			h.bornAt = 5;
			fail.push("数字被当成了时间点");
		} catch (e) { }

		// 通过钩子把时间点交给脚本：C++ 侧随后用一个真实时间点触发这个回调。
		globalThis.timeHookId = addTimeCallback((at) => {
			globalThis.gotTime = at instanceof Date ? at.getTime() : -1;
		});

		if (fail.length > 0) {
			for (const message of fail)
				std.puts("FAIL: " + message + "\n");
			globalThis.chronoFailed = true;
		}
	)");

	// C++ → 脚本：按值声明的时间点形参必须翻译成 Date。
	if (timeCallbacks.size() != 1) {
		std::println(stderr, "FAIL: 时间点回调没有注册成功。");
		return 1;
	}
	timeCallbacks[0](timePoint);

	const auto readGlobal = [&](const char *name) -> JSValue {
		return JS_GetPropertyStr(ctx.ctx, ctx.global().v, name);
	};
	{
		const JSValue failedFlag = readGlobal("chronoFailed");
		const bool failed = !JS_IsUndefined(failedFlag);
		JS_FreeValue(ctx.ctx, failedFlag);
		if (failed)
			return 1;

		const JSValue gotTimeValue = readGlobal("gotTime");
		double gotTime = 0;
		JS_ToFloat64(ctx.ctx, &gotTime, gotTimeValue);
		JS_FreeValue(ctx.ctx, gotTimeValue);
		if (gotTime != 1700000000123) {
			std::println(stderr, "FAIL: 回调收到的时间点不对，得到 {}。", gotTime);
			return 1;
		}
	}

	std::println("成员变量、参数与返回值、以及回调形参上的时间点都是 Date");
	std::println("PASS");
	return 0;
}
