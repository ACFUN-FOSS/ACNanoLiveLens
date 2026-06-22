#include "login_win.hxx"
#include "appstate.hxx"
#include "assets.hxx"
#include "msg_box.hxx"
#include "rmluipp.hxx"
#include "RmlUIWin/window_manager.hxx"
#include "utils.hxx"

using namespace std::chrono_literals;
using namespace RmlUIWin;
using namespace Essentials::Memory;
using namespace Essentials::IO;
using namespace Essentials::Misc;

static void saveQrCodeImage(std::string_view base64Data, stdf::path filepath) {
	auto imageData = cppcodec::base64_rfc4648::decode(std::string(base64Data));

	std::ofstream file{ filepath, std::ios::binary };
	if (!file) {
		std::println("[错误] 无法创建文件: {}", filepath.string());
		return;
	}
    std::ostream_iterator<unsigned char> output_iterator{ file };

	std::ranges::copy(imageData, output_iterator);
	file.close();

	std::println("[二维码] 已保存到: {} ({} 字节)", filepath.string(), imageData.size());
}

class LoginWin::Impl
{
public:
	Impl()
		: uiState_{ []() -> UIState {
			auto mainWin = newBox(UiWin{ "login", { 660, 660 }, getAssetsDir() / "login_win.rml", true });
			SimpleEventListenerManager mainWinRootEleEventMan{ mainWin->getRootElement() };
			auto &win = getAppState().winManager->transferWin(std::move(mainWin));
			return { &win, std::move(mainWinRootEleEventMan) };
		}() } {
		bindEventHandlers();
		refreshQrCodeUi(false);
		//centerToMainWin();
		
		// 二维码已生成
		ws.on("7", [&](const WsData &data) {
			dbgLog("收到二维码");
			auto respData = nlohmann::json::parse(data.payload)["data"];
			std::string imageData = respData["imageData"];
			saveQrCodeImage(imageData, getAssetsDir() / "runtime" / "qrcode.png");

			auto &qrCodeEle = UNWRAP(findChildOrSelfById(&uiState_.mainWin_->getRootElement(), "qr-code"));
			qrCodeEle.SetAttribute("src", (getAssetsDir() / "runtime" / "qrcode.png").string());
			refreshQrCodeUi(true);

		});
		// 二维码已扫描
		ws.on("8", [&](const WsData&) {
			UNWRAP(findChildOrSelfById(&uiState_.mainWin_->getRootElement(), "login-hint"))
				.SetInnerRML("请确认登入");
		});
		// 二维码过期
		ws.on("9", [&](const WsData&) {
		});
		// 登入成功
		ws.on("10", [&](const WsData& data) {
		});


		[](Impl *that, UiWin::AsyncOpScope asyncOpScope) -> coro::result<void> {
			auto keepAsyncOpAlive = std::move(asyncOpScope);
			try {
				//std::println("Connect started on thread {}", std::this_thread::get_id());
				//std::println("impl: {}", ptrToHex(that));
				co_await that->ws.connectAsync(*getAppState().coroRuntime, getAppState().mainThreadExecutor);
			} catch (const std::exception& e) {
				//std::println("Connect catch on thread {}", std::this_thread::get_id());
				//std::println("impl: {}", ptrToHex(that));

				MsgBox::popupOKMsgBox(MsgBox::Type::EERR, "无法连接到后端");
				that->uiState_.mainWin_->setShouldClose();
			}
		}(this, uiState_.mainWin_->startAsyncOp());

	}

	~Impl() {
		
	}
	Impl(Impl &&) = delete;
	Impl(const Impl &) = delete;
	Impl &operator=(const Impl &) = delete;
	Impl &operator=(Impl &&) = delete;

private:
	struct UIState
	{
		gsl::not_null<UiWin *> mainWin_;
		SimpleEventListenerManager mainWinRootEleEventMan_;
		int frameCount = 0;
	};

	void bindEventHandlers() {
		uiState_.mainWin_->setDocumentChangedCb([this] {
			uiState_.mainWinRootEleEventMan_.reBind(uiState_.mainWin_->getRootElement());
			
		});

		uiState_.mainWin_->setUpdateCb([this]{
			ws.execCb();

			// uiState_.frameCount++;
			// if (uiState_.frameCount > 2) {
			// 	return;
			// }

			// try {
			// 	//
			// 	if (uiState_.frameCount == 2) {
			// 		ws.connect();
			// 		ws.send({ {"type", 7} });
			// 	}
			// } catch (const std::exception &e) {
			// 	//std::println("[错误] 登录窗口更新时出错: {}", e.what());
			// 	MsgBox::popupOKMsgBox(MsgBox::Type::EERR, "无法连接到后端");
			// 	uiState_.mainWin_->setShouldClose();
			// }
		});

		uiState_.mainWinRootEleEventMan_.clear();
		uiState_.mainWinRootEleEventMan_.on("refresh-btn", "click", [this](Rml::Event &event) {
			refreshQrCode();
			//event.StopPropagation();
		});
	}

	void refreshQrCode() {
	}

	void refreshQrCodeUi(bool hasQrCode) {
		auto &rootElement = uiState_.mainWin_->getRootElement();
		auto &qrCodeEle = UNWRAP(findChildOrSelfById(&rootElement, "qr-code"));
		auto &placeholderEle = UNWRAP(findChildOrSelfById(&rootElement, "qr-code-placeholder"));

		qrCodeEle.SetProperty("display", hasQrCode ? "block" : "none");
		placeholderEle.SetProperty("display", hasQrCode ? "none" : "block");
	}

	void centerToMainWin() {
		auto &mainWin = getAppState().winManager->getMainWin();
		auto mainWinPos = mainWin.getWinPos();
		auto mainWinSize = Backend::GetWindowSize(mainWin.getNativeWin());
		auto loginWinSize = Backend::GetWindowSize(uiState_.mainWin_->getNativeWin());

		uiState_.mainWin_->setWinPos(
			mainWinPos + Rml::Vector2i{
				(mainWinSize.x - loginWinSize.x) / 2,
				(mainWinSize.y - loginWinSize.y) / 2,
			}
		);
	}


	UIState uiState_;
	bool closePrepared_ = false;

	Ws ws{
        "ws://localhost:15368/",
        "type",
        3s,
        []() -> nlohmann::json {
            return {
                {"type", 1}
            };
        }
    };

	
};

LoginWin::LoginWin()
	: pImpl{ stdx::pimpl::make_unique<Impl>() }
{
}

LoginWin::~LoginWin() = default;
