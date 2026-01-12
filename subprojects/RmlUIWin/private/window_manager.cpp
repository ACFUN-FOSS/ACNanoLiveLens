#include <RmlUIWin/window_manager.h>
#include <RmlUi_Backend.h>
#include <RmlUi/Debugger.h>
#include <iostream>
#include <algorithm>
#include <cassert>
#include <print>
#include <stdexcept>

using namespace rmlui_wrapper;

bool processKeyDownShortcuts(Rml::Context* context, Rml::Input::KeyIdentifier key, int key_modifier, float native_dp_ratio, bool priority) {
    if (!context)
        return true;

    // Result should return true to allow the event to propagate to the next handler.
    bool result = false;

    // This function is intended to be called twice by the backend, before and after submitting the key event to the context. This way we can
    // intercept shortcuts that should take priority over the context, and then handle any shortcuts of lower priority if the context did not
    // intercept it.
    if (priority)
    {
        // Priority shortcuts are handled before submitting the key to the context.

        // Toggle debugger and set dp-ratio using Ctrl +/-/0 keys.
        if (key == Rml::Input::KI_F8)
        {
            Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
        }
        else if (key == Rml::Input::KI_0 && key_modifier & Rml::Input::KM_CTRL)
        {
            context->SetDensityIndependentPixelRatio(native_dp_ratio);
        }
        else if (key == Rml::Input::KI_1 && key_modifier & Rml::Input::KM_CTRL)
        {
            context->SetDensityIndependentPixelRatio(1.f);
        }
        else if ((key == Rml::Input::KI_OEM_MINUS || key == Rml::Input::KI_SUBTRACT) && key_modifier & Rml::Input::KM_CTRL)
        {
            const float new_dp_ratio = Rml::Math::Max(context->GetDensityIndependentPixelRatio() / 1.2f, 0.5f);
            context->SetDensityIndependentPixelRatio(new_dp_ratio);
        }
        else if ((key == Rml::Input::KI_OEM_PLUS || key == Rml::Input::KI_ADD) && key_modifier & Rml::Input::KM_CTRL)
        {
            const float new_dp_ratio = Rml::Math::Min(context->GetDensityIndependentPixelRatio() * 1.2f, 2.5f);
            context->SetDensityIndependentPixelRatio(new_dp_ratio);
        }
        else
        {
            // Propagate the key down event to the context.
            result = true;
        }
    }
    else
    {
        // We arrive here when no priority keys are detected and the key was not consumed by the context. Check for shortcuts of lower priority.
        if (key == Rml::Input::KI_R && key_modifier & Rml::Input::KM_CTRL)
        {
            for (int i = 0; i < context->GetNumDocuments(); i++)
            {
                Rml::ElementDocument* document = context->GetDocument(i);
                const Rml::String& src = document->GetSourceURL();
                if (src.size() > 4 && src.substr(src.size() - 4) == ".rml")
                {
                    document->ReloadStyleSheet();
                }
            }
        }
        else
        {
            result = true;
        }
    }

    return result;
}

void UiWin::EventListener::ProcessEvent(Rml::Event& event) {
    std::cout << "Event received: " << event.GetType().c_str() << std::endl;
}

UiWin::UiWin(std::string name, Rml::Vector2i size, std::string document_path, bool is_main) 
    : _name(std::move(name)), _size(size), _documentPath(std::move(document_path)), _isMainWin(is_main) {
    // 根據是否為主窗口來獲取或創建窗口
    if (_isMainWin) { 
        _win = Backend::GetMainWindow();
        if (!_win) {
            throw std::runtime_error(std::string("Failed to get main window for: ") + _name);
        }
    } else {
        _win = Backend::CreateWindow(_name.c_str(), _size.x, _size.y, true);
        if (!_win) {
            throw std::runtime_error(std::string("Failed to create window: ") + _name);
        }
    }

    // 創建 RmlUi context
    _context = Rml::CreateContext(_name, _size);
    if (!_context) {
        throw std::runtime_error(std::string("Failed to create context for window: ") + _name);
    }

    // 將 context 關聯到窗口
    Backend::AttachContext(_win, _context, &processKeyDownShortcuts);

    // 載入文檔
    _document = _context->LoadDocument(_documentPath);
    if (!_document) {
        throw std::runtime_error(std::string("Failed to load document for window: ") + _name);
    }
    _document->AddEventListener("click", &_eventListener);
    _document->Show();

    std::println("Created window: {}, ptr: {}", _name, static_cast<void *>(_win));
}

UiWin::UiWin(UiWin&& other) noexcept {
    _name = std::move(other._name);
    _size = other._size;
    _documentPath = std::move(other._documentPath);
    _isMainWin = other._isMainWin;
    _win = other._win;
    other._win = nullptr;
    _context = other._context;
    other._context = nullptr;
    _document = other._document;
    other._document = nullptr;
}

UiWin& UiWin::operator=(UiWin&& other) noexcept {
    if (this == &other)
        return *this;

    std::println("Move window: {}(ptr: {}) to {} (ptr: {})", other._name, static_cast<void *>(other._win), _name, static_cast<void *>(_win));

    destroy();

    _name = std::move(other._name);
    _size = other._size;
    _documentPath = std::move(other._documentPath);
    _isMainWin = other._isMainWin;
    _win = other._win;
    other._win = nullptr;
    _context = other._context;
    other._context = nullptr;
    _document = other._document;
    other._document = nullptr;

    return *this;
}

UiWin::~UiWin() {
    destroy();
}

void UiWin::destroy() {
    std::println("Destroying window: {}, ptr: {}", _name, static_cast<void *>(_win));

    if (_context) {
        _context->RemoveEventListener("click", &_eventListener);
        Rml::RemoveContext(_name.c_str());
        _context = nullptr;
    }
    if (!_isMainWin && _win) {
        Backend::DestroyWindow(_win);
        _win = nullptr;
    }
}

gsl::not_null<GLFWwindow*> UiWin::getNativeWin() const { 
    return _win; 
}

Rml::Context &UiWin::getContext() const { 
    return *_context; 
}

void UiWin::update() const {
    if (_context)
        _context->Update();
}

void UiWin::render() const {
    if (_context)
        _context->Render();
}

const std::string& UiWin::getName() const { 
    return _name; 
}

bool UiWin::isMainWin() const { 
    return _isMainWin; 
}

// WinManager 实现

void WinManager::transferWin(std::unique_ptr<UiWin>&& window) {
    wins_.push_back(std::move(window));
}

void WinManager::updateAll() {
    for (auto& window : wins_) {
        window->update();
    }
}

void WinManager::renderAll() {
    for (auto& window : wins_)
    {
        auto glfwWin = window->getNativeWin();
        Backend::BeginFrame(glfwWin);
        window->render();
        Backend::PresentFrame(glfwWin);
    }
}

void WinManager::cleanupClosedWindows() {
    std::erase_if(wins_,
        [](const auto& win) {
            return Backend::ShouldWindowClose(win->getNativeWin());
    });
}

bool WinManager::hasOpenWins() const {
    return !wins_.empty();
}

UiWin &WinManager::getMainWin() {
    auto mainWinIt = std::ranges::find_if(
        wins_,
        [](const auto& win) { return win->isMainWin(); }
    );
    assert(mainWinIt != wins_.end());
    return **mainWinIt;
}
