#ifndef ELEMENT_QJS_BINDINGING_HXX
#define ELEMENT_QJS_BINDINGING_HXX
#include <metapp/implement/variant_intf.h>
#include <EatiEssentials/memory/memory.hxx>
#include <EatiEssentials/memory/memsafety.hxx>
#include <EatiEssentials/memory/lifetime_informant.hxx>
#include <metapp/allmetatypes.h>
#include <quickjspp.hpp>


void setDefaultMetaRepo(const metapp::MetaRepo &metarepo);

void regClass(
	qjs::Context &ctx,
	const metapp::MetaType &type,
	std::function<metapp::Variant(const metapp::Variant &)> getPointerOfObj,
	bool moveable
);

void regWrapperOfClass(
	qjs::Context &ctx,
	const metapp::MetaType &type,
	const metapp::MetaType &wrappedType,
	std::function<metapp::Variant(const metapp::Variant &)> getPointerOfObj
);

qjs::Value makeRefJsTwinObject(JSContext &ctx, const metapp::MetaType &type, metapp::Variant cppObj, Essentials::Memory::Rc<ESSM::LifetimeInformant::LifetimeInfo> lifetimeInfoOfCppObj = {});
qjs::Value makeOwnedJsTwinObject(JSContext &ctx, const metapp::MetaType &type, metapp::Variant cppObj);
#endif // !Guard
