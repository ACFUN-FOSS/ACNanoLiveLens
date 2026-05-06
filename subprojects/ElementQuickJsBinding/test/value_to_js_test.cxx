#include "test_common.hxx"

int main() {
    qjs::Runtime rt;
    qjs::Context ctx{ rt };
    initJs(rt, ctx);

	setDefaultMetaRepo(metaRepo);

	//metaRepo.

    regClass(ctx, UNWRAP(metapp::getMetaType<TestStruct>()), [](metapp::Variant cpp) {
        auto &ref = cpp.get<TestStruct &>();
        assert(&ref);
        return &ref;
    }, true);

    regClass(ctx, UNWRAP(metapp::getMetaType<TestStruct2>()), [](metapp::Variant cpp) {
        auto &ref = cpp.get<TestStruct2 &>();
        assert(&ref);
        return &ref;
    }, true);

	regClass(ctx, UNWRAP(metapp::getMetaType<CannotMove>()), [](metapp::Variant cpp) {
		auto &ref = cpp.get<CannotMove &>();
		assert(&ref);
		return &ref;
	}, false);

    std::optional<TestStruct> testStruct;
    testStruct.emplace("nimshab", 99);

    std::println("Creating JS object ============");
	auto obj = makeOwnedJsTwinObject(
		UNWRAP(ctx.ctx),
		UNWRAP(metapp::getMetaType<TestStruct>()),
		std::move(testStruct.value())
	);
    std::println("JS object created ============");

	//testStruct.reset();

    try_eval_module(ctx, rt, R"(
        import * as std from 'std';
        function foo(obj) {
            std.puts(obj.foo() + "\n");
            // The returned value should be "created and moved"
            let obj2 = obj.returnObj();
            std.puts(obj2.bar() + "\n");
            //obj.returnCannotMove();
			obj.returnObjVector();
        }
        globalThis.foo = foo;
    )");
    auto foo = ctx.global()["foo"];

    std::println("Calling JS ============");
    try {
	    auto res = JS_Call(ctx.ctx, qjs::Value{ foo }.v, JS_UNDEFINED, 1, &obj.v);
	if (JS_IsException(res))
		js_std_dump_error(ctx.ctx);
    } catch (const std::exception &ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
    }
    std::println("JS call done ============");
}
