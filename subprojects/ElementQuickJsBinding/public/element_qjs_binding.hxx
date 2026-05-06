#ifndef ELEMENT_QJS_BINDINGING_HXX
#define ELEMENT_QJS_BINDINGING_HXX
#include "metapp/implement/variant_intf.h"
#include <EatiEssentials/memsafety.hxx>
#include <metapp/allmetatypes.h>
#include <quickjspp.hpp>
#include <EatiEssentials/memory.hxx>

struct LifetimeInformant
{
	struct LifetimeInfo
	{
		std::chrono::steady_clock::time_point createTime;
		std::optional<std::chrono::steady_clock::time_point> deathTime;
		std::optional<std::chrono::steady_clock::time_point> movedAwayTime;
		bool isAlive = true;
		bool isMovedAway = false;
	};

	LifetimeInformant();
	~LifetimeInformant();
	LifetimeInformant(const LifetimeInformant &);
	LifetimeInformant(LifetimeInformant &&) noexcept;
	LifetimeInformant &operator=(const LifetimeInformant &);
	LifetimeInformant &operator=(LifetimeInformant &&) noexcept;
		

	ESSM::Rc<LifetimeInfo> info;
};

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

qjs::Value makeRefJsTwinObject(JSContext &ctx, const metapp::MetaType &type, metapp::Variant cppObj, ESSM::Rc<LifetimeInformant::LifetimeInfo> lifetimeInfoOfCppObj = {});
qjs::Value makeOwnedJsTwinObject(JSContext &ctx, const metapp::MetaType &type, metapp::Variant cppObj);
#endif // !Guard
