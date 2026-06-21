#include "utils.hxx"
#include "assets.hxx"
#include "safe_element_instancer.hxx"
#include "winframe.hxx"
#include <appstate.hxx>

using namespace Rml;
using namespace Essentials::Memory;
using namespace Essentials::IO;
using namespace Essentials::Misc;



WinFrame::WinFrame(const std::string_view tag)
	: RmlUIElement{ tag, true } {
	std::println("CONSTRUCT: WindFrame: {}", ptrToHex(this));

	SetInnerRML(Rml::String{ readTextAssetCached(getAssetsDir() / "winframe.rml") });
	//throw std::runtime_error{ "Test excption" };
}

WinFrame::~WinFrame() {
	std::println("DECONSTRUCT: WindFrame: {}", ptrToHex(this));
}

void WinFrame::reg(RmlUISystem &rmlui) {
    rmlui.regElement("winframe", newBox(SafeElementInstancer<WinFrame>{}));
}

void WinFrame::onMounted() {
	initAfterConstruct();
	bindEventHandlers();
}

void WinFrame::onUpdate() {
	if (isDragging_) {
		auto &win = UNWRAP(getAppState().winManager->getWinOfElement(*this));
		
		auto deltaMousePos = win.getMousePos() - mousePosWhenBeginDrag_;
		win.setWinPos(win.getWinPos() + deltaMousePos);
	}
}


void WinFrame::processDefaultAction(Rml::Event &event) {
	RmlUIElement::processDefaultAction(event);
}


void WinFrame::initAfterConstruct() {
	//dbgLog("INIT: WinFrame");
	//reload();
	requireElement(*this, "title-text").SetInnerRML(GetAttribute<std::string>("title", ""));
}

void WinFrame::reload() {
	// DBG
	return;
	dbgLog("RELOAD: WinFrame");
	auto &contentEle = UNWRAP(findChildOrSelfById(this, "content"));
	auto childOwner = this->RemoveChild(&contentEle);

	SetInnerRML(Rml::String{ readTextAssetCached(getAssetsDir() / "winframe.rml") });

	// Mount content element after set inner RML
	this->AppendChild(std::move(childOwner));

	initAfterConstruct();
	bindEventHandlers();
}

void WinFrame::bindEventHandlers() {
	//auto myelePtr = findChildOrSelfById(this, "close-btn");
	//UNWRAP(myelePtr).AddEventListener(Rml::EventId::Mousedown, &testListener);
	eventListenerMan_.clear();
	eventListenerMan_.on("close-btn", "mousedown", [this](Event &event) {
		event.StopPropagation();
	});
	eventListenerMan_.on("close-btn", "mouseup", [this](Event &event) {
		dbgLog("close-btn click");
		auto &win = UNWRAP(getAppState().winManager->getWinOfElement(*this));
		win.setShouldClose();
		
		event.StopPropagation();
	});
	eventListenerMan_.on("title-bar", "mousedown", [this](Event &event) {
		dbgLog("Drag begin");
		auto &win = UNWRAP(getAppState().winManager->getWinOfElement(*this));
		isDragging_ = true;
		mousePosWhenBeginDrag_ = win.getMousePos();
	});
	eventListenerMan_.on("title-bar", "mouseup", [this](Event &event) {
		dbgLog("Drag end");
		isDragging_ = false;
	});
}
