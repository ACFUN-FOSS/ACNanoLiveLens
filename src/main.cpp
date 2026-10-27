#include "Core/assets.hxx"
#include "appstate.hxx"
#include "js_engine.hxx"
#include "js_hook.hxx"
#include "script_style.hxx"
#include "platform/crash_handler.hxx"
#include "rmlui_sys.hxx"
#include "rmluipp.hxx"
#include "RmlUIWin/window_manager.hxx"
#include "rmlui_element.hxx"

#include "danmaku_monitor_win.hxx"
#include "test_win.hxx"
#include "login_win.hxx"
#include "msg_box.hxx"
#include "uiwin_bizlogic_obj_async_op_scope.hxx"

using namespace RmlUIWin;
using namespace Essentials::Memory;
using namespace Essentials::IO;

static bool ctrlCPressed = false;

static void runUiFrame(WinManager &winMan, coro::manual_executor &mainThreadExecutor) {
	// 脚本注入的 RCSS 与文档对账：文档首次加载、F6 重载样式、Ctrl+R 重建文档、
	// F5 重载脚本都会换掉文档的样式容器，由这里补挂回去（幂等，无变化时只是几次比较）。
	ScriptStyle::syncAll();

	mainThreadExecutor.loop_once();
	winMan.updateAll();
	winMan.renderAll();
	winMan.cleanupClosedWindows();
}

static void drainUiShutdown(WinManager &winMan, coro::manual_executor &mainThreadExecutor) {
	winMan.requestCloseAllWindows();

	while (winMan.hasVisibleWins()) {
		// Keep pumping events and the coroutine executor until every window
		// has finished its async work and can be hidden safely.
		Backend::ProcessEvents(false);
		runUiFrame(winMan, mainThreadExecutor);
	}
}

static void shutdown(WinManager &winMan) {
	winMan.requestCloseAllWindows();
}

static void runUiMainLoop(WinManager &winMan, coro::manual_executor &mainThreadExecutor) {
	while (winMan.hasVisibleWins()) {

		if (ctrlCPressed) {
			drainUiShutdown(winMan, mainThreadExecutor);
			return;
		}

		const bool shouldContinue = Backend::ProcessEvents(false);
		if (!shouldContinue) {
			drainUiShutdown(winMan, mainThreadExecutor);
			return;
		}

		runUiFrame(winMan, mainThreadExecutor);
	}
}

static void rmluiMain() {

    RmlUISystem rmlui{ *Backend::GetSystemInterface(), *Backend::GetRenderInterface() };

    // 載入字體
    Rml::LoadFontFace((getAssetsDir() / "NokiaSans-Regular.ttf").string());
	Rml::LoadFontFace((getAssetsDir() / "wqy-zenhei.ttc").string(), true);

    registerCustomElements(rmlui);

    //Essentials::Special::callNullptr();

	coro::runtime coroRuntime;
	auto mainThreadExecutor = coroRuntime.make_manual_executor();


	{
        // 使用窗口管理器管理所有窗口
		RmlUIWin::WinManager winMan;

		App::initState({
			 &rmlui,
			 &winMan,
			 &coroRuntime,
			 mainThreadExecutor
		});

		// 脚本引擎。声明在窗口与业务对象之前：脚本回调持有 JS 引用，
		// 引擎必须比它们活得更久。
		JsEngine jsEngine;

		try {

			// 初始化引擎（注册 C++ 类型、时间点翻译器、JS 函数桥接与钩子注册函数），
			// 再加载 assets/scripts 下的全部用户脚本。
			jsEngine.init();
			jsEngine.loadScripts(getAssetsDir() / "scripts");

			// 安装脚本热重载入口：F5 由 RmlUIElement 转发到这里。
			App::getState().reloadScripts = [&jsEngine] {
				jsEngine.hotReload();
			};
			// 该回调捕获了 jsEngine 的引用，而它存放在全局 App 状态中，
			// 销毁时机晚于 jsEngine；离开本作用域前必须摘除，避免悬空引用。
			const auto removeReloadEntry = gsl::final_action{ [&] {
				App::getState().reloadScripts = nullptr;
			} };

			AcliveBackendDaemon acliveBackendDaemon{ mainThreadExecutor };
			acliveBackendDaemon.onCrashLimitExceeded([] {
				MsgBox::popupOKMsgBox(MsgBox::Type::EERR,
					"後端崩潰已超過三次。\n"
					"请联系 AcFun 弹幕姬问题反馈 QQ 群，并附上日志文件。"
				);

				App::die(EXIT_FAILURE);
			});

			//loadJsScript();

        	//TestWin testWin;
			
			//LoginWin loginWin;
			//UiWinBizLogicObjHandler<LoginWin> loginWin;
			//loginWin->getUiWin().show();

			// SysStartUp：RmlUi 上下文、窗口管理器、音频系统、脚本引擎都已初始化完毕，
			// 即将显示第一个窗体（登入窗体）。此处尚未接入登入窗体，故紧接着触发
			// PostLogin 并直接显示应用窗体。
			JsHook::triggerSysStartUp(std::chrono::system_clock::now());

			UiWinBizLogicObjHandler<DanmakuMonitorWin> danmakuMonitorWin{ nullptr };

			// PostLogin：用户已成功登入，即将显示应用窗体（弹幕监视窗体）。
			JsHook::triggerPostLogin();

			danmakuMonitorWin->getUiWin().show();

		    //assert(false);

			runUiMainLoop(winMan, *mainThreadExecutor);
		} catch (const ExcpToShowToUser &e) {
			std::println("Exception to show to user: {}", e.humanFriendlyMsg());
			MsgBox::popupOKMsgBox(MsgBox::Type::EERR, e.humanFriendlyMsg());
		}
    }
}

static void testSTLHardending() {
	std::vector<int> v{ 1, 3, 5};
	std::println("{}", v[3]);
	std::exit(0);
}


int crashHandlerProtectedMain() {
#ifdef WIN32
    system("chcp 65001");
#endif

	// testSTLHardending();
	//NLS_ASSERT(false);

	CtrlCLibrary::SetCtrlCHandler([](enum CtrlCLibrary::CtrlSignal signal) {
		if (signal == CtrlCLibrary::kCtrlCSignal) {
			ctrlCPressed = true;
			std::println("Ctrl+C pressed");
		}
		return true;
	});

    initSound();

    // 初始化后端
    Backend::Initialize("RmlUi App", 360, 450, true);

    //Essentials::Special::callNullptr();
    rmluiMain();

    // 关闭后端
	gsl::final_action _{ [&]() {
		Backend::Shutdown();
	} };

    return 0;
}


int main() {
    runProtectedMain();
}
