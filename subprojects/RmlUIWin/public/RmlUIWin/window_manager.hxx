#pragma once

#include <RmlUi/Core.h>
#include <GLFW/glfw3.h>
#include <gsl/gsl>
#include <string>
#include <memory>
#include <vector>
#include <filesystem>
#include <unordered_map>
#include <EatiEssentials/memory/memory.hxx>
#include <EatiEssentials/memory/memsafety.hxx>
#include <RmlUi_Backend.h>
#include <RmlUi_Platform_GLFW.h>

template <typename T> class UiWinBizLogicObjContext;

namespace RmlUIWin {

class WinManager;

class UiWin
{
public:

    class EventListener : public Rml::EventListener
    {
    public:
        void ProcessEvent(Rml::Event& event) override;
    };

    UiWin(std::string name, Rml::Vector2i size, std::filesystem::path documentPath, WinManager &winManager, bool isMain = false, bool isTransparent = false);
    UiWin(const UiWin &) = delete;
    UiWin& operator=(const UiWin &) = delete;
    UiWin(UiWin &&other) noexcept = delete;
    UiWin& operator=(UiWin &&other) noexcept = delete;

    ~UiWin();

    [[nodiscard]] gsl::not_null<GLFWwindow *> getNativeWin() const LIFETIMEBOUND;
    [[nodiscard]] Rml::Context &getContext() const LIFETIMEBOUND;
    [[nodiscard]] Rml::ElementDocument &getDocument() const LIFETIMEBOUND;

    void update();
    void render() const;
    void reload();

    void setUpdateCb(std::function<void()> cb);
    void setShowCb(std::function<void()> cb);
    void setDocumentChangedCb(std::function<void()> cb);

    [[nodiscard]] std::string_view getName() const;
    [[nodiscard]] bool isMainWin() const;
    [[nodiscard]] Rml::Vector2i getMousePos() const;
    [[nodiscard]] Rml::Vector2i getWinSize() const;
    [[nodiscard]] Rml::Vector2i getWinPos() const;
    [[nodiscard]] Rml::Element &getRootElement() const;
    void setWinPos(const Rml::Vector2i pos);
    void centerToPrimaryMonitor();
    void setPosInMonitor(const MonitorArea &monitorArea, const Rml::Vector2i relativePos);
    void hide();
    void show();
    void requestClose();
	[[nodiscard]] bool isHidden() const noexcept;
	bool isPendingClose() const;
	bool hasUnfinishedOp() const;
	void setRunningAsyncOp(bool running) noexcept;

private:
    
    void applyCloseRequestState();
    void refreshClosingVisualState();
    void requestCloseFromNativeEvent();
    [[nodiscard]] bool shouldDestroyNow() const noexcept;
    [[nodiscard]] bool shouldShowClosingVisualState() const noexcept;
    [[nodiscard]] bool canCloseNow() const noexcept;
    void destroy();
    void attachDocument(Rml::ElementDocument &document);
    void detachDocument() const;
    void notifyDocumentChanged() const;

    struct RmlCStyleData;
    struct SelfData;

    struct Data
    {
        ESSM::Box<RmlCStyleData> _rmlCStyleData;
        ESSM::Box<SelfData> _selfData;
    };

    std::optional<Data> _data;

    WinManager *_winManager = nullptr;

    //template <typename T> friend class ::UiWinBizLogicObjAsyncOpScope;
	template <typename T> friend class UiWinBizLogicObjContext;
    friend class WinManager;
};

class WinManager {
public:
    WinManager();
    ~WinManager();

    WinManager(const WinManager&) = delete;
    WinManager& operator=(const WinManager&) = delete;
    WinManager(WinManager&&) = delete;
    WinManager& operator=(WinManager&&) = delete;

    void updateAll();
    void renderAll();
    void cleanupClosedWindows();
    void requestCloseAllWindows();

    [[nodiscard]] bool hasOpenWins() const;
    [[nodiscard]] bool hasVisibleWins() const;
    [[nodiscard]] UiWin &getMainWin() const;
    [[nodiscard]] bool hasMainWin() const;
    [[nodiscard]] MonitorArea getPrimaryMonitorArea() const;
    [[nodiscard]] Rml::Vector<MonitorArea> getMonitorAreas() const;
    [[nodiscard]] UiWin *getWinOfElement(const Rml::Element &element) const;
    [[nodiscard]] UiWin *getWinOfContext(const Rml::Context& context) const;

    void reloadWindow(UiWin &window);
    void setModalWin(UiWin *window);
    [[nodiscard]] UiWin *getModalWin() const;
    bool processKeyDownShortcuts(Rml::Context *context, Rml::Input::KeyIdentifier key, int key_modifier, float native_dp_ratio, bool priority);

private:
    friend class UiWin;

    void registerWindow(UiWin &window);
    void unregisterWindow(const UiWin &window);
    [[nodiscard]] bool isInputAllowedForContext(const Rml::Context *context) const;

    std::vector<gsl::not_null<UiWin *>> wins_;
    std::unordered_map<gsl::not_null<Rml::Context *>, gsl::not_null<UiWin *>> context2Win_;
    UiWin *modalWin_ = nullptr;
};

} // namespace RmlUIWin
