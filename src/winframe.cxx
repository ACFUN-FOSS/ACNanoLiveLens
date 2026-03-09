//
// Created by nikob on 2026/1/19.
//

#include "winframe.hxx"
#include "safe_element_instancer.hxx"

#include <print>
#include <RmlUi/Core/Factory.h>
#include <RmlUi/Core/Event.h>
#include <RmlUi/Core\ElementInstancer.h>
#include <EatiEssentials/memory.hxx>
#include <EatiEssentials/memsafety.hxx>
#include <EatiEssentials/misc.hxx>
#include <EatiEssentials/io.hxx>


#include "appstate.hxx"
#include "assets.hxx"
#include "rmluipp.hxx"
#include "utils.hxx"


using namespace Rml;
using namespace Essentials::Memory;
using namespace Essentials::IO;
using namespace Essentials::Misc;

/**
 * 自定义一个元素代码太多了，我们必须把它封装一下，未来会有更多自定义元素。
 */

WinFrame::WinFrame(const std::string_view tag)
    : Element{ std::string{ tag } } {
	std::println("CONSTRUCT: WindFrame: {}", ptrToHex(this));

	SetInnerRML(readFile(getAssetsDir() / "winframe.rml"));
	

	//auto myelePtr = findChildOrSelfById(this, "close-btn");
	//assert(myelePtr);
	//myelePtr->AddEventListener(Rml::EventId::Mousedown, &testListener);

	bindEventHandlers();

	//throw std::runtime_error{ "Test excption" };
}

WinFrame::~WinFrame() {
	std::println("DECONSTRUCT: WindFrame: {}", ptrToHex(this));
}

void WinFrame::reg(RmlUISystem &rmlui) {
    rmlui.regElement("winframe", newBox(SafeElementInstancer<WinFrame>{}));
}


void WinFrame::OnUpdate() {
	//printElementTree(*this);
	//triggerDebugger();
	try {
		if (!firstInited) {
			initAfterConstruct();
			firstInited = true;
		}

		// drag
		if (isDragging_) {
			std::println("DRAG: {}", ptrToHex(this));
			auto &win = UNWRAP(getAppState().winManager.getWinOfElement(*this));
			
			auto deltaMousePos = win.getMousePos() - mousePosWhenBeginDrag_;
			win.setWinPos(win.getWinPos() + deltaMousePos);
		}

		// TODO: Call child class OnUpdate here
	}
	catch (...) {
		// Failure in update should not propagate into Rml.
		std::println("WinFrame::OnUpdate caught exception");
		// disable further processing
		isDragging_ = false;
	}
}


void WinFrame::ProcessDefaultAction(Rml::Event &event) {
	//dbgLog(event);
	try {
		// 为啥检测个按键要这么复杂？？！
		if (event.GetId() == EventId::Keydown &&
			event.GetParameter<Rml::Input::KeyIdentifier>("key_identifier", Rml::Input::KeyIdentifier::KI_FINAL)
			== Rml::Input::KeyIdentifier::KI_F6) {
			reload();

			// 重新加载样式表时，RmlUI 会创建一个临时的本类对象，然后丢弃不用，有点迷。
			GetOwnerDocument()->ReloadStyleSheet();
		}

		if (event.GetId() == EventId::Keydown &&
			event.GetParameter<Rml::Input::KeyIdentifier>("key_identifier", Rml::Input::KeyIdentifier::KI_FINAL)
			== Rml::Input::KeyIdentifier::KI_F7) {
			std::println("self: {}", ptrToHex(this));
		}

		// TODO: Call child class ProcessDefaultAction here

		Element::ProcessDefaultAction(event);
	}
	catch (...) {
		std::println("WinFrame::ProcessDefaultAction caught exception");
	}
}



void WinFrame::initAfterConstruct() {
	//dbgLog("INIT: WinFrame");
	//reload();
}

void WinFrame::reload() {
	dbgLog("RELOAD: WinFrame");
	auto &contentEle = UNWRAP(findChildOrSelfById(this, "content"));
	auto childOwner = this->RemoveChild(&contentEle);

	SetInnerRML(readFile(getAssetsDir() / "winframe.rml"));

	// Mount content element after set inner RML
	this->AppendChild(std::move(childOwner));

	bindEventHandlers();
}

void WinFrame::bindEventHandlers() {
	//auto myelePtr = findChildOrSelfById(this, "close-btn");
	//UNWRAP(myelePtr).AddEventListener(Rml::EventId::Mousedown, &testListener);
	 
	eventListenerMan_.on("close-btn", "mousedown", [this](Event &event) {
		//event.StopPropagation();
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

	// TODO: bind child class event handlers here
}
