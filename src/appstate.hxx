#ifndef NANOLIVELENS_APPSTATE_HXX
#define NANOLIVELENS_APPSTATE_HXX
#include "RmlUIWin/window_manager.hxx"
#include "rmlui_sys.hxx"

#include <functional>

namespace App {

struct State
{
	gsl::not_null<RmlUISystem *const> rmluiSys;
    gsl::not_null<RmlUIWin::WinManager *const> winManager;

	gsl::not_null<coro::runtime *const> coroRuntime;
	ESSM::Rc<coro::executor> mainThreadExecutor;

	// 重新加载全部脚本（由 main 安装为 JsEngine::hotReload，按 F5 触发）。
	// 元素类只需要 App::State，不直接依赖 JsEngine，依赖方向保持单向。
	std::function<void()> reloadScripts;
};

void initState(State &&state);
State &getState();

void die(int code);

}

#endif