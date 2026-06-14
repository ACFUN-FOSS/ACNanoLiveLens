#ifndef _H_O7641
#define _H_O7641

#include <metapp/allmetatypes.h>
#include <quickjspp.hpp>
#include <quickjs/quickjs.h>
#include <EatiEssentials/memsafety.hxx>
#include <pimpl.hpp>

#include "ElementQuickJsBinding/lifetime_informant.hxx"
#include "metapp/metatype.h"
#include "metapp/utilities/utility.h"

namespace ElementEngine::QJSBinding
{

struct Translator
{
	std::function<JSValue(const metapp::Variant &)> translateToJS;
	std::function<std::any(JSValue)> detranslate;
};


template <typename T>
concept Translatable = requires(JSValue jsVal, T cppVal) {
	{ T::translateToJS(cppVal) } -> std::same_as<JSValue>;
	{ T::detranslate(jsVal) } -> std::same_as<T>;
};

struct TypeInfoCreatingData
{
	gsl::not_null<const metapp::MetaType *> type;
	// Called only for twin conversion. cppVal is a non-owning access Variant for an
	// existing C++ object, normally wrapping T & or T *, not an owned/transplanted T.
	std::function<ESSM::Rc<LifetimeInformant::LifetimeInfo>(const metapp::Variant &cppVal)>  shareLifetimeInfoFunc;
	bool moveable;
	std::optional<Translator> translator;
};

struct TypeInfo
{
	gsl::not_null<const metapp::MetaType *> type;
	// Same contract as TypeInfoCreatingData::shareLifetimeInfoFunc.
	// cppVal should be dereferenceable to the registered raw type T.
	std::function<ESSM::Rc<LifetimeInformant::LifetimeInfo>(const metapp::Variant &cppVal)> shareLifetimeInfoFunc;
	bool moveable;
	std::optional<Translator> translator;

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

	TypeInfo &findTypeInfoOfJsTwin(JSValue jsvalue);
	bool checkTwinObjLifetime(JSValue jsvalue);
	metapp::Variant getPointerToCppObjByJsTwinObject(JSValue jsvalue);

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
					auto cppObjRef = metapp::dereference(cppVal);
					return getLifetimeInfo<T>(cppObjRef.get<T &>());
				} else {
					return nullptr;
				}
			},
			.moveable = std::is_move_constructible_v<T>,
			.translator = []() {
				if constexpr (Translatable<T>) {
					return Translator{
						.translateToJS = [](const metapp::Variant &cppVal) -> JSValue{
							return T::translateToJS(metapp::dereference(cppVal).get<const T &>());
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
