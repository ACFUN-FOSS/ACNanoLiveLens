#include "test_common.hxx"


int main() {

    qjs::Runtime rt;
    qjs::Context ctx{ rt };
    initJs(rt, ctx);

    regClass(ctx, UNWRAP(metapp::getMetaType<TestStruct>()), [](metapp::Variant cpp) {
        auto pointer = cpp.get<TestStruct *>();
        assert(pointer);
        return pointer;
    });

    std::optional<TestStruct> testStruct;
    testStruct.emplace("nimshab", 99);

	auto obj = makeRefJsTwinObject(
		UNWRAP(ctx.ctx),
		UNWRAP(metapp::getMetaType<TestStruct>()),
		testStruct.value(),
		testStruct.value().lifetimeInformant.info
	);

	testStruct.reset();

    try_eval_module(ctx, rt, R"(
        import * as std from 'std';
        function foo(obj) {
            obj.foo();
            //std.puts('hello world');
        }
        globalThis.foo = foo;
    )");
    auto foo = ctx.global()["foo"];

	auto res = JS_Call(ctx.ctx, qjs::Value{ foo }.v, JS_UNDEFINED, 1, &obj.v);
	if (JS_IsException(res))
		js_std_dump_error(ctx.ctx);
}
