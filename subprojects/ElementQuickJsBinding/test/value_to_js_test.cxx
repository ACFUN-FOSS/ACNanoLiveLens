#include "test_common.hxx"

int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    qjs::Runtime rt;
    qjs::Context ctx{ rt };
    initJs(rt, ctx);

    Binding binding{ rt, ctx };

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
            // Container support: owning containers are transplanted
            // into a live JS view with index access, length and iteration.
            let owned = obj.returnObjVector();
            std.puts("array length: " + owned.length + "\n");
            std.puts(owned[0].bar() + "\n");
            std.puts(owned[1].bar() + "\n");
            let iterated = [ ];
            for (let e of owned)
                iterated.push(e.bar());
            std.puts("iterated: " + iterated.join(",") + "\n");
            // Stage 2: index write-back into the transplanted container.
            owned[0] = owned[1];
            std.puts("owned[0] after write-back: " + owned[0].who() + "\n");
            // Stage 2: a reference to a member container becomes a live view
            // (twin) sharing the subject's lifetime.
            let view = obj.getMemberVector();
            std.puts("view length: " + view.length + "\n");
            std.puts("view[0]: " + view[0].who() + "\n");
            view[1] = view[0];
            std.puts("view[1] after write-back: " + view[1].who() + "\n");
            // Stage 2: const container references produce read-only views.
            let readonly = obj.getConstVector();
            std.puts("readonly length: " + readonly.length + "\n");
            std.puts("readonly[0]: " + readonly[0].who() + "\n");
            try {
                readonly[0] = readonly[1];
                std.puts("ERROR: const container write did not throw\n");
                globalThis.constWriteFailed = true;
            } catch (e) {
                std.puts("const container write rejected as expected\n");
            }
            // Stage 3: directed parameter marshalling -- JS numbers are
            // converted towards the declared int parameters.
            std.puts("addAges(1.9, 2.1) = " + obj.addAges(1.9, 2.1) + "\n");
            // Stage 3: string_view parameter accepts a JS string.
            std.puts("greet: " + obj.greet("quickjs") + "\n");
            // Stage 3: string_view return value becomes a JS string.
            std.puts("viewName: " + obj.viewName() + "\n");
            // Stage 3: span is a live non-owning view over the member vector.
            let sv = obj.getSpanView();
            std.puts("span length: " + sv.length + "\n");
            std.puts("span[0]: " + sv[0].who() + "\n");
            sv[1] = sv[0];
            std.puts("span[1] after write-back: " + sv[1].who() + "\n");
            // Stage 3: a span returned by value is still a view (a span never
            // owns its elements), so it also becomes a live view.
            let sv2 = obj.returnSpanByValue();
            std.puts("span by value length: " + sv2.length + "\n");
            std.puts("span by value[0]: " + sv2[0].who() + "\n");
            // Stage 3: a map reference becomes a live map view.
            let m = obj.getMap();
            std.puts("map a: " + m["a"] + "\n");
            m["c"] = "gamma";
            std.puts("map c after insert: " + m["c"] + "\n");
            std.puts("map length: " + m.length + "\n");
            let kv = [ ];
            for (const [k, v] of m)
                kv.push(k + "=" + v);
            std.puts("map iterated: " + kv.join(",") + "\n");
            // Stage 3: an owning map returned by value is transplanted.
            let om = obj.returnMap();
            std.puts("ownedMap one: " + om["one"] + "\n");
            om["three"] = 3;
            std.puts("ownedMap three: " + om["three"] + "\n");
            // Stage 3: const map write rejection.
            let cm = obj.getConstMap();
            std.puts("constMap a: " + cm["a"] + "\n");
            try {
                cm["x"] = "y";
                std.puts("ERROR: const map write did not throw\n");
                globalThis.constMapWriteFailed = true;
            } catch (e) {
                std.puts("const map write rejected as expected\n");
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

    JSValue constWriteFlag = JS_GetPropertyStr(ctx.ctx, ctx.global().v, "constWriteFailed");
    bool constWriteFailed = !JS_IsUndefined(constWriteFlag);
    JS_FreeValue(ctx.ctx, constWriteFlag);
    if (constWriteFailed) {
        std::println(stderr, "FAIL: const container write was not rejected.");
        JS_FreeValue(ctx.ctx, obj);
        return 1;
    }

    JSValue constMapWriteFlag = JS_GetPropertyStr(ctx.ctx, ctx.global().v, "constMapWriteFailed");
    bool constMapWriteFailed = !JS_IsUndefined(constMapWriteFlag);
    JS_FreeValue(ctx.ctx, constMapWriteFlag);
    if (constMapWriteFailed) {
        std::println(stderr, "FAIL: const map write was not rejected.");
        JS_FreeValue(ctx.ctx, obj);
        return 1;
    }

    std::println("JS call done ============");
    JS_FreeValue(ctx.ctx, obj);
    std::println("PASS");
}
