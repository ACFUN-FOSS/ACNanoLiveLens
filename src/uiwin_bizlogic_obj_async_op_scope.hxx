#ifndef NANOLIVELENS_UIWIN_BIZLOGIC_OBJ_ASYNC_OP_SCOPE_HXX
#define NANOLIVELENS_UIWIN_BIZLOGIC_OBJ_ASYNC_OP_SCOPE_HXX
#include "RmlUIWin/window_manager.hxx"



template <typename T>
concept IsBizLogicObjDuckType = requires(T obj) {
    { obj.getLogicObjCtx().hasUnfinishedAsyncOp() } -> std::convertible_to<bool>;
    { obj.getLogicObjCtx().requestClose() }  -> std::same_as<void>;
    { obj.getLogicObjCtx().isPendingClose() }  -> std::convertible_to<bool>;
};

template <typename T> class UiWinBizLogicObjContext;
template <typename T> class UiWinBizLogicObjHandler_;
template <typename T> class UiWinBizLogicObjAsyncOpScope;

GSL_POINTER
template <typename T>
class UiWinBizLogicObjContext {
public:
	UiWinBizLogicObjContext(UiWinBizLogicObjHandler_<T> &h LIFETIMEBOUND) : handler{ &h } {}
	UiWinBizLogicObjContext(const UiWinBizLogicObjContext &other) = default;
	UiWinBizLogicObjContext(UiWinBizLogicObjContext &&other) = default;
	UiWinBizLogicObjContext &operator =(const UiWinBizLogicObjContext &other) = default;
	UiWinBizLogicObjContext &operator =(UiWinBizLogicObjContext &&other) = default;

	void markOneAsyncOpBegin() {
		assert(asyncOpCount >= 0);
		asyncOpCount++;
		assert(asyncOpCount >= 0);
	}

	void markOneAsyncOpFinish() {
		assert(asyncOpCount >= 0);
		asyncOpCount--;
		assert(asyncOpCount >= 0);
	}

	bool hasUnfinishedAsyncOp() const {
		// TODO: rename
		return (*handler).hasUnfinishedOp();
	}
	void requestClose() {
		(*handler).requestClose();
	}
	bool isPendingClose() const {
		return (*handler).isPendingClose();
	}
	coro::result<void> close() {
		co_return;
	}


private:
	friend class UiWinBizLogicObjHandler_<T>;
	friend class UiWinBizLogicObjAsyncOpScope<T>;
	gsl::not_null<UiWinBizLogicObjHandler_<T> *> handler;

	size_t asyncOpCount = 0;
};

template <typename T>
class UiWinBizLogicObjAsyncOpScope
{
public:
	UiWinBizLogicObjAsyncOpScope(UiWinBizLogicObjContext<T> &ctx LIFETIMEBOUND)
		requires IsBizLogicObjDuckType<T>
		: ctx{ &ctx }
	{
		ctx.markOneAsyncOpBegin();
	}
	UiWinBizLogicObjAsyncOpScope(const UiWinBizLogicObjAsyncOpScope &) = default;
	UiWinBizLogicObjAsyncOpScope(UiWinBizLogicObjAsyncOpScope &&) = default;
	UiWinBizLogicObjAsyncOpScope &operator =(const UiWinBizLogicObjAsyncOpScope &) = delete;
	UiWinBizLogicObjAsyncOpScope &operator =(UiWinBizLogicObjAsyncOpScope &&) = delete;

	T &that() {
        return **(ctx->handler); 
    }

	~UiWinBizLogicObjAsyncOpScope() {
		ctx->markOneAsyncOpFinish();
	}
private:
	gsl::not_null<UiWinBizLogicObjContext<T> *> ctx;
};

template <typename T>
class UiWinBizLogicObjHandler_
{
public:

	UiWinBizLogicObjHandler_()
		requires std::constructible_from<T, UiWinBizLogicObjContext<T>>
		&& IsBizLogicObjDuckType<T>
		: logicObj_{ UiWinBizLogicObjContext{ *this } }
	{ }


    template <typename... Args>
		requires std::constructible_from<T, UiWinBizLogicObjContext<T>, Args...>
		&& IsBizLogicObjDuckType<T>
    UiWinBizLogicObjHandler_(Args&&... args)
		: logicObj_{ UiWinBizLogicObjContext{ *this }, std::forward<Args>(args)... }
    { }

	// 当且仅当 T 可拷贝时，这个构造函数才存在
	UiWinBizLogicObjHandler_(const UiWinBizLogicObjHandler_ &other)
		requires
			std::constructible_from<T, UiWinBizLogicObjContext<T>, const T &>
			&& IsBizLogicObjDuckType<T>
		: logicObj_{ UiWinBizLogicObjContext{ *this }, other.logicObj_ } {}

	// 如果 T 不可拷贝，显式将其删除（可选，但能提供更好的编译错误信息）
	UiWinBizLogicObjHandler_(const UiWinBizLogicObjHandler_ &)
		requires (!std::constructible_from<T, UiWinBizLogicObjContext<T>, const T &>)
		= delete;

	// 当且仅当 T 可移动时，这个构造函数才存在
	UiWinBizLogicObjHandler_(UiWinBizLogicObjHandler_ &&other) noexcept
		requires
			std::constructible_from<T, UiWinBizLogicObjContext<T>, T &&>
			&& IsBizLogicObjDuckType<T>
		: logicObj_{ UiWinBizLogicObjContext{ *this }, std::move(other.logicObj_) } {}

	
	UiWinBizLogicObjHandler_(UiWinBizLogicObjHandler_ &&)
		requires
			(!std::constructible_from<T, UiWinBizLogicObjContext<T>, T &&>)
		= delete;

	~UiWinBizLogicObjHandler_() {
		
	}


	bool hasUnfinishedAsyncOp() const {
		return logicObj_.getLogicObjCtx().hasUnfinishedAsyncOp();
	}
	void requestClose() {
		logicObj_.getLogicObjCtx().requestClose();
	}
	bool isPendingClose() const {
		return logicObj_.getLogicObjCtx().isPendingClose();
	}
	coro::result<void> close() {
		co_return;
	}

	T &operator *() {
		return logicObj_;
	}

	const T &operator *() const {
		return logicObj_;
	}
	
private:
    T logicObj_;
};

template <typename T>
concept IsBizLogic = requires(T obj) {
    { obj.getLogicObjCtx() } -> std::convertible_to<UiWinBizLogicObjContext<T>>;
};

template <IsBizLogic T>
class UiWinBizLogicObjHandler : public UiWinBizLogicObjHandler_<T>
{
public:
	using UiWinBizLogicObjHandler_<T>::UiWinBizLogicObjHandler_;
};


#endif // !NANOLIVELENS_UIWIN_BIZLOGIC_OBJ_ASYNC_OP_SCOPE_HXX
