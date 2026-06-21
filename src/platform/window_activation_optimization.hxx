#ifndef NANOLIVELENS_WINDOW_ACTIVATION_OPTIMIZATION_HXX
#define NANOLIVELENS_WINDOW_ACTIVATION_OPTIMIZATION_HXX

class MsgBoxWindowActivationGuard
{
public:
	explicit MsgBoxWindowActivationGuard(GLFWwindow *msgBoxWindow);
	~MsgBoxWindowActivationGuard();

	MsgBoxWindowActivationGuard(const MsgBoxWindowActivationGuard &) = delete;
	MsgBoxWindowActivationGuard(MsgBoxWindowActivationGuard &&) = delete;
	MsgBoxWindowActivationGuard &operator=(const MsgBoxWindowActivationGuard &) = delete;
	MsgBoxWindowActivationGuard &operator=(MsgBoxWindowActivationGuard &&) = delete;

	void update();

private:
	GLFWwindow *msgBoxWindow_ = nullptr;
};

#endif
