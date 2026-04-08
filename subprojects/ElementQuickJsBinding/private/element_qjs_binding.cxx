#include <gsl/gsl>
#include <quickjs/quickjs.h>
#include <quickjspp.hpp>
#include <metapp/variant.h>
#include <metapp/allmetatypes.h>
#include <metapp/interfaces/metaclass.h>

struct TypeJsInfo
{
	JSClassID classID;

};

class Hasher
{
public:
	size_t operator() (const gsl::not_null<const metapp::MetaType *> &key) const {
		return reinterpret_cast<size_t>(&*key);
	}
};

class EqualFn
{
public:
	bool operator() (const gsl::not_null<const metapp::MetaType *> &t1, const gsl::not_null<const metapp::MetaType *> &t2) const {
		return t1->equal(&*t2);
	}
};

std::unordered_map<gsl::not_null<const metapp::MetaType *>, TypeJsInfo, Hasher, EqualFn> metaType2TypeJsInfo;

void printJsValue(qjs::Context &ctx, JSValue jsval) {
	auto jsStr = JS_ToString(ctx.ctx, jsval);
	auto cStr = JS_ToCString(ctx.ctx, jsStr);
	if (cStr)
		std::cout << cStr << std::endl;
	std::cout << "(undefined)" << std::endl;
}

struct JsTwinObjOpaque
{
	gsl::not_null<const metapp::MetaType *> type;
	metapp::Variant cppObjPtrInVariant;
	metapp::Variant cppObjRefInVariant;
};

JsTwinObjOpaque *getJsTwinObjOpaque(JSValue jsvalue) {
	//return static_cast<JsTwinObjOpaque *>(JS_GetOpaque2(&ctx, jsvalue, JS_GetClassID(jsvalue)));
	return static_cast<JsTwinObjOpaque *>(JS_GetOpaque(jsvalue, JS_GetClassID(jsvalue)));
}

metapp::Variant unwrapJsTwinObject(JSContext &ctx, JSValue jsvalue) {
	auto opaque = getJsTwinObjOpaque(jsvalue);
	assert(opaque && "Not a JsTwinObject");
	return opaque->cppObjPtrInVariant;
}

metapp::Variant jsValue2Cpp(JSContext &ctx, JSValue jsvalue) {
	auto opaque = getJsTwinObjOpaque(jsvalue);
	
	// Is javascript twin object
	if (opaque) {
		return unwrapJsTwinObject(ctx, jsvalue);
	}

	// Is primitive type
	if (JS_IsNumber(jsvalue)) {
		double num;
		JS_ToFloat64(&ctx, &num, jsvalue);
		return metapp::Variant(num);
	} else if (JS_IsString(jsvalue)) {
		size_t len;
		const char *str = JS_ToCStringLen(&ctx, &len, jsvalue);
		metapp::Variant varStr{ std::string{ str, len } };
		JS_FreeCString(&ctx, str);
		return varStr;
	} else if (JS_IsBool(jsvalue)) {
		bool b = JS_ToBool(&ctx, jsvalue);
		return metapp::Variant(b);
	} else {
		assert("TODO: Show an error message here");
		return { };
	}

}

qjs::Value variant2Js(JSContext &ctx, const metapp::Variant &val);

/**
 * @brief JS 孪生对象的代理函数数据
 */
struct JsTwinFunctionData
{
	gsl::not_null<const metapp::Variant *> callable;
};
std::map<int, JsTwinFunctionData> magic2JsTwinFunctionData;
int newestMagicNum = 0;


void regClass(qjs::Context &ctx, const metapp::MetaType &type) {
	auto &currTypeJsInfo = metaType2TypeJsInfo[&type];

	// 1. Build a prototype object with properties and methods
	qjs::Value proto = ctx.newObject();

	// 1.1 Build javascript proxies for every methods
	std::vector<std::pair<JSValue, std::string>> jsFuncs;
	auto metaClass = type.getMetaClass();
	for (auto &cppFuncItem : metaClass->getCallableView()) {
		
		int funcMagicNum = ++newestMagicNum;
		magic2JsTwinFunctionData.insert({
			funcMagicNum,
			JsTwinFunctionData{
				&cppFuncItem.asCallable()
			}
		});

		jsFuncs.push_back(
			{
				JS_NewCFunctionMagic(ctx.ctx,
					[](JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv,
					   int magic) noexcept -> JSValue {
							auto functionDataIt = magic2JsTwinFunctionData.find(magic);
							assert(functionDataIt != magic2JsTwinFunctionData.end() && "Cannot find JsTwinFunctionData wiith magicNum");
							auto &functionData = functionDataIt->second;

							
							// subject: pointer to C++ object
							metapp::Variant subject = unwrapJsTwinObject(*ctx, this_val);
							std::vector<metapp::Variant> args;
							for (int i = 0; i < argc; ++i) {
								args.push_back(jsValue2Cpp(*ctx, argv[i]));
							}
							
							// Call C++ function
							auto res = getNonReferenceMetaType(*functionData.callable)->getMetaCallable()->invoke(
								*functionData.callable,
								subject,
								args
							);
							
							auto jsRes = variant2Js(*ctx, res);
							return jsRes.release();
					},
					cppFuncItem.getName().c_str(),
					cppFuncItem.asCallable().getMetaType()->getMetaCallable()->getParameterCountInfo(cppFuncItem.asCallable()).getMinParameterCount(),
					JS_CFUNC_generic_magic,
					funcMagicNum
				),
				cppFuncItem.getName()
			}
		);
	}

	for (auto jsFunc : jsFuncs) {
		//auto func = qjs::Value{ ctx.ctx, jsFunc };
		proto[jsFunc.second.c_str()] = std::move(jsFunc.first);
	}

	//auto prop = JS_GetPropertyStr(ctx.ctx, proto.v, "string");
	//printJsValue(ctx, prop);
	//JS_FreeValue(ctx.ctx, prop);

	// 2. Build a class
	JSClassDef classDef{
		.class_name = type.getMetaClass()->getType(&type).getName().c_str(),
		.finalizer = [](JSRuntime *rt, JSValue val) {
			auto opaque = getJsTwinObjOpaque(val);
			assert(opaque && "Not a JsTwinObject");
			delete opaque;
		},
		.gc_mark = [](JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) {
		},
		.call = nullptr,
		.exotic = nullptr
	};
	JS_NewClassID(&currTypeJsInfo.classID);
	JS_NewClass(JS_GetRuntime(ctx.ctx), currTypeJsInfo.classID, &classDef);
	JS_SetClassProto(ctx.ctx, currTypeJsInfo.classID, proto.v);
	proto.release();
}

qjs::Value makeJsTwinObject(qjs::Context &ctx, const metapp::MetaType &type, metapp::Variant cppObj) {


	//auto jsfunc = try_eval(ctx, runtime, "tests").as<std::function<void(MyWidget *)>>();

	auto &typeJsInfo = metaType2TypeJsInfo[&type];
	assert(typeJsInfo.classID != 0 && "Type not registered in regClass");

	// std::string *d = new std::string("Hello from JsTwinObject");
	//qjs::Value dd{ d };
	//qjs::js_traits<qjs::Value>::wrap(ctx.ctx, d);


	JSValue jsObj = JS_NewObjectClass(ctx.ctx, typeJsInfo.classID);

	gsl::owner<JsTwinObjOpaque *> opaque{ new JsTwinObjOpaque{
		gsl::not_null<const metapp::MetaType *>{ &type },
		cppObj,
		metapp::Variant{ }	// FIXME
	}};

	JS_SetOpaque(jsObj, opaque);
	if (JS_IsException(jsObj))
		return jsObj;
	//JS_SetPrototype(ctx.ctx, jsObj, JS_GetClassProto(ctx.ctx, typeJsInfo.classID));
	return { ctx.ctx, std::move(jsObj) };
}


                                                                                                                                                                                                                                                                                                                    
qjs::Value cppValue2Js(JSContext &ctx, std::string_view &&val) {
	return { &ctx, JS_NewString(&ctx, val.data()) };
}
qjs::Value cppValue2Js(JSContext &ctx, float val) {
	return { &ctx, JS_NewFloat64(&ctx, val) };
}
qjs::Value cppValue2Js(JSContext &ctx, int val) {
	return { &ctx, JS_NewInt32(&ctx, val) };
}
qjs::Value cppValue2Js(JSContext &ctx, bool val) {
	return { &ctx, JS_NewBool(&ctx, val) };
}

qjs::Value variant2Js(JSContext &ctx, const metapp::Variant &val) {
	if (val.isEmpty()) {
		return { &ctx, JS_NULL };
	}
	if (val.getMetaType()->isArithmetic()) {
		if (val.getMetaType()->isIntegral()) {
			auto intVal = val.get<int>();
			return cppValue2Js(ctx, intVal);
		} else if (val.getMetaType()->isFloat()) {
			auto doubleVal = val.get<float>();
			return cppValue2Js(ctx, doubleVal);
		} else {
			assert("Unsupported arithmetic type");
			return { &ctx, JS_UNDEFINED };
		}
	}
	if (val.canGet<std::string_view>()) {
		auto strVal = val.get<std::string_view>();
		return cppValue2Js(ctx, std::move(strVal));
	}
	if (val.canGet<std::string>()) {
		auto strVal = val.get<std::string>();
		return cppValue2Js(ctx, std::move(strVal));
	}


	assert(false && "Unsupported variant type");
	//return ctx.newValue(JS_UNDEFINED);
}
