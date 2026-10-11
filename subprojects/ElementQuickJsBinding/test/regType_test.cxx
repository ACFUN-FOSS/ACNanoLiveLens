#include <print>
#include <quickjspp.hpp>
#include <metapp/allmetatypes.h>
#include <metapp/interfaces/metaclass.h>
#include <metapp/metarepo.h>
#include <EatiEssentials/assert.hxx>
#include "ElementQuickJsBinding/binding.hxx"

using namespace ElementEngine::QJSBinding;

// EESS_USE_CUSTOM_ASSERT_HANDLER requires the executable to provide this handler.
[[noreturn]] void eatiEssentialsAssertHandler(const char *expr, const char *file, const int line) {
	std::println(stderr, "EatiEssentials assert failed: {}\n{}:{}", expr, file, line);
	std::abort();
}

class LifetimeAwareTestClass
{
public:
	ESSM::LifetimeInformant lifetimeInformant;
};

metapp::MetaRepo metaRepo;

template<>
struct metapp::DeclareMetaType<LifetimeAwareTestClass> : metapp::DeclareMetaTypeBase<LifetimeAwareTestClass>
{
	static void setup() {
		metaRepo.registerType<LifetimeAwareTestClass>("LifetimeAwareTestClass");
	}
	static const metapp::MetaClass *getMetaClass() {
		static const metapp::MetaClass metaClass {
			metapp::getMetaType<LifetimeAwareTestClass>(),
			[](metapp::MetaClass &) { }
		};
		return &metaClass;
	}
};

int main() {
	qjs::Runtime rt;
	qjs::Context ctx{ rt };
	Binding binding{ metaRepo, rt, ctx };

	try {
		binding.regTypeStatic<LifetimeAwareTestClass>();
		std::println("regTypeStatic: type registered.");
	} catch (const std::exception &ex) {
		std::println(stderr, "FAIL: regTypeStatic threw: {}", ex.what());
		return 1;
	}

	// Duplicate registration must be rejected.
	try {
		binding.regTypeStatic<LifetimeAwareTestClass>();
		std::println(stderr, "FAIL: duplicate registration did not throw.");
		return 1;
	} catch (const std::exception &) { }

	// The registered type is lifetime-aware, so a twin object must share
	// its lifetime info and report the C++ object as alive.
	LifetimeAwareTestClass obj;
	JSValue twin = binding.cpp2JSTwin(metapp::Variant::reference(obj));
	if (JS_IsException(twin)) {
		auto exc = ctx.getException();
		std::println(stderr, "FAIL: cpp2JSTwin threw a JS exception: {}", (std::string)exc);
		return 1;
	}
	if (!binding.checkEJSObjLifetime(twin)) {
		std::println(stderr, "FAIL: a live C++ object was reported as invalid.");
		JS_FreeValue(ctx.ctx, twin);
		return 1;
	}
	JS_FreeValue(ctx.ctx, twin);

	std::println("PASS");
	return 0;
}
