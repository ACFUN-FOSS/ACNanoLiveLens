#include "msg_box.hxx"
#include "appstate.hxx"
#include "Core/assets.hxx"
#include "rmluipp.hxx"
#include "RmlUIWin/window_manager.hxx"
#include "platform/window_activation_optimization.hxx"

using namespace RmlUIWin;
using namespace Essentials::Memory;
using namespace Essentials::IO;

class MsgBox::Impl
{
public:
	Impl(Type type, std::string_view text)
		: type_{ type }
		, text_{ text }
		, selfShouldBeMainWin_{ []() -> bool {
			auto &winManager = UNWRAP(getAppState().winManager);
			return !winManager.hasMainWin();
		}() }
		, uiState_{ [this]() -> UIState {
			auto &winManager = UNWRAP(getAppState().winManager);
			auto &win = winManager.createWindow(
				"msg_box",
				{ 560, 250 },
				getAssetsDir() / "msg_box.rml",
				selfShouldBeMainWin_
			);
			SimpleEventListenerManager mainWinRootEleEventMan{ win.getRootElement() };
			return { &win, std::move(mainWinRootEleEventMan) };
		}() } {
		bindEventHandlers();
		refreshUi();
		centerToMainWinOrPrimaryMonitor();
	}

	~Impl() {
		prepareForClose();
	}
	Impl(Impl &&) = delete;
	Impl(const Impl &) = delete;
	Impl &operator=(const Impl &) = delete;
	Impl &operator=(Impl &&) = delete;

	void showModal() {
		auto &winManager = *getAppState().winManager;
		auto oldModalWin = winManager.getModalWin();
		winManager.setModalWin(uiState_.mainWin_);
		MsgBoxWindowActivationGuard activationGuard{ uiState_.mainWin_->getNativeWin() };

		switch (type_) {
			case Type::EERR:
				playSound(Sound::ERRR);
				break;
			case Type::EWARN:
				playSound(Sound::INFO);
				break;
			case Type::EINFO:
				playSound(Sound::INFO);
				break;
			default:
				break;
		}

		while (true) {
			auto shouldContinue = Backend::ProcessEvents(false);
			if (!shouldContinue) {
				requestClose();
			}

			activationGuard.update();

			uiState_.mainWin_->update();

			auto nativeWin = uiState_.mainWin_->getNativeWin();
			Backend::BeginFrame(nativeWin);
			uiState_.mainWin_->render();
			Backend::PresentFrame(nativeWin);

			if (Backend::ShouldWindowClose(nativeWin)) {
				prepareForClose();
				getAppState().winManager->cleanupClosedWindows();
				break;
			}
		}

		winManager.setModalWin(oldModalWin);
	}

private:
	struct UIState
	{
		gsl::not_null<UiWin *> mainWin_;
		SimpleEventListenerManager mainWinRootEleEventMan_;
	};

	void bindEventHandlers() {
		uiState_.mainWin_->setDocumentChangedCb([this] {
			uiState_.mainWinRootEleEventMan_.reBind(uiState_.mainWin_->getRootElement());
			//bindEventHandlers();
			refreshUi();
		});

		uiState_.mainWinRootEleEventMan_.clear();
		uiState_.mainWinRootEleEventMan_.on("ok-btn", "mouseup", [this](Rml::Event &event) {
			requestClose();
			event.StopPropagation();
		});
	}

	void refreshUi() {
		auto &document = uiState_.mainWin_->getDocument();
		auto &rootElement = uiState_.mainWin_->getRootElement();
		auto &msgBoxEle = requireElement(rootElement, "msg-box");
		auto &titleEle = requireElement(rootElement, "msg-box-title");
		auto &textEle = requireElement(rootElement, "msg-box-text");

		msgBoxEle.SetClass("msg-box-error", type_ == Type::EERR);
		msgBoxEle.SetClass("msg-box-warning", type_ == Type::EWARN);
		msgBoxEle.SetClass("msg-box-info", type_ == Type::EINFO);

		setElementText(document, titleEle, getTitleText());
		setElementText(document, textEle, text_);
	}

	void centerToMainWinOrPrimaryMonitor() {
		if (selfShouldBeMainWin_) {
			uiState_.mainWin_->centerToPrimaryMonitor();
			return;
		}
		auto &mainWin = getAppState().winManager->getMainWin();
		auto mainWinPos = mainWin.getWinPos();
		auto mainWinSize = mainWin.getWinSize();
		auto msgBoxSize = uiState_.mainWin_->getWinSize();

		uiState_.mainWin_->setWinPos(
			mainWinPos + Rml::Vector2i{
				(mainWinSize.x - msgBoxSize.x) / 2,
				(mainWinSize.y - msgBoxSize.y) / 2,
			}
		);
	}

	void requestClose() {
		prepareForClose();
		uiState_.mainWin_->setShouldClose();
	}

	void prepareForClose() {
		if (closePrepared_) {
			return;
		}

		closePrepared_ = true;
		uiState_.mainWinRootEleEventMan_.clear();
	}

	void setElementText(Rml::ElementDocument &document, Rml::Element &element, const std::string_view text) {
		element.SetInnerRML("");
		element.AppendChild(document.CreateTextNode(std::string{ text }));
	}

	[[nodiscard]] std::string_view getTitleText() const {
		switch (type_) {
		case Type::EERR:
			return "Error";
		case Type::EWARN:
			return "Warning";
		case Type::EINFO:
			return "Information";
		}

		std::unreachable();
	}

	Type type_;
	std::string text_;
	bool selfShouldBeMainWin_ = false;
	bool closePrepared_ = false;
	UIState uiState_;
	
};

MsgBox::MsgBox(Type type, std::string_view text)
	: pImpl{ stdx::pimpl::make_unique<Impl>(type, text) }
{
}

MsgBox::~MsgBox() = default;

void MsgBox::popupOKMsgBox(Type type, std::string_view text) {
	MsgBox msgBox{ type, text };
	msgBox.showModal();
}

void MsgBox::showModal() {
	pImpl->showModal();
}
