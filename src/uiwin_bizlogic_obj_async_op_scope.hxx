#ifndef NANOLIVELENS_UIWIN_BIZLOGIC_OBJ_ASYNC_OP_SCOPE_HXX
#define NANOLIVELENS_UIWIN_BIZLOGIC_OBJ_ASYNC_OP_SCOPE_HXX

struct UiWinBizLogicObjAsyncOpScope
{
    
};


struct UiWinBizLogicObjContext
{

};

template <typename T, typename... Args>
concept UiWinBizLogicObj =
    requires(UiWinBizLogicObjContext &ctx, Args... args) {
        T{ctx, args...};
    };


template <UiWinBizLogicObj T>
struct UiWinBizLogicObjHandler
{
    template <typename... Args>
    UiWinBizLogicObjHandler(Args... args)
        : logicObj_(std::make_optional<T>(args...))
    {
    }
private:
    std::optional<T> logicObj_;
};
#endif // !NANOLIVELENS_UIWIN_BIZLOGIC_OBJ_ASYNC_OP_SCOPE_HXX