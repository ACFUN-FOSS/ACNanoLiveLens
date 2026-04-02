#include "rmlui_element.hxx"
#include "utils.hxx"
#include "winframe.hxx"
#include "danmaku_item.hxx"

using namespace Essentials::Misc;

RmlUIElement::RmlUIElement(const std::string_view tag, bool isWindowElement)
    : Element{ std::string{ tag } },
	  isWindowElement{ isWindowElement } {

}

void RmlUIElement::OnUpdate() {
    try {
        onUpdate();
    } catch (...) {
        std::println("RmlUIElement::OnUpdate caught exception");
    }
}

void RmlUIElement::ProcessDefaultAction(Rml::Event &event) {
    try {
        processDefaultAction(event);
    } catch (...) {
        std::println("RmlUIElement::ProcessDefaultAction caught exception");
    }
}

void RmlUIElement::onUpdate() {
    Element::OnUpdate();
}

void RmlUIElement::processDefaultAction(Rml::Event &event) {
    using namespace Rml;

    if (event.GetId() == EventId::Keydown &&
        event.GetParameter<Rml::Input::KeyIdentifier>("key_identifier", Rml::Input::KeyIdentifier::KI_FINAL)
        == Rml::Input::KeyIdentifier::KI_F6) {
        reload();
		dbgLog("Reloading style sheet");
        GetOwnerDocument()->ReloadStyleSheet();
    }

    if (event.GetId() == EventId::Keydown &&
        event.GetParameter<Rml::Input::KeyIdentifier>("key_identifier", Rml::Input::KeyIdentifier::KI_FINAL)
        == Rml::Input::KeyIdentifier::KI_F7) {
        std::println("self: {}", ptrToHex(this));
    }

    Element::ProcessDefaultAction(event);
}

void RmlUIElement::reload() {
}

bool RmlUIElement::getIsWindowElement() const {
    return isWindowElement;
}

void registerCustomElements(RmlUISystem &rmlui) {
    WinFrame::reg(rmlui);
    DanmakuItem::reg(rmlui);
}
