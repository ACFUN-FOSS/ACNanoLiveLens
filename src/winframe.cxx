//
// Created by nikob on 2026/1/19.
//

#include "winframe.hxx"

#include <print>
#include <RmlUi/Core/Factory.h>
#include <EatiEssentials/memsafety.hxx>
#include <EatiEssentials/io.hxx>

#include "appstate.hxx"
#include "assets.hxx"
#include "rmluipp.hxx"
#include "utils.hxx"

using namespace Rml;
using namespace Essentials::Memory;
using namespace Essentials::IO;


WinFrame::WinFrame(const std::string_view tag)
    : Element{ std::string{ tag } } {

	SetInnerRML(readFile(getAssetsDir() / "winframe.rml"));
	eventListenerMan_.on("close-btn", "mousedown", [this](Event &event) {
		event.StopPropagation();
	});
	eventListenerMan_.on("close-btn", "mouseup", [this](Event &event) {
		dbgLog("close-btn click");
		event.StopPropagation();
	});
	eventListenerMan_.on("title-bar", "mousedown", [this](Event &event) {
		dbgLog("Drag begin");
		auto &win = UNWRAP(getAppState().winManager.getWinOfElement(*this));
		isDragging_ = true;
		mousePosWhenBeginDrag_ = win.getMousePos();
	});
	eventListenerMan_.on("title-bar", "mouseup", [this](Event &event) {
		dbgLog("Drag end");
		isDragging_ = false;
	});
}

void WinFrame::reg(RmlUISystem &rmlui) {
    rmlui.regElement("winframe", newBox(Rml::ElementInstancerGeneric<WinFrame>{}));
}


void WinFrame::OnUpdate() {
	//printElementTree(*this);
	//triggerDebugger();
	static bool first = true;
	if (first) {
		first = false;

		
	}

	// drag
	if (isDragging_) {
		auto &win = UNWRAP(getAppState().winManager.getWinOfElement(*this));
		
		auto deltaMousePos = win.getMousePos() - mousePosWhenBeginDrag_;
		win.setWinPos(win.getWinPos() + deltaMousePos);
	}
}

void WinFrame::ProcessDefaultAction(Rml::Event &event) {
	//dbgLog(event);

	// Test crash handler
	//auto &ele = UNWRAP(findParentOrSelfById(event.GetTargetElement(), "ti2le-bar"));
	
	Element::ProcessDefaultAction(event);
}

