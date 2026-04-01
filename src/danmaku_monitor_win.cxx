#include "danmaku_monitor_win.hxx"
#include "appstate.hxx"
#include "assets.hxx"
#include "rmluipp.hxx"
#include "RmlUIWin/window_manager.hxx"

using namespace RmlUIWin;
using namespace Essentials::Memory;
using namespace Essentials::IO;

static std::string formatTimestamp(const std::chrono::system_clock::time_point &tp) {
    return std::format("{:%H:%M:%S}", tp);
}

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
				addDanmaku({ "sender", "content", std::chrono::system_clock::now() });
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

        auto timeStr = formatTimestamp(danmaku.timestamp);
		auto &document = uiState_.mainWin_->getDocument();

        auto danmakuEle = document.CreateElement("div");
		danmakuEle->SetClass("danmaku-item", true);
        
        auto senderEle = document.CreateElement("span");
        senderEle->SetClass("danmaku-sender", true);
        senderEle->SetInnerRML(danmaku.sender.c_str());
        danmakuEle->AppendChild(std::move(senderEle));

        auto contentEle = document.CreateElement("span");
        contentEle->SetClass("danmaku-content", true);
        contentEle->SetInnerRML(danmaku.content.c_str());
        danmakuEle->AppendChild(std::move(contentEle));

        auto timeEle = document.CreateElement("span");
        timeEle->SetClass("danmaku-time", true);
        timeEle->SetInnerRML(timeStr.c_str());
        danmakuEle->AppendChild(std::move(timeEle));

        danmakuList_->AppendChild(std::move(danmakuEle));

        danmakuList_->ScrollIntoView(false);
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
