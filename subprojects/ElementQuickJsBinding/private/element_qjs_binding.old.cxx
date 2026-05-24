#include "ElementQuickJsBinding/element_qjs_binding.hxx"
#include "metapp/implement/variant_intf.h"
#include <exception>
#include <print>
#include <gsl/gsl>
#include <quickjs/quickjs.h>
#include <metapp/variant.h>
#include <metapp/interfaces/metaclass.h>
#include <metapp/interfaces/metaindexable.h>

using namespace Essentials::Memory;

LifetimeInformant::LifetimeInformant()
	: info{ newBox(
		LifetimeInfo{ std::chrono::steady_clock::now() })} {
}
LifetimeInformant::~LifetimeInformant() {
	info->isAlive = false;
	info->deathTime = std::chrono::steady_clock::now();
}
	
LifetimeInformant::LifetimeInformant(const LifetimeInformant &other)
	: LifetimeInformant{ } {
}

LifetimeInformant::LifetimeInformant(LifetimeInformant &&other) noexcept
	: LifetimeInformant{ } {
	other.info->isMovedAway = true;
	other.info->movedAwayTime = std::chrono::steady_clock::now();
}

LifetimeInformant &LifetimeInformant::operator=(const LifetimeInformant &other) {
	if (this == &other)
		return *this;
	return *this;
}

LifetimeInformant &LifetimeInformant::operator=(LifetimeInformant &&other) noexcept {
	if (this == &other)
		return *this;
	return *this;
}



struct TypeJsInfo
{
	JSClassID classID;
	std::function<metapp::Variant(const metapp::Variant &v)> getPointerOfValue;
	bool moveable;
};

struct TypeWrapperJsInfo
{
	enum class WrapperType
	{
		SEQ_CONTAINER,
		RC,
		BOX,
		WEAK
	} wrapperType;

	std::function<metapp::Variant(const metapp::Variant &v)> getPointerOfValue;
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

static const metapp::MetaRepo *defaultMetaRepo = nullptr;

void setDefaultMetaRepo(const metapp::MetaRepo &metarepo) {
	defaultMetaRepo = &metarepo;
}

static const metapp::MetaRepo &getDefaultMetaRepo() {
	return UNWRAP(defaultMetaRepo);
}

std::unordered_map<gsl::not_null<const metapp::MetaType *>, TypeJsInfo, Hasher, EqualFn> metaType2TypeJsInfo;

std::unordered_map<gsl::not_null<const metapp::MetaType *>, TypeWrapperJsInfo, Hasher, EqualFn> metaType2TypeWrapperJsInfo;

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
	Rc<LifetimeInformant::LifetimeInfo> lifetimeInfoOfCppObj;

	// If the JS object is a reference to a C++ object, this field will be empty,
	// if the JS object owns the C++ object, this field will be the C++ object.
	metapp::Variant ownedCppObjInVariant;
};

JsTwinObjOpaque *getJsTwinObjOpaque(JSValue jsvalue) {
	//return static_cast<JsTwinObjOpaque *>(JS_GetOpaque2(&ctx, jsvalue, JS_GetClassID(jsvalue)));
	return static_cast<JsTwinObjOpaque *>(JS_GetOpaque(jsvalue, JS_GetClassID(jsvalue)));
}

bool checkTwinObjLifetime(JSContext &ctx, JSValue jsvalue) {
	auto opaque = getJsTwinObjOpaque(jsvalue);
	if (!opaque)
		return true;

	if (opaque->lifetimeInfoOfCppObj) {
		if (!opaque->lifetimeInfoOfCppObj->isAlive) {
			auto elapsedStr = [&]() -> std::string {
				if (!opaque->lifetimeInfoOfCppObj->deathTime)
					return std::string{};
				auto elapsed = std::chrono::steady_clock::now() - opaque->lifetimeInfoOfCppObj->deathTime.value();
				auto mins = std::chrono::duration_cast<std::chrono::minutes>(elapsed);
				auto secs = std::chrono::duration_cast<std::chrono::seconds>(elapsed - mins);
				return std::format("({}m{}s)", mins.count(), secs.count());
			}();
			auto msg = std::format(
				"Attempt to access a C++ object that has been already dead. "
				"The object has been dead before {}",
				elapsedStr
			);
			JS_ThrowTypeError(&ctx, "%s", msg.c_str());
			return false;
		}
		if (opaque->lifetimeInfoOfCppObj->isMovedAway) {
			auto elapsedStr = [&]() -> std::string {
				if (!opaque->lifetimeInfoOfCppObj->movedAwayTime)
					return std::string{};
				auto elapsed = std::chrono::steady_clock::now() - *opaque->lifetimeInfoOfCppObj->movedAwayTime;
				auto mins = std::chrono::duration_cast<std::chrono::minutes>(elapsed);
				auto secs = std::chrono::duration_cast<std::chrono::seconds>(elapsed - mins);
				return std::format("({}m{}s)", mins.count(), secs.count());
			}();
			auto msg = std::format(
				"Attempt to access a C++ object that has been already moved away. "
				"The object has been moved away before {}",
				elapsedStr
			);
			JS_ThrowTypeError(&ctx, "%s", msg.c_str());
			return false;
		}
	}
	return true;
}

// Does the type has its own JS representation (we will not twin a value of
// this type)
static bool doesTypeHasItsOwnJSRepresentation(const metapp::MetaType &type) {
	if (!type.isClass())
		return false;

	if (type.isPointer() || type.isReference())
		return false;

	// C++ sequence container will convert into JS array instead of creating a twin.
	if (type.isArray() || type.getMetaIterable)
		return true;

	// C++ enum will convert into JS string instead of creating a twin.
	if (type.isEnum())
		return true;

	// C++ std::string will convert into JS string instead of creating a twin.
	if (&type == metapp::getMetaType<std::string>())
		return true;

	return false;
	
	// C++ time_point will convert into JS date instead of creating a twin.
	//if (&type == metapp::getMetaType<std::chrono::time_point>())
	//	return true;
}

metapp::Variant getPointerOfJsTwinObject(JSContext &ctx, JSValue jsvalue) {
	auto opaque = getJsTwinObjOpaque(jsvalue);
	assert(opaque && "Not a JsTwinObject");

	if (!opaque->ownedCppObjInVariant.isEmpty()) {
		auto &typeJsInfo = metaType2TypeJsInfo[opaque->type];
		assert(typeJsInfo.classID != 0 && "Type not registered in regClass");
		return typeJsInfo.getPointerOfValue(opaque->ownedCppObjInVariant);
	}
	return opaque->cppObjPtrInVariant;
}

metapp::Variant jsValue2Cpp(JSContext &ctx, JSValue jsvalue) {
	auto opaque = getJsTwinObjOpaque(jsvalue);
	
	// Is javascript twin object
	if (opaque) {
		if (!checkTwinObjLifetime(ctx, jsvalue))
			return { };
		return getPointerOfJsTwinObject(ctx, jsvalue);
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


void regClass(
	qjs::Context &ctx,
	const metapp::MetaType &type,
	std::function<metapp::Variant(const metapp::Variant &)> getPointerOfObj,
	bool moveable
) {
	// 1. Build a prototype object with properties and methods
	qjs::Value proto = ctx.newObject();

	// 1.1 Build javascript proxies for every methods
	std::vector<std::pair<JSValue, std::string>> jsFuncs;
	auto &metaClass = UNWRAP(type.getMetaClass());
	for (auto &cppFuncItem : metaClass.getCallableView()) {
		
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
							if (!checkTwinObjLifetime(*ctx, this_val))
								return JS_EXCEPTION;
							metapp::Variant subject = getPointerOfJsTwinObject(*ctx, this_val);
							std::vector<metapp::Variant> args;
							for (int i = 0; i < argc; ++i) {
								args.push_back(jsValue2Cpp(*ctx, argv[i]));
							}
							
							// Call C++ function
							auto &metaCallable = UNWRAP(getNonReferenceMetaType(*functionData.callable)->getMetaCallable());
							if (auto &returnType = UNWRAP(metaCallable.getReturnType(*functionData.callable));
								!returnType.isVoid()) {
								auto returnTypeName = getDefaultMetaRepo().getType(&returnType).getName();
								
								if (!doesTypeHasItsOwnJSRepresentation(returnType)) {

									auto &typeJsInfo = metaType2TypeJsInfo[&returnType];
									//assert(typeJsInfo.classID != 0 && "Type not registered in regClass");
									if (typeJsInfo.classID == 0) {
										std::println("Type {} not registered via regClass", returnTypeName);
										std::terminate();
									}
									if (!typeJsInfo.moveable) {

										JS_ThrowTypeError(ctx, "Return type of the function (%s) is not moveable, hence it cannot \"move\" into the JavaScript VM.", returnTypeName.c_str());
										return JS_EXCEPTION;
									}
								}
							}
							auto res = metaCallable.invoke(
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
			gsl::owner<JsTwinObjOpaque *> opaque{ getJsTwinObjOpaque(val) };
			assert(opaque && "Not a JsTwinObject");
			delete opaque;
		},
		.gc_mark = [](JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) {
		},
		.call = nullptr,
		.exotic = nullptr
	};

	auto &currTypeJsInfo = metaType2TypeJsInfo[&type];
	currTypeJsInfo.getPointerOfValue = getPointerOfObj;
	currTypeJsInfo.moveable = moveable;

	JSClassID classID = 0;
	JS_NewClassID(&classID);
	currTypeJsInfo.classID = classID;
	JS_NewClass(JS_GetRuntime(ctx.ctx), currTypeJsInfo.classID, &classDef);
	JS_SetClassProto(ctx.ctx, currTypeJsInfo.classID, proto.v);
	proto.release();
}

qjs::Value makeRefJsTwinObject(JSContext &ctx, const metapp::MetaType &type, metapp::Variant cppObj, Rc<LifetimeInformant::LifetimeInfo> lifetimeInfoOfCppObj) {


	//auto jsfunc = try_eval(ctx, runtime, "tests").as<std::function<void(MyWidget *)>>();

	auto &typeJsInfo = metaType2TypeJsInfo[&type];
	assert(typeJsInfo.classID != 0 && "Type not registered in regClass");

	// std::string *d = new std::string("Hello from JsTwinObject");
	//qjs::Value dd{ d };
	//qjs::js_traits<qjs::Value>::wrap(ctx.ctx, d);


	JSValue jsObj = JS_NewObjectClass(&ctx, typeJsInfo.classID);

	gsl::owner<JsTwinObjOpaque *> opaque{ new JsTwinObjOpaque{
		gsl::not_null<const metapp::MetaType *>{ &type },
		cppObj,
		std::move(lifetimeInfoOfCppObj)
	}};

	JS_SetOpaque(jsObj, opaque);
	if (JS_IsException(jsObj))
		return jsObj;
	//JS_SetPrototype(ctx.ctx, jsObj, JS_GetClassProto(ctx.ctx, typeJsInfo.classID));
	return { &ctx, std::move(jsObj) };
}

qjs::Value makeOwnedJsTwinObject(JSContext &ctx, const metapp::MetaType &type, metapp::Variant cppObj) {
	auto &typeJsInfo = metaType2TypeJsInfo[&type];
	assert(typeJsInfo.classID != 0 && "Type not registered in regClass");

	JSValue jsObj = JS_NewObjectClass(&ctx, typeJsInfo.classID);

	gsl::owner<JsTwinObjOpaque *> opaque{ new JsTwinObjOpaque{
		gsl::not_null<const metapp::MetaType *>{ &type },
		cppObj,
		{},
		std::move(cppObj)
	}};

	JS_SetOpaque(jsObj, opaque);
	if (JS_IsException(jsObj))
		return jsObj;
	return { &ctx, std::move(jsObj) };
}

qjs::Value makeJsArrayByCppSeqContainer(JSContext &ctx, metapp::Variant cppSeqContainer) {
	auto metaType = metapp::getNonReferenceMetaType(cppSeqContainer);
	auto metaIndexable = metaType->getMetaIndexable();
	auto sizeInfo = metaIndexable->getSizeInfo(cppSeqContainer);
	auto size = sizeInfo.getSize();

	JSValue jsArray = JS_NewArray(&ctx);
	for (std::size_t i = 0; i < size; ++i) {
		auto elementVar = metaIndexable->get(cppSeqContainer, i);
		auto jsVal = variant2Js(ctx, elementVar);
		JS_DefinePropertyValueUint32(&ctx, jsArray, static_cast<uint32_t>(i), jsVal.val, JS_PROP_C_W_E);
	}
	return { &ctx, std::move(jsArray) };
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

	if (val.getMetaType()->isReference()) {
		// TODO
		return makeRefJsTwinObject(ctx, UNWRAP(val.getMetaType()), val);
	}
	if (val.getMetaType()->isPointer()) {
		return makeRefJsTwinObject(ctx, UNWRAP(val.getMetaType()), val);
	}

	if (auto jsInfo = metaType2TypeWrapperJsInfo.find(val.getMetaType());
		jsInfo != metaType2TypeWrapperJsInfo.end()) {
		// Is sequence container of objects
		switch (jsInfo->second.wrapperType) {
		case TypeWrapperJsInfo::WrapperType::SEQ_CONTAINER:
			return makeJsArrayByCppSeqContainer(ctx, val);
        case TypeWrapperJsInfo::WrapperType::RC:
        case TypeWrapperJsInfo::WrapperType::BOX:
        case TypeWrapperJsInfo::WrapperType::WEAK:
            break;
        }
    }

	// Is object
	return makeOwnedJsTwinObject(ctx, UNWRAP(val.getMetaType()), val);


	assert(false && "Unsupported variant type");
	//return ctx.newValue(JS_UNDEFINED);
}
