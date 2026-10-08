#ifndef NANOLIVELENS_TOGGLE_HXX
#define NANOLIVELENS_TOGGLE_HXX

#include "rmlui_element.hxx"
#include "rmluipp.hxx"

// 通用拨动开关/滑块（自绘自定义控件，用法参考 DanmakuItem）。
//
// 自绘内容来自 assets/toggle.rml（仅一个 .knob 滑块），
// 滑轨由元素自身承担，外观由 .on 类驱动（见 main.rcss）：
//     .on      → 红色滑轨，滑块右移（开启）
//     无 .on   → 灰色滑轨，滑块居左（关闭）
//
// 与具体业务无关：元素自身处理 click（点击即翻转自身状态），
// 并通过 setToggledCb 回调通知宿主，由宿主维护业务状态
//（如弹幕窗口的 scrollLocked_）。
class Toggle : public RmlUIElement
{
public:
	// 状态翻转回调，参数为翻转后的新状态。
	using ToggledCb = std::function<void(bool)>;

	Toggle(const std::string_view tag);
	Toggle(const Toggle &) = delete;
	Toggle(Toggle &&) = delete;
	~Toggle();
	Toggle &operator=(const Toggle &) = delete;
	Toggle &operator=(Toggle &&) = delete;

	static void reg(RmlUISystem &rmlui);

	// 设置开关状态（幂等：状态未变化时不触碰 DOM）。
	void setOn(bool on);
	bool isOn() const { return on_; }

	// 设置点击翻转后的回调。热重载会重建 DOM 并生成新实例，需重新设置。
	void setToggledCb(ToggledCb cb);

protected:
	void onMounted() override;
	void onUpdate() override;
	void processDefaultAction(Rml::Event &event) override;
	void reload() override;

private:
	void initAfterConstruct();
	void syncVisualState();

	// 点击翻转子控件自身状态（监听器在 onMounted 中挂到本元素上）。
	SimpleEventListener clickListener_{
		[this](Rml::Event &) {
			setOn(!on_);

			if (toggledCb_) {
				toggledCb_(on_);
			}
		}
	};

	ToggledCb toggledCb_;
	bool on_ = true;
	bool clickListenerBound_ = false;
};

#endif //NANOLIVELENS_TOGGLE_HXX
