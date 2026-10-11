#include "ElementQuickJsBinding/binding.hxx"
#include "ElementQuickJsBinding/ejs_obj.hxx"
#include <EatiEssentials/container_and_view_and_ranges/container_and_view_and_range.hxx>
#include <EatiEssentials/container_and_view_and_ranges/container_lease.hxx>
#include "metapp/interfaces/metaclass.h"
#include "metapp/interfaces/metamappable.h"
#include "pimpl.hpp"
#include "quickjs/quickjs.h"
#include <cassert>
#include <charconv>
#include <format>
#include <map>
#include <print>
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

// Defined below; usable by types without a MetaClass (e.g. containers).
static std::string getTypeNameSafe(const metapp::MetaType &type);

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

	// Lazily created JS classes for container types that are not explicitly
	// registered via regType. Keyed by the interned container metatype.
	std::map<gsl::not_null<const metapp::MetaType *>, JSClassID> containerClassIds;

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
		// Return the owned value Variant as-is: its data address is the C++
		// object itself, which is exactly what metapp's getPointer() maps to
		// the member-function instance. (Wrapping it with Variant::reference
		// would create a reference to the Variant object, and the member call
		// would operate on the Variant's internals instead.)
		return opaque->ownedCppObjInVariant;
	}

	assert(false && "Unknown EJSObj making method.");
	return { };
}

metapp::Variant Binding::getPointerToCppObjByEJSObj(JSValue jsvalue) {
	return getCppObjRefByEJSObj(jsvalue);
}

metapp::Variant Binding::js2Cpp(JSValue jsVal) {
	if (JS_IsObject(jsVal))
		return getCppObjRefByEJSObj(jsVal);

	if (JS_IsNumber(jsVal)) {
		double num = 0;
		JS_ToFloat64(ctx->ctx, &num, jsVal);
		return metapp::Variant{ num };
	}

	if (JS_IsString(jsVal)) {
		size_t len = 0;
		const char *str = JS_ToCStringLen(ctx->ctx, &len, jsVal);
		metapp::Variant varStr{ std::string{ str, len } };
		JS_FreeCString(ctx->ctx, str);
		return varStr;
	}

	if (JS_IsBool(jsVal))
		return metapp::Variant{ JS_ToBool(ctx->ctx, jsVal) != 0 };

	if (JS_IsNull(jsVal) || JS_IsUndefined(jsVal) || JS_IsUninitialized(jsVal))
		return metapp::Variant{ };

	throw std::invalid_argument{ "Unsupported JS value type." };
}

metapp::Variant Binding::js2CppAs(const metapp::MetaType &targetType, JSValue jsVal) {
	// EJSObj arguments are passed through as live C++ references; whether the
	// reference is usable as the parameter type is decided by metapp's
	// invoke-time cast (with const-correctness checks).
	if (JS_IsObject(jsVal)) {
		auto opaque = getEJSObjOpaque(jsVal);
		if (!opaque)
			throw std::invalid_argument{ "Cannot marshal a non-EJSObj JS object into a C++ parameter." };
		if (auto invalidReason = opaque->getLifetimeInvalidReason())
			throw std::runtime_error{ *invalidReason };
		return getCppObjRefByEJSObj(jsVal);
	}

	auto generic = js2Cpp(jsVal);
	if (generic.isEmpty())
		return generic;

	// Validate the conversion instead of returning the cast result: the cast
	// result may be a *view* (e.g. std::string_view) into the temporaries of
	// this function, which would dangle right after the return. The original
	// Variant is owned by the caller's argument list, and metapp's invoke
	// performs the same cast with its temporaries alive during the call.
	try {
		[[maybe_unused]] auto probe = generic.cast(&targetType);
	} catch (const std::exception &) {
		throw std::runtime_error{
			std::format("Cannot marshal the JS value into C++ parameter type {}.",
				getTypeNameSafe(targetType))
		};
	}
	return generic;
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

// Name for a type that has no MetaClass (e.g. std::vector<T> never registered
// into the meta repo). Falls back to composing the element type name.
static std::string getTypeNameSafe(const metapp::MetaType &type) {
	if (auto *metaClass = type.getMetaClass()) {
		if (auto name = metaClass->getType(&type).getName(); !name.empty())
			return name;
	}
	if (type.getUpTypeCount() > 0) {
		if (auto *upType = type.getUpType(); upType) {
			if (auto name = getTypeNameSafe(*upType); !name.empty())
				return std::format("CppContainer<{}>", name);
		}
	}
	return std::format("CppContainer#{}", reinterpret_cast<const void *>(&type));
}

static bool parseDecimalIndex(std::string_view name, std::uint64_t &outIndex) {
	if (name.empty() || name.size() > 20)
		return false;
	auto [ptr, ec] = std::from_chars(name.data(), name.data() + name.size(), outIndex);
	return ec == std::errc{ } && ptr == name.data() + name.size();
}

// The Variant that backs a container EJSObj: a reference into an existing
// C++ container for twined views, or the owned container for transplanted ones.
static metapp::Variant &containerVariantOf(EJSObjOpaque &opaque) {
	switch (opaque.makingMethod) {
	case EJSObjOpaque::MakingMethod::twined:
		return opaque.cppObjRefInVariant;
	case EJSObjOpaque::MakingMethod::transplanted:
		return opaque.ownedCppObjInVariant;
	}
	assert(false && "Unknown EJSObj making method.");
	return opaque.ownedCppObjInVariant;
}

// Common prologue for container exotic handlers: lifetime check for twined
// views, so accessing a view after its source died raises a JS TypeError.
static bool checkContainerLifetimeOrThrow(JSContext *rawCtx, EJSObjOpaque &opaque) {
	if (auto invalidReason = opaque.getLifetimeInvalidReason()) {
		JS_ThrowTypeError(rawCtx, "%s", invalidReason->c_str());
		return false;
	}
	return true;
}

// Exotic handler backing JS live container views: index reads and "length"
// are answered directly from the C++ container referenced by the opaque.
static int containerGetOwnProperty(JSContext *rawCtx, JSPropertyDescriptor *desc,
	JSValueConst obj, JSAtom prop) noexcept
{
	auto bindingFoundInMap = ctxToBindingMap.find(rawCtx);
	assert(bindingFoundInMap != ctxToBindingMap.end() && "No Binding is associated with this QuickJS context.");
	auto &binding = *bindingFoundInMap->second;

	auto opaque = getEJSObjOpaque(obj);
	if (!opaque)
		return 0;

	gsl::owner<const char *> propName{ JS_AtomToCString(rawCtx, prop) };
	if (!propName)
		return -1;
	std::string name{ propName };
	JS_FreeCString(rawCtx, propName);

	try {
		if (!checkContainerLifetimeOrThrow(rawCtx, *opaque))
			return -1;

		auto &containerVar = containerVariantOf(*opaque);
		if (containerVar.isEmpty())
			return 0;

		auto &containerType = UNWRAP(metapp::getNonReferenceMetaType(containerVar));
		auto *metaIndexable = containerType.getMetaIndexable();
		assert(metaIndexable && "Container EJSObj whose type is not indexable.");
		auto size = metaIndexable->getSizeInfo(containerVar).getSize();

		JSValue jsVal;
		if (name == "length") {
			jsVal = JS_NewInt64(rawCtx, static_cast<std::int64_t>(size));
		} else if (std::uint64_t index{ }; parseDecimalIndex(name, index) && index < size) {
			auto elementVar = metaIndexable->get(containerVar, index);
			jsVal = binding.cpp2JSAuto(std::move(elementVar));
		} else {
			return 0;
		}

		if (JS_IsException(jsVal))
			return -1;
		if (!desc) {
			JS_FreeValue(rawCtx, jsVal);
			return 1;
		}
		// Index reads are plain data properties; writes go through
		// containerSetProperty, so no writable flag here.
		desc->flags = JS_PROP_ENUMERABLE;
		desc->value = jsVal;
		desc->getter = JS_UNDEFINED;
		desc->setter = JS_UNDEFINED;
		return 1;
	} catch (const std::exception &ex) {
		JS_ThrowTypeError(rawCtx, "%s", ex.what());
		return -1;
	}
}

// Exotic write-back: index assignment pushes the value (marshalled by js2Cpp)
// into the C++ container. Const containers and out-of-range indices throw.
static int containerSetProperty(JSContext *rawCtx, JSValueConst obj,
	JSAtom prop, JSValueConst val, JSValueConst receiver, int flags) noexcept
{
	auto bindingFoundInMap = ctxToBindingMap.find(rawCtx);
	assert(bindingFoundInMap != ctxToBindingMap.end() && "No Binding is associated with this QuickJS context.");
	auto &binding = *bindingFoundInMap->second;

	auto opaque = getEJSObjOpaque(obj);
	if (!opaque)
		return 0;

	gsl::owner<const char *> propName{ JS_AtomToCString(rawCtx, prop) };
	if (!propName)
		return -1;
	std::string name{ propName };
	JS_FreeCString(rawCtx, propName);

	try {
		if (!checkContainerLifetimeOrThrow(rawCtx, *opaque))
			return -1;

		auto &containerVar = containerVariantOf(*opaque);
		if (containerVar.isEmpty())
			return 0;

		std::uint64_t index{ };
		if (!parseDecimalIndex(name, index)) {
			JS_ThrowTypeError(rawCtx,
				"Container views only support index assignment, not \"%s\".",
				name.c_str());
			return -1;
		}

		auto &containerType = UNWRAP(metapp::getNonReferenceMetaType(containerVar));
		if (containerType.isConst()) {
			JS_ThrowTypeError(rawCtx,
				"Attempt to modify a read-only (const) container view of %s.",
				getTypeNameSafe(containerType).c_str());
			return -1;
		}
		auto *metaIndexable = containerType.getMetaIndexable();
		assert(metaIndexable && "Container EJSObj whose type is not indexable.");
		if (index >= metaIndexable->getSizeInfo(containerVar).getSize()) {
			JS_ThrowRangeError(rawCtx,
				"Container index %llu is out of range.",
				static_cast<unsigned long long>(index));
			return -1;
		}

		// Directed marshalling towards the declared element type.
		auto *elementType = metaIndexable->getValueType(containerVar, index);
		auto valueVar = elementType
			? binding.js2CppAs(*elementType, val)
			: binding.js2Cpp(val);
		// MetaIndexable::set casts the value to the element type internally,
		// and throws (converted into a JS TypeError) on type mismatch.
		metaIndexable->set(containerVar, index, valueVar);
		return 1;
	} catch (const std::exception &ex) {
		JS_ThrowTypeError(rawCtx, "%s", ex.what());
		return -1;
	}
}

// Install Symbol.iterator on a container proto via a JS helper, so that
// for...of / spread syntax work on live container views.
static void installSeqContainerIterator(qjs::Context &ctx, qjs::Value &proto) {
	static bool installed = false;
	if (!installed) {
		ctx.eval(R"(
			function __qjs_installSeqContainerIterator(proto) {
				proto[Symbol.iterator] = function * () {
					const n = this.length;
					for (let i = 0; i < n; i++)
						yield this[i];
				};
			}
		)", "<qjsbinding-internal>", JS_EVAL_TYPE_GLOBAL);
		installed = true;
	}

	qjs::Value installer{ ctx.global()["__qjs_installSeqContainerIterator"] };
	if (JS_IsUndefined(installer.v))
		throw std::runtime_error{ "Failed to define the container iterator installer in the JS VM." };

	JSValueConst arg = proto.v;
	JSValue ret = JS_Call(ctx.ctx, installer.v, JS_UNDEFINED, 1, &arg);
	if (JS_IsException(ret))
		throw std::runtime_error{ "Failed to install Symbol.iterator for the container proto." };
	JS_FreeValue(ctx.ctx, ret);
}

// ---------------------------------------------------------------------------
// Map (MetaMappable) support
// ---------------------------------------------------------------------------

// Build a C++ key Variant for a mappable container from a JS property name.
// Returns nullopt when the name cannot be parsed as a key of keyType, so
// property reads can fall through to the prototype chain.
static std::optional<metapp::Variant> makeMapKeyVariant(const metapp::MetaType &keyType,
	const std::string &name)
{
	auto &rawKeyType = UNWRAP(metapp::getNonReferenceMetaType(&keyType));

	if (rawKeyType.getTypeKind() == metapp::tkStdString)
		return metapp::Variant{ name };

	if (rawKeyType.isArithmetic()) {
		if (rawKeyType.isIntegral()) {
			if (std::uint64_t index{ }; parseDecimalIndex(name, index))
				return metapp::Variant{ static_cast<long long>(index) };
			return std::nullopt;
		}
		if (double value{ };
			!name.empty() &&
			std::from_chars(name.data(), name.data() + name.size(), value).ec == std::errc{ })
			return metapp::Variant{ value };
		return std::nullopt;
	}

	throw std::runtime_error{
		std::format("Map key type {} is not supported from JavaScript.",
			getTypeNameSafe(rawKeyType))
	};
}

// Exotic handler backing JS live map views: property reads answer
// "length" (entry count) and key lookups from the C++ map.
static int mapGetOwnProperty(JSContext *rawCtx, JSPropertyDescriptor *desc,
	JSValueConst obj, JSAtom prop) noexcept
{
	auto bindingFoundInMap = ctxToBindingMap.find(rawCtx);
	assert(bindingFoundInMap != ctxToBindingMap.end() && "No Binding is associated with this QuickJS context.");
	auto &binding = *bindingFoundInMap->second;

	auto opaque = getEJSObjOpaque(obj);
	if (!opaque)
		return 0;

	gsl::owner<const char *> propName{ JS_AtomToCString(rawCtx, prop) };
	if (!propName)
		return -1;
	std::string name{ propName };
	JS_FreeCString(rawCtx, propName);

	try {
		if (!checkContainerLifetimeOrThrow(rawCtx, *opaque))
			return -1;

		auto &mapVar = containerVariantOf(*opaque);
		if (mapVar.isEmpty())
			return 0;

		auto &mapType = UNWRAP(metapp::getNonReferenceMetaType(mapVar));

		JSValue jsVal;
		if (name == "length") {
			std::int64_t count = 0;
			metapp::mappableForEach(mapVar, [&](const metapp::Variant &, const metapp::Variant &) {
				++count;
				return true;
			});
			jsVal = JS_NewInt64(rawCtx, count);
		} else {
			// Unknown names must fall through to the prototype chain, so an
			// unparseable key returns "not found" instead of throwing.
			auto keyType = mapType.getUpType(0);
			if (!keyType)
				return 0;
			auto keyVar = makeMapKeyVariant(*keyType, name);
			if (!keyVar)
				return 0;
			auto valueVar = metapp::mappableGet(mapVar, *keyVar);
			if (valueVar.isEmpty())
				return 0;
			jsVal = binding.cpp2JSAuto(std::move(valueVar), opaque->lifetimeInfoOfCppObj);
		}

		if (JS_IsException(jsVal))
			return -1;
		if (!desc) {
			JS_FreeValue(rawCtx, jsVal);
			return 1;
		}
		desc->flags = JS_PROP_ENUMERABLE;
		desc->value = jsVal;
		desc->getter = JS_UNDEFINED;
		desc->setter = JS_UNDEFINED;
		return 1;
	} catch (const std::exception &ex) {
		JS_ThrowTypeError(rawCtx, "%s", ex.what());
		return -1;
	}
}

// Exotic write-back for maps: key assignment inserts or updates the entry.
// Const maps and unsupported key names throw.
static int mapSetProperty(JSContext *rawCtx, JSValueConst obj,
	JSAtom prop, JSValueConst val, JSValueConst receiver, int flags) noexcept
{
	auto bindingFoundInMap = ctxToBindingMap.find(rawCtx);
	assert(bindingFoundInMap != ctxToBindingMap.end() && "No Binding is associated with this QuickJS context.");
	auto &binding = *bindingFoundInMap->second;

	auto opaque = getEJSObjOpaque(obj);
	if (!opaque)
		return 0;

	gsl::owner<const char *> propName{ JS_AtomToCString(rawCtx, prop) };
	if (!propName)
		return -1;
	std::string name{ propName };
	JS_FreeCString(rawCtx, propName);

	try {
		if (!checkContainerLifetimeOrThrow(rawCtx, *opaque))
			return -1;

		auto &mapVar = containerVariantOf(*opaque);
		if (mapVar.isEmpty())
			return 0;

		auto &mapType = UNWRAP(metapp::getNonReferenceMetaType(mapVar));
		if (mapType.isConst()) {
			JS_ThrowTypeError(rawCtx,
				"Attempt to modify a read-only (const) map view of %s.",
				getTypeNameSafe(mapType).c_str());
			return -1;
		}

		auto *keyType = mapType.getUpType(0);
		if (!keyType) {
			JS_ThrowTypeError(rawCtx, "Map type %s does not declare a key type.",
				getTypeNameSafe(mapType).c_str());
			return -1;
		}
		auto keyVar = makeMapKeyVariant(*keyType, name);
		if (!keyVar) {
			JS_ThrowTypeError(rawCtx,
				"\"%s\" cannot be interpreted as a map key.",
				name.c_str());
			return -1;
		}

		auto *mappedType = mapType.getUpTypeCount() >= 2 ? mapType.getUpType(1) : nullptr;
		auto valueVar = mappedType
			? binding.js2CppAs(*mappedType, val)
			: binding.js2Cpp(val);
		metapp::mappableSet(mapVar, *keyVar, valueVar);
		return 1;
	} catch (const std::exception &ex) {
		JS_ThrowTypeError(rawCtx, "%s", ex.what());
		return -1;
	}
}

// Native proto method keys(): returns a JS array of the map's keys, so the
// JS-side Symbol.iterator can walk key -> value pairs.
static JSValue mapKeysNative(JSContext *ctx, JSValueConst this_val,
	int argc, JSValueConst *argv) noexcept
{
	try {
		auto bindingFoundInMap = ctxToBindingMap.find(ctx);
		assert(bindingFoundInMap != ctxToBindingMap.end() && "No Binding is associated with this QuickJS context.");
		auto &binding = *bindingFoundInMap->second;

		auto opaque = getEJSObjOpaque(this_val);
		if (!opaque)
			throw std::runtime_error{ "\"keys\" was called on a non-container object." };
		if (!checkContainerLifetimeOrThrow(ctx, *opaque))
			return JS_EXCEPTION;

		auto &mapVar = containerVariantOf(*opaque);
		JSValue arr = JS_NewArray(ctx);
		try {
			std::int64_t index = 0;
			metapp::mappableForEach(mapVar, [&](const metapp::Variant &key, const metapp::Variant &) {
				JS_SetPropertyInt64(ctx, arr, index++, binding.cpp2JSAuto(key));
				return true;
			});
		} catch (...) {
			JS_FreeValue(ctx, arr);
			throw;
		}
		return arr;
	} catch (const std::exception &ex) {
		return throwJsTypeError(*ctx, ex);
	}
}

// Install Symbol.iterator on a map proto via a JS helper, so that
// for...of yields [key, value] pairs of the live map view.
static void installMapIterator(qjs::Context &ctx, qjs::Value &proto) {
	static bool installed = false;
	if (!installed) {
		ctx.eval(R"(
			function __qjs_installMapIterator(proto) {
				proto[Symbol.iterator] = function * () {
					const ks = this.keys();
					for (const k of ks)
						yield [k, this[k]];
				};
			}
		)", "<qjsbinding-internal>", JS_EVAL_TYPE_GLOBAL);
		installed = true;
	}

	qjs::Value installer{ ctx.global()["__qjs_installMapIterator"] };
	if (JS_IsUndefined(installer.v))
		throw std::runtime_error{ "Failed to define the map iterator installer in the JS VM." };

	JSValueConst arg = proto.v;
	JSValue ret = JS_Call(ctx.ctx, installer.v, JS_UNDEFINED, 1, &arg);
	if (JS_IsException(ret))
		throw std::runtime_error{ "Failed to install Symbol.iterator for the map proto." };
	JS_FreeValue(ctx.ctx, ret);
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

	// A transplanted object owns its C++ object, but views/twins derived from
	// it still need lifetime tracking: the owned object dies when the JS object
	// is garbage collected, and its informant marks the shared state dead.
	Rc<LifetimeInformant::LifetimeInfo> lifetimeInfo;
	if (typeInfo.shareLifetimeInfoFunc)
		lifetimeInfo = typeInfo.shareLifetimeInfoFunc(metapp::Variant::reference(cppObjRef));

	return makeEJSObj(*ctx->ctx, typeInfo.jsVMRuntimeData.classID, EJSObjOpaque{
		.makingMethod = EJSObjOpaque::MakingMethod::transplanted,
		.type = typeInfo.type,
		.cppObjRefInVariant = {},
		.lifetimeInfoOfCppObj = std::move(lifetimeInfo),
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

JSValue Binding::cpp2JSAuto(metapp::Variant cppVal,
	const Rc<LifetimeInformant::LifetimeInfo> &lifetimeInfo) {
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

	auto &rawType = getRawType(cppVal);
	auto *typeInfo = findTypeInfoByCppType(regedTypes, rawType);

	// Container-like types without explicit registration are handled
	// structurally: sequential containers (MetaIndexable) and associative
	// containers (MetaMappable). Reference/pointer forms become non-owning
	// live views (twins); owning values are transplanted into the JS VM --
	// except non-owning view types (std::span, marked tkUser): even passed
	// by value a span is still a view, so it is exposed as a live view too.
	if (!typeInfo && (rawType.getMetaIndexable() || rawType.getMetaMappable())) {
		const bool isIndexable = rawType.getMetaIndexable() != nullptr;

		if (isReferenceOrPointerVariant(cppVal)) {
			if (!lifetimeInfo)
				throw std::runtime_error{
					std::format(
						"Cannot create a live container view of {} without lifetime information: the view would dangle when its source dies.",
						getTypeNameSafe(rawType))
				};
			return isIndexable
				? cpp2JSContainerTwin(makeRefFromReferenceOrPointerVariant(cppVal), lifetimeInfo)
				: cpp2JSMapTwin(makeRefFromReferenceOrPointerVariant(cppVal), lifetimeInfo);
		}

		if (rawType.getTypeKind() == metapp::tkUser) {
			if (!lifetimeInfo)
				throw std::runtime_error{
					std::format(
						"{} is a non-owning view type passed by value; exposing it as a live view requires lifetime information, which is unavailable here.",
						getTypeNameSafe(rawType))
				};
			return cpp2JSContainerTwin(std::move(cppVal), lifetimeInfo);
		}

		return isIndexable
			? cpp2JSContainerTransplant(std::move(cppVal))
			: cpp2JSMapTransplant(std::move(cppVal));
	}

	if (isReferenceOrPointerVariant(cppVal)) {
		auto jsObj = cpp2JSTwin(std::move(cppVal));
		if (lifetimeInfo && !JS_IsException(jsObj)) {
			auto opaque = getEJSObjOpaque(jsObj);
			assert(opaque && "Created twin object without opaque.");
			// Own the shared lifetime state: the view/twin may outlive the
			// object twin it originated from, but the state itself tracks death.
			opaque->lifetimeInfoOfCppObj = lifetimeInfo;
		}
		return jsObj;
	}

	if (typeInfo && typeInfo->translator) {
		auto cppObjRef = isReferenceOrPointerVariant(cppVal)
			? makeRefFromReferenceOrPointerVariant(cppVal)
			: makeRefVariantOfVariant(cppVal);
		return typeInfo->translator->translateToJS(typeInfo->translator->makeTranslateInput(cppObjRef));
	}

	return cpp2JSTransplant(std::move(cppVal));
}

JSValue Binding::cpp2JSContainerTransplant(metapp::Variant containerVal) {
	assert(!containerVal.isEmpty() && "Expected an owning container Variant.");

	auto &rawType = getRawType(containerVal);
	auto classID = getOrMakeContainerClass(rawType);
	return makeEJSObj(*ctx->ctx, static_cast<int>(classID), EJSObjOpaque{
		.makingMethod = EJSObjOpaque::MakingMethod::transplanted,
		.type = &rawType,
		.cppObjRefInVariant = {},
		.lifetimeInfoOfCppObj = {},
		.ownedCppObjInVariant = std::move(containerVal)
	});
}

JSValue Binding::cpp2JSContainerTwin(metapp::Variant containerRef,
	Rc<LifetimeInformant::LifetimeInfo> lifetimeInfo) {
	// containerRef either references an existing container (ref/pointer
	// forms) or owns a non-owning view object (std::span by value); both
	// keep the view data alive as long as this Variant is alive.
	assert(!containerRef.isEmpty() && "Expected a container Variant.");

	auto &rawType = getRawType(containerRef);
	auto classID = getOrMakeContainerClass(rawType);
	return makeEJSObj(*ctx->ctx, static_cast<int>(classID), EJSObjOpaque{
		.makingMethod = EJSObjOpaque::MakingMethod::twined,
		.type = &rawType,
		.cppObjRefInVariant = std::move(containerRef),
		.lifetimeInfoOfCppObj = std::move(lifetimeInfo),
		.ownedCppObjInVariant = {}
	});
}

JSClassID Binding::getOrMakeContainerClass(const metapp::MetaType &type) {
	if (auto it = detail->containerClassIds.find(&type); it != detail->containerClassIds.end())
		return it->second;

	JSClassID classID = 0;
	JS_NewClassID(&classID);

	static JSClassExoticMethods exoticMethods{
		.get_own_property = containerGetOwnProperty,
		.set_property = containerSetProperty,
	};

	qjs::Value proto = ctx->newObject();
	installSeqContainerIterator(*ctx, proto);

	std::string className = getTypeNameSafe(type);
	JSClassDef classDef{
		.class_name = className.c_str(),
		.finalizer = [](JSRuntime *, JSValue val) noexcept {
			freeEJSObjOpaque(val);
		},
		.gc_mark = nullptr,
		.call = nullptr,
		.exotic = &exoticMethods
	};

	if (JS_NewClass(JS_GetRuntime(ctx->ctx), classID, &classDef) < 0) {
		throw std::runtime_error{
			std::format("注册容器类 {} 失败。", className)
		};
	}

	JS_SetClassProto(ctx->ctx, classID, proto.v);
	proto.release();

	detail->containerClassIds.emplace(&type, classID);
	return classID;
}

JSValue Binding::cpp2JSMapTransplant(metapp::Variant containerVal) {
	assert(!containerVal.isEmpty() && "Expected an owning map Variant.");

	auto &rawType = getRawType(containerVal);
	auto classID = getOrMakeMapClass(rawType);
	return makeEJSObj(*ctx->ctx, static_cast<int>(classID), EJSObjOpaque{
		.makingMethod = EJSObjOpaque::MakingMethod::transplanted,
		.type = &rawType,
		.cppObjRefInVariant = {},
		.lifetimeInfoOfCppObj = {},
		.ownedCppObjInVariant = std::move(containerVal)
	});
}

JSValue Binding::cpp2JSMapTwin(metapp::Variant containerRef,
	Rc<LifetimeInformant::LifetimeInfo> lifetimeInfo) {
	assert(isRefVariant(containerRef) && "Expected a reference Variant to the map.");

	auto &rawType = getRawType(containerRef);
	auto classID = getOrMakeMapClass(rawType);
	return makeEJSObj(*ctx->ctx, static_cast<int>(classID), EJSObjOpaque{
		.makingMethod = EJSObjOpaque::MakingMethod::twined,
		.type = &rawType,
		.cppObjRefInVariant = std::move(containerRef),
		.lifetimeInfoOfCppObj = std::move(lifetimeInfo),
		.ownedCppObjInVariant = {}
	});
}

JSClassID Binding::getOrMakeMapClass(const metapp::MetaType &type) {
	if (auto it = detail->containerClassIds.find(&type); it != detail->containerClassIds.end())
		return it->second;

	JSClassID classID = 0;
	JS_NewClassID(&classID);

	static JSClassExoticMethods exoticMethods{
		.get_own_property = mapGetOwnProperty,
		.set_property = mapSetProperty,
	};

	qjs::Value proto = ctx->newObject();
	proto["keys"] = JS_NewCFunction(ctx->ctx, mapKeysNative, "keys", 0);
	installMapIterator(*ctx, proto);

	std::string className = getTypeNameSafe(type);
	JSClassDef classDef{
		.class_name = className.c_str(),
		.finalizer = [](JSRuntime *, JSValue val) noexcept {
			freeEJSObjOpaque(val);
		},
		.gc_mark = nullptr,
		.call = nullptr,
		.exotic = &exoticMethods
	};

	if (JS_NewClass(JS_GetRuntime(ctx->ctx), classID, &classDef) < 0) {
		throw std::runtime_error{
			std::format("注册 map 类 {} 失败。", className)
		};
	}

	JS_SetClassProto(ctx->ctx, classID, proto.v);
	proto.release();

	detail->containerClassIds.emplace(&type, classID);
	return classID;
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

	TypeInfo typeInfo{
		.type = typeInfoCd.type,
		.shareLifetimeInfoFunc = std::move(typeInfoCd.shareLifetimeInfoFunc),
		.moveable = typeInfoCd.moveable,
		.translator = std::move(typeInfoCd.translator)
	};

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

				// Directed marshalling: each argument is converted towards
				// the declared parameter type, so numbers land in the exact
				// C++ arithmetic type and string_view params work.
				auto &callableVar = *functionData.callable;
				auto &metaCallable = UNWRAP(callableVar.getMetaType()->getMetaCallable());
				auto paramCountInfo = metaCallable.getParameterCountInfo(callableVar);

				std::vector<metapp::Variant> args2CppResult;
				args2CppResult.reserve(args.size());
				for (std::size_t i = 0; i < args.size(); ++i) {
					const bool hasDeclaredParam =
						i < static_cast<std::size_t>(paramCountInfo.getMaxParameterCount());
					const metapp::MetaType *paramType = hasDeclaredParam
						? metaCallable.getParameterType(callableVar, static_cast<int>(i))
						: nullptr;
					if (paramType)
						args2CppResult.push_back(binding.js2CppAs(*paramType, args[i]));
					else
						args2CppResult.push_back(binding.js2Cpp(args[i]));
				}

				// Call C++ function
				auto res = metaCallable.invoke(
					*functionData.callable,
					subject,
					args2CppResult
				);

				// Returned container views and element twins inherit the
				// subject's lifetime info, so they die together with it.
				auto subjectOpaque = getEJSObjOpaque(this_val);
				auto jsRes = binding.cpp2JSAuto(res,
					subjectOpaque
						? subjectOpaque->lifetimeInfoOfCppObj
						: Rc<LifetimeInformant::LifetimeInfo>{ });
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
