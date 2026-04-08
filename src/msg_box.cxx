#include "msg_box.hxx"
#include "appstate.hxx"
#include "assets.hxx"
#include "rmluipp.hxx"
#include "RmlUIWin/window_manager.hxx"

using namespace RmlUIWin;
using namespace Essentials::Memory;
using namespace Essentials::IO;

class MsgBox::Impl
{
public:
	Impl(Type type, std::string_view text)
		: type_{ type }
		, text_{ text }
		, uiState_{ []() -> UIState {
			auto mainWin = newBox(UiWin{ "msg_box", { 360, 190 }, getAssetsDir() / "msg_box.rml", false });
			SimpleEventListenerManager mainWinRootEleEventMan{ mainWin->getRootElement() };
			auto &win = getAppState().winManager->transferWin(std::move(mainWin));
			return { &win, std::move(mainWinRootEleEventMan) };
		}() } {
		bindEventHandlers();
		refreshUi();
		centerToMainWin();
	}

	~Impl() {
		prepareForClose();
	}
	Impl(Impl &&) = delete;
	Impl(const Impl &) = delete;
	Impl &operator=(const Impl &) = delete;
	Impl &operator=(Impl &&) = delete;

	void showModal() {
		auto oldModalWin = getModalWin();
		setModalWin(uiState_.mainWin_);

		while (true) {
			auto shouldContinue = Backend::ProcessEvents(false);
			if (!shouldContinue) {
				requestClose();
			}

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

		setModalWin(oldModalWin);
	}

private:
	struct UIState
	{
		gsl::not_null<UiWin *> mainWin_;
		SimpleEventListenerManager mainWinRootEleEventMan_;
	};

	void bindEventHandlers() {
		uiState_.mainWin_->setReloadCb([this] {
			uiState_.mainWinRootEleEventMan_.reBind(uiState_.mainWin_->getRootElement());
			bindEventHandlers();
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

	void centerToMainWin() {
		auto &mainWin = getAppState().winManager->getMainWin();
		auto mainWinPos = mainWin.getWinPos();
		auto mainWinSize = Backend::GetWindowSize(mainWin.getNativeWin());
		auto msgBoxSize = Backend::GetWindowSize(uiState_.mainWin_->getNativeWin());

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
	UIState uiState_;
	bool closePrepared_ = false;
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
