#include "test_common.hxx"

int main() {
    qjs::Runtime rt;
    qjs::Context ctx{ rt };
    initJs(rt, ctx);

    Binding binding{ metaRepo, rt, ctx };

    binding.regTypeStatic<TestStruct>();
    binding.regTypeStatic<TestStruct2>();
    binding.regTypeStatic<CannotMove>();

    std::optional<TestStruct> testStruct;
    testStruct.emplace("nimshab", 99);

    std::println("Creating JS object ============");
    auto obj = binding.cpp2JSTransplant(metapp::Variant{ std::move(testStruct.value()) });
    if (JS_IsException(obj)) {
        auto exc = ctx.getException();
        std::println(stderr, "FAIL: cpp2JSTransplant threw a JS exception: {}", (std::string)exc);
        return 1;
    }
    std::println("JS object created ============");

    //testStruct.reset();

    try_eval_module(ctx, rt, R"(
        import * as std from 'std';
        function foo(obj) {
            std.puts(obj.foo() + "\n");
            // The returned value should be "created and moved"
            let obj2 = obj.returnObj();
            std.puts(obj2.bar() + "\n");
            // returnCannotMove is registered as not-moveable, so the call
            // must be rejected with a JS TypeError.
            try {
                obj.returnCannotMove();
                std.puts("ERROR: returnCannotMove did not throw\n");
                globalThis.cannotMoveFailed = true;
            } catch (e) {
                std.puts("returnCannotMove rejected as expected\n");
            }
        }
        globalThis.foo = foo;
    )");
    auto foo = ctx.global()["foo"];

    std::println("Calling JS ============");
    auto res = JS_Call(ctx.ctx, qjs::Value{ foo }.v, JS_UNDEFINED, 1, &obj);
    if (JS_IsException(res)) {
        js_std_dump_error(ctx.ctx);
        std::println(stderr, "FAIL: JS call threw an exception.");
        JS_FreeValue(ctx.ctx, obj);
        return 1;
    }
    JS_FreeValue(ctx.ctx, res);

    JSValue cannotMoveFlag = JS_GetPropertyStr(ctx.ctx, ctx.global().v, "cannotMoveFailed");
    bool cannotMoveFailed = !JS_IsUndefined(cannotMoveFlag);
    JS_FreeValue(ctx.ctx, cannotMoveFlag);
    if (cannotMoveFailed) {
        std::println(stderr, "FAIL: returnCannotMove was not rejected.");
        JS_FreeValue(ctx.ctx, obj);
        return 1;
    }

    std::println("JS call done ============");
    JS_FreeValue(ctx.ctx, obj);
    std::println("PASS");
}
