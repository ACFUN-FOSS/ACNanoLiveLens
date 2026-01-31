#include <RmlUIWin/window_manager.hxx>
#include <RmlUi_Backend.h>
#include <RmlUi/Debugger.h>
#include <EatiEssentials/memory.hxx>
#include <EatiEssentials/memsafety.hxx>
#include <EatiEssentials/special.hxx>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

#include "appstate.hxx"
#include "sound/sound.hxx"
#include "assets.hxx"
#include "rmluipp.hxx"
#include "rmlui_sys.hxx"
#include "custom_elements.hxx"
#include "platform/crash_handler.hxx"

using namespace RmlUIWin;
using namespace Essentials::Memory;

void rmluiMain() {
    RmlUISystem rmlui{ *Backend::GetSystemInterface(), *Backend::GetRenderInterface() };

    // 載入字體
    Rml::LoadFontFace((getAssetsDir() / "NokiaSans-Regular.ttf").string());

    registerCustomElements(rmlui);

    //Essentials::Special::callNullptr();

    {
        // 使用窗口管理器管理所有窗口
        auto &winMan = getAppState().winManager;

        // 第一個窗口使用主窗口（在 Backend::Initialize 時已創建）
        // 主窗口創建時大小忽略傳入的大小
        auto mainWin = newBox(UiWin{ "main", {}, getAssetsDir() / "main.rml", true });
        // 後續窗口自動創建新窗口，加載不同的 RML 文件
        //auto secondaryWin = newBox(UiWin{ "RmlUi App - Secondary", {800, 600}, "assets/secondary.rml", false });
        //auto thirdWin = newBox(UiWin{ "RmlUi App - Third", {800, 600}, "assets/third.rml", false });


		auto &mainWinRootEle = UNWRAP(mainWin->getContext().GetRootElement());
		SimpleEventListenerManager mainWinRootEleEventMan{ mainWinRootEle };
		mainWinRootEleEventMan.on("test-btn-1", "click", [](auto &&_) {
			assert(false);
		});
		mainWinRootEleEventMan.on("test-btn-2", "click", [](auto&& _) {
			Essentials::Special::callNullptr();
		});

        // 使用std::move將窗口所有權轉移給管理器
        winMan.transferWin(std::move(mainWin));
        //windowManager.transferWin(std::move(secondaryWin));
        //windowManager.transferWin(std::move(thirdWin));

        bool running = true;
        while (running && winMan.hasOpenWins()) {
            // 处理输入和窗口事件
            running = Backend::ProcessEvents(false);

            // 更新所有窗口
            winMan.updateAll();

            // 渲染所有窗口
            winMan.renderAll();

            // 清理已關閉的窗口
            winMan.cleanupClosedWindows();
        }
    }
}




int crashHandlerProtectedMain() {
#ifdef WIN32
    system("chcp 65001");
#endif
    initSound();

    // 初始化后端
    Backend::Initialize("RmlUi App", 800, 600, true);

    //Essentials::Special::callNullptr();
    rmluiMain();

    // 关闭后端
    Backend::Shutdown();
    return 0;
}


int main() {
    runProtectedMain();
}

