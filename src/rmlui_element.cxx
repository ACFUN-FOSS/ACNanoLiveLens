#include "rmlui_element.hxx"
#include "utils.hxx"
#include "winframe.hxx"
#include "danmaku_item.hxx"
#include "sound/sound.hxx"

using namespace Essentials::Misc;

RmlUIElement::RmlUIElement(const std::string_view tag, bool isWindowElement)
    : Element{ std::string{ tag } },
	  isWindowElement{ isWindowElement } {

}

void RmlUIElement::OnUpdate() {
    try {
        onUpdate();
    } catch (std::exception &e) {
        std::println("RmlUIElement::OnUpdate caught exception: {}", e.what());
    }
}

void RmlUIElement::ProcessDefaultAction(Rml::Event &event) {
    try {
        processDefaultAction(event);
    } catch (std::exception &e) {
        std::println("RmlUIElement::ProcessDefaultAction caught exception: {}", e.what());
    }
}

void RmlUIElement::onUpdate() {
    Element::OnUpdate();
}

void RmlUIElement::processDefaultAction(Rml::Event &event) {
    using namespace Rml;

    if (event.GetId() == EventId::Keydown &&
        event.GetParameter<Input::KeyIdentifier>("key_identifier", Input::KeyIdentifier::KI_FINAL)
        == Input::KeyIdentifier::KI_F6) {
		playSound(Sound::RELOAD);
        reload();
		dbgLog("Reloading style sheet");
        GetOwnerDocument()->ReloadStyleSheet();
    }

    if (event.GetId() == EventId::Keydown &&
        event.GetParameter<Input::KeyIdentifier>("key_identifier", Input::KeyIdentifier::KI_FINAL)
        == Input::KeyIdentifier::KI_F7) {
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
