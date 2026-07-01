#include "Core/assets.hxx"
#include "appstate.hxx"
#include "js_binding.hxx"
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
//#include "js_bindings.hxx"

using namespace RmlUIWin;
using namespace Essentials::Memory;
using namespace Essentials::IO;

static bool ctrlCPressed = false;

static void runUiFrame(WinManager &winMan, coro::manual_executor &mainThreadExecutor) {
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

		//qjs::Runtime rt;
		//qjs::Context ctx(rt);

	//initJs(rt, ctx);
		
		initAppState({
			 &rmlui,
			 &winMan,
			 &coroRuntime,
			 mainThreadExecutor
		});

		try {

			AcliveBackendDaemon acliveBackendDaemon{ mainThreadExecutor };
			acliveBackendDaemon.onCrashLimitExceeded([] {
				MsgBox::popupOKMsgBox(MsgBox::Type::EERR,
					"後端崩潰已超過三次。\n"
					"请联系 AcFun 弹幕姬问题反馈 QQ 群，并附上日志文件。"
				);


				Rml::Shutdown();
				std::_Exit(EXIT_FAILURE);
			});

			//loadJsScript();

        	//TestWin testWin;
			//DanmakuMonitorWin danmakuMonitorWin;
			//LoginWin loginWin;
			UiWinBizLogicObjHandler<LoginWin> loginWin;
			loginWin->getUiWin().show();

		    //assert(false);

			//JSBindings::init(rmlui, danmakuMonitorWin);

			runUiMainLoop(winMan, *mainThreadExecutor);
		} catch (const std::exception &e) {
			std::println("Exception: {}", e.what());
			MsgBox::popupOKMsgBox(MsgBox::Type::EERR, e.what());
		}
		//JSBindings::shutdown();
    }
}




int crashHandlerProtectedMain() {
#ifdef WIN32
    system("chcp 65001");
#endif

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
    Backend::Shutdown();

	
    return 0;
}


int main() {
    runProtectedMain();
}
