#include "test_common.hxx"

// Proxy properties (C++ 对象成员变量在 JS 侧的使用)
//
// 变量是通过 MetaClass::registerVariable 注册的成员变量。注册后，它们会以同名的
// 访问器属性出现在默认原型上：读取属性会读取关联 C++ 对象的成员并转换为 JS 值，
// 写入属性会把 JS 值转换回成员类型并赋值进关联 C++ 对象。

struct PropertyInner
{
	std::string label;
	int value = 0;

	LifetimeInformant lifetimeInformant;

	PropertyInner() = default;
	PropertyInner(std::string label, int value)
		: label{ std::move(label) }, value{ value } { }
};

// serial 是 const 成员：读取可用的同时，写入必须被拒绝。
struct PropertyOwner
{
	std::string name;
	int age = 0;
	const int serial;
	PropertyInner inner{ "inner", 1 };
	std::vector<PropertyInner> items{ { "a", 1 }, { "b", 2 } };

	LifetimeInformant lifetimeInformant;

	PropertyOwner(std::string name, int age, int serial)
		: name{ std::move(name) }, age{ age }, serial{ serial } { }
};

template<>
struct metapp::DeclareMetaType<PropertyInner> : metapp::DeclareMetaTypeBase<PropertyInner>
{
	static void setup() {
		metaRepo.registerType<PropertyInner>("PropertyInner");
	}
	static const metapp::MetaClass *getMetaClass() {
		static const metapp::MetaClass metaClass{
			metapp::getMetaType<PropertyInner>(),
			[](metapp::MetaClass &mc) {
				mc.registerVariable("label", &PropertyInner::label);
				mc.registerVariable("value", &PropertyInner::value);
			}
		};
		return &metaClass;
	}
};

template<>
struct metapp::DeclareMetaType<PropertyOwner> : metapp::DeclareMetaTypeBase<PropertyOwner>
{
	static void setup() {
		metaRepo.registerType<PropertyOwner>("PropertyOwner");
	}
	static const metapp::MetaClass *getMetaClass() {
		static const metapp::MetaClass metaClass{
			metapp::getMetaType<PropertyOwner>(),
			[](metapp::MetaClass &mc) {
				mc.registerVariable("name", &PropertyOwner::name);
				mc.registerVariable("age", &PropertyOwner::age);
				mc.registerVariable("serial", &PropertyOwner::serial);
				mc.registerVariable("inner", &PropertyOwner::inner);
				mc.registerVariable("items", &PropertyOwner::items);
			}
		};
		return &metaClass;
	}
};

// 读取一个失败的全局标记：JS 侧置位即表示某项断言失败。
static bool getGlobalFlag(qjs::Context &ctx, const char *name) {
	JSValue flag = JS_GetPropertyStr(ctx.ctx, ctx.global().v, name);
	bool set = !JS_IsUndefined(flag);
	JS_FreeValue(ctx.ctx, flag);
	return set;
}

int main() {
	std::setvbuf(stdout, nullptr, _IONBF, 0);

	qjs::Runtime rt;
	qjs::Context ctx{ rt };
	initJs(rt, ctx);

	Binding binding{ metaRepo, rt, ctx };
	binding.regTypeStatic<PropertyOwner>();
	binding.regTypeStatic<PropertyInner>();

	// ---------------------------------------------------------------
	// 移植体的属性读写
	// ---------------------------------------------------------------
	std::println("== properties of a transplanted object ==");

	PropertyOwner owner{ "nimshab", 99, 1234 };
	auto obj = binding.cpp2JSTransplant(metapp::Variant{ std::move(owner) });
	if (JS_IsException(obj)) {
		auto exc = ctx.getException();
		std::println(stderr, "FAIL: cpp2JSTransplant threw a JS exception: {}", (std::string)exc);
		return 1;
	}

	try_eval_module(ctx, rt, R"(
		import * as std from 'std';

		function checkProperties(obj) {
			// 读取：注册的变量以属性形式出现在关联 C++ 对象上。
			if (obj.name !== "nimshab" || obj.age !== 99 || obj.serial !== 1234) {
				std.puts("ERROR: property read returned unexpected values: " +
					obj.name + ", " + obj.age + ", " + obj.serial + "\n");
				globalThis.propertyReadFailed = true;
			} else {
				std.puts("property read: " + obj.name + ", " + obj.age + ", " + obj.serial + "\n");
			}

			// 写入：应当落回关联 C++ 对象的成员变量。
			obj.name = "renamed";
			obj.age = 30;
			if (obj.name !== "renamed" || obj.age !== 30) {
				std.puts("ERROR: property write did not take effect: " +
					obj.name + ", " + obj.age + "\n");
				globalThis.propertyWriteFailed = true;
			} else {
				std.puts("property write: " + obj.name + ", " + obj.age + "\n");
			}

			// const 成员是只读的。
			try {
				obj.serial = 1;
				std.puts("ERROR: const member write did not throw\n");
				globalThis.readOnlyWriteFailed = true;
			} catch (e) {
				std.puts("const member write rejected as expected\n");
			}

			// 类型不匹配的写入同样被拒绝，且不改变成员的值。
			try {
				obj.age = "not-a-number";
				std.puts("ERROR: mismatched type write did not throw\n");
				globalThis.mismatchedWriteFailed = true;
			} catch (e) {
				std.puts("mismatched member write rejected as expected\n");
			}
			if (obj.age !== 30) {
				std.puts("ERROR: rejected write changed the member: " + obj.age + "\n");
				globalThis.rejectedWriteChangedFailed = true;
			}

			// 成员对象的属性读取得到孪生体，写入落回同一个 C++ 对象。
			std.puts("inner: " + obj.inner.label + ", " + obj.inner.value + "\n");
			obj.inner.value = 42;
			if (obj.inner.value !== 42) {
				std.puts("ERROR: nested member write did not take effect\n");
				globalThis.nestedWriteFailed = true;
			}
			let innerTwin = obj.inner;
			innerTwin.label = "inner-updated";
			if (obj.inner.label !== "inner-updated") {
				std.puts("ERROR: nested member write through a twin did not take effect\n");
				globalThis.nestedTwinWriteFailed = true;
			}

			// 成员容器的属性读取得到活视图，元素写回成员容器本身。
			if (obj.items.length !== 2) {
				std.puts("ERROR: member container length = " + obj.items.length + "\n");
				globalThis.memberContainerFailed = true;
			} else {
				std.puts("items: " + obj.items.length + ", " + obj.items[0].label + "\n");
			}
			obj.items[0].label = "a-updated";
			obj.items[1] = obj.items[0];
			if (obj.items[0].label !== "a-updated" || obj.items[1].label !== "a-updated") {
				std.puts("ERROR: member container write-back did not take effect\n");
				globalThis.memberContainerWriteFailed = true;
			}
		}

		function readName(obj) {
			return obj.name;
		}

		function writeAge(obj, age) {
			obj.age = age;
			return obj.age;
		}

		globalThis.checkProperties = checkProperties;
		globalThis.readName = readName;
		globalThis.writeAge = writeAge;
	)");

	{
		auto checkProperties = ctx.global()["checkProperties"];
		auto res = JS_Call(ctx.ctx, qjs::Value{ checkProperties }.v, JS_UNDEFINED, 1, &obj);
		if (JS_IsException(res)) {
			js_std_dump_error(ctx.ctx);
			std::println(stderr, "FAIL: JS call threw an exception.");
			JS_FreeValue(ctx.ctx, obj);
			return 1;
		}
		JS_FreeValue(ctx.ctx, res);
	}

	bool anyFlagFailed = false;
	for (const char *flag : {
		"propertyReadFailed", "propertyWriteFailed", "readOnlyWriteFailed",
		"mismatchedWriteFailed", "rejectedWriteChangedFailed", "nestedWriteFailed",
		"nestedTwinWriteFailed", "memberContainerFailed", "memberContainerWriteFailed"
	}) {
		if (getGlobalFlag(ctx, flag)) {
			std::println(stderr, "FAIL: {}", flag);
			anyFlagFailed = true;
		}
	}
	if (anyFlagFailed) {
		JS_FreeValue(ctx.ctx, obj);
		return 1;
	}
	JS_FreeValue(ctx.ctx, obj);

	// ---------------------------------------------------------------
	// 孪生体的属性读写与生命周期
	// ---------------------------------------------------------------
	std::println("== properties of a twin and their lifetime ==");

	auto ownerOnStack = std::make_unique<PropertyOwner>("alive", 5, 6);
	auto twin = binding.cpp2JSTwin(metapp::Variant::reference(*ownerOnStack));
	if (JS_IsException(twin)) {
		auto exc = ctx.getException();
		std::println(stderr, "FAIL: cpp2JSTwin threw a JS exception: {}", (std::string)exc);
		return 1;
	}

	// 孪生体的属性读写会作用在被孪生的 C++ 对象上。
	{
		auto writeAge = ctx.global()["writeAge"];
		JSValue ageArg = JS_NewInt32(ctx.ctx, 7);
		JSValue args[]{ twin, ageArg };
		auto res = JS_Call(ctx.ctx, qjs::Value{ writeAge }.v, JS_UNDEFINED, 2, args);
		JS_FreeValue(ctx.ctx, ageArg);
		if (JS_IsException(res)) {
			js_std_dump_error(ctx.ctx);
			std::println(stderr, "FAIL: writing a property through a twin threw.");
			JS_FreeValue(ctx.ctx, twin);
			return 1;
		}
		JS_FreeValue(ctx.ctx, res);

		if (ownerOnStack->age != 7) {
			std::println(stderr, "FAIL: the twin did not write into the C++ object, age = {}", ownerOnStack->age);
			JS_FreeValue(ctx.ctx, twin);
			return 1;
		}
		std::println("write through the twin reached the C++ object: age = {}", ownerOnStack->age);
	}

	// 销毁 C++ 对象后，属性访问必须抛出 JS 异常。
	ownerOnStack.reset();

	{
		auto readName = ctx.global()["readName"];
		auto res = JS_Call(ctx.ctx, qjs::Value{ readName }.v, JS_UNDEFINED, 1, &twin);
		if (!JS_IsException(res)) {
			std::println(stderr, "FAIL: reading a property of a dead object did not throw.");
			JS_FreeValue(ctx.ctx, res);
			JS_FreeValue(ctx.ctx, twin);
			return 1;
		}
		auto exc = ctx.getException();
		std::println("Expected error after C++ object destruction: {}", static_cast<std::string>(exc));
		JS_FreeValue(ctx.ctx, res);
	}

	JS_FreeValue(ctx.ctx, twin);

	std::println("PASS");
	return 0;
}
