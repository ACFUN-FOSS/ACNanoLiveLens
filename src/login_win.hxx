#ifndef NANOLIVELENS_LOGIN_WIN_HXX
#define NANOLIVELENS_LOGIN_WIN_HXX

class LoginWin
{
public:
    LoginWin();
    ~LoginWin();

    LoginWin(const LoginWin &) = delete;
    LoginWin &operator=(const LoginWin &) = delete;
    LoginWin(LoginWin &&) = delete;
    LoginWin &operator=(LoginWin &&) = delete;

private:
    class Impl;
    stdx::pimpl::unique_ptr<Impl> pImpl;
};

#endif