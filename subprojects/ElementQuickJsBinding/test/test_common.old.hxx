#include <string>
#include <iostream>
#include <vector>
#include <print>
#include <quickjs/quickjs.h>
#include <quickjs/quickjs-libc.h>
#include <quickjspp.hpp>
#include <metapp/variant.h>
#include <metapp/interfaces/metaclass.h>
#include "element_qjs_binding.hxx"
#include "metapp/metarepo.h"

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
};


struct TestStruct2
{
	std::string name;
	int age;
	std::string bar() {
		return "bar";
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

	// This doesn't compile, cause metapp requires T of a "MetaIndexable"
	// to be resizeable, std::vector<T>::resize requires T can be construct with
	// empty arguments.....
	// what to do?
	std::vector<TestStruct2> returnObjVector() {
		//std::vector<TestStruct2> a{ };

		return { };
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
