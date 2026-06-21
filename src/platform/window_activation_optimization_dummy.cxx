#include "platform/window_activation_optimization.hxx"

MsgBoxWindowActivationGuard::MsgBoxWindowActivationGuard(GLFWwindow *msgBoxWindow)
	: msgBoxWindow_{ msgBoxWindow } {
}

MsgBoxWindowActivationGuard::~MsgBoxWindowActivationGuard() = default;

void MsgBoxWindowActivationGuard::update() {
}
