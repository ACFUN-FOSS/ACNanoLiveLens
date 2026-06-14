#include "ElementQuickJsBinding/binding.hxx"
#include <EatiEssentials/container_and_view_and_ranges/container_and_view_and_range.hxx>
#include <EatiEssentials/container_and_view_and_ranges/container_lease.hxx>
#include "metapp/interfaces/metaclass.h"
#include "pimpl.hpp"
#include "quickjs/quickjs.h"
#include <stdexcept>
#include <rfl.hpp>

using namespace Essentials::ContainerAndView;
using namespace Essentials::Memory;

namespace ElementEngine::QJSBinding
{

static std::map<gsl::not_null<JSContext *>, gsl::not_null<Binding *>> ctxToBindingMap;

static std::string_view getTypeName(const metapp::MetaType &type) {
	return UNWRAP(type.getMetaClass()).getType(&type).getName();
}

// Normalize a Variant's C++ representation to the type stored in regedTypes.
// For example, T, T &, and T * should all resolve to T when looking up binding metadata.
static const metapp::MetaType &getRawType(const metapp::Variant &cppVal) {
	return UNWRAP(metapp::getNonReferenceMetaType(metapp::getPointedType(cppVal)));
}

bool TypeInfo::isTranslatableType() {
	return translator.has_value();
}

struct Binding::Detail
{
	AssoContainerLease<gsl::not_null<JSContext *>, gsl::not_null<Binding *>> ctxToBindingMapLease;

	Detail(Binding& binding LIFETIMEBOUND, qjs::Context &ctx LIFETIMEBOUND)
		: ctxToBindingMapLease{ ctxToBindingMap, ctx.ctx, &binding } { }
};

Binding::Binding(metapp::MetaRepo &metaRepo, qjs::Runtime &rt, qjs::Context &ctx)
	: metaRepo{ &metaRepo }, rt{ &rt }, ctx{ &ctx },
	  detail{ stdx::pimpl::make_unique<Detail>(*this, ctx) }  { }

struct JsTwinObjOpaque
{
	gsl::not_null<const metapp::MetaType *> type;
	LifetimeAwareWRef<Binding> binding;
	metapp::Variant cppObjPtrInVariant;
	Rc<LifetimeInformant::LifetimeInfo> lifetimeInfoOfCppObj;

	// For twined & translated value: this field will be empty,
	// For transplanted value: this field will be the C++ object.
	metapp::Variant ownedCppObjInVariant;
};

static JsTwinObjOpaque *getJsTwinObjOpaque(JSValue jsvalue) {
	//return static_cast<JsTwinObjOpaque *>(JS_GetOpaque2(&ctx, jsvalue, JS_GetClassID(jsvalue)));
	return static_cast<JsTwinObjOpaque *>(JS_GetOpaque(jsvalue, JS_GetClassID(jsvalue)));
}

TypeInfo &Binding::findTypeInfoOfJsTwin(JSValue jsvalue) {
	auto twinOpaque = getJsTwinObjOpaque(jsvalue);
	if (!twinOpaque)
		throw std::invalid_argument{ "jsvalue is not a twin." };

	return findOrThrow(
		regedTypes,
		[&](auto &typeInfo){
			return typeInfo.type == twinOpaque->type;
		},
		[&](){ return std::runtime_error{
			std::format("类型 {} 没有被注册进 Binding。",
				getTypeName(*twinOpaque->type)
		) }; }
	);

}

bool Binding::checkTwinObjLifetime(JSValue jsvalue) {
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
			JS_ThrowTypeError(ctx->ctx, "%s", msg.c_str());
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
			JS_ThrowTypeError(ctx->ctx, "%s", msg.c_str());
			return false;
		}
	}
	return true;
}

// bool Binding::checkTwinObjLifetime(JSValue jsvalue) {
// 	auto &opaque = EXCEPT(getJsTwinObjOpaque(jsvalue), "Not a twin object");

// 	if (!opaque.ownedCppObjInVariant.isEmpty()) {
// 		auto &typeJsInfo = findTypeInfoOfJsTwin(opaque.type);
// 		assert(typeJsInfo.classID != 0 && "Type not registered in regClass");
// 		return typeJsInfo.getPointerOfValue(opaque.ownedCppObjInVariant);
// 	}
// 	return opaque.cppObjPtrInVariant;
// }

static Binding &getBindingByTwinObjOpaque(JSValue jsvalue) {
	auto opaque = getJsTwinObjOpaque(jsvalue);
	if (!opaque)
		throw std::invalid_argument{ "jsvalue is not a twin." };

	if (!opaque->binding)
		throw std::runtime_error{ "The binding of the twin object is gone." };

	return *opaque->binding;
}

metapp::Variant Binding::getPointerToCppObjByJsTwinObject(JSValue jsvalue) {
	auto opaque = getJsTwinObjOpaque(jsvalue);
	if (!opaque)
		throw std::invalid_argument{ "jsvalue is not a twin." };

	if (!opaque->ownedCppObjInVariant.isEmpty())
		return opaque->ownedCppObjInVariant;
	return opaque->cppObjPtrInVariant;
}

static TypeInfo *findTypeInfoByCppType(std::vector<TypeInfo> &regedTypes, const metapp::MetaType &type) {
	auto typeIt = std::ranges::find_if(
		regedTypes,
		[&](auto &typeInfo) {
			return typeInfo.type->equal(metapp::getNonReferenceMetaType(&type));
		}
	);

	if (typeIt == regedTypes.end())
		return nullptr;

	return &*typeIt;
}

static TypeInfo &findRegedTypeInfoByCppType(std::vector<TypeInfo> &regedTypes, const metapp::MetaType &type) {
	auto typeInfo = findTypeInfoByCppType(regedTypes, type);
	if (!typeInfo) {
		throw std::runtime_error{
			std::format("Type {} is not registered in Binding.", getTypeName(type))
		};
	}
	return *typeInfo;
}

JSValue Binding::cpp2JSTwin(metapp::Variant cppObjPtr) {
	auto &typeInfo = findRegedTypeInfoByCppType(regedTypes, getRawType(cppObjPtr));
	assert(typeInfo.jsVMRuntimeData.classID != 0 && "Type not registered in QuickJS runtime.");

	JSValue jsObj = JS_NewObjectClass(ctx->ctx, typeInfo.jsVMRuntimeData.classID);
	if (JS_IsException(jsObj))
		return jsObj;

	Rc<LifetimeInformant::LifetimeInfo> lifetimeInfo;
	if (typeInfo.shareLifetimeInfoFunc)
		lifetimeInfo = typeInfo.shareLifetimeInfoFunc(cppObjPtr);

	gsl::owner<JsTwinObjOpaque *> opaque{ new JsTwinObjOpaque{
		.type = typeInfo.type,
		.binding = *this,
		.cppObjPtrInVariant = std::move(cppObjPtr),
		.lifetimeInfoOfCppObj = std::move(lifetimeInfo),
		.ownedCppObjInVariant = {}
	} };

	JS_SetOpaque(jsObj, opaque);
	return jsObj;
}

JSValue Binding::cpp2JSTransplant(metapp::Variant cppObj) {
	auto &typeInfo = findRegedTypeInfoByCppType(regedTypes, getRawType(cppObj));
	assert(typeInfo.jsVMRuntimeData.classID != 0 && "Type not registered in QuickJS runtime.");

	if (!typeInfo.moveable) {
		JS_ThrowTypeError(
			ctx->ctx,
			"Type %s is not moveable, hence it cannot move into the JavaScript VM.",
			getTypeName(*typeInfo.type).data()
		);
		return JS_EXCEPTION;
	}

	JSValue jsObj = JS_NewObjectClass(ctx->ctx, typeInfo.jsVMRuntimeData.classID);
	if (JS_IsException(jsObj))
		return jsObj;

	gsl::owner<JsTwinObjOpaque *> opaque{ new JsTwinObjOpaque{
		.type = typeInfo.type,
		.binding = *this,
		.cppObjPtrInVariant = metapp::Variant::reference(cppObj),
		.lifetimeInfoOfCppObj = {},
		.ownedCppObjInVariant = std::move(cppObj)
	} };

	JS_SetOpaque(jsObj, opaque);
	return jsObj;
}

JSValue Binding::cpp2JSTranslate(metapp::Variant cppObjPtr) {
	auto &typeInfo = findRegedTypeInfoByCppType(regedTypes, getRawType(cppObjPtr));
	if (!typeInfo.translator) {
		throw std::runtime_error{
			std::format("Type {} is not translatable.", getTypeName(*typeInfo.type))
		};
	}
	return typeInfo.translator->translateToJS(cppObjPtr);
}

JSValue Binding::cpp2JSAuto(metapp::Variant cppVal, LifetimeInformant::LifetimeInfo *lifetimeInfo) {
	if (cppVal.isEmpty())
		return JS_NULL;

	auto type = metapp::getNonReferenceMetaType(cppVal);
	if (type->isVoid())
		return JS_UNDEFINED;

	if (type->isArithmetic()) {
		if (type->getTypeKind() == metapp::tkBool)
			return JS_NewBool(ctx->ctx, cppVal.cast<bool>().get<bool>());
		if (type->isIntegral())
			return JS_NewInt64(ctx->ctx, cppVal.cast<long long>().get<long long>());
		return JS_NewFloat64(ctx->ctx, cppVal.cast<double>().get<double>());
	}

	if (cppVal.canGet<std::string>())
		return JS_NewString(ctx->ctx, cppVal.get<std::string>().c_str());

	if (cppVal.canGet<std::string_view>()) {
		auto str = cppVal.get<std::string_view>();
		return JS_NewStringLen(ctx->ctx, str.data(), str.size());
	}

	if (type->isReference() || type->isPointer()) {
		auto jsObj = cpp2JSTwin(std::move(cppVal));
		if (lifetimeInfo && !JS_IsException(jsObj)) {
			auto opaque = getJsTwinObjOpaque(jsObj);
			assert(opaque && "Created twin object without opaque.");
			opaque->lifetimeInfoOfCppObj = Rc<LifetimeInformant::LifetimeInfo>{
				lifetimeInfo,
				[](LifetimeInformant::LifetimeInfo *) {}
			};
		}
		return jsObj;
	}

	if (auto typeInfo = findTypeInfoByCppType(regedTypes, getRawType(cppVal));
		typeInfo && typeInfo->translator) {
		return typeInfo->translator->translateToJS(cppVal);
	}

	return cpp2JSTransplant(std::move(cppVal));
}

void Binding::regType(TypeInfoCreatingData &&typeInfoCd) {

	if (std::ranges::find_if(regedTypes, [&](auto &typeInfo){
		return typeInfo.type == typeInfoCd.type;
	}) != regedTypes.end()) {
		throw std::runtime_error{
			std::format("类型 {} 已被注册进 Binding。",
				getTypeName(*typeInfoCd.type)
			) };
	}

	auto typeInfo = rfl::as<TypeInfo>(std::move(typeInfoCd));

	// New type setup
    // Translatable type
	// We don't create type for translatable type.
    if (typeInfo.isTranslatableType()) {
		regedTypes.push_back(std::move(typeInfo));
        return;
    }

	// Non-translatable type

	// 1. Build prototype
	qjs::Value proto = ctx->newObject();

	// 1.1. Build proxy methods and add into the prototype
	struct JsTwinMethodData
	{
		gsl::not_null<const metapp::Variant *const> callable;
	};
	static std::map<int, JsTwinMethodData> jsProxyMethodsData;
	static int jsProxyMethodNewestMagicNum = 0;

	std::vector<std::pair<JSValue, std::string>> jsMethods;
	auto &metaClass = UNWRAP(typeInfo.type->getMetaClass());
	for (auto &cppFuncItem : metaClass.getCallableView()) {
		
		int funcMagicNum = ++jsProxyMethodNewestMagicNum;
		jsProxyMethodsData.insert({
			funcMagicNum,
			JsTwinMethodData{
				&cppFuncItem.asCallable()
			}
		});

		static auto proxyMethod = [](JSContext &ctx, JSValueConst this_val, std::span<JSValueConst> args,
		   int magic) -> JSValue {
				auto functionDataIt = jsProxyMethodsData.find(magic);
				assert(functionDataIt != jsProxyMethodsData.end() && "Cannot find JsTwinFunctionData with magicNum");
				auto &functionData = functionDataIt->second;
				auto &binding = getBindingByTwinObjOpaque(this_val);

				// 校验：通过 JS 上下文在全局映射中查找的 Binding，应与通过 JS 对象 opaque 获取的 Binding 一致
				{
					auto bindingFoundInMap = ctxToBindingMap.find(&ctx);
					assert(
						bindingFoundInMap != ctxToBindingMap.end()
						&& bindingFoundInMap->second == &binding
					);
				}
				
				if (!binding.checkTwinObjLifetime(this_val))
					return JS_EXCEPTION;
				metapp::Variant subject = binding.getPointerToCppObjByJsTwinObject(this_val);
				std::vector<metapp::Variant> args2CppResult;
				for (auto arg : args) {
					// NOLINENEXTLINE(cppcoreguidelines-pro-bounds-pointer-arithmetic)
					args2CppResult.push_back(binding.js2Cpp(arg));
				}
				
				// Call C++ function
				auto &metaCallable = UNWRAP(getNonReferenceMetaType(functionData.callable)->getMetaCallable());
				auto res = metaCallable.invoke(
					*functionData.callable,
					subject,
					args2CppResult
				);
				
				auto jsRes = binding.cpp2JSAuto(res);
				return jsRes;
		};

		jsMethods.push_back({
			JS_NewCFunctionMagic(ctx->ctx,
				[](JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv,
					int magic) noexcept -> JSValue {
					try {
						return proxyMethod(
							*ctx,
							this_val,
							std::span<JSValueConst>{ argv, static_cast<std::size_t>(argc) },
							magic
						);
					} catch (const std::exception &ex) {
						JS_ThrowTypeError(ctx, "%s", ex.what());
						return JS_EXCEPTION;
					}
				},
				cppFuncItem.getName().c_str(),
				cppFuncItem.asCallable().getMetaType()->getMetaCallable()->getParameterCountInfo(cppFuncItem.asCallable()).getMinParameterCount(),
				JS_CFUNC_generic_magic,
				funcMagicNum
			),
			cppFuncItem.getName()
		});

	}

	for (auto jsFunc : jsMethods) {
		proto[jsFunc.second.c_str()] = std::move(jsFunc.first);
	}

	// 1.2. Build proxy properties and add into the prototype
	// TODO

	// 2. Build a class for C++ type
	std::string className{ getTypeName(*typeInfo.type) };
	JSClassDef classDef{
		.class_name = className.c_str(),
		.finalizer = [](JSRuntime *rt, JSValue val) noexcept {
			gsl::owner<JsTwinObjOpaque *> opaque{ getJsTwinObjOpaque(val) };
			assert(opaque && "Not a JsTwinObject");
			delete opaque;
		},
		.gc_mark = [](JSRuntime *rt, JSValueConst val, JS_MarkFunc *mark_func) noexcept {
		},
		.call = nullptr,
		.exotic = nullptr
	};

	JSClassID classID = 0;
	JS_NewClassID(&classID);
	typeInfo.jsVMRuntimeData.classID = classID;

	if (JS_NewClass(JS_GetRuntime(ctx->ctx), classID, &classDef) < 0) {
		//JS_ThrowInternalError(ctx->ctx, "Can't register class %s", classDef.class_name);
		throw std::runtime_error{
			std::format("注册类 {} 失败。",
				className)
		};
	}

	JS_SetClassProto(ctx->ctx, classID, proto.v);
	proto.release();

	regedTypes.push_back(std::move(typeInfo));
}



}
