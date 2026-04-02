#include "danmaku_monitor_win.hxx"
#include "danmaku_item.hxx"
#include "appstate.hxx"
#include "assets.hxx"
#include "rmluipp.hxx"
#include "RmlUIWin/window_manager.hxx"
#include "utils.hxx"

using namespace RmlUIWin;
using namespace Essentials::Memory;
using namespace Essentials::IO;

class DanmakuMonitorWin::Impl
{
public:
    Impl()
        : uiState_{ [] -> UIState {
            auto mainWin = newBox(UiWin{ "danmaku_monitor", {}, getAssetsDir() / "danmaku_monitor.rml", true });
            auto &mainWinRootEle = UNWRAP(mainWin->getContext().GetRootElement());
            SimpleEventListenerManager mainWinRootEleEventMan{ mainWinRootEle };
            auto &win = getAppState().winManager->transferWin(std::move(mainWin));
            return { &win, std::move(mainWinRootEleEventMan) };
        }() }
		, danmakuList_{ 
			&requireElement(UNWRAP(uiState_.mainWin_->getContext().GetRootElement()), "danmaku-list") 
		} {
			uiState_.mainWinRootEleEventMan_.on("add-danmaku-btn", "click", [this](auto &&_) {
				addDanmaku({
					"sender",
					"contentcontent contentcontent contentcontent contentcontent contentcontent contentcontent",
					std::chrono::system_clock::now()
				});
			});
			uiState_.mainWin_->setUpdateCb([this]() {
				startPendingDanmakuContainerAnim();
				scrollToEnd();
			});
    }

    ~Impl() = default;
    Impl(Impl &&) = delete;
    Impl(const Impl &) = delete;
    Impl &operator=(const Impl &) = delete;
    Impl &operator=(Impl &&) = delete;

    void addDanmaku(const DanmakuInfo &danmaku)
    {
        if (!danmakuList_)
            return;

		auto &document = uiState_.mainWin_->getDocument();

		auto danmakuItemAppearAnimContainerEle = document.CreateElement("div");
		danmakuItemAppearAnimContainerEle->SetClass("danmaku-item-appear-anim-container", true);

		auto danmakuItemEle = document.CreateElement("danmaku-item");

		//auto danmakuItem = Rml::ElementPtr{new DanmakuItem{ "danmaku-item" }};
		auto &danmakuItemRef = dynamic_cast<DanmakuItem &>(*danmakuItemEle);
		danmakuItemRef.setDanmakuInfo({
			danmaku.sender,
			danmaku.content,
			danmaku.timestamp
		});

		auto &danmakuItem_ = 
			UNWRAP(danmakuItemAppearAnimContainerEle->AppendChild(std::move(danmakuItemEle)));

		auto &danmakuItemAppearAnimContainerEle_ = 
			UNWRAP(danmakuList_->AppendChild(std::move(danmakuItemAppearAnimContainerEle)));



		pendingAnimDanmaku.emplace(&danmakuItemAppearAnimContainerEle_, &danmakuItem_);
    }

	void scrollToEnd() {

	}

	// 为待处理的弹幕容器添加动画
 	// 从待处理队列中取出一个弹幕元素，为其创建动态样式并应用
	// 这么做是因为 RmlUI 的 transition 动画只有在添加类时才会触发
	// TODO：当前实现，动画播放过程会导致整个页面的布局持续变化，性能较差，
	// 需要优化一下，如改成弹幕外面套一个高度不会变的容器。
  	void startPendingDanmakuContainerAnim() {
  		if (!pendingAnimDanmaku.empty()) {
  			// 从待处理队列中取出一个弹幕元素
  			auto ele = pendingAnimDanmaku.top();
  			pendingAnimDanmaku.pop();

  			// 生成唯一的类名并创建动态样式字符串
  			auto className = std::format("anim-{}", randomInt(0, 1000000));
  			auto newStyleSrc = std::format(
  				".{}{{"
  				"height: {}px;"
  				"}}",
  				className,
  				ele.danmakuEle->GetOffsetHeight()
  			);
  			std::cout << "newStyleSrc: " << newStyleSrc << std::endl;
  			
  			// 获取文档对象
  			auto &document = uiState_.mainWin_->getDocument();

  			// 创建样式表并合并到文档的样式表容器中
  			auto newStyle = Rml::Factory::InstanceStyleSheetString(newStyleSrc);
  			auto &sheet = UNWRAP(document.GetStyleSheetContainer());
  			
  			auto newFullStyle = sheet.CombineStyleSheetContainer(*newStyle);
  			document.SetStyleSheetContainer(newFullStyle);
  			
  			// 将动态样式类应用到弹幕容器元素
  			ele.danmakuItemAppearAnimContainerEle->SetClass(className, true);

  		}
  	}

    void clearDanmaku()
    {
        if (!danmakuList_)
            return;

        while (danmakuList_->GetNumChildren() > 0)
        {
            danmakuList_->RemoveChild(danmakuList_->GetChild(0));
        }
    }

private:
    struct UIState
    {
        gsl::not_null<UiWin *> mainWin_;
        SimpleEventListenerManager mainWinRootEleEventMan_;
    } uiState_;


	struct DanmakuGuiInfo
	{
		gsl::not_null<Rml::Element *const> danmakuItemAppearAnimContainerEle;
		gsl::not_null<Rml::Element *const> danmakuEle;
	};

	std::stack<DanmakuGuiInfo> pendingAnimDanmaku;

    Rml::Element *danmakuList_{ nullptr };
};

DanmakuMonitorWin::DanmakuMonitorWin()
    : pImpl{ stdx::pimpl::make_unique<Impl>() }
{
}

DanmakuMonitorWin::~DanmakuMonitorWin() = default;

void DanmakuMonitorWin::addDanmaku(const DanmakuInfo &danmaku)
{
    pImpl->addDanmaku(danmaku);
}

void DanmakuMonitorWin::clearDanmaku()
{
    pImpl->clearDanmaku();
}
