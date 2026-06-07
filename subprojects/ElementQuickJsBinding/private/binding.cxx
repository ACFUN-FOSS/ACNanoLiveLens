#include "ElementQuickJsBinding/binding.hxx"
#include <rfl.hpp>

namespace ElementEngine::QJSBinding
{

void regType(TypeInfoCreatingData typeInfoCd) {
    auto typeInfo = rfl::as<TypeInfo>(typeInfoCd);

    // 0. Make JS class
    auto classID = JS_NewClass(UNWRAP(ctx.ctx), typeInfo.type->name());

    // 先暂时不实现驱动程序
	// 1. check if type is translatable
    if (typeInfo.translator) {
        auto &translator = typeInfo.translator.value();
        
    }
	
	
}

}
