#include "ElementQuickJsBinding/binding.hxx"
#include "ElementQuickJsBinding/ejs_obj.hxx"
#include <EatiEssentials/container_and_view_and_ranges/container_and_view_and_range.hxx>
#include <EatiEssentials/container_and_view_and_ranges/container_lease.hxx>
#include "metapp/interfaces/metaclass.h"
#include "pimpl.hpp"
#include "quickjs/quickjs.h"
#include <cassert>
#include <format>
#include <map>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
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

static bool isRefVariant(const metapp::Variant &cppVal) {
	return UNWRAP(cppVal.getMetaType()).isReference();
}

static bool isReferenceOrPointerVariant(const metapp::Variant &cppVal) {
	auto &type = UNWRAP(cppVal.getMetaType());
	return type.isReference() || type.isPointer();
}

static metapp::Variant makeRefVariantOfVariant(const metapp::Variant &cppVal) {
	return metapp::Variant::reference(cppVal);
}

static JSValue throwJsTypeError(JSContext &ctx, const std::exception &ex) {
	JS_ThrowTypeError(&ctx, "%s", ex.what());
	return JS_EXCEPTION;
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


TypeInfo &Binding::findTypeInfoOfEJSObj(JSValue jsvalue) {
	auto ejsObjOpaque = getEJSObjOpaque(jsvalue);
	if (!ejsObjOpaque)
		throw std::invalid_argument{ "jsvalue is not a EJSObj." };

	return findOrThrow(
		regedTypes,
		[&](auto &typeInfo){
			return typeInfo.type == ejsObjOpaque->type;
		},
		[&](){ return std::runtime_error{
			std::format("类型 {} 没有被注册进 Binding。",
				getTypeName(*ejsObjOpaque->type)
		) }; }
	);

}

bool Binding::checkEJSObjLifetime(JSValue jsvalue) {
	auto opaque = getEJSObjOpaque(jsvalue);
	if (!opaque)
		return true;

	if (auto invalidReason = opaque->getLifetimeInvalidReason()) {
		JS_ThrowTypeError(ctx->ctx, "%s", invalidReason->c_str());
		return false;
	}
	return true;
}

metapp::Variant Binding::getCppObjRefByEJSObj(JSValue jsvalue) {
	auto opaque = getEJSObjOpaque(jsvalue);
	if (!opaque)
		throw std::invalid_argument{ "jsvalue is not a EJSObj." };

	switch (opaque->makingMethod) {
	case EJSObjOpaque::MakingMethod::twined:
		return opaque->cppObjRefInVariant;
	case EJSObjOpaque::MakingMethod::transplanted:
		assert(!opaque->ownedCppObjInVariant.isEmpty() && "Transplanted EJSObj must own a C++ object.");
		return makeRefVariantOfVariant(opaque->ownedCppObjInVariant);
	}

	assert(false && "Unknown EJSObj making method.");
	return { };
}

metapp::Variant Binding::getPointerToCppObjByEJSObj(JSValue jsvalue) {
	return getCppObjRefByEJSObj(jsvalue);
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

static metapp::Variant makeRefFromReferenceOrPointerVariant(const metapp::Variant &cppVal) {
	assert(isReferenceOrPointerVariant(cppVal) && "Expected a Variant holding reference or pointer.");
	return metapp::depointer(cppVal);
}

JSValue Binding::cpp2JSTwin(metapp::Variant cppObjRef) {
	auto &typeInfo = findRegedTypeInfoByCppType(regedTypes, getRawType(cppObjRef));
	if (typeInfo.jsVMRuntimeData.classID == 0)
		throw std::logic_error{ "Type is registered as translatable and cannot be converted to an EJSObj twin." };

	//auto cppObjRef = makeRefFromReferenceOrPointerVariant(cppObjRefOrPtr);
	Rc<LifetimeInformant::LifetimeInfo> lifetimeInfo;
	if (typeInfo.shareLifetimeInfoFunc)
		lifetimeInfo = typeInfo.shareLifetimeInfoFunc(cppObjRef);

	return makeEJSObj(*ctx->ctx, typeInfo.jsVMRuntimeData.classID, EJSObjOpaque{
		.makingMethod = EJSObjOpaque::MakingMethod::twined,
		.type = typeInfo.type,
		.cppObjRefInVariant = std::move(cppObjRef),
		.lifetimeInfoOfCppObj = std::move(lifetimeInfo),
		.ownedCppObjInVariant = {}
	});
}

JSValue Binding::cpp2JSTransplant(metapp::Variant cppObjRef) {
	auto &typeInfo = findRegedTypeInfoByCppType(regedTypes, getRawType(cppObjRef));
	if (typeInfo.jsVMRuntimeData.classID == 0)
		throw std::logic_error{ "Type is registered as translatable and cannot be transplanted as an EJSObj." };

	if (!typeInfo.moveable) {
		throw std::runtime_error{
			std::format(
				"Type {} is not moveable, hence it cannot move into the JavaScript VM.",
				getTypeName(*typeInfo.type)
			)
		};
	}

	return makeEJSObj(*ctx->ctx, typeInfo.jsVMRuntimeData.classID, EJSObjOpaque{
		.makingMethod = EJSObjOpaque::MakingMethod::transplanted,
		.type = typeInfo.type,
		.cppObjRefInVariant = {},
		.lifetimeInfoOfCppObj = {},
		.ownedCppObjInVariant = std::move(cppObjRef)
	});
}

// The value wrappered by cppValRef can be any type, including pointer.
JSValue Binding::cpp2JSTranslate(metapp::Variant cppValRef) {
	if (!isRefVariant(cppValRef))
		throw std::invalid_argument{ "cppValRef is not a reference variant." };

	auto &typeInfo = findRegedTypeInfoByCppType(regedTypes, getRawType(cppValRef));
	if (!typeInfo.translator) 
		throw std::invalid_argument{
			std::format("cppValRef Type {} is not translatable.", getTypeName(*typeInfo.type))
		};
		
	auto cppObjRef = isReferenceOrPointerVariant(cppValRef)
		? makeRefFromReferenceOrPointerVariant(cppValRef)
		: makeRefVariantOfVariant(cppValRef);
	return typeInfo.translator->translateToJS(typeInfo.translator->makeTranslateInput(cppObjRef));
}

JSValue Binding::cpp2JSAuto(metapp::Variant cppVal, LifetimeInformant::LifetimeInfo *lifetimeInfo) {
	if (cppVal.isEmpty())
		return JS_NULL;

	auto nonReferenceType = metapp::getNonReferenceMetaType(cppVal);
	if (nonReferenceType->isVoid())
		return JS_UNDEFINED;

	if (nonReferenceType->isArithmetic()) {
		if (nonReferenceType->getTypeKind() == metapp::tkBool)
			return JS_NewBool(ctx->ctx, cppVal.cast<bool>().get<bool>());
		if (nonReferenceType->isIntegral())
			return JS_NewInt64(ctx->ctx, cppVal.cast<long long>().get<long long>());
		return JS_NewFloat64(ctx->ctx, cppVal.cast<double>().get<double>());
	}

	if (cppVal.canGet<std::string>())
		return JS_NewString(ctx->ctx, cppVal.get<std::string>().c_str());

	if (cppVal.canGet<std::string_view>()) {
		auto str = cppVal.get<std::string_view>();
		return JS_NewStringLen(ctx->ctx, str.data(), str.size());
	}

	if (isReferenceOrPointerVariant(cppVal)) {
		auto jsObj = cpp2JSTwin(std::move(cppVal));
		if (lifetimeInfo && !JS_IsException(jsObj)) {
			auto opaque = getEJSObjOpaque(jsObj);
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
		auto cppObjRef = typeInfo->makeVariantRef(cppVal);
		return typeInfo->translator->translateToJS(typeInfo->translator->makeTranslateInput(cppObjRef));
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
				auto bindingFoundInMap = ctxToBindingMap.find(&ctx);
				assert(bindingFoundInMap != ctxToBindingMap.end() && "No Binding is associated with this QuickJS context.");
				auto &binding = *bindingFoundInMap->second;
				
				if (!binding.checkEJSObjLifetime(this_val))
					return JS_EXCEPTION;
				metapp::Variant subject = binding.getCppObjRefByEJSObj(this_val);
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
						return throwJsTypeError(*ctx, ex);
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
			freeEJSObjOpaque(val);
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
