#ifndef NANOLIVELENS_LOGIN_WIN_HXX
#define NANOLIVELENS_LOGIN_WIN_HXX

#include "uiwin_bizlogic_obj_async_op_scope.hxx"

class LoginWin
{
public:
    LoginWin(UiWinBizLogicObjContext<LoginWin> ctx);
    ~LoginWin();

    LoginWin(const LoginWin &) = delete;
    LoginWin &operator=(const LoginWin &) = delete;
    LoginWin(LoginWin &&) = delete;
    LoginWin &operator=(LoginWin &&) = delete;

	UiWinBizLogicObjContext<LoginWin>& getLogicObjCtx();
	RmlUIWin::UiWin &getUiWin();


private:
    class Impl;
    stdx::pimpl::unique_ptr<Impl> pImpl;
};

#endif
