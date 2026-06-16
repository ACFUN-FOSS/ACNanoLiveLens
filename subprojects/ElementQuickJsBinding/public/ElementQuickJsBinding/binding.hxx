#ifndef _H_O7641
#define _H_O7641

#include <metapp/allmetatypes.h>
#include <quickjspp.hpp>
#include <quickjs/quickjs.h>
#include <EatiEssentials/memory.hxx>
#include <EatiEssentials/memsafety.hxx>
#include <pimpl.hpp>
#include <any>
#include <functional>
#include <optional>
#include <vector>

#include "ElementQuickJsBinding/lifetime_informant.hxx"
#include "metapp/metatype.h"
#include "metapp/utilities/utility.h"

namespace ElementEngine::QJSBinding
{

struct Translator
{
	std::function<std::any(const metapp::Variant &)> makeTranslateInput;
	// Holds ESSM::Refw<T> for the registered C++ type T.
	std::function<JSValue(const std::any &)> translateToJS;
	std::function<std::any(JSValue)> detranslate;
};


template <typename T>
concept Translatable = requires(JSValue jsVal, T &cppRef) {
	{ T::translateToJS(cppRef) } -> std::same_as<JSValue>;
	{ T::detranslate(jsVal) } -> std::same_as<T>;
};

struct TypeInfoCreatingData
{
	gsl::not_null<const metapp::MetaType *> type;
	// Called only for twin conversion. cppVal is a non-owning access Variant for an
	// existing C++ object, normally wrapping T &.
	std::function<ESSM::Rc<LifetimeInformant::LifetimeInfo>(const metapp::Variant &cppVal)>  shareLifetimeInfoFunc;
	bool moveable;
	std::optional<Translator> translator;
	std::function<metapp::Variant(metapp::Variant &cppVal)> makeVariantRef;
};

struct TypeInfo
{
	gsl::not_null<const metapp::MetaType *> type;
	// Same contract as TypeInfoCreatingData::shareLifetimeInfoFunc.
	// cppVal should hold a reference to the registered raw type T.
	std::function<ESSM::Rc<LifetimeInformant::LifetimeInfo>(const metapp::Variant &cppVal)> shareLifetimeInfoFunc;
	bool moveable;
	std::optional<Translator> translator;
	std::function<metapp::Variant(metapp::Variant &cppVal)> makeVariantRef;

	struct JSVMRuntimeData
	{
		JSClassID classID;
	} jsVMRuntimeData;

	bool isTranslatableType();
};

class Binding
{
private:
	gsl::not_null<metapp::MetaRepo *> metaRepo;

	gsl::not_null<qjs::Runtime *const> rt;
	gsl::not_null<qjs::Context *const> ctx;
	std::vector<TypeInfo> regedTypes;
	
	struct Detail;
	stdx::pimpl::unique_ptr<Detail> detail;
	
public:
	LifetimeInformant lifetimeInformant;
	Binding(metapp::MetaRepo &metaRepo LIFETIMEBOUND, qjs::Runtime &rt LIFETIMEBOUND, qjs::Context &ctx LIFETIMEBOUND);

	TypeInfo &findTypeInfoOfEJSObj(JSValue jsvalue);
	bool checkEJSObjLifetime(JSValue jsvalue);
	metapp::Variant getCppObjRefByEJSObj(JSValue jsvalue);
	metapp::Variant getPointerToCppObjByEJSObj(JSValue jsvalue);

	void deregType(const metapp::MetaType &type);

	JSValue cpp2JSTwin(metapp::Variant cppObjPtr);

	JSValue cpp2JSTransplant(metapp::Variant cppObj);

	JSValue cpp2JSTranslate(metapp::Variant cppObjPtr);

	JSValue cpp2JSAuto(metapp::Variant cppVal, LifetimeInformant::LifetimeInfo *lifetimeInfo = nullptr);

	template <typename T>
	JSValue cpp2JSAutoStatic(const T &cppVal) {

	}

	metapp::Variant js2Cpp(JSValue jsVal);

	void regType(TypeInfoCreatingData &&typeInfo);
	
	template <typename T>
	void regTypeStatic() {
		// Analysis T, and make TypeInfoCreatingData from it
		// and call regType.
		regType({
			.type = metapp::getMetaType<T>(),
			.shareLifetimeInfoFunc = [](const metapp::Variant &cppVal) {
				if constexpr (LifetimeAware<T>) {
					return getLifetimeInfo<T>(cppVal.get<T &>());
				} else {
					return nullptr;
				}
			},
			.moveable = std::is_move_constructible_v<T>,
			.makeVariantRef = [](metapp::Variant &cppVal) -> metapp::Variant {
				return metapp::Variant::reference(cppVal.get<T &>());
			},
			.translator = []() {
				if constexpr (Translatable<T>) {
					return Translator{
						.makeTranslateInput = [](const metapp::Variant &cppVal) -> std::any{
							return ESSM::Refw<T>{ cppVal.get<T &>() };
						},
						.translateToJS = [](const std::any &cppRef) -> JSValue{
							return T::translateToJS(std::any_cast<ESSM::Refw<T>>(cppRef).get());
						},
						.detranslate = [](JSValue jsVal) -> T{
							return T::detranslate(jsVal);
						}
					};
				} else {
					return std::nullopt;
				}
			}()
		});
	}

	
};

}

#endif // !Guard
