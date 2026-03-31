#include "assets.hxx"
#include "appstate.hxx"
#include "platform/crash_handler.hxx"
#include "sound/sound.hxx"
#include "rmlui_sys.hxx"
#include "rmluipp.hxx"
#include "RmlUIWin/window_manager.hxx"
#include "rmlui_element.hxx"
#include "test_win.hxx"

using namespace RmlUIWin;
using namespace Essentials::Memory;
using namespace Essentials::IO;

static void rmluiMain() {
    RmlUISystem rmlui{ *Backend::GetSystemInterface(), *Backend::GetRenderInterface() };

    // 載入字體
    Rml::LoadFontFace((getAssetsDir() / "NokiaSans-Regular.ttf").string());

    registerCustomElements(rmlui);

    //Essentials::Special::callNullptr();

    {
        // 使用窗口管理器管理所有窗口
        auto &winMan = getAppState().winManager;

        RmlUIWin::onReloadTriggered = [](Rml::Context &context) {
            playSound(Sound::RELOAD);
            auto children = getAllChildrenRecursively(UNWRAP(context.GetRootElement()));


            auto windowEles = [&]() {
                std::vector<Refw<RmlUIElement>> windowEles;
                for (auto &child : children) {
                    if (auto ele = dynamic_cast<RmlUIElement *>(&child.get())) {
                        if (ele->getIsWindowElement())
                            windowEles.emplace_back(UNWRAP(ele));
                    }
                }
                return windowEles;
            }();

            for (auto &child : windowEles) {
				child.get().reload();
            }
        };



        TestWin testWin;

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

