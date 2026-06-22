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
#include <EatiEssentials/memsafety.hxx>
#include <RmlUi_Platform_GLFW.h>

namespace RmlUIWin {

class WinManager;

class UiWin
{
public:
    class AsyncOpScope
    {
    public:
        AsyncOpScope() = default;
        explicit AsyncOpScope(UiWin *owner) noexcept;
        ~AsyncOpScope();

        AsyncOpScope(const AsyncOpScope &) = delete;
        AsyncOpScope &operator=(const AsyncOpScope &) = delete;
        AsyncOpScope(AsyncOpScope &&other) noexcept;
        AsyncOpScope &operator=(AsyncOpScope &&other) noexcept;

    private:
        void release() noexcept;

        UiWin *owner_ = nullptr;
    };

    class EventListener : public Rml::EventListener
    {
    public:
        void ProcessEvent(Rml::Event& event) override;
    };

    UiWin(std::string name, Rml::Vector2i size, std::filesystem::path documentPath, bool isMain = false, bool isTransparent = false);

    UiWin(const UiWin &) = delete;
    UiWin& operator=(const UiWin &) = delete;
    UiWin(UiWin &&other) noexcept = delete;
    UiWin& operator=(UiWin &&other) noexcept = delete;

    ~UiWin();

    [[nodiscard]] gsl::not_null<GLFWwindow *> getNativeWin() const LIFETIMEBOUND;
    [[nodiscard]] Rml::Context &getContext() const LIFETIMEBOUND;
    [[nodiscard]] Rml::ElementDocument &getDocument() const LIFETIMEBOUND;

    void update() const;
    void render() const;
    void reload();

    void setUpdateCb(std::function<void()> cb);
    void setDocumentChangedCb(std::function<void()> cb);

    [[nodiscard]] std::string_view getName() const;
    [[nodiscard]] bool isMainWin() const;
    [[nodiscard]] Rml::Vector2i getMousePos() const;
    [[nodiscard]] Rml::Vector2i getWinPos() const;
    [[nodiscard]] Rml::Element &getRootElement() const;
    [[nodiscard]] AsyncOpScope startAsyncOp() noexcept;
    void setWinPos(const Rml::Vector2i pos);
    void setShouldClose();

private:
    void acquireAsyncOp() noexcept;
    void releaseAsyncOp() noexcept;
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

    UiWin &transferWin(std::unique_ptr<UiWin>&& window) LIFETIMEBOUND;
    void updateAll();
    void renderAll();
    void cleanupClosedWindows();
    void requestCloseAllWindows();

    [[nodiscard]] bool hasOpenWins() const;
    [[nodiscard]] UiWin &getMainWin() const;
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

    std::vector<std::unique_ptr<UiWin>> wins_;
    std::unordered_map<gsl::not_null<Rml::Context *>, gsl::not_null<UiWin *>> context2Win_;
    UiWin *modalWin_ = nullptr;
};

} // namespace RmlUIWin
