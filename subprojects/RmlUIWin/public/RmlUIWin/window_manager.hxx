#pragma once

#include <RmlUi/Core.h>
#include <GLFW/glfw3.h>
#include <gsl/gsl>
#include <string>
#include <memory>
#include <vector>
#include <filesystem>
#include <EatiEssentials/memory.hxx>
#include <RmlUi_Platform_GLFW.h>

// 前向声明
bool processKeyDownShortcuts(Rml::Context* context, Rml::Input::KeyIdentifier key, int key_modifier, float native_dp_ratio, bool priority);

namespace RmlUIWin {

class UiWin
{
public:
    class EventListener : public Rml::EventListener
    {
    public:
        void ProcessEvent(Rml::Event& event) override;
    };

    UiWin(std::string name, Rml::Vector2i size, std::filesystem::path documentPath, bool isMain = false, bool isTransparent = false);
    
    UiWin(const UiWin &) = delete;
    UiWin& operator=(const UiWin &) = delete;
    UiWin(UiWin &&other) noexcept = default;
    UiWin& operator=(UiWin &&other) noexcept;
    
    ~UiWin();

    [[nodiscard]] gsl::not_null<GLFWwindow *> getNativeWin() const;
    [[nodiscard]] Rml::Context &getContext() const;
    
    void update() const;
    void render() const;
    
    [[nodiscard]] const std::string_view getName() const;
    [[nodiscard]] bool isMainWin() const;
	[[nodiscard]] Rml::Vector2i getMousePos() const;
	[[nodiscard]] Rml::Vector2i getWinPos() const;
    void setWinPos(const Rml::Vector2i pos);

private:
    void destroy();
	
	struct RmlCStyleData;
	struct SelfData;

	struct Data
	{
		ESSM::Box<RmlCStyleData> _rmlCStyleData;
		ESSM::Box<SelfData> _selfData;
	};
	
	std::optional<Data> _data;
	
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
    [[nodiscard]] bool hasOpenWins() const;
    
    // 获取主窗口
    [[nodiscard]] UiWin &getMainWin() const;

    [[nodiscard]] UiWin *getWinOfElement(const Rml::Element &element) const;

	std::vector<std::function<void()>> reloadCbs;


private:
    std::vector<std::unique_ptr<UiWin>> wins_;
};

} // namespace rmlui_wrapper
