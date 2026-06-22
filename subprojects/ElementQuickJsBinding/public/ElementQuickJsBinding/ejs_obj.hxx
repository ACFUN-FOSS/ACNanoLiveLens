#ifndef _H_KYCQ5
#define _H_KYCQ5

#include <gsl/gsl>
#include <EatiEssentials/memory/memory.hxx>
#include <EatiEssentials/memory/lifetime_informant.hxx>
#include <metapp/allmetatypes.h>
#include <quickjs/quickjs.h>
#include <optional>
#include <string>

namespace ElementEngine::QJSBinding
{

struct EJSObjOpaque
{
public:
	enum class MakingMethod
	{
		// 孪生
		twined,
		// 移植
		transplanted
	} makingMethod;


	gsl::not_null<const metapp::MetaType *> type;

	metapp::Variant cppObjRefInVariant;
	ESSM::Rc<LifetimeInformant::LifetimeInfo> lifetimeInfoOfCppObj;

	// If the JS object is a reference to a C++ object, this field will be empty,
	// If the JS object owns the C++ object, this field will be the C++ object.
	metapp::Variant ownedCppObjInVariant;

	std::optional<std::string> getLifetimeInvalidReason();

private:
};


EJSObjOpaque *getEJSObjOpaque(JSValue jsvalue);

JSValue makeEJSObj(JSContext &ctx, int classID, EJSObjOpaque opaque);

void freeEJSObjOpaque(JSValue jsvalue);

}

#endif // !Guard
