#include "toggle.hxx"
#include "Core/assets.hxx"
#include "safe_element_instancer.hxx"
#include "utils.hxx"

using namespace Rml;
using namespace Essentials::Memory;
using namespace Essentials::IO;
using namespace Essentials::Misc;

Toggle::Toggle(const std::string_view tag)
	: RmlUIElement{ tag, false } {
	std::println("CONSTRUCT: Toggle: {}", ptrToHex(this));
	SetInnerRML(Rml::String{ readTextAssetCached(getAssetsDir() / "toggle.rml") });
}

Toggle::~Toggle() {
	std::println("DECONSTRUCT: Toggle: {}", ptrToHex(this));
}

void Toggle::reg(RmlUISystem &rmlui) {
	rmlui.regElement("toggle", newBox(SafeElementInstancer<Toggle>{}));
}

void Toggle::onMounted() {
	initAfterConstruct();

	// click 监听器挂到元素自身。热重载会重建 DOM 并生成新实例，
	// 因此这里只需保证单个实例内不重复绑定。
	if (!clickListenerBound_) {
		AddEventListener(Rml::String{ "click" }, &clickListener_);
		clickListenerBound_ = true;
	}
}

void Toggle::onUpdate() {
}

void Toggle::processDefaultAction(Rml::Event &event) {
	// 不转发给基类：避免按键默认动作在本控件上重复触发（与 DanmakuItem 一致）。
	//RmlUIElement::processDefaultAction(event);
}

void Toggle::initAfterConstruct() {
	syncVisualState();
}

void Toggle::syncVisualState() {
	// .on 类驱动外观：红色滑轨 + 滑块右移（见 main.rcss）。
	SetClass("on", on_);
}

void Toggle::setOn(bool on) {
	// 幂等：状态未变化时不触碰 DOM。
	if (on_ == on) {
		return;
	}

	on_ = on;
	syncVisualState();
}

void Toggle::setToggledCb(ToggledCb cb) {
	toggledCb_ = std::move(cb);
}

void Toggle::reload() {
	dbgLog("RELOAD: Toggle");
	SetInnerRML(Rml::String{ readFile(getAssetsDir() / "toggle.rml") });
	initAfterConstruct();
}
