#ifndef _H_O7641
#define _H_O7641

#include <metapp/allmetatypes.h>
#include <quickjs/quickjs.h>

#include "lifetime_informant.hxx"

namespace ElementEngine::QJSBinding
{

struct TypeInfoCreatingData
{
	std::function<ESSM::Rc<LifetimeInformant::LifetimeInfo>()>  shareLifetimeInfoFunc;
	bool moveable;
};

struct TypeInfo
{
	JSClassID classID;
	std::function<ESSM::Rc<LifetimeInformant::LifetimeInfo>()>  shareLifetimeInfoFunc;
	bool moveable;
};

template <typename T>
void regTypeStatic() {

}

void regType(TypeInfoCreatingData typeInfo);

void deregType(const metapp::MetaType *type);

JSValue cpp2JSTwin(JSContext &ctx, metapp::Variant cppObjPtr);

JSValue cpp2JSTransplant(JSContext &ctx, metapp::Variant cppObj);

JSValue cpp2JSTranslate(JSContext &ctx, metapp::Variant cppObjPtr);

JSValue cpp2JSAuto(JSContext &ctx, metapp::Variant cppVal);


}

#endif // !Guard
