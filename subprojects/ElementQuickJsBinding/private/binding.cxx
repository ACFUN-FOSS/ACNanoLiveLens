#include "ElementQuickJsBinding/binding.hxx"
#include <EatiEssentials/container_and_view_and_ranges/container_and_view_and_range.hxx>
#include <EatiEssentials/container_and_view_and_ranges/container_lease.hxx>
#include "metapp/interfaces/metaclass.h"
#include "pimpl.hpp"
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
	return opaque->cppObjPtrInVariant;
}


void Binding::regType(TypeInfoCreatingData &&typeInfoCd) {
    auto typeInfo = rfl::as<TypeInfo>(std::move(typeInfoCd));


	// New type setup
    // Translatable type
	// We don't create type for translatable type.
    if (typeInfo.isTranslatableType()) {
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

	std::vector<std::pair<JSValue, std::string>> jsFuncs;
	auto &metaClass = UNWRAP(typeInfo.type->getMetaClass());
	for (auto &cppFuncItem : metaClass.getCallableView()) {
		
		int funcMagicNum = ++jsProxyMethodNewestMagicNum;
		jsProxyMethodsData.insert({
			funcMagicNum,
			JsTwinMethodData{
				&cppFuncItem.asCallable()
			}
		});

		auto proxyMethod = [](JSContext *ctx, JSValueConst this_val, int argc, JSValueConst *argv,
		   int magic) -> JSValue {
				auto functionDataIt = jsProxyMethodsData.find(magic);
				assert(functionDataIt != jsProxyMethodsData.end() && "Cannot find JsTwinFunctionData with magicNum");
				auto &functionData = functionDataIt->second;
				auto &binding = getBindingByTwinObjOpaque(this_val);

				{
					auto bindingFoundInMap = ctxToBindingMap.find(ctx);
					assert(
						bindingFoundInMap != ctxToBindingMap.end()
						&& bindingFoundInMap->second == &binding
					);
				}
				
				if (!binding.checkTwinObjLifetime(this_val))
					return JS_EXCEPTION;
				metapp::Variant subject = binding.getPointerToCppObjByJsTwinObject(this_val);
				std::vector<metapp::Variant> args;
				for (int i = 0; i < argc; ++i) {
					args.push_back(jsValue2Cpp(*ctx, argv[i]));
				}
				
				// Call C++ function
				auto &metaCallable = UNWRAP(getNonReferenceMetaType(functionData.callable)->getMetaCallable());
				if (auto &returnType = UNWRAP(metaCallable.getReturnType(*functionData.callable));
					!returnType.isVoid()) {
					auto returnTypeName = getDefaultMetaRepo().getType(&returnType).getName();
					
					// old >>>>>>>>>>
					// if (!doesTypeHasItsOwnJSRepresentation(returnType)) {
					// 	auto &typeJsInfo = metaType2TypeJsInfo[&returnType];
					// 	//assert(typeJsInfo.classID != 0 && "Type not registered in regClass");
					// 	if (typeJsInfo.classID == 0) {
					// 		std::println("Type {} not registered via regClass", returnTypeName);
					// 		std::terminate();
					// 	}
					// 	if (!typeJsInfo.moveable) {
					// 		JS_ThrowTypeError(ctx, "Return type of the function (%s) is not moveable, hence it cannot \"move\" into the JavaScript VM.", returnTypeName.c_str());
					// 		return JS_EXCEPTION;
					// 	}
					// }
					// old <<<<<<<<<<<<
				}
				auto res = metaCallable.invoke(
					*functionData.callable,
					subject,
					args
				);
				
				auto jsRes = variant2Js(*ctx, res);
				return jsRes.release();
		}

		jsFuncs.push_back({
			JS_NewCFunctionMagic(ctx->ctx,
				,
				cppFuncItem.getName().c_str(),
				cppFuncItem.asCallable().getMetaType()->getMetaCallable()->getParameterCountInfo(cppFuncItem.asCallable()).getMinParameterCount(),
				JS_CFUNC_generic_magic,
				funcMagicNum
			),
			cppFuncItem.getName()
		});
	}

	
	
}



}
