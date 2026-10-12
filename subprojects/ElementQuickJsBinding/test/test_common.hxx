#include <string>
#include <iostream>
#include <map>
#include <span>
#include <string_view>
#include <vector>
#include <print>
#include <cassert>
#include <cstdlib>
#include <quickjs/quickjs.h>
#include <quickjs/quickjs-libc.h>
#include <quickjspp.hpp>
#include <metapp/variant.h>
#include <metapp/interfaces/metaclass.h>
#include <EatiEssentials/assert.hxx>
#include "ElementQuickJsBinding/binding.hxx"
#include "metapp/metarepo.h"

using namespace ElementEngine::QJSBinding;
using namespace Essentials::Memory;

// EESS_USE_CUSTOM_ASSERT_HANDLER requires the executable to provide this handler.
// Each test is a single-TU executable, so a non-inline definition here is fine.
[[noreturn]] void eatiEssentialsAssertHandler(const char *expr, const char *file, const int line) {
	std::cerr << "EatiEssentials assert failed: " << expr << '\n' << file << ':' << line << std::endl;
	std::abort();
}

static void initJs(qjs::Runtime &rt, qjs::Context &ctx) {
	js_std_init_handlers(rt.rt);
	JS_SetModuleLoaderFunc(rt.rt, nullptr, js_module_loader, nullptr);
	js_init_module_std(ctx.ctx, "std");
	js_init_module_os(ctx.ctx, "os");
}

struct CannotMove
{
	CannotMove() = default;
	CannotMove(const CannotMove &) = delete;
	CannotMove(CannotMove &&) noexcept = delete;
	CannotMove &operator=(const CannotMove &) = delete;
	CannotMove &operator=(CannotMove &&) noexcept = delete;
	~CannotMove() = default;

	std::string bar() {
		return "bar";
	}

	// Observable state, so tests can tell a twin (writes reach the original)
	// apart from a copy.
	int value = 0;
};


struct TestStruct2
{
	std::string name;
	int age;
	std::string bar() {
		return "bar";
	}
	std::string who() const {
		return name;
	}
	LifetimeInformant lifetimeInformant;

    TestStruct2(std::string name, int age)
        : name{ std::move(name) }, age{ age } {
        std::println("TestStruct2 has been created");
    }

    TestStruct2(const TestStruct2 &other)
        : name{ other.name }, age{ other.age } {
        std::println("TestStruct2 has been copied");
    }

    TestStruct2(TestStruct2 &&other) noexcept
        : name{ std::move(other.name) }, age{ other.age } {
        std::println("TestStruct2 has been moved");
    }

    ~TestStruct2() {
        std::println("TestStruct2 has been destroyed");
    }

    TestStruct2 &operator=(const TestStruct2 &other) = default;
    TestStruct2 &operator=(TestStruct2 &&other) noexcept = default;

};

struct TestStruct
{
	std::string name;
	int age;
	std::string foo() {
		return "foo";
	}

    TestStruct2 returnObj() {
        return { name, age };
    }

	std::vector<TestStruct2> returnObjVector() {
		return { { "alpha", 1 }, { "beta", 2 } };
	}

	// Returns a reference to the member container: the JS side gets a
	// non-owning live view (twin), whose lifetime is bound to this object.
	std::vector<TestStruct2> memberVector{ { "member-a", 10 }, { "member-b", 20 } };
	std::vector<TestStruct2> &getMemberVector() {
		return memberVector;
	}
	const std::vector<TestStruct2> &getConstVector() const {
		return memberVector;
	}

	// Stage 3: a span view over the member vector (non-owning live view).
	std::span<TestStruct2> getSpanView() {
		return memberVector;
	}
	// Returns a span BY VALUE: the span object itself is owned by the
	// returned Variant, but it still points into foreign memory. The
	// binding must refuse to transplant it.
	std::span<TestStruct2> returnSpanByValue() {
		return memberVector;
	}

	// Stage 3: map views / transplants (MetaMappable).
	std::map<std::string, std::string> mapping{ { "a", "alpha" }, { "b", "beta" } };
	std::map<std::string, std::string> &getMap() {
		return mapping;
	}
	const std::map<std::string, std::string> &getConstMap() const {
		return mapping;
	}
	std::map<std::string, int> returnMap() {
		return { { "one", 1 }, { "two", 2 } };
	}

	// Stage 3: string_view return value (converted into a JS string).
	std::string_view viewName() {
		return name;
	}

	// Stage 3: directed parameter marshalling (string_view parameter).
	std::string greet(std::string_view who) {
		return "hello " + std::string{ who };
	}

	// Stage 3: directed parameter marshalling (int parameters).
	int addAges(int a, int b) {
		return a + b;
	}

	CannotMove returnCannotMove() {
		return { };
	}


	LifetimeInformant lifetimeInformant;

    TestStruct(std::string name, int age)
        : name{ std::move(name) }, age{ age } {
        std::println("TestStruct has been created");
    }

    TestStruct(const TestStruct &other)
        : name{ other.name }, age{ other.age } {
        std::println("TestStruct has been copied");
    }

    TestStruct(TestStruct &&other) noexcept
        : name{ std::move(other.name) }, age{ other.age } {
        std::println("TestStruct has been moved");
    }

    ~TestStruct() {
        std::println("TestStruct has been destroyed");
    }

    TestStruct &operator=(const TestStruct &other) = default;
    TestStruct &operator=(TestStruct &&other) noexcept = default;
};

metapp::MetaRepo metaRepo;

template<>
struct metapp::DeclareMetaType<TestStruct> : metapp::DeclareMetaTypeBase<TestStruct>
{
	static void setup() {
		metaRepo.registerType<TestStruct>("TestStruct");
	}
	static const metapp::MetaClass *getMetaClass() {
		static const metapp::MetaClass metaClass {
			metapp::getMetaType<TestStruct>(),
			[](metapp::MetaClass &mc) {
				mc.registerVariable("name", &TestStruct::name);
				mc.registerVariable("age", &TestStruct::age);
				mc.registerCallable("foo", &TestStruct::foo);
                mc.registerCallable("returnObj", &TestStruct::returnObj);
				mc.registerCallable("returnCannotMove", &TestStruct::returnCannotMove);
				mc.registerCallable("returnObjVector", &TestStruct::returnObjVector);
				mc.registerCallable("getMemberVector", &TestStruct::getMemberVector);
				mc.registerCallable("getConstVector", &TestStruct::getConstVector);
				mc.registerCallable("getSpanView", &TestStruct::getSpanView);
				mc.registerCallable("returnSpanByValue", &TestStruct::returnSpanByValue);
				mc.registerCallable("getMap", &TestStruct::getMap);
				mc.registerCallable("getConstMap", &TestStruct::getConstMap);
				mc.registerCallable("returnMap", &TestStruct::returnMap);
				mc.registerCallable("viewName", &TestStruct::viewName);
				mc.registerCallable("greet", &TestStruct::greet);
				mc.registerCallable("addAges", &TestStruct::addAges);
			}
		};
		return &metaClass;
	}
};


template<>
struct metapp::DeclareMetaType<TestStruct2> : metapp::DeclareMetaTypeBase<TestStruct2>
{
	static void setup() {
		metaRepo.registerType<TestStruct2>("TestStruct2");
	}
	static const metapp::MetaClass *getMetaClass() {
		static const metapp::MetaClass metaClass {
			metapp::getMetaType<TestStruct2>(),
			[](metapp::MetaClass &mc) {
				mc.registerVariable("name", &TestStruct2::name);
				mc.registerVariable("age", &TestStruct2::age);
				mc.registerCallable("bar", &TestStruct2::bar);
				mc.registerCallable("who", &TestStruct2::who);
			}
		};
		return &metaClass;
	}
};

template<>
struct metapp::DeclareMetaType<CannotMove> : metapp::DeclareMetaTypeBase<CannotMove>
{
	static void setup() {
		metaRepo.registerType<CannotMove>("CannotMove");
	}
	static const metapp::MetaClass *getMetaClass() {
		static const metapp::MetaClass metaClass{
			metapp::getMetaType<CannotMove>(),
			[](metapp::MetaClass &mc) {
				mc.registerVariable("value", &CannotMove::value);
				mc.registerCallable("bar", &CannotMove::bar);
			}
		};
		return &metaClass;
	}
};


qjs::Value
try_eval_module(qjs::Context &ctx, qjs::Runtime &rt, std::string_view code) {
	try {
		return ctx.eval(code, "<import>", JS_EVAL_TYPE_MODULE);
	} catch (const qjs::exception &ex) {
		//js_std_dump_error(ctx);
		auto exc = ctx.getException();
		std::cerr << (exc.isError() ? "Error: " : "Throw: ") << (std::string)exc << std::endl;
		if ((bool)exc["stack"])
			std::cerr << (std::string)exc["stack"] << std::endl;

		js_std_free_handlers(rt.rt);
		return ctx.newObject();
	}
}
