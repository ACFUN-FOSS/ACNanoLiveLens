#include "danmaku_monitor_win.hxx"
#include "danmaku_item.hxx"
#include "appstate.hxx"
#include "Core/assets.hxx"
//#include "js_binding.hxx"
#include "rmluipp.hxx"
#include "RmlUIWin/window_manager.hxx"
#include "msg_box.hxx"

#include "utils.hxx"

using namespace RmlUIWin;
using namespace Essentials::Memory;
using namespace Essentials::IO;
using namespace Essentials::Misc;

// 太复杂，需要进一步封装
// TODO：或将 UiWin 初始化放在外部，要求构造时传入（不一定）
class DanmakuMonitorWin::Impl
{
public:
    Impl()
        : uiState_{ *App::getState().winManager, getAssetsDir() / "danmaku_monitor.rml" }
		, danmakuList_{
			uiState_.mainWin_, "#danmaku-list"
		} {
			uiState_.mainWin_.setUpdateCb([this]() {
				static bool first = true;
				if (first) {
					// read test danmakus from json
					auto danmakus = rfl::json::read<std::vector<DanmakuInfo>>(
						readFile(getAssetsDir() / "test_danmaku.json")
					).value();

					// add danmakus to gui
					for (auto &danmaku : danmakus) {
						addDanmaku(danmaku);
					}

					first = false;
				}
				startPendingDanmakuContainerAnim();
				scrollToEnd();
			});

			uiState_.mainWin_.setDocumentChangedCb([this]() {
				auto danmakuInGui = danmakuInGui_;
				dbgLog("DanmakuMonitorWin: reload: clearDanmaku");

				auto &mainWinRootEle = uiState_.mainWin_.getRootElement();
				// Event bindings are restored by SimpleEventListenerManager.
				// ElementDynRef resolves against the current document.

				clearDanmaku();
				// Re-spawn danmakus
				for (auto &danmakuInGui : danmakuInGui) {
					addDanmaku(danmakuInGui.danmakuInfo);
				}
			});

			uiState_.mainWinRootEleEventMan_.on("add-danmaku-btn", "click", [this](auto &&_) {
			addDanmaku({
				"sender",
				"contentcontent contentcontent contentcontent contentcontent contentcontent contentcontent",
				std::chrono::system_clock::now()
			});
		});

		uiState_.mainWin_.show();
    }

    ~Impl() = default;
    Impl(Impl &&) = delete;
    Impl(const Impl &) = delete;
    Impl &operator=(const Impl &) = delete;
    Impl &operator=(Impl &&) = delete;

	void createUIState() {
		//uiState_.mainWinRootEleEventMan_
	}
    void addDanmaku(const DanmakuInfo &danmaku)
    {

		//MsgBox::popupOKMsgBox(MsgBox::Type::EINFO, "Add Danmaku");


		//try_eval_module();

		auto &document = uiState_.mainWin_.getDocument();
		//dbgLog("document: {}", ptrToHex(&document));

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
		danmakuInGui_.emplace_back(
			DanmakuGuiInfo{
				&danmakuItemAppearAnimContainerEle_,
				&danmakuItem_
			},
			danmaku
		);

		// {
		// 	auto jsTwinObj = makeJsTwinObject(*App::getState().jsCtx, UNWRAP(metapp::getMetaType<DanmakuGuiInfo>()), danmakuInGui_.back());
		// 	auto func = App::getState().jsCtx->global()["test"];
		// 	auto res = JS_Call(App::getState().jsCtx->ctx, qjs::Value{ func }.v, JS_UNDEFINED, 1, &jsTwinObj.v);
		// 	if (JS_IsException(res))
		// 		js_std_dump_error(App::getState().jsCtx->ctx);
		// }
    }

	void scrollToEnd() {

		auto &root = uiState_.mainWin_.getRootElement();
		//danmaku-list-scroll-container
		//auto &scrollContainer = UNWRAP(findChildOrSelfById(&root, "danmaku-list-scroll-container"));
		//scrollContainer.ScrollIntoView(true);
		danmakuList_->ScrollIntoView(false);
		//danmakuList.SetScrollTop(danmakuList.GetScrollHeight());
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
  			auto &document = uiState_.mainWin_.getDocument();

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

		for (auto &danmakuInGui : danmakuInGui_) {
			danmakuList_->RemoveChild(
				danmakuInGui.guiInfo.danmakuItemAppearAnimContainerEle
			);
		}

		danmakuInGui_.clear();
    }

	RmlUIWin::UiWin &getUiWin() {
		return uiState_.mainWin_;
	}

private:
    struct UIState
    {
        UiWin mainWin_;
        SimpleEventListenerManager mainWinRootEleEventMan_;

		UIState(WinManager &winManager, std::filesystem::path documentPath)
			: mainWin_{ "danmaku_monitor", { 466, 666 }, std::move(documentPath), winManager, true }
			, mainWinRootEleEventMan_{ mainWin_ } {
		}
    } uiState_;

	std::stack<DanmakuGuiInfo> pendingAnimDanmaku;
	std::vector<DanmakuInGui> danmakuInGui_;

    ElementDynRef danmakuList_;

	std::vector<DanmakuInfo> danmakuInfos_;
};

template<>
struct metapp::DeclareMetaType<DanmakuMonitorWin::DanmakuGuiInfo> : metapp::DeclareMetaTypeBase<DanmakuMonitorWin::DanmakuGuiInfo>
{
	static void setup() {
		App::getGlobalMetaRepo().registerType<DanmakuMonitorWin::DanmakuGuiInfo>("DanmakuGuiInfo");
	}
	static const metapp::MetaClass *getMetaClass() {
		static const metapp::MetaClass metaClass {
			metapp::getMetaType<DanmakuMonitorWin::DanmakuGuiInfo>(),
			[](metapp::MetaClass &mc) {
				mc.registerVariable("danmakuItemAppearAnimContainerEle", &DanmakuMonitorWin::DanmakuGuiInfo::danmakuItemAppearAnimContainerEle);
				mc.registerVariable("danmakuEle", &DanmakuMonitorWin::DanmakuGuiInfo::danmakuEle);
			}
		};
		return &metaClass;
	}
};

// void DanmakuMonitorWin::setupJsBinding(qjs::Context &ctx) {
// 	auto metaType = metapp::getMetaType<DanmakuMonitorWin::DanmakuGuiInfo>();
// 	//regClass(ctx, UNWRAP(metaType));
// }

DanmakuMonitorWin::DanmakuMonitorWin(UiWinBizLogicObjContext<DanmakuMonitorWin> ctx)
    : pImpl{ stdx::pimpl::make_unique<Impl>() }
	, ctx_{ std::move(ctx) }
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

UiWinBizLogicObjContext<DanmakuMonitorWin>& DanmakuMonitorWin::getLogicObjCtx()
{
	return ctx_;
}

RmlUIWin::UiWin &DanmakuMonitorWin::getUiWin()
{
	return pImpl->getUiWin();
}
