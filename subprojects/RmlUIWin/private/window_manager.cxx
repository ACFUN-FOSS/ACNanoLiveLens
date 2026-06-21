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
using namespace Essentials::Memory;

namespace RmlUIWin
{

static WinManager *activeWinManager = nullptr;

static bool processKeyDownShortcutsBridge(Rml::Context *context, Rml::Input::KeyIdentifier key, int key_modifier, float native_dp_ratio, bool priority) {
	if (!activeWinManager) {
		return true;
	}

	return activeWinManager->processKeyDownShortcuts(context, key, key_modifier, native_dp_ratio, priority);
}

struct UiWin::RmlCStyleData
{
	gsl::not_null<GLFWwindow *> _win;
	gsl::not_null<Rml::Context *> _context;
	gsl::not_null<Rml::ElementDocument *> _document;
};

struct UiWin::SelfData
{
	EventListener _eventListener{};
	std::string _name;
	Rml::Vector2i _size;
	std::filesystem::path _documentPath;
	bool _isMainWin;
	bool _isTransparent;
	std::function<void()> _updateCb;
	std::function<void()> _documentChangedCb;
};

void UiWin::EventListener::ProcessEvent(Rml::Event &event) {
}

UiWin::UiWin(std::string name, Rml::Vector2i size, std::filesystem::path documentPath, bool isMain, bool isTransparent)
	: _data{ Data {
		._rmlCStyleData = { [&]() {
			auto &win = [&]() -> auto & {
				if (isMain) {
					auto winptr = Backend::GetMainWindow();
					if (!winptr) {
						throw std::runtime_error("Failed to get main window for: "s + name);
					}
					return *winptr;
				}

				auto winptr = Backend::CreateWindow(name.c_str(), size.x, size.y, true);
				if (!winptr) {
					throw std::runtime_error("Failed to create window: "s + name);
				}
				return *winptr;
			}();

			auto scale = Backend::GetMonitorContentScale();
			size.x *= static_cast<int>(scale.x);
			size.y *= static_cast<int>(scale.y);

			if (isMain) {
				Backend::SetWindowSize(&win, size);
			}

			auto contextptr = Rml::CreateContext(name, size);
			if (!contextptr) {
				throw std::runtime_error("Failed to create context for window: "s + name);
			}

			auto documentptr = contextptr->LoadDocument(documentPath.string());
			if (!documentptr) {
				throw std::runtime_error("Failed to load document for window: "s + name);
			}

			return newBox(RmlCStyleData{
				._win = &win,
				._context = contextptr,
				._document = documentptr,
			});
		}() },
		._selfData = newBox(SelfData{
			._eventListener = EventListener{},
			._name = std::move(name),
			._size = size,
			._documentPath = std::move(documentPath),
			._isMainWin = isMain,
			._isTransparent = isTransparent,
		})
	} } {
	Backend::AttachContext(_data->_rmlCStyleData->_win, _data->_rmlCStyleData->_context, &processKeyDownShortcutsBridge);
	attachDocument(*_data->_rmlCStyleData->_document);
	Rml::Debugger::Initialise(_data->_rmlCStyleData->_context);
	std::println("Created window: {}, ptr: {}", _data->_selfData->_name, ptrToHex(_data->_rmlCStyleData->_win));
}

UiWin::~UiWin() {
	destroy();
}

void UiWin::destroy() {
	if (!_data) {
		return;
	}

	std::println("Destroying window: {}, ptr: {}", _data->_selfData->_name, ptrToHex(_data->_rmlCStyleData->_win));

	detachDocument();
	if (_winManager) {
		_winManager->unregisterWindow(*this);
	}

	Rml::RemoveContext(_data->_selfData->_name.c_str());

	if (!_data->_selfData->_isMainWin && _data->_rmlCStyleData->_win) {
		Backend::DestroyWindow(_data->_rmlCStyleData->_win);
	}
}

[[nodiscard]] gsl::not_null<GLFWwindow *> UiWin::getNativeWin() const LIFETIMEBOUND {
	return _data->_rmlCStyleData->_win;
}

[[nodiscard]] Rml::Context &UiWin::getContext() const LIFETIMEBOUND {
	return *_data->_rmlCStyleData->_context;
}

[[nodiscard]] Rml::ElementDocument &UiWin::getDocument() const LIFETIMEBOUND {
	return *_data->_rmlCStyleData->_document;
}

void UiWin::update() const {
	if (_data->_rmlCStyleData->_context) {
		_data->_rmlCStyleData->_context->Update();
	}
	if (_data->_selfData->_updateCb) {
		try {
			_data->_selfData->_updateCb();
		} catch (const std::exception &e) {
			std::println("UI Runtime error: {}", e.what());
		}
	}
}

void UiWin::render() const {
	if (_data->_rmlCStyleData->_context) {
		_data->_rmlCStyleData->_context->Render();
	}
}

void UiWin::reload() {
	detachDocument();
	_data->_rmlCStyleData->_document->Close();

	auto documentptr = _data->_rmlCStyleData->_context->LoadDocument(_data->_selfData->_documentPath.string());
	if (!documentptr) {
		throw std::runtime_error("Failed to load document for window: "s + _data->_selfData->_name);
	}

	_data->_rmlCStyleData->_document = documentptr;
	attachDocument(*documentptr);
}

void UiWin::setUpdateCb(std::function<void()> cb) {
	_data->_selfData->_updateCb = std::move(cb);
}

void UiWin::setDocumentChangedCb(std::function<void()> cb) {
	_data->_selfData->_documentChangedCb = std::move(cb);
	notifyDocumentChanged();
}

[[nodiscard]] std::string_view UiWin::getName() const {
	return _data->_selfData->_name;
}

[[nodiscard]] bool UiWin::isMainWin() const {
	return _data->_selfData->_isMainWin;
}

[[nodiscard]] Rml::Vector2i UiWin::getMousePos() const {
	return UNWRAP(SystemInterface_GLFW::instance).GetMousePosition();
}

[[nodiscard]] Rml::Vector2i UiWin::getWinPos() const {
	return Backend::GetWindowPos(_data->_rmlCStyleData->_win);
}

[[nodiscard]] Rml::Element &UiWin::getRootElement() const LIFETIMEBOUND {
	return UNWRAP(_data->_rmlCStyleData->_context->GetRootElement());
}

void UiWin::setWinPos(const Rml::Vector2i pos) {
	Backend::SetWindowPos(_data->_rmlCStyleData->_win, pos);
}

void UiWin::setShouldClose() {
	Backend::SetShouldClose(_data->_rmlCStyleData->_win);
}

void UiWin::attachDocument(Rml::ElementDocument &document) {
	document.AddEventListener("click", &_data->_selfData->_eventListener);
	document.Show();
	notifyDocumentChanged();
}

void UiWin::detachDocument() const {
	if (_data && _data->_rmlCStyleData->_document) {
		_data->_rmlCStyleData->_document->RemoveEventListener("click", &_data->_selfData->_eventListener);
	}
}

void UiWin::notifyDocumentChanged() const {
	if (_data->_selfData->_documentChangedCb) {
		try {
			_data->_selfData->_documentChangedCb();
		} catch (const std::exception &e) {
			std::println("UI Runtime error: {}", e.what());
		}
	}
}

WinManager::WinManager() {
	activeWinManager = this;
	Backend::SetContextInputFilter([](Rml::Context *context) {
		return activeWinManager ? activeWinManager->isInputAllowedForContext(context) : true;
	});
}

WinManager::~WinManager() {
	wins_.clear();
	Backend::SetContextInputFilter(nullptr);
	if (activeWinManager == this) {
		activeWinManager = nullptr;
	}
}

UiWin &WinManager::transferWin(std::unique_ptr<UiWin> &&window) LIFETIMEBOUND {
	window->_winManager = this;
	registerWindow(*window);
	wins_.push_back(std::move(window));
	return *wins_.back();
}

void WinManager::updateAll() {
	for (auto &window : wins_) {
		window->update();
	}
}

void WinManager::renderAll() {
	for (auto &window : wins_) {
		auto glfwWin = window->getNativeWin();
		Backend::BeginFrame(glfwWin);
		window->render();
		Backend::PresentFrame(glfwWin);
	}
}

void WinManager::cleanupClosedWindows() {
	std::erase_if(wins_, [](const auto &win) {
		return Backend::ShouldWindowClose(win->getNativeWin());
	});
}

void WinManager::requestCloseAllWindows() {
	for (auto &window : wins_) {
		window->setShouldClose();
	}
}

[[nodiscard]] bool WinManager::hasOpenWins() const {
	return !wins_.empty();
}

[[nodiscard]] UiWin &WinManager::getMainWin() const {
	auto mainWinIt = std::ranges::find_if(wins_, [](const auto &win) {
		return win->isMainWin();
	});

	assert(mainWinIt != wins_.end());
	return **mainWinIt;
}

[[nodiscard]] UiWin *WinManager::getWinOfElement(const Rml::Element &element) const {
	if (!element.GetContext()) {
		return nullptr;
	}

	return getWinOfContext(*element.GetContext());
}

[[nodiscard]] UiWin *WinManager::getWinOfContext(const Rml::Context &context) const {
	auto it = context2Win_.find(const_cast<Rml::Context *>(&context));
	return it != context2Win_.end() ? it->second.get() : nullptr;
}

void WinManager::reloadWindow(UiWin &window) {
	window.reload();
}

void WinManager::setModalWin(UiWin *window) {
	modalWin_ = window;
}

[[nodiscard]] UiWin *WinManager::getModalWin() const {
	return modalWin_;
}

void WinManager::registerWindow(UiWin &window) {
	context2Win_.emplace(&window.getContext(), &window);
}

void WinManager::unregisterWindow(const UiWin &window) {
	context2Win_.erase(const_cast<Rml::Context *>(&window.getContext()));
	if (modalWin_ == &window) {
		modalWin_ = nullptr;
	}
}

[[nodiscard]] bool WinManager::isInputAllowedForContext(const Rml::Context *context) const {
	if (!modalWin_) {
		return true;
	}

	return &modalWin_->getContext() == context;
}

bool WinManager::processKeyDownShortcuts(Rml::Context *context, Rml::Input::KeyIdentifier key, int key_modifier, float native_dp_ratio, bool priority) {
	if (!context) {
		return true;
	}
	if (!isInputAllowedForContext(context)) {
		return false;
	}

	bool result = false;

	if (priority) {
		if (key == Rml::Input::KI_F8) {
			std::cout << (Rml::Debugger::IsVisible() ? "Hiding debugger" : "Showing debugger") << std::endl;
			Rml::Debugger::SetVisible(!Rml::Debugger::IsVisible());
		} else if (key == Rml::Input::KI_0 && key_modifier & Rml::Input::KM_CTRL) {
			context->SetDensityIndependentPixelRatio(native_dp_ratio);
		} else if (key == Rml::Input::KI_1 && key_modifier & Rml::Input::KM_CTRL) {
			context->SetDensityIndependentPixelRatio(1.f);
		} else if ((key == Rml::Input::KI_OEM_MINUS || key == Rml::Input::KI_SUBTRACT) && key_modifier & Rml::Input::KM_CTRL) {
			const float new_dp_ratio = Rml::Math::Max(context->GetDensityIndependentPixelRatio() / 1.2f, 0.5f);
			context->SetDensityIndependentPixelRatio(new_dp_ratio);
		} else if ((key == Rml::Input::KI_OEM_PLUS || key == Rml::Input::KI_ADD) && key_modifier & Rml::Input::KM_CTRL) {
			const float new_dp_ratio = Rml::Math::Min(context->GetDensityIndependentPixelRatio() * 1.2f, 2.5f);
			context->SetDensityIndependentPixelRatio(new_dp_ratio);
		} else {
			result = true;
		}
	} else {
		if (key == Rml::Input::KI_R && key_modifier & Rml::Input::KM_CTRL) {
			for (int i = 0; i < context->GetNumDocuments(); i++) {
				Rml::ElementDocument *document = context->GetDocument(i);
				const Rml::String &src = document->GetSourceURL();
				if (src.size() > 4 && src.substr(src.size() - 4) == ".rml") {
					std::println("Reloading: {}", src);
					document->ReloadStyleSheet();
				}
			}

			if (auto *uiWin = getWinOfContext(*context)) {
				reloadWindow(*uiWin);
			}
		} else {
			result = true;
		}
	}

	return result;
}

}
