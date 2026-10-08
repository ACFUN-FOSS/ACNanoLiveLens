#include "danmaku_monitor_win.hxx"
#include "danmaku_item.hxx"
#include "toggle.hxx"
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
using namespace std::chrono_literals;


// 太复杂，需要进一步封装
// TODO：或将 UiWin 初始化放在外部，要求构造时传入（不一定）
class DanmakuMonitorWin::Impl
{
	using AsyncOpScope = UiWinBizLogicObjAsyncOpScope<DanmakuMonitorWin>;
public:
    Impl(AcliveBackendClient *client, UiWinBizLogicObjContext<DanmakuMonitorWin> ctx)
        : uiState_{ *App::getState().winManager, getAssetsDir() / "danmaku_monitor.rml" },
			danmakuList_{
				uiState_.mainWin_, "#danmaku-list"
			},
			scrollContainer_{
				uiState_.mainWin_, "#danmaku-list-scroll-container"
			},
			notLiveHint_{
				uiState_.mainWin_, "#not-live-hint"
			},
			client{ client },
			ctx_{ std::move(ctx) }
		{
			uiState_.mainWin_.setShowCb([this]() {
				if (!this->client) {
					// read test danmakus from json
					auto danmakus = rfl::json::read<std::vector<DanmakuInfo>>(
						readFile(getAssetsDir() / "test_danmaku.json")
					).value();

					// add danmakus to gui
					for (auto &danmaku : danmakus) {
						addDanmaku(danmaku);
					}
				}

				[](AsyncOpScope asyncOpScope) -> coro::result<void> {
					while(true) {
						co_await coroSleep(2s);
						std::println("test sleep");
						if (asyncOpScope.that().pImpl->uiState_.mainWin_.isPendingClose())
							break;
					}
					
				}(ctx_);
			});


			uiState_.mainWin_.setUpdateCb([this]() {
				// 判断用户是否在直播
				// TODO: 太繁琐，应简化
				if (this->client != nullptr && this->client->isConnected()) {
					const auto now = std::chrono::steady_clock::now();
					if (liveStatusRequestPending_ && now >= liveStatusRequestDeadline_) {
						liveStatusRequestPending_ = false;
						setLiveState(false);
					}
					if (now >= nextLiveStatusAt_ && !liveStatusRequestPending_) {
						this->client->requestLiveStatus();
						liveStatusRequestPending_ = true;
						nextLiveStatusAt_ = now + 5s;
						liveStatusRequestDeadline_ = now + 2s;
					}
				} else {
					setLiveState(true);
				}

				// 处理弹幕动画
				startPendingDanmakuContainerAnim();
				scrollToEnd();
			});

    		// AcliveBackendClient 直播动态响应（弹幕、礼物……）
			if (client) {
				client->onResp([that = LifetimeAwareWRef{ *this }](const AnyResp &response) mutable {
					if (!that.isValid())
						return;
					if (const auto *status = std::get_if<LiveStatusResp>(&response)) {
						that->liveStatusRequestPending_ = false;
						if (status->meta.result == 1 && status->data.liveID) {
							that->setLiveState(true);
						} else {
							that->setLiveState(false);
						}
					}
				});
				client->onLiveActivityEnded([that = LifetimeAwareWRef{ *this }](std::uint64_t) mutable {
					if (!that.isValid())
						return;
					that->setLiveState(false);
				});
				client->onLiveActivity([that = LifetimeAwareWRef{ *this }](const LiveActivity &activity) mutable {
					if (!that.isValid())
						return;
					that->setLiveState(true);
					std::visit(overloaded{
						[that](const DanmakuActivity &danmaku) mutable {
							that->addDanmaku({
								danmaku.data.danmuInfo.userInfo.nickname,
								danmaku.data.content,
								danmaku.data.danmuInfo.sendTime
							});
						},
						[](const LikeActivity &) {
						},
						[](const GiftActivity &) {
						}
					}, activity);
				});
			}

			uiState_.mainWin_.setDocumentReloadedCb([this]() {
				
				auto danmakuInGui = danmakuInGui_;
				dbgLog("DanmakuMonitorWin: reload: clearDanmaku");

				// Event bindings are restored by SimpleEventListenerManager.
				// ElementDynRef resolves against the current document.

				// DOM 重建会生成新的 Toggle 实例，需重新注册翻转回调；
				// 随后统一把业务状态同步回新建的 UI。
				setupAutoScrollToggle();
				syncToUI();

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

		// 用户在滚动容器上滚轮输入 → 关闭自动滚动锁定
		uiState_.mainWinRootEleEventMan_.on("danmaku-list-scroll-container", "mousescroll", [this](Rml::Event &) {
			setScrollLocked(false);
		});

		// 自动滚动开关由 Toggle 自绘元素自行处理点击，
		// 这里只做初始化：注册翻转回调，再统一同步一次业务状态到 UI。
		setupAutoScrollToggle();
		syncToUI();

		//uiState_.mainWin_.show();
   }

    ~Impl() = default;
    Impl(Impl &&) = delete;
    Impl(const Impl &) = delete;
    Impl &operator=(const Impl &) = delete;
    Impl &operator=(Impl &&) = delete;

	void createUIState() {
		//uiState_.mainWinRootEleEventMan_
	}
    void addDanmaku(const DanmakuInfo &danmaku) {

		//MsgBox::popupOKMsgBox(MsgBox::Type::EINFO, "Add Danmaku");


		//try_eval_module();

		// 达到上限时先淘汰最旧的一条，防止内存/DOM 无限增长。
		// 弹幕窗口内最多保留的弹幕条数：达到上限后再添加新弹幕，
		// 会先淘汰最旧的一条。
		constexpr std::size_t kMaxDanmakuInGui = 200;
		if (danmakuInGui_.size() >= kMaxDanmakuInGui) {
			dbgLog("danmakuInGui_.size() > kMaxDanmakuInGui, Evicting oldest danmaku");
			evictOldestDanmaku();
		}

		auto &document = uiState_.mainWin_.getDocument();
		//dbgLog("document: {}", ptrToHex(&document));

		auto danmakuItemAppearAnimContainerEle = document.CreateElement("div");
		danmakuItemAppearAnimContainerEle->SetClass("danmaku-item-appear-anim-container", true);
		//danmakuItemAppearAnimContainerEle->SetClass("show", true);

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


		//danmakuItemAppearAnimContainerEle_.SetClass("show", true);
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

	// 解析自动滚动开关（通用 Toggle 元素）；元素缺失或类型不符时返回 nullptr。
	Toggle *autoScrollToggle() {
		auto *ele = uiState_.mainWin_.getDocument().QuerySelector("#auto-scroll-toggle");
		return ele ? dynamic_cast<Toggle *>(ele) : nullptr;
	}

	// 统一的"状态 → UI"同步入口：把当前业务状态整体回写到 UI。
	// 所有状态到 UI 的写入都集中在这里；状态变化后、文档热重载重建 DOM 后
	// 统一调用一次即可，避免同步逻辑散落各处。
	void syncToUI() {
		// 自动滚动开关外观（通用 Toggle 元素）
		if (auto *toggle = autoScrollToggle()) {
			toggle->setOn(scrollLocked_);
		}

		// 直播状态：未开播时显示提示、隐藏弹幕列表
		notLiveHint_->SetProperty("display", isLive_ ? "none" : "block");
		danmakuList_->SetProperty("display", isLive_ ? "block" : "none");
	}

	// 初始化/热重载后配置开关：仅注册翻转回调（外观回写统一归 syncToUI）。
	// 文档热重载会重建 DOM 并生成新的 Toggle 实例，故需重设回调。
	void setupAutoScrollToggle() {
		auto *toggle = autoScrollToggle();
		if (!toggle) {
			dbgLog("DanmakuMonitorWin: auto-scroll-toggle element not found");
			return;
		}

		toggle->setToggledCb([this](bool on) {
			// 用户点击开关：元素已自行翻转外观，这里只更新业务状态。
			setScrollLocked(on);
		});
	}

	// 更新滚动锁定状态并同步 UI（状态未变化时不触碰 DOM）。
	void setScrollLocked(bool locked) {
		if (scrollLocked_ == locked) {
			return;
		}

		scrollLocked_ = locked;
		syncToUI();
	}

	void scrollToEnd() {
		// 滚动锁定关闭时，不做任何滚动操作（用户在看历史弹幕）。
		if (!scrollLocked_) {
			return;
		}

		// 保留动画：以"距底距离"驱动每帧滚动增量系数——
		//   factor = clamp(distance / 32, 0, 2)
		//   step   = baseStep · factor
		// 其中 baseStep 为每帧"基础步长"，默认 4dp；
		// 整条曲线连续（C⁰），近距离逐渐减速到静止。
		// scrollContainer_ 通过 ElementDynRef 解析；RmlUi 中只有具备 overflow 样式的元素
		// 才参与 scroll 模型，故此处直接指向真正的滚动容器 #danmaku-list-scroll-container，
		// 而不是没有 overflow 的 #danmaku-list。
		constexpr float kBaseScrollStepDp = 4.0f;
		constexpr float kFullSpeedDistanceDp = 40.0f;
		constexpr float kMaxFactor = 14.0f;

		auto &scrollContainer = scrollContainer_.resolve();

		const float scrollTop = scrollContainer.GetScrollTop();
		const float scrollHeight = scrollContainer.GetScrollHeight();
		const float clientHeight = scrollContainer.GetClientHeight();
		const float distanceToBottom = scrollHeight - (scrollTop + clientHeight);

		if (distanceToBottom <= 0.0f) {
			return;
		}

		const float factor = std::min(kMaxFactor, distanceToBottom / kFullSpeedDistanceDp);
		const float scrollStep = kBaseScrollStepDp * factor;

		scrollContainer.SetScrollTop(scrollTop + scrollStep);
	}


  	void startPendingDanmakuContainerAnim() {
  		if (!pendingAnimDanmaku.empty()) {
  			// 从待处理队列中取出一个弹幕元素
  			auto ele = pendingAnimDanmaku.top();
  			pendingAnimDanmaku.pop();

  			// 为待处理的弹幕容器添加动画
  			// 从待处理队列中取出一个弹幕元素，为其创建动态样式并应用
  			// 这么做是因为 RmlUI 的 transition 动画只有在添加类时才会触发
  			// 此实现动画播放过程会导致整个页面的布局持续变化，性能较差，需要优化一下。
  			// 生成唯一的类名并创建动态样式字符串
  			// **已废弃这种基于动态创建样式的 height 动画，改其他方式。**
  			// auto className = std::format("anim-{}", randomInt(0, std::numeric_limits<int>::max()));
  			// auto newStyleSrc = std::format(
  			// 	".{}{{"
  			// 	"height: {}px;"
  			// 	"}}",
  			// 	className,
  			// 	ele.danmakuEle->GetOffsetHeight()
  			// );
  			// std::cout << "newStyleSrc: " << newStyleSrc << std::endl;
  	  //
  			// // 获取文档对象
  			// auto &document = uiState_.mainWin_.getDocument();
	    //
  			// // 创建样式表并合并到文档的样式表容器中
  			// auto newStyle = Rml::Factory::InstanceStyleSheetString(newStyleSrc);
  			// auto &sheet = UNWRAP(document.GetStyleSheetContainer());
  	  //
  			// auto newFullStyle = sheet.CombineStyleSheetContainer(*newStyle);
  			// document.SetStyleSheetContainer(newFullStyle);
  	  //
  			// // 将动态样式类应用到弹幕容器元素
  			// ele.danmakuItemAppearAnimContainerEle->SetClass(className, true);

  			// 激活放大动画
  			ele.danmakuItemAppearAnimContainerEle->SetClass("show", true);
  		}
  	}



	void clearDanmaku() {

		for (auto &danmakuInGui : danmakuInGui_) {
			danmakuList_->RemoveChild(
				danmakuInGui.guiInfo.danmakuItemAppearAnimContainerEle
			);
		}

		danmakuInGui_.clear();
    }

	// 淘汰最旧的一条弹幕：先从 DOM 摘除，再从列表里弹出。
	// 同步做 scroll 补偿：被删元素下方所有内容整体上移 H_old dp，
	// 反向调整 scrollTop 让视口内元素视觉位置不变，避免列表"跳动"。
	void evictOldestDanmaku() {
		auto &oldest = danmakuInGui_.front();
		auto removedEle = oldest.guiInfo.danmakuItemAppearAnimContainerEle;

		const float removedHeight = static_cast<float>(removedEle->GetOffsetHeight());

		danmakuList_->RemoveChild(oldest.guiInfo.danmakuItemAppearAnimContainerEle);
		danmakuInGui_.pop_front();

		auto &scrollContainer = scrollContainer_.resolve();
		const float currentTop = scrollContainer.GetScrollTop();
		scrollContainer.SetScrollTop(std::max(0.0f, currentTop - removedHeight));
	}

	RmlUIWin::UiWin &getUiWin() {
		return uiState_.mainWin_;
	}

	void setLiveState(bool isLive) {
		if (isLive == isLive_)
			return;

		isLive_ = isLive;
		syncToUI();

		if (!isLive) {
			clearDanmaku();
			if (client && activeLiverUID_) {
				client->stopLiveActivity(*activeLiverUID_);
				activeLiverUID_.reset();
			}
		} else if (client) {
			if (const auto userID = client->authenticatedUserID()) {
				activeLiverUID_ = *userID;
				client->startLiveActivity(*activeLiverUID_);
			}
		}
	}

	LifetimeInformant lifetimeInformant;

//private:
    struct UIState
    {
        UiWin mainWin_;
        SimpleEventListenerManager mainWinRootEleEventMan_;

		UIState(WinManager &winManager, std::filesystem::path documentPath)
			: mainWin_{ "danmaku_monitor", { 466, 666 }, std::move(documentPath), winManager, true }
			, mainWinRootEleEventMan_{ mainWin_ } {
		}
    } uiState_;

	UiWinBizLogicObjContext<DanmakuMonitorWin> ctx_;

	std::stack<DanmakuGuiInfo> pendingAnimDanmaku;
	std::deque<DanmakuInGui> danmakuInGui_;

    ElementDynRef danmakuList_;
    ElementDynRef scrollContainer_;
    ElementDynRef notLiveHint_;


	std::vector<DanmakuInfo> danmakuInfos_;
	AcliveBackendClient *client;
	bool isLive_ = false;
	// 滚动锁定：开启时执行自动滚动跟随新弹幕；
	// 用户在 scrollContainer 上输入滚轮事件时自动关闭。
	bool scrollLocked_ = true;
	bool liveStatusRequestPending_ = false;
	std::optional<std::uint64_t> activeLiverUID_;
	std::chrono::steady_clock::time_point nextLiveStatusAt_{};
	std::chrono::steady_clock::time_point liveStatusRequestDeadline_{};
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

DanmakuMonitorWin::DanmakuMonitorWin(UiWinBizLogicObjContext<DanmakuMonitorWin> ctx, AcliveBackendClient *client)
    : pImpl{ stdx::pimpl::make_unique<Impl>(client, std::move(ctx)) }
{
}

DanmakuMonitorWin::~DanmakuMonitorWin() = default;

void DanmakuMonitorWin::addDanmaku(const DanmakuInfo &danmaku) {
    pImpl->addDanmaku(danmaku);
}

void DanmakuMonitorWin::clearDanmaku() {
    pImpl->clearDanmaku();
}

UiWinBizLogicObjContext<DanmakuMonitorWin>& DanmakuMonitorWin::getLogicObjCtx() {
	return pImpl->ctx_;
}

RmlUIWin::UiWin &DanmakuMonitorWin::getUiWin() {
	return pImpl->getUiWin();
}
