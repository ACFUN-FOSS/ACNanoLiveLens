#include "test_common.hxx"

int main() {

    qjs::Runtime rt;
    qjs::Context ctx{ rt };
    initJs(rt, ctx);

    Binding binding{ metaRepo, rt, ctx };

    try {
        binding.regTypeStatic<TestStruct>();
        std::println("regTypeStatic: TestStruct registered.");
    } catch (const std::exception &ex) {
        std::println(stderr, "FAIL: regTypeStatic threw: {}", ex.what());
        return 1;
    }

    std::optional<TestStruct> testStruct;
    testStruct.emplace("nimshab", 99);

    auto obj = binding.cpp2JSTwin(metapp::Variant::reference(testStruct.value()));
    if (JS_IsException(obj)) {
        auto exc = ctx.getException();
        std::println(stderr, "FAIL: cpp2JSTwin threw a JS exception: {}", (std::string)exc);
        return 1;
    }

    // While the C++ object is alive, the twin must report it as valid
    // and method calls must succeed.
    if (!binding.checkEJSObjLifetime(obj)) {
        auto exc = ctx.getException();
        std::println(stderr, "FAIL: a live C++ object was reported as invalid: {}", (std::string)exc);
        JS_FreeValue(ctx.ctx, obj);
        return 1;
    }

    try_eval_module(ctx, rt, R"(
        function foo(obj) {
            return obj.foo();
        }
        globalThis.foo = foo;
    )");
    auto foo = ctx.global()["foo"];

    {
        auto res = JS_Call(ctx.ctx, qjs::Value{ foo }.v, JS_UNDEFINED, 1, &obj);
        if (JS_IsException(res)) {
            js_std_dump_error(ctx.ctx);
            std::println(stderr, "FAIL: JS call on a live object threw.");
            JS_FreeValue(ctx.ctx, obj);
            return 1;
        }
        JS_FreeValue(ctx.ctx, res);
    }

    // Destroy the C++ object: subsequent JS calls must raise a TypeError.
    testStruct.reset();

    {
        auto res = JS_Call(ctx.ctx, qjs::Value{ foo }.v, JS_UNDEFINED, 1, &obj);
        if (!JS_IsException(res)) {
            std::println(stderr, "FAIL: JS call on a dead object did not throw.");
            JS_FreeValue(ctx.ctx, res);
            JS_FreeValue(ctx.ctx, obj);
            return 1;
        }
        auto exc = ctx.getException();
        std::println("Expected error after C++ object destruction: {}", (std::string)exc);
        JS_FreeValue(ctx.ctx, res);
    }

    JS_FreeValue(ctx.ctx, obj);
    std::println("PASS");
}
