#ifndef ELEMENT_QJS_BINDINGING_HXX
#define ELEMENT_QJS_BINDINGING_HXX
#include <metapp/allmetatypes.h>
#include <quickjspp.hpp>

void regClass(qjs::Context &ctx, const metapp::MetaType &type);
qjs::Value makeJsTwinObject(qjs::Context &ctx, const metapp::MetaType &type, metapp::Variant cppObj);

#endif // !Guard
