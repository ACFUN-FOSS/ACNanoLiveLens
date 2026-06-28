#include "danmaku_item.hxx"
#include "danmaku_monitor_win.hxx"
#include "Core/assets.hxx"
#include "safe_element_instancer.hxx"
#include "utils.hxx"

using namespace Rml;
using namespace Essentials::Memory;
using namespace Essentials::IO;
using namespace Essentials::Misc;

DanmakuItem::DanmakuItem(const std::string_view tag)
	: RmlUIElement{ tag, false } {
	std::println("CONSTRUCT: DanmakuItem: {}", ptrToHex(this));
	SetInnerRML(Rml::String{ readTextAssetCached(getAssetsDir() / "danmaku_item.rml") });
}

DanmakuItem::~DanmakuItem() {
	std::println("DECONSTRUCT: DanmakuItem: {}", ptrToHex(this));
}

void DanmakuItem::reg(RmlUISystem &rmlui) {
	rmlui.regElement("danmaku-item", newBox(SafeElementInstancer<DanmakuItem>{}));
}

void DanmakuItem::onMounted() {
	initAfterConstruct();
	bindEventHandlers();
}

void DanmakuItem::onUpdate() {
}

void DanmakuItem::processDefaultAction(Rml::Event &event) {
	//RmlUIElement::processDefaultAction(event);
}

void DanmakuItem::initAfterConstruct() {
}

void DanmakuItem::bindEventHandlers() {
	eventListenerMan_.clear();
}

void DanmakuItem::reload() {
	dbgLog("RELOAD: DanmakuItem");
	SetInnerRML(Rml::String{ readTextAssetCached(getAssetsDir() / "danmaku_item.rml") });
	initAfterConstruct();
	bindEventHandlers();
	if (currentDanmakuInfo_) {
		setDanmakuInfo(*currentDanmakuInfo_);
	}
}

void DanmakuItem::setDanmakuInfo(const DanmakuInfo &info) {
	currentDanmakuInfo_ = info;

	auto senderEle = findChildOrSelfById(this, "sender");
	if (senderEle) {
		senderEle->SetInnerRML(info.sender.c_str());
	}

	auto contentEle = findChildOrSelfById(this, "content");
	if (contentEle) {
		contentEle->SetInnerRML(info.content.c_str());
	}

	auto timeEle = findChildOrSelfById(this, "time");
	if (timeEle) {
		auto timeStr = std::format("{:%H:%M:%S}", info.timestamp);
		timeEle->SetInnerRML(timeStr.c_str());
	}
}
