#ifndef _H_O7641
#define _H_O7641

#include <metapp/allmetatypes.h>
// Type-level whitelist of C++20 view types (std::span, std::string_view):
// metatype declarations that must be visible in every TU instantiating
// their metapp metatypes (ODR), so the binding can treat them as views.
#include "ElementQuickJsBinding/metapp_view_metatypes.hxx"
// Enums are marshalled by name (a JS string like 'RELOAD' <-> the enumerator)
// for every enum type, without any per-enum binding code. Also an ODR-level
// declaration, hence included here.
#include "ElementQuickJsBinding/metapp_enum_metatype.hxx"
#include <quickjspp.hpp>
#include <quickjs/quickjs.h>
#include <EatiEssentials/memory/memory.hxx>
#include <EatiEssentials/memory/memsafety.hxx>
#include <EatiEssentials/memory/lifetime_informant.hxx>
#include <pimpl.hpp>
#include <any>
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "metapp/metatype.h"
#include "metapp/utilities/utility.h"

namespace ElementEngine::QJSBinding
{

struct Translator
{
	std::function<std::any(const metapp::Variant &)> makeTranslateInput;
	// Holds ESSM::Refw<T> for the registered C++ type T.
	// The context is passed in because building the JS representation of a
	// value often needs it (a JS Date for a time point, an Array, ...).
	std::function<JSValue(qjs::Context &, const std::any &)> translateToJS;
	std::function<metapp::Variant(qjs::Context &, JSValue)> detranslate;
};


template <typename T>
concept Translatable = requires(qjs::Context &ctx, JSValue jsVal, T &cppRef) {
	{ T::translateToJS(ctx, cppRef) } -> std::same_as<JSValue>;
	{ T::detranslate(ctx, jsVal) } -> std::same_as<T>;
};

class Binding;

// Print the pending JS exception of ctx (including its stack when available) and
// consume it, so the exception doesn't leak into unrelated error reporting.
void dumpJsException(JSContext &ctx, std::string_view where);

namespace internal
{

// Bridge state shared by every copy of the std::function produced by
// JsFunctionBridge: owns one reference to the JS function. It must be
// released with the very context that created it, i.e. on the thread owning
// that context.
class JsFunctionHolder
{
public:
	// Takes over one reference of func.
	JsFunctionHolder(JSContext *ctx, JSValue func) noexcept
		: ctx_{ ctx }, func_{ func } {
	}
	~JsFunctionHolder() {
		JS_FreeValue(ctx_, func_);
	}

	JsFunctionHolder(const JsFunctionHolder &) = delete;
	JsFunctionHolder(JsFunctionHolder &&) = delete;
	JsFunctionHolder &operator=(const JsFunctionHolder &) = delete;
	JsFunctionHolder &operator=(JsFunctionHolder &&) = delete;

	[[nodiscard]] JSContext *ctx() const noexcept {
		return ctx_;
	}
	[[nodiscard]] JSValue func() const noexcept {
		return func_;
	}

private:
	JSContext *ctx_;
	JSValue func_;
};

}

// Customization point turning a JS function into a C++ callable type.
//
// Register the C++ callable type with regTypeStatic (e.g.
// regTypeStatic<std::function<void(int)>>()) and a JS function is then
// accepted as a value of that type: the bridge calls the JS function with
// its arguments converted by cpp2JSAuto and converts the result back with
// js2CppAs, exactly like the arguments of a proxied method.
//
// The bridged std::function holds one reference to the JS function, so it may
// be copied, stored and called freely; it must not outlive the Binding nor be
// used from a thread other than the one owning the JS context.
template <typename T>
struct JsFunctionBridge
{
	static constexpr bool enabled = false;
};

template <typename RT, typename... Args>
struct JsFunctionBridge<std::function<RT(Args...)>>
{
	static constexpr bool enabled = true;

	// Throws std::invalid_argument when jsFunc is not a function.
	// Definition is after class Binding, which it needs.
	static std::function<RT(Args...)> fromJs(Binding &binding LIFETIMEBOUND,
		JSContext *ctx LIFETIMEBOUND, JSValue jsFunc);
};

struct TypeInfoCreatingData
{
	gsl::not_null<const metapp::MetaType *> type;
	// Called only for twin conversion. cppVal is a non-owning access Variant for an
	// existing C++ object, normally wrapping T &.
	std::function<ESSM::Rc<ESSM::LifetimeInformant::LifetimeInfo>(const metapp::Variant &cppVal)>  shareLifetimeInfoFunc;
	bool moveable;
	std::optional<Translator> translator;
	// Only set for JS function bridge types (std::function<R(Args...)>):
	// builds an instance of the type from a JS function. See JsFunctionBridge.
	std::function<metapp::Variant(JSContext *, JSValue)> makeFromJsFunction;
};

struct TypeInfo
{
	gsl::not_null<const metapp::MetaType *> type;
	// Same contract as TypeInfoCreatingData::shareLifetimeInfoFunc.
	// cppVal should hold a reference to the registered raw type T.
	std::function<ESSM::Rc<ESSM::LifetimeInformant::LifetimeInfo>(const metapp::Variant &cppVal)> shareLifetimeInfoFunc;
	bool moveable;
	std::optional<Translator> translator;
	// Same contract as TypeInfoCreatingData::makeFromJsFunction.
	std::function<metapp::Variant(JSContext *, JSValue)> makeFromJsFunction;

	struct JSVMRuntimeData
	{
		JSClassID classID;
	} jsVMRuntimeData;

	bool isTranslatableType();
	[[nodiscard]] bool isJsFunctionBridgeType() const noexcept {
		return static_cast<bool>(makeFromJsFunction);
	}
};

class Binding
{
private:
	gsl::not_null<qjs::Runtime *const> rt;
	gsl::not_null<qjs::Context *const> ctx;
	std::vector<TypeInfo> regedTypes;
	
	struct Detail;
	stdx::pimpl::unique_ptr<Detail> detail;
	
public:
	ESSM::LifetimeInformant lifetimeInformant;
	Binding(qjs::Runtime &rt LIFETIMEBOUND, qjs::Context &ctx LIFETIMEBOUND);

	TypeInfo &findTypeInfoOfEJSObj(JSValue jsvalue);
	bool checkEJSObjLifetime(JSValue jsvalue);
	metapp::Variant getCppObjRefByEJSObj(JSValue jsvalue);
	metapp::Variant getPointerToCppObjByEJSObj(JSValue jsvalue);

	void deregType(const metapp::MetaType &type);

	JSValue cpp2JSTwin(metapp::Variant cppObjRef);

	JSValue cpp2JSTransplant(metapp::Variant cppObjRef);

	JSValue cpp2JSTranslate(metapp::Variant cppValRef);

	JSValue cpp2JSAuto(metapp::Variant cppVal,
		const ESSM::Rc<ESSM::LifetimeInformant::LifetimeInfo> &lifetimeInfo = { });

private:
	// Container support (structural, not registration-based).
	// Transplant an owning container Variant into a JS live container view,
	// whose exotic object proxies index access back into the C++ container.
	JSValue cpp2JSContainerTransplant(metapp::Variant containerVal);
	// Create a non-owning live container view (twin) over an existing C++
	// container. lifetimeInfo must track the container (or its owner), so the
	// view can detect dangling access after the source dies.
	JSValue cpp2JSContainerTwin(metapp::Variant containerRef,
		ESSM::Rc<ESSM::LifetimeInformant::LifetimeInfo> lifetimeInfo);
	// Lazily create (and cache) the JS class for a container metatype.
	JSClassID getOrMakeContainerClass(const metapp::MetaType &type);

	// Lazily create (and cache) the JS class for a mappable (associative)
	// container metatype.
	JSClassID getOrMakeMapClass(const metapp::MetaType &type);

	JSValue cpp2JSMapTransplant(metapp::Variant containerVal);

	JSValue cpp2JSMapTwin(metapp::Variant containerRef,
		ESSM::Rc<ESSM::LifetimeInformant::LifetimeInfo> lifetimeInfo);

public:

	template <typename T>
	JSValue cpp2JSAutoStatic(const T &cppVal) {
		return cpp2JSAuto(metapp::Variant{ cppVal });
	}

	// Directed marshalling: convert a JS value into a Variant of the given
	// C++ parameter type (e.g. the declared parameter of a proxied method).
	// EJSObj arguments are passed through as live C++ references; primitive
	// JS values are converted and cast to the target type, failing with a
	// descriptive error instead of a generic mismatch deep inside invoke.
	metapp::Variant js2CppAs(const metapp::MetaType &targetType, JSValue jsVal);

	metapp::Variant js2Cpp(JSValue jsVal);

	void regType(TypeInfoCreatingData &&typeInfo);

	// Register a global function callable from scripts.
	//
	// callable must be a callable recognised by metapp, normally
	// std::function<ReturnType(ParameterTypes...)>. Its declared parameter
	// types are the marshalling contract of the JS arguments, exactly like
	// the parameters of a proxied method: each JS argument is converted
	// towards the declared parameter type (js2CppAs, which also accepts a JS
	// function where a std::function parameter is declared), and the return
	// value is converted with cpp2JSAuto.
	//
	// The function is installed on the JS global object under name.
	void regGlobalFunction(std::string_view name, metapp::Variant callable);

	template <typename F>
	void regGlobalFunction(std::string_view name, F &&func) {
		regGlobalFunction(name, metapp::Variant{ std::forward<F>(func) });
	}

private:
	// The JS function bridge entry point of T, if T has one: an empty
	// std::function means "not a bridge type" (see TypeInfo).
	template <typename T>
	static std::function<metapp::Variant(JSContext *, JSValue)> makeJsFunctionBridgeMaker() {
		if constexpr (JsFunctionBridge<T>::enabled) {
			return [](JSContext *ctx, JSValue jsFunc) -> metapp::Variant {
				// Recovered from the binding that installs this maker.
				Binding &binding = *bindingOfContext_(ctx);
				return metapp::Variant{ JsFunctionBridge<T>::fromJs(binding, ctx, jsFunc) };
			};
		} else {
			return { };
		}
	}

	static Binding *bindingOfContext_(JSContext *ctx LIFETIMEBOUND);

public:

	template <typename T>
	void regTypeStatic() {
		// Analysis T, and make TypeInfoCreatingData from it
		// and call regType.
		regType({
			.type = metapp::getMetaType<T>(),
			.shareLifetimeInfoFunc = [](const metapp::Variant &cppVal) {
				if constexpr (ESSM::LifetimeAware<T>) {
					return ESSM::getLifetimeInfo<T>(cppVal.get<T &>());
				} else {
					return nullptr;
				}
			},
			.moveable = std::is_move_constructible_v<T>,
			.translator = []() {
				if constexpr (Translatable<T>) {
					return Translator{
						.makeTranslateInput = [](const metapp::Variant &cppVal) -> std::any{
							return ESSM::Refw<T>{ cppVal.get<T &>() };
						},
						.translateToJS = [](qjs::Context &ctx, const std::any &cppRef) -> JSValue{
							return T::translateToJS(ctx, std::any_cast<ESSM::Refw<T>>(cppRef).get());
						},
						.detranslate = [](qjs::Context &ctx, JSValue jsVal) -> metapp::Variant{
							return metapp::Variant{ T::detranslate(ctx, jsVal) };
						}
					};
				} else {
					return std::nullopt;
				}
			}(),
			.makeFromJsFunction = makeJsFunctionBridgeMaker<T>()
		});
	}

	
};

// Marshalling helpers for the JS function bridge.

namespace internal
{

// Convert one bridged argument into a Variant, dispatching on how the
// parameter was *declared* rather than on the type deduced from the argument
// expression. A parameter declared as `T &` always receives an lvalue, so
// deducing from the expression would classify every reference parameter as
// by-value, making the bridge try to copy (and, for non-copyable types such as
// Rml::Element, fail with a bare "Not constructible").
//
//   * declared as a reference -> keep the reference: the JS function receives a
//     twin of the C++ object, so reads and writes go through to the original.
//   * declared by value -> transplant/copy it, so the JS function owns its copy.
template <typename DeclaredArg, typename ActualArg>
metapp::Variant makeBridgeArgVariant(ActualArg &arg) {
	if constexpr (std::is_lvalue_reference_v<DeclaredArg>) {
		return metapp::Variant::reference(arg);
	} else {
		return metapp::Variant{ arg };
	}
}

// What a bridged callback returns when its JS body could not be called: a void
// callback just returns, any other needs a value to hand back to the C++ caller.
template <typename RT>
RT bridgeFailureResult() {
	if constexpr (std::is_void_v<RT>) {
		return;
	} else if constexpr (std::is_default_constructible_v<RT>) {
		return RT{ };
	} else {
		throw std::runtime_error{
			"The JS callback failed and its return type has no default value."
		};
	}
}

}

template <typename RT, typename... Args>
std::function<RT(Args...)> JsFunctionBridge<std::function<RT(Args...)>>::fromJs(
	Binding &binding, JSContext *ctx, JSValue jsFunc) {

	if (!JS_IsFunction(ctx, jsFunc))
		throw std::invalid_argument{ "Cannot bridge a non-function JS value into std::function." };

	auto holder = std::make_shared<internal::JsFunctionHolder>(ctx, JS_DupValue(ctx, jsFunc));
	Binding *bindingPtr = &binding;

	return std::function<RT(Args...)>{
		[holder, bindingPtr](Args... args) -> RT {
			JSContext *const callCtx = holder->ctx();

			// std::tie keeps every argument as an lvalue, and the declared type
			// is looked up by index, so each argument is converted according to
			// how it was declared (see makeBridgeArgVariant).
			auto actualArgs = std::tie(args...);

			std::array<JSValue, sizeof...(Args)> jsArgs{ };
			std::size_t convertedCount = 0;
			const auto freeConvertedArgs = [&] {
				for (std::size_t i = 0; i < convertedCount; ++i)
					JS_FreeValue(callCtx, jsArgs[i]);
			};

			// Marshal the arguments; free whatever was already created when one
			// of them fails to convert.
			try {
				[&]<std::size_t... Is>(std::index_sequence<Is...>) {
					((jsArgs[Is] = bindingPtr->cpp2JSAuto(
						internal::makeBridgeArgVariant<
							std::tuple_element_t<Is, std::tuple<Args...>>>(
								std::get<Is>(actualArgs))
					), ++convertedCount), ...);
				}(std::index_sequence_for<Args...>{});
			} catch (...) {
				freeConvertedArgs();
				throw;
			}

			// A failed conversion returns JS_EXCEPTION, which must never be
			// handed to the callback as if it were a real argument.
			for (const auto jsArg : jsArgs) {
				if (JS_IsException(jsArg)) {
					dumpJsException(*callCtx, "JS 回调的参数");
					freeConvertedArgs();
					return internal::bridgeFailureResult<RT>();
				}
			}

			JSValue result = JS_Call(callCtx, holder->func(), JS_UNDEFINED,
				static_cast<int>(sizeof...(Args)), jsArgs.data());
			for (const auto jsArg : jsArgs)
				JS_FreeValue(callCtx, jsArg);

			if (JS_IsException(result)) {
				dumpJsException(*callCtx, "JS 回调");
				JS_FreeValue(callCtx, result);
				return internal::bridgeFailureResult<RT>();
			}

			if constexpr (std::is_void_v<RT>) {
				JS_FreeValue(callCtx, result);
				return;
			} else {
				auto cppResult = bindingPtr->js2CppAs(*metapp::getMetaType<RT>(), result);
				JS_FreeValue(callCtx, result);
				return cppResult.cast<RT>().get<RT>();
			}
		}
	};
}

}

#endif // !Guard
