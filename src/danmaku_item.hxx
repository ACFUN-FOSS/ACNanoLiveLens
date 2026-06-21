#ifndef NANOLIVELENS_DANMAKU_ITEM_HXX
#define NANOLIVELENS_DANMAKU_ITEM_HXX

#include "rmlui_element.hxx"
#include "rmluipp.hxx"

class DanmakuItem : public RmlUIElement
{
public:
	struct DanmakuInfo
	{
		std::string sender;
		std::string content;
		std::chrono::system_clock::time_point timestamp;
	};

	DanmakuItem(const std::string_view tag);
	DanmakuItem(const DanmakuItem &) = delete;
	DanmakuItem(DanmakuItem &&) = delete;
	~DanmakuItem();
	DanmakuItem &operator=(const DanmakuItem &) = delete;
	DanmakuItem &operator=(DanmakuItem &&) = delete;

	static void reg(RmlUISystem &rmlui);

	void setDanmakuInfo(const DanmakuInfo &info);

protected:
	void onMounted() override;
	void onUpdate() override;
	void processDefaultAction(Rml::Event &event) override;
	void reload() override;

private:
	void initAfterConstruct();
	void bindEventHandlers();

	SimpleEventListenerManager eventListenerMan_{ *this };

	std::optional<DanmakuInfo> currentDanmakuInfo_;
};

#endif //NANOLIVELENS_DANMAKU_ITEM_HXX
