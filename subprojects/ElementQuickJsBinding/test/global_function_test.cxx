#include "test_common.hxx"

#include <chrono>
#include <cstdint>
#include <format>
#include <string>
#include <vector>

// 全局函数与 JS 函数桥接
//
// regGlobalFunction 把 C++ 可调用对象注册为脚本可以直接调用的全局函数；其形参类型
// 即脚本实参的编组契约，与注册类型的代理方法完全一致。
//
// JsFunctionBridge 让脚本函数可以作为 std::function<R(Args...)> 实参传给 C++：
// 只要用 regTypeStatic 注册该 std::function 类型，桥接体就会按形参类型把 C++ 值
// 转换给脚本，并把脚本的返回值转换回 C++ 类型。

// 一个「可翻译」的时间点类型：注册时提供 translator，把时间点翻译为 JS 的 Date。
// 翻译器需要创建 JS 值（这里是 Date），因此它接收 qjs::Context。
struct TestTimePoint
{
	std::chrono::system_clock::time_point value;
};

int main() {
	std::setvbuf(stdout, nullptr, _IONBF, 0);

	qjs::Runtime rt;
	qjs::Context ctx{ rt };
	initJs(rt, ctx);

	Binding binding{ rt, ctx };

	// JS 函数 → std::function<void(int)> 的桥接，以及时间点的翻译。
	// 注意：std::function<void(std::string)> 有意不注册，用于验证「未注册桥接
	// 类型」的错误路径。
	binding.regTypeStatic<std::function<void(int)>>();
	binding.regType({
		.type = metapp::getMetaType<TestTimePoint>(),
		.shareLifetimeInfoFunc = { },
		.moveable = true,
		.translator = Translator{
			.makeTranslateInput = [](const metapp::Variant &cppVal) -> std::any {
				return cppVal.get<TestTimePoint>();
			},
			.translateToJS = [](qjs::Context &ctx, const std::any &cppVal) -> JSValue {
				const auto timePoint = std::any_cast<TestTimePoint>(cppVal).value;
				const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
					timePoint.time_since_epoch()).count();
				JSValue args[]{ JS_NewInt64(ctx.ctx, ms) };
				JSValue dateCtor = JS_GetPropertyStr(ctx.ctx, ctx.global().v, "Date");
				JSValue date = JS_CallConstructor(ctx.ctx, dateCtor, 1, args);
				JS_FreeValue(ctx.ctx, dateCtor);
				JS_FreeValue(ctx.ctx, args[0]);
				return date;
			},
			.detranslate = [](qjs::Context &ctx, JSValue jsVal) -> metapp::Variant {
				JSValue getTime = JS_GetPropertyStr(ctx.ctx, jsVal, "getTime");
				if (!JS_IsFunction(ctx.ctx, getTime)) {
					JS_FreeValue(ctx.ctx, getTime);
					throw std::invalid_argument{ "Expected a Date." };
				}
				JSValue msVal = JS_Call(ctx.ctx, getTime, jsVal, 0, nullptr);
				JS_FreeValue(ctx.ctx, getTime);
				if (JS_IsException(msVal)) {
					JS_FreeValue(ctx.ctx, msVal);
					throw std::invalid_argument{ "Expected a Date." };
				}
				double ms = 0;
				JS_ToFloat64(ctx.ctx, &ms, msVal);
				JS_FreeValue(ctx.ctx, msVal);
				return metapp::Variant{ TestTimePoint{
					std::chrono::system_clock::time_point{
						std::chrono::milliseconds{ static_cast<std::int64_t>(ms) }
					}
				} };
			}
		}
	});

	// ---------------------------------------------------------------
	// 注册全局函数
	// ---------------------------------------------------------------
	std::vector<std::function<void(int)>> storedCallbacks;
	int throwingCallbackCalls = 0;

	std::function<int(std::function<void(int)>)> addCallbackHook =
		[&](std::function<void(int)> callback) -> int {
			storedCallbacks.push_back(std::move(callback));
			return static_cast<int>(storedCallbacks.size());
		};
	std::function<std::string(std::string, int)> describe =
		[](std::string name, int age) -> std::string {
			return std::format("{}/{}", name, age);
		};
	std::function<double(double, double)> addNumbers =
		[](double a, double b) -> double {
			return a + b;
		};
	std::function<int()> countCallbacks =
		[&]() -> int {
			return static_cast<int>(storedCallbacks.size());
		};

	binding.regGlobalFunction("addCallbackHook", addCallbackHook);
	binding.regGlobalFunction("describe", describe);
	binding.regGlobalFunction("addNumbers", addNumbers);
	binding.regGlobalFunction("countCallbacks", countCallbacks);

	// 形参类型没有注册成桥接类型：传脚本函数必须被拒绝（而不是静默失败）。
	std::function<void(std::function<void(std::string)>)> guardedCallbackHook =
		[](std::function<void(std::string)>) { };
	binding.regGlobalFunction("guardedCallbackHook", guardedCallbackHook);

	// 成员函数指针有「关联对象」，不能被注册为全局函数。
	bool rejectedMemberFunction = false;
	try {
		binding.regGlobalFunction("badMemberFunction",
			metapp::Variant{ &TestStruct::addAges });
	} catch (const std::invalid_argument &) {
		rejectedMemberFunction = true;
	}
	if (!rejectedMemberFunction) {
		std::println(stderr, "FAIL: registering a member function as a global function was not rejected.");
		return 1;
	}

	// ---------------------------------------------------------------
	// 以引用声明的形参
	//
	// 桥接体必须按「声明类型」而不是「实参表达式的类型」判断：形参声明为 T& 时，
	// 实参一定是左值，若按表达式推导就会把它当成按值传递，进而尝试拷贝 T。
	// CannotMove 既不可拷贝也不可移动，正是 Rml::Element 那类类型的最小复现。
	// 期望行为：脚本拿到的是孪生体，读到的值与写回的值都直达原对象。
	// ---------------------------------------------------------------
	std::vector<std::function<void(CannotMove &)>> storedRefCallbacks;
	std::function<int(std::function<void(CannotMove &)>)> addRefCallbackHook =
		[&](std::function<void(CannotMove &)> callback) -> int {
			storedRefCallbacks.push_back(std::move(callback));
			return static_cast<int>(storedRefCallbacks.size());
		};
	binding.regTypeStatic<CannotMove>();
	binding.regTypeStatic<std::function<void(CannotMove &)>>();
	binding.regGlobalFunction("addRefCallbackHook", addRefCallbackHook);

	CannotMove nonCopyableObj;
	nonCopyableObj.value = 7;

	try_eval_module(ctx, rt, R"(
		import * as std from 'std';

		addRefCallbackHook(function (obj) {
			globalThis.seenRefValue = obj.value;
			globalThis.seenRefBar = obj.bar();
			obj.value = 99;
		});
	)");

	if (storedRefCallbacks.size() != 1) {
		std::println(stderr, "FAIL: the by-reference callback was not registered.");
		return 1;
	}

	// 桥接体在这里不得抛出 "Not constructible"。
	storedRefCallbacks[0](nonCopyableObj);

	const auto readGlobalInt = [&](const char *name) {
		JSValue value = JS_GetPropertyStr(ctx.ctx, ctx.global().v, name);
		std::int32_t result = 0;
		JS_ToInt32(ctx.ctx, &result, value);
		JS_FreeValue(ctx.ctx, value);
		return result;
	};

	if (readGlobalInt("seenRefValue") != 7) {
		std::println(stderr, "FAIL: the JS callback did not read the object through the reference.");
		return 1;
	}
	if (nonCopyableObj.value != 99) {
		std::println(stderr, "FAIL: the JS callback wrote to a copy, not to the original object.");
		return 1;
	}
	std::println("a non-copyable object was bridged by reference (twin), read = 7, written = {}",
		nonCopyableObj.value);

	// ---------------------------------------------------------------
	// 脚本侧调用
	// ---------------------------------------------------------------
	try_eval_module(ctx, rt, R"(
		import * as std from 'std';

		// 全局函数被调用，并把脚本函数桥接给 C++。
		globalThis.hookId = addCallbackHook(function (v) { globalThis.got = v; });
		std.puts("addCallbackHook returned " + globalThis.hookId + "\n");

		// 返回值按声明类型编组回脚本。
		std.puts("describe: " + describe("nimshab", 99) + "\n");
		if (describe("nimshab", 99) !== "nimshab/99") {
			std.puts("ERROR: unexpected describe() result\n");
			globalThis.returnValueFailed = true;
		}
		if (addNumbers(1.5, 2.5) !== 4) {
			std.puts("ERROR: unexpected addNumbers() result\n");
			globalThis.returnValueFailed = true;
		}

		// 形参不是脚本函数时抛异常。
		try {
			addCallbackHook(123);
			std.puts("ERROR: a non-function argument was accepted\n");
			globalThis.nonFunctionArgFailed = true;
		} catch (e) {
			std.puts("non-function argument rejected: " + e + "\n");
		}

		// 未注册桥接类型的 std::function 形参同样拒绝脚本函数。
		try {
			guardedCallbackHook(function (s) { });
			std.puts("ERROR: an unregistered bridge type was accepted\n");
			globalThis.unbridgedArgFailed = true;
		} catch (e) {
			std.puts("unbridged std::function parameter rejected: " + e + "\n");
		}

		// 会抛异常的脚本回调：桥接体必须捕获并上报，而不是让异常穿过 C++。
		addCallbackHook(function () { throw new Error("boom from JS"); });
	)");

	const auto hookId = [&]() {
		JSValue value = JS_GetPropertyStr(ctx.ctx, ctx.global().v, "hookId");
		std::int32_t id = 0;
		JS_ToInt32(ctx.ctx, &id, value);
		JS_FreeValue(ctx.ctx, value);
		return id;
	}();
	if (hookId != 1 || countCallbacks() != 2) {
		std::println(stderr, "FAIL: unexpected hook ids: JS id = {}, C++ count = {}",
			hookId, countCallbacks());
		return 1;
	}
	std::println("two callbacks were registered, first id = {}", hookId);

	// ---------------------------------------------------------------
	// C++ 侧回调（桥接体必须在 JS 调用返回之后仍然可用）
	// ---------------------------------------------------------------
	storedCallbacks[0](42);
	{
		JSValue got = JS_GetPropertyStr(ctx.ctx, ctx.global().v, "got");
		std::int32_t value = 0;
		JS_ToInt32(ctx.ctx, &value, got);
		JS_FreeValue(ctx.ctx, got);
		if (value != 42) {
			std::println(stderr, "FAIL: the bridged callback did not reach JS, got = {}", value);
			return 1;
		}
		std::println("the bridged callback reached JS: got = {}", value);
	}

	// 抛异常的脚本回调：打印错误并返回默认值，C++ 侧不受影响。
	storedCallbacks[1](0);
	throwingCallbackCalls = static_cast<int>(storedCallbacks.size());
	std::println("a throwing JS callback did not propagate into C++ ({} callbacks kept)",
		throwingCallbackCalls);

	// ---------------------------------------------------------------
	// 时间点的翻译（Translator 会收到 Context，因此可以构造 Date）
	// ---------------------------------------------------------------
	{
		const auto timePoint = std::chrono::system_clock::time_point{
			std::chrono::milliseconds{ 1700000000123 }
		};
		auto jsDate = binding.cpp2JSAuto(metapp::Variant{ TestTimePoint{ timePoint } });
		if (JS_IsException(jsDate)) {
			js_std_dump_error(ctx.ctx);
			std::println(stderr, "FAIL: translating a time point threw.");
			return 1;
		}

		// 回到 C++ 方向走 detranslate。
		auto back = binding.js2CppAs(*metapp::getMetaType<TestTimePoint>(), jsDate);
		const auto backTimePoint = back.get<TestTimePoint>().value;
		JS_FreeValue(ctx.ctx, jsDate);

		if (backTimePoint != timePoint) {
			std::println(stderr, "FAIL: the time point did not survive the round trip.");
			return 1;
		}
		std::println("time point round trip through a JS Date succeeded");
	}

	bool anyFlagFailed = false;
	for (const char *flag : {
		"returnValueFailed", "nonFunctionArgFailed", "unbridgedArgFailed"
	}) {
		JSValue value = JS_GetPropertyStr(ctx.ctx, ctx.global().v, flag);
		const bool set = !JS_IsUndefined(value);
		JS_FreeValue(ctx.ctx, value);
		if (set) {
			std::println(stderr, "FAIL: {}", flag);
			anyFlagFailed = true;
		}
	}
	if (anyFlagFailed)
		return 1;

	std::println("PASS");
	return 0;
}
