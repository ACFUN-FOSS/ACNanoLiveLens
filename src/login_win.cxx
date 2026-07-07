#include "login_win.hxx"
#include "appstate.hxx"
#include "Core/assets.hxx"
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
	using AsyncOpScope = UiWinBizLogicObjAsyncOpScope<LoginWin>;
public:
	Impl(UiWinBizLogicObjContext<LoginWin> &&ctx)
		: ctx_(std::move(ctx)),
		uiState_{ *App::getState().winManager, getAssetsDir() / "login_win.rml" } {
		bindEventHandlers();
		refreshQrCodeUi(false);
		uiState_.mainWin_.centerToPrimaryMonitor();
		client_.bindRuntime(*App::getState().coroRuntime, App::getState().mainThreadExecutor);
		client_.onResp([this](const AnyResp &resp) {
			handleResp(resp);
		});
		client_.onReconnectAttempt([this](std::chrono::system_clock::time_point disconnectAt, std::size_t attemptCount) {
			std::println("[重连] 尝试次数: {}", attemptCount);
		});


	}

	~Impl() {
		
	}
	Impl(Impl &&) = delete;
	Impl(const Impl &) = delete;
	Impl &operator=(const Impl &) = delete;
	Impl &operator=(Impl &&) = delete;

//private:
	struct UIState
	{
		UiWin mainWin_;
		SimpleEventListenerManager mainWinRootEleEventMan_;
		int frameCount = 0;

		UIState(WinManager &winManager, std::filesystem::path documentPath)
			: mainWin_{ "login", { 460, 460 }, std::move(documentPath), winManager, true }
			, mainWinRootEleEventMan_{ mainWin_.getRootElement() } {
		}
	};

	void bindEventHandlers() {
		uiState_.mainWin_.setDocumentChangedCb([this] {
			uiState_.mainWinRootEleEventMan_.reBind(uiState_.mainWin_.getRootElement());
			
		});

		uiState_.mainWin_.setShowCb([this] {
			std::println("show!");
			[](AsyncOpScope asyncOpScope) -> coro::result<void> {
				auto &that = *asyncOpScope.that().pImpl;
				try {
					//std::println("------开始!");
					co_await that.client_.connectAsync();
					//std::println("------OK!");
					that.client_.requestQrCodeLogin();
				} catch (const std::exception& e) {
					std::println("------[错误] 连接后端时出错: {}", e.what());
					MsgBox::popupOKMsgBox(MsgBox::Type::EERR, "无法连接到后端");
					App::die(-2);
				}
			}(ctx_);
		});

		uiState_.mainWin_.setUpdateCb([this]{
			try {
				client_.exec();
			} catch (const AcliveBackendError &e) {
				std::println("[错误] 后端响应错误: {}", e.what());
				MsgBox::popupOKMsgBox(MsgBox::Type::EERR, e.meta().error.value_or("后端返回错误"));
				uiState_.mainWin_.requestClose();
			} catch (const std::exception &e) {
				std::println("[错误] 处理后端响应时出错: {}", e.what());
				//MsgBox::popupOKMsgBox(MsgBox::Type::EERR, "后端响应格式无效");
				//uiState_.mainWin_->requestClose();
			}
		});

		uiState_.mainWinRootEleEventMan_.clear();
		uiState_.mainWinRootEleEventMan_.on("refresh-btn", "click", [this](Rml::Event &event) {
			client_.requestQrCodeLogin();
		});
	}

	void refreshQrCodeUi(bool hasQrCode) {
		auto &rootElement = uiState_.mainWin_.getRootElement();
		auto &qrCodeEle = UNWRAP(findChildOrSelfById(&rootElement, "qr-code"));
		auto &placeholderEle = UNWRAP(findChildOrSelfById(&rootElement, "qr-code-placeholder"));

		qrCodeEle.SetProperty("display", hasQrCode ? "block" : "none");
		placeholderEle.SetProperty("display", hasQrCode ? "none" : "block");
	}

	void handleResp(const AnyResp &resp) {
		std::visit(overloaded{
			[this](const QrCodeLoginResp &qrResp) {
				dbgLog("收到二维码");

				auto qrCodeImageFile = getAssetsDir()
					/ "runtime"
					/ std::format("qrcode-{}.png", qrCodeRefreshCount++);
				saveQrCodeImage(qrResp.data.imageData, qrCodeImageFile);

				auto &qrCodeEle = UNWRAP(findChildOrSelfById(&uiState_.mainWin_.getRootElement(), "qr-code"));
				qrCodeEle.SetAttribute("src", qrCodeImageFile.string());
				UNWRAP(findChildOrSelfById(&uiState_.mainWin_.getRootElement(), "login-hint"))
					.SetInnerRML("请使用 AcFun App 扫码登录");
				refreshQrCodeUi(true);
			},
			[this](const QrCodeScannedResp &) {
				UNWRAP(findChildOrSelfById(&uiState_.mainWin_.getRootElement(), "login-hint"))
					.SetInnerRML("请确认登入");
			},
			[this](const QrCodeLoginTerminatedResp &) {
				UNWRAP(findChildOrSelfById(&uiState_.mainWin_.getRootElement(), "login-hint"))
					.SetInnerRML("二维码已过期或已取消，请刷新");
				refreshQrCodeUi(false);
			},
			[this](const QrCodeLoginSuccessResp &) {
				UNWRAP(findChildOrSelfById(&uiState_.mainWin_.getRootElement(), "login-hint"))
					.SetInnerRML("登入成功");
				uiState_.mainWin_.hide();
			}
		}, resp);
	}


	UIState uiState_;
	bool closePrepared_ = false;

	AcliveBackendClient client_{
		"ws://localhost:15368/"
	};
	int qrCodeRefreshCount = 0;

	UiWinBizLogicObjContext<LoginWin> ctx_;
	
};

LoginWin::LoginWin(UiWinBizLogicObjContext<LoginWin> ctx)
	: pImpl{ stdx::pimpl::make_unique<Impl>(std::move(ctx)) }
{
}


UiWinBizLogicObjContext<LoginWin>& LoginWin::getLogicObjCtx() {
	return pImpl->ctx_;
}

RmlUIWin::UiWin &LoginWin::getUiWin() {
	return pImpl->uiState_.mainWin_;
}

LoginWin::~LoginWin() = default;
