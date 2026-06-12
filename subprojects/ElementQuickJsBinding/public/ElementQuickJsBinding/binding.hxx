#ifndef _H_O7641
#define _H_O7641

#include <metapp/allmetatypes.h>
#include <quickjspp.hpp>
#include <quickjs/quickjs.h>
#include <EatiEssentials/memsafety.hxx>
#include <pimpl.hpp>

#include "ElementQuickJsBinding/lifetime_informant.hxx"
#include "metapp/metatype.h"

namespace ElementEngine::QJSBinding
{

struct Translator
{
	// std::any: Refw<const T>
	std::function<JSValue(std::any)> translateToJS;
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
	std::function<ESSM::Rc<LifetimeInformant::LifetimeInfo>(std::any cppValRefw)>  shareLifetimeInfoFunc;
	bool moveable;
	std::optional<Translator> translator;
};

struct TypeInfo
{
	gsl::not_null<const metapp::MetaType *> type;
	std::function<ESSM::Rc<LifetimeInformant::LifetimeInfo>()> shareLifetimeInfoFunc;
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
			.shareLifetimeInfoFunc = [](std::any cppValRefw) {
				if constexpr (LifetimeAware<T>) {
					//static_assert(false, "DBG 1");
					return getLifetimeInfo<T>(std::any_cast<ESSM::Refw<const T>>(cppValRefw));
				} else {
					//static_assert(false, "DBG 2");
					return nullptr;
				}
			},
			.moveable = std::is_move_constructible_v<T>,
			.translator = []() {
				if constexpr (Translatable<T>) {
					return Translator{
						.translateToJS = [](std::any cppVal) -> JSValue{
							return T::translateToJS(std::any_cast<ESSM::Refw<const T>>(cppVal));
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
