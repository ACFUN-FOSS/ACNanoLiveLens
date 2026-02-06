#include "RmlUIWin/window_manager.hxx"
#include <iostream>
#include <algorithm>
#include <cassert>
#include <print>
#include <stdexcept>

#include <EatiEssentials/misc.hxx>
#include <RmlUi_Backend.h>
#include <RmlUi/Debugger.h>

using namespace std::string_literals;
using namespace Essentials::Misc;

namespace RmlUIWin
{

static bool processKeyDownShortcuts(Rml::Context* context, Rml::Input::KeyIdentifier key, int key_modifier, float native_dp_ratio, bool priority) {
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
            std::cout << "RML reloading candidate: ";
            for (int i = 0; i < context->GetNumDocuments(); i++) {
                Rml::ElementDocument* document = context->GetDocument(i);
                const Rml::String& src = document->GetSourceURL();
                std::cout << src << ' ';
                if (src.size() > 4 && src.substr(src.size() - 4) == ".rml")
                    std::cout << " -- YES";
                else
                    std::cout << " -- NO";
                std::cout << std::endl;
            }
            std::cout << std::endl;

            for (int i = 0; i < context->GetNumDocuments(); i++)
            {
                Rml::ElementDocument* document = context->GetDocument(i);
                const Rml::String& src = document->GetSourceURL();
                if (src.size() > 4 && src.substr(src.size() - 4) == ".rml")
                {
                    std::println("Reloading: {}", src);
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

/**
 * The non-RAII resource handles for UiWin.
 */
struct UiWin::RmlCStyleData
{
	gsl::not_null<GLFWwindow *> _win;
	gsl::not_null<Rml::Context *> _context;
	gsl::not_null<Rml::ElementDocument *> _document;
	gsl::not_null<SystemInterface_GLFW *> _systemInterface;
};

struct UiWin::SelfData
{
	EventListener _eventListener{};
	std::string _name;
	Rml::Vector2i _size;
	std::filesystem::path _documentPath;
	bool _isMainWin;
	bool _isTransparent;
};

void UiWin::EventListener::ProcessEvent(Rml::Event& event) {
    //std::cout << "Event received: " << event.GetType().c_str() << std::endl;
    
}

UiWin::UiWin(std::string name, Rml::Vector2i size, std::filesystem::path documentPath, bool isMain, bool isTransparent)
	: _data{ Data {
		._rmlCStyleData = { [&]() {
			auto &win = [&] -> auto & {
				// 根據是否為主窗口來獲取或創建窗口
				if (isMain) {
					auto winptr = Backend::GetMainWindow();
					if (!winptr)
						throw std::runtime_error("Failed to get main window for: "s + name);
					return *winptr;
				} else {
					auto winptr = Backend::CreateWindow(name.c_str(), size.x, size.y, true);
					if (!winptr)
						throw std::runtime_error("Failed to create window: "s + name);
					return *winptr;
				}
			}();

			// 如果是主窗口，读取窗口大小
			if (isMain)
				size = Backend::GetWindowSize(&win);

			// 創建 RmlUi context
			auto contextptr = Rml::CreateContext(name, size);
			if (!contextptr)
				throw std::runtime_error("Failed to create context for window: "s + name);


			auto documentptr = contextptr->LoadDocument(documentPath.string());
			if (!documentptr)
				throw std::runtime_error("Failed to load document for window: "s + name);

			return newBox(RmlCStyleData {
				._win = &win,
				._context = contextptr,
				._document = documentptr,
				._systemInterface = dynamic_cast<SystemInterface_GLFW *>(Rml::GetSystemInterface())
			});

		}()},
		._selfData = newBox(SelfData {
			._eventListener = EventListener{ },
			._name = std::move(name),
			._size = size,
			._documentPath = std::move(documentPath),
			._isMainWin = isMain,
			._isTransparent = isTransparent,
		})
	} } {

    // 將 context 關聯到窗口并显示
    Backend::AttachContext(_data->_rmlCStyleData->_win, _data->_rmlCStyleData->_context, &processKeyDownShortcuts);
	_data->_rmlCStyleData->_document->AddEventListener("click", &_data->_selfData->_eventListener);
	_data->_rmlCStyleData->_document->Show();

    std::println("Created window: {}, ptr: {}", _data->_selfData->_name, ptrToHex(_data->_rmlCStyleData->_win));
}

UiWin &UiWin::operator=(UiWin &&other) noexcept {
    std::println("Moving window: {}, ptr: {}", _data->_selfData->_name, ptrToHex(_data->_rmlCStyleData->_win));
	_data = std::move(other._data);
	return *this;
}

UiWin::~UiWin() {
    destroy();
}

void UiWin::destroy() {
	if (!_data)
		return;

    std::println("Destroying window: {}, ptr: {}", _data->_selfData->_name, ptrToHex(_data->_rmlCStyleData->_win));

	_data->_rmlCStyleData->_context->RemoveEventListener("click", &_data->_selfData->_eventListener);
	Rml::RemoveContext(_data->_selfData->_name.c_str());

	if (!_data->_selfData->_isMainWin && _data->_rmlCStyleData->_win) {
		Backend::DestroyWindow(_data->_rmlCStyleData->_win);
	}
    
}

[[nodiscard]] gsl::not_null<GLFWwindow*> UiWin::getNativeWin() const { 
    return _data->_rmlCStyleData->_win; 
}

[[nodiscard]] Rml::Context &UiWin::getContext() const { 
    return *_data->_rmlCStyleData->_context;
}

void UiWin::update() const {
    if (_data->_rmlCStyleData->_context)
		_data->_rmlCStyleData->_context->Update();
}

void UiWin::render() const {
    if (_data->_rmlCStyleData->_context)
		_data->_rmlCStyleData->_context->Render();
}

[[nodiscard]] const std::string_view UiWin::getName() const { 
    return _data->_selfData->_name;
}

[[nodiscard]] bool UiWin::isMainWin() const { 
    return _data->_selfData->_isMainWin;
}

[[nodiscard]] Rml::Vector2i UiWin::getMousePos() const {
	return _data->_rmlCStyleData->_systemInterface->GetMousePosition();
}

[[nodiscard]] Rml::Vector2i UiWin::getWinPos() const {
    return Backend::GetWindowPos(_data->_rmlCStyleData->_win);
}

[[nodiscard]] UiWin *WinManager::getWinOfElement(const Rml::Element &element) const {
    auto it = std::ranges::find_if(
        wins_,
        [&](const auto& win) { return &win->getContext() == element.GetContext(); }
    );
    return it != wins_.end() ? it->get() : nullptr;
}

void UiWin::setWinPos(const Rml::Vector2i pos) {
    Backend::SetWindowPos(_data->_rmlCStyleData->_win, pos);
}


// Ensure subclasses of UiWin are default-moveable
class Test1 : public UiWin
{
public:
	using UiWin::UiWin;
	Test1(Test1 &&) = default;
};



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

[[nodiscard]] bool WinManager::hasOpenWins() const {
    return !wins_.empty();
}

[[nodiscard]] UiWin &WinManager::getMainWin() const {
    auto mainWinIt = std::ranges::find_if(
        wins_,
        [](const auto& win) { return win->isMainWin(); }
    );

    assert(mainWinIt != wins_.end());
    return **mainWinIt;
}

}