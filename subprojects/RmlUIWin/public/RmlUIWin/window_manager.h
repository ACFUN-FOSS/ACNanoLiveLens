#pragma once

#include <RmlUi/Core.h>
#include <GLFW/glfw3.h>
#include <gsl/gsl>
#include <string>
#include <memory>
#include <vector>

// 前向声明
bool processKeyDownShortcuts(Rml::Context* context, Rml::Input::KeyIdentifier key, int key_modifier, float native_dp_ratio, bool priority);

namespace rmlui_wrapper {

class UiWin
{
public:
    class EventListener : public Rml::EventListener
    {
    public:
        void ProcessEvent(Rml::Event& event) override;
    };

    UiWin(std::string name, Rml::Vector2i size, std::string document_path, bool is_main = false);
    
    UiWin(const UiWin&) = delete;
    UiWin& operator=(const UiWin&) = delete;
    UiWin(UiWin&& other) noexcept;
    UiWin& operator=(UiWin&& other) noexcept;
    
    ~UiWin();

    gsl::not_null<GLFWwindow*> getNativeWin() const;
    Rml::Context &getContext() const;
    
    void update() const;
    void render() const;
    
    const std::string& getName() const;
    bool isMainWin() const;

private:
    void destroy();

    EventListener _eventListener;
    std::string _name;
    Rml::Vector2i _size;
    std::string _documentPath;
    bool _isMainWin;
    gsl::owner<GLFWwindow*> _win = nullptr;
    Rml::Context *_context = nullptr;
    Rml::ElementDocument *_document = nullptr;
};

// 窗口管理器类
class WinManager {
public:
    WinManager() = default;
    ~WinManager() = default;

    // 禁止拷贝，允许移动
    WinManager(const WinManager&) = delete;
    WinManager& operator=(const WinManager&) = delete;
    WinManager(WinManager&&) = default;
    WinManager& operator=(WinManager&&) = default;

    // 添加窗口
    void transferWin(std::unique_ptr<UiWin>&& window);
    
    // 更新所有窗口
    void updateAll();
    
    // 渲染所有窗口
    void renderAll();
    
    // 清理关闭的窗口
    void cleanupClosedWindows();
    
    // 检查是否还有窗口打开
    bool hasOpenWins() const;
    
    // 获取主窗口
    UiWin &getMainWin();

private:
    std::vector<std::unique_ptr<UiWin>> wins_;
};

} // namespace rmlui_wrapper
