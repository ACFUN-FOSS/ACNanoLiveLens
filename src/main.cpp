#include <RmlUIWin/window_manager.h>
#include <RmlUi_Backend.h>
#include <RmlUi/Debugger.h>

using namespace rmlui_wrapper;

int main() {
    // 初始化后端
    Backend::Initialize("RmlUi App", 800, 600, true);

    // 设置 RmlUi 接口
    Rml::SetSystemInterface(Backend::GetSystemInterface());
    Rml::SetRenderInterface(Backend::GetRenderInterface());
    Rml::Initialise();

    // 載入字體
    Rml::LoadFontFace("assets/NokiaSans-Regular.ttf");

    {
        // 使用窗口管理器管理所有窗口
        WinManager windowManager;
        
        // 第一個窗口使用主窗口（在 Backend::Initialize 時已創建）
        auto mainWin = std::make_unique<UiWin>("main", Rml::Vector2i(800, 600), "assets/main.rml", true);
        // 後續窗口自動創建新窗口，加載不同的 RML 文件
        auto secondaryWin = std::make_unique<UiWin>("RmlUi App - Secondary", Rml::Vector2i(800, 600), "assets/secondary.rml", false);
        auto thirdWin = std::make_unique<UiWin>("RmlUi App - Third", Rml::Vector2i(800, 600), "assets/third.rml", false);

        // 使用std::move將窗口所有權轉移給管理器
        windowManager.transferWin(std::move(mainWin));
        windowManager.transferWin(std::move(secondaryWin));
        windowManager.transferWin(std::move(thirdWin));

        bool running = true;
        while (running && windowManager.hasOpenWins()) {
            // 处理输入和窗口事件
            running = Backend::ProcessEvents(false);

            // 更新所有窗口
            windowManager.updateAll();
            
            // 渲染所有窗口
            windowManager.renderAll();
            
            // 清理已關閉的窗口
            windowManager.cleanupClosedWindows();
        }
    }

    // 关闭 RmlUi
    Rml::Shutdown();

    // 关闭后端
    Backend::Shutdown();

    return 0;
}
