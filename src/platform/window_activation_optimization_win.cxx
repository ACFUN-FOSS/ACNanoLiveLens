#include "platform/window_activation_optimization.hxx"

#ifdef WIN32

#define WIN32_LEAN_AND_MEAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <windows.h>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace {

bool isSameProcessWindow(HWND hwnd) {
	if (!hwnd)
		return false;

	DWORD processId = 0;
	GetWindowThreadProcessId(hwnd, &processId);
	return processId == GetCurrentProcessId();
}

void forceActivateWindow(HWND hwnd) {
	if (!hwnd || GetForegroundWindow() == hwnd)
		return;

	ShowWindow(hwnd, SW_RESTORE);
	SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	BringWindowToTop(hwnd);
	SetForegroundWindow(hwnd);
	SetActiveWindow(hwnd);
	SetFocus(hwnd);
}

}

MsgBoxWindowActivationGuard::MsgBoxWindowActivationGuard(GLFWwindow *msgBoxWindow)
	: msgBoxWindow_{ msgBoxWindow } {
	update();
}

MsgBoxWindowActivationGuard::~MsgBoxWindowActivationGuard() = default;

void MsgBoxWindowActivationGuard::update() {
	if (!msgBoxWindow_)
		return;

	auto msgBoxHwnd = glfwGetWin32Window(msgBoxWindow_);
	if (!msgBoxHwnd)
		return;

	auto foregroundWindow = GetForegroundWindow();
	if (!foregroundWindow)
		return;

	if (foregroundWindow == msgBoxHwnd)
		return;

	if (!isSameProcessWindow(foregroundWindow))
		return;

	forceActivateWindow(msgBoxHwnd);
}

#endif
