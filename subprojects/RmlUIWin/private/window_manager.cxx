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

static Rml::Element *findChildOrSelfByIdRecursive(Rml::Element *parent, std::string_view id) {
	if (!parent) {
		return nullptr;
	}

	if (parent->GetId() == id) {
		return parent;
	}

	for (int i = 0; i < parent->GetNumChildren(); i++) {
		if (auto *result = findChildOrSelfByIdRecursive(parent->GetChild(i), id)) {
			return result;
		}
	}

	return nullptr;
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
	bool _isHidden = false;
	bool _pendingClose = false;
	bool _runningAsyncOp = false;
	bool _firstFrame = true;
	std::function<void()> _updateCb;
	std::function<void()> _showCb;
	std::function<void()> _documentChangedCb;
	std::size_t _nextDocumentObserverId = 1;
	std::vector<std::pair<std::size_t, std::function<void()>>> _documentObservers;
};

DocumentChangedObserverToken::DocumentChangedObserverToken(UiWin& window, std::size_t id) noexcept
	: window_{ &window }, id_{ id } {}

DocumentChangedObserverToken::~DocumentChangedObserverToken() {
	reset();
}

DocumentChangedObserverToken::DocumentChangedObserverToken(DocumentChangedObserverToken&& other) noexcept
	: window_{ std::exchange(other.window_, nullptr) }
	, id_{ std::exchange(other.id_, 0) } {}

DocumentChangedObserverToken& DocumentChangedObserverToken::operator=(DocumentChangedObserverToken&& other) noexcept {
	if (this != &other) {
		reset();
		window_ = std::exchange(other.window_, nullptr);
		id_ = std::exchange(other.id_, 0);
	}
	return *this;
}

void DocumentChangedObserverToken::reset() noexcept {
	if (window_) {
		window_->removeDocumentChangedObserver(id_);
		window_ = nullptr;
		id_ = 0;
	}
}

void UiWin::EventListener::ProcessEvent(Rml::Event &event) {
}

UiWin::UiWin(std::string name, Rml::Vector2i size, std::filesystem::path documentPath, WinManager &winManager, bool isMain, bool isTransparent)
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

			std::println("raw size: {}, {}", size.x, size.y);

			auto scale = Backend::GetMonitorContentScale();

			size.x = static_cast<int>(size.x * scale.x);
			size.y = static_cast<int>(size.y * scale.y);
			std::println("scale: {}, {}, size: {}, {}", scale.x, scale.y, size.x, size.y);

			//if (isMain) {
				Backend::SetWindowSize(&win, size);
			//}

			auto contextptr = Rml::CreateContext(name, size);
			if (!contextptr) {
				Backend::DestroyWindow(&win);
				throw std::runtime_error("Failed to create context for window: "s + name);
			}

			auto documentptr = contextptr->LoadDocument(documentPath.string());
			if (!documentptr) {
				Rml::RemoveContext(contextptr->GetName());
				Backend::DestroyWindow(&win);
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
	} }
	, _winManager{ &winManager } {
	if (isMain) {
		assert(std::ranges::none_of(_winManager->wins_, [](const auto &win) {
			return win->isMainWin();
		}));
	} else {
		assert(_winManager->hasMainWin() &&
			"Cannot create a non-main UiWin before the backend main window has been adopted. "
			"Create a main window first, or explicitly adopt the backend main window for early-startup UI.");
	}

	Backend::AttachContext(_data->_rmlCStyleData->_win, _data->_rmlCStyleData->_context, &processKeyDownShortcutsBridge);
	attachDocument(*_data->_rmlCStyleData->_document);
	hide();
	_winManager->registerWindow(*this);
	Rml::Debugger::Initialise(_data->_rmlCStyleData->_context);
	std::println("Created window: {}, ptr: {}", _data->_selfData->_name, ptrToHex(_data->_rmlCStyleData->_win));
}

UiWin::~UiWin() {
	assert(!_data || !_data->_selfData->_runningAsyncOp);
	destroy();
}

void UiWin::destroy() {
	if (!_data) {
		return;
	}

	std::println("Destroying window: {}, ptr: {}", _data->_selfData->_name, ptrToHex(_data->_rmlCStyleData->_win));

	detachDocument();
	Rml::RemoveContext(_data->_selfData->_name.c_str());

	if (_winManager) {
		_winManager->unregisterWindow(*this);
		_winManager = nullptr;
	}

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

void UiWin::update() {
	applyCloseRequestState();

	if (_data->_rmlCStyleData->_context) {
		_data->_rmlCStyleData->_context->Update();
	}
	//if (_data->_selfData->_firstFrame && _data->_selfData->)
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

void UiWin::setShowCb(std::function<void()> cb) {
	_data->_selfData->_showCb = std::move(cb);
}

void UiWin::setDocumentChangedCb(std::function<void()> cb) {
	_data->_selfData->_documentChangedCb = std::move(cb);
	notifyDocumentChanged();
}

DocumentChangedObserverToken UiWin::observeDocumentChanged(std::function<void()> cb) {
	const auto id = _data->_selfData->_nextDocumentObserverId++;
	_data->_selfData->_documentObservers.emplace_back(id, std::move(cb));
	return DocumentChangedObserverToken{ *this, id };
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

[[nodiscard]] Rml::Vector2i UiWin::getWinSize() const {
	return Backend::GetWindowSize(_data->_rmlCStyleData->_win);
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

void UiWin::centerToPrimaryMonitor() {
	assert(_winManager && "UiWin is not registered to any WinManager.");
	const auto monitorArea = _winManager->getPrimaryMonitorArea();
	const auto winSize = getWinSize();
	setPosInMonitor(monitorArea, {
		(monitorArea.size.x - winSize.x) / 2,
		(monitorArea.size.y - winSize.y) / 2,
	});
}

void UiWin::setPosInMonitor(const MonitorArea &monitorArea, const Rml::Vector2i relativePos) {
	setWinPos(monitorArea.pos + relativePos);
}

void UiWin::hide() {
	if (!_data || _data->_selfData->_isHidden) {
		return;
	}

	_data->_rmlCStyleData->_document->Hide();
	Backend::HideWindow(_data->_rmlCStyleData->_win);
	_data->_selfData->_isHidden = true;
}

void UiWin::show() {
	if (!_data || !_data->_selfData->_isHidden) {
		return;
	}

	_data->_rmlCStyleData->_document->Show();
	Backend::ShowWindow(_data->_rmlCStyleData->_win);
	_data->_selfData->_isHidden = false;

	if (_data->_selfData->_showCb) {
		try {
			_data->_selfData->_showCb();
		} catch (const std::exception &e) {
			std::println("UI Runtime error: {}", e.what());
		}
	}
}

void UiWin::requestClose() {
	_data->_selfData->_pendingClose = true;
	refreshClosingVisualState();
	applyCloseRequestState();
}

bool UiWin::isPendingClose() const {
	return _data && _data->_selfData->_pendingClose;
}

bool UiWin::hasUnfinishedOp() const {
	return _data && _data->_selfData->_runningAsyncOp;
}

void UiWin::setRunningAsyncOp(bool running) noexcept {
	if (!_data) {
		return;
	}

	_data->_selfData->_runningAsyncOp = running;
}

void UiWin::applyCloseRequestState() {
	if (!_data || !_data->_selfData->_pendingClose) {
		return;
	}

	if (canCloseNow()) {
		hide();
		_data->_selfData->_pendingClose = false;
		refreshClosingVisualState();
	}
}

void UiWin::refreshClosingVisualState() {
	if (!_data) {
		return;
	}

	auto *rootElement = _data->_rmlCStyleData->_context->GetRootElement();
	if (!rootElement) {
		return;
	}

	auto *winframeElement = findChildOrSelfByIdRecursive(rootElement, "winframe");
	if (!winframeElement) {
		return;
	}

	const auto shouldShow = shouldShowClosingVisualState();
	winframeElement->SetClass("closing", shouldShow);

	auto *titleTextElement = findChildOrSelfByIdRecursive(winframeElement, "title-text");
	if (!titleTextElement) {
		return;
	}

	const auto title = winframeElement->GetAttribute<std::string>("title", "");
	titleTextElement->SetInnerRML(
		shouldShow ? title + " [正在结束未完成的工作]" : title);
}

void UiWin::requestCloseFromNativeEvent() {
	if (!_data) {
		return;
	}

	Backend::ClearShouldClose(_data->_rmlCStyleData->_win);
	_data->_selfData->_pendingClose = true;
	refreshClosingVisualState();
	applyCloseRequestState();
}

[[nodiscard]] bool UiWin::shouldDestroyNow() const noexcept {
	return false;
}

[[nodiscard]] bool UiWin::shouldShowClosingVisualState() const noexcept {
	return _data && _data->_selfData->_pendingClose && !canCloseNow();
}

[[nodiscard]] bool UiWin::canCloseNow() const noexcept {
	return _data && !_data->_selfData->_runningAsyncOp;
}

[[nodiscard]] bool UiWin::isHidden() const noexcept {
	return !_data || _data->_selfData->_isHidden;
}

void UiWin::attachDocument(Rml::ElementDocument &document) {
	document.AddEventListener("click", &_data->_selfData->_eventListener);
	notifyDocumentChanged();
}

void UiWin::detachDocument() const {
	if (_data && _data->_rmlCStyleData->_document) {
		//_data->_rmlCStyleData->_document->RemoveEventListener("click", &_data->_selfData->_eventListener);
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

	const auto observers = _data->_selfData->_documentObservers;
	for (const auto& [id, observer] : observers) {
		if (!observer) {
			continue;
		}
		try {
			observer();
		} catch (const std::exception& e) {
			std::println("UI document observer error: {}", e.what());
		}
	}
}

void UiWin::removeDocumentChangedObserver(std::size_t id) noexcept {
	if (!_data) {
		return;
	}
	std::erase_if(_data->_selfData->_documentObservers, [id](const auto& observer) {
		return observer.first == id;
	});
}

WinManager::WinManager() {
	activeWinManager = this;
	Backend::SetContextInputFilter([](Rml::Context *context) {
		return activeWinManager ? activeWinManager->isInputAllowedForContext(context) : true;
	});
}

WinManager::~WinManager() {
	assert(!hasOpenWins() && "Should be no open wins after shutdown");
	Backend::SetContextInputFilter(nullptr);
	if (activeWinManager == this) {
		activeWinManager = nullptr;
	}
}

void WinManager::updateAll() {
	for (auto &window : wins_) {
		if (window->isHidden()) {
			continue;
		}
		window->update();
	}
}

void WinManager::renderAll() {
	for (auto &window : wins_) {
		if (window->isHidden()) {
			continue;
		}
		auto glfwWin = window->getNativeWin();
		Backend::BeginFrame(glfwWin);
		window->render();
		Backend::PresentFrame(glfwWin);
	}
}

void WinManager::cleanupClosedWindows() {
	for (auto &winRef : wins_) {
		auto *win = winRef.get();
		if (Backend::ShouldWindowClose(win->getNativeWin())) {
			win->requestCloseFromNativeEvent();
		}
	}
}

void WinManager::requestCloseAllWindows() {
	for (auto &window : wins_) {
		window->requestClose();
	}
}

[[nodiscard]] bool WinManager::hasOpenWins() const {
	return !wins_.empty();
}

[[nodiscard]] bool WinManager::hasVisibleWins() const {
	return std::ranges::any_of(wins_, [](const auto &win) {
		return !win->isHidden();
	});
}

[[nodiscard]] UiWin &WinManager::getMainWin() const {
	auto mainWinIt = std::ranges::find_if(wins_, [](const auto &win) {
		return win->isMainWin();
	});

	assert(mainWinIt != wins_.end());
	return **mainWinIt;
}

[[nodiscard]] bool WinManager::hasMainWin() const {
	return std::ranges::any_of(wins_, [](const auto &win) {
		return win->isMainWin();
	});
}

[[nodiscard]] MonitorArea WinManager::getPrimaryMonitorArea() const {
	return Backend::GetPrimaryMonitorArea();
}

[[nodiscard]] Rml::Vector<MonitorArea> WinManager::getMonitorAreas() const {
	return Backend::GetMonitorAreas();
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
	assert(std::ranges::find_if(wins_, [&window](const auto &winRef) {
		return winRef.get() == &window;
	}) == wins_.end());
	wins_.push_back(gsl::not_null<UiWin *>{ &window });
	const auto [_, inserted] = context2Win_.emplace(&window.getContext(), &window);
	assert(inserted);
}

void WinManager::unregisterWindow(const UiWin &window) {
	std::erase_if(wins_, [&window](const auto &winRef) {
		return winRef.get() == &window;
	});
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
				//reloadWindow(*uiWin);
			}
		} else {
			result = true;
		}
	}

	return result;
}

}
