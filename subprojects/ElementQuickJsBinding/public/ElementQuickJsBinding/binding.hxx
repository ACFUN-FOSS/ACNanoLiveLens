#ifndef _H_O7641
#define _H_O7641

#include <metapp/allmetatypes.h>
#include <quickjs/quickjs.h>

#include "ElementQuickJsBinding/lifetime_informant.hxx"
#include "metapp/metatype.h"

namespace ElementEngine::QJSBinding
{

struct Translator
{
	std::function<JSValue(std::any)> translateToJS;
	std::function<std::any(JSValue)> detranslate;
};


template <typename T>
concept Translatable = requires(T obj) {
	{obj.translateToJS} -> std::same_as<decltype(Translator::translateToJS)>;
	{obj.detranslate} -> std::same_as<decltype(Translator::detranslate)>;
}

struct TypeInfoCreatingData
{
	gsl::not_null<const metapp::MetaType *> type;
	std::function<ESSM::Rc<LifetimeInformant::LifetimeInfo>()>  shareLifetimeInfoFunc;
	bool moveable;
};

struct TypeInfo
{
	gsl::not_null<const metapp::MetaType *> type;
	JSClassID classID;
	std::function<ESSM::Rc<LifetimeInformant::LifetimeInfo>()> shareLifetimeInfoFunc;
	bool moveable;
};

void regType(TypeInfoCreatingData typeInfo);

template <typename T>
void regTypeStatic() {
	// Analysis T, and make TypeInfoCreatingData from it
	// and call regType.
	regType({
		.type = metapp::getMetaType<T>(),
		.shareLifetimeInfoFunc = []() {
			if constexpr (LifetimeAware<T>) {
				//static_assert(false, "DBG 1");
				return getLifetimeInfo<T>();
			} else {
				//static_assert(false, "DBG 2");
				return nullptr;
			}
		},
		.moveable = std::is_move_constructible_v<T>
	});
}

void deregType(const metapp::MetaType *type);

JSValue cpp2JSTwin(JSContext &ctx, metapp::Variant cppObjPtr);

JSValue cpp2JSTransplant(JSContext &ctx, metapp::Variant cppObj);

JSValue cpp2JSTranslate(JSContext &ctx, metapp::Variant cppObjPtr);

JSValue cpp2JSAuto(JSContext &ctx, metapp::Variant cppVal, LifetimeInformant::LifetimeInfo *lifetimeInfo = nullptr);

template <typename T>
JSValue cpp2JSAutoStatic(const T &cppVal) {
	
}


}

#endif // !Guard
