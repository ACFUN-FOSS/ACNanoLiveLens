#ifndef _H_KYCQ5
#define _H_KYCQ5

#include <gsl/gsl>
#include <EatiEssentials/memory.hxx>
#include <metapp/allmetatypes.h>

#include "lifetime_informant.hxx"

namespace ElementEngine::QJSBinding
{

class EJSObj
{
public:
	enum class MakingMethod
	{
		twined,
		transplanted
	};

	class Opaque
	{
		gsl::not_null<const metapp::MetaType *> type;
		metapp::Variant cppObjPtrInVariant;
		ESSM::Rc<LifetimeInformant::LifetimeInfo> lifetimeInfoOfCppObj;

		// If the JS object is a reference to a C++ object, this field will be empty,
		// if the JS object owns the C++ object, this field will be the C++ object.
		metapp::Variant ownedCppObjInVariant;
	};
private:
};

}

#endif // !Guard
