#include "Core/aclive_backend_client.hxx"

using namespace std::chrono_literals;
using namespace Essentials::Memory;

namespace {

enum class LogType
{
	TRACE,	// 跟踪信息
	INFO,	// 断连、重试等
};

constexpr LogType logLevel = LogType::TRACE;

[[nodiscard]] std::string_view getLogTypeName(const LogType type) noexcept {
	switch (type) {
	case LogType::TRACE:
		return "TRACE";
	case LogType::INFO:
		return "INFO";
	}

	return "UNKNOWN";
}

[[nodiscard]] bool shouldLog(const LogType type) noexcept {
	return static_cast<int>(type) >= static_cast<int>(logLevel);
}

template <typename... T>
void abclog(LogType type, const std::format_string<T...> Fmt, T&&... Args) {
	if (!shouldLog(type)) {
		return;
	}

	std::println(
		"[AcliveBackendClient/{}] {}",
		getLogTypeName(type),
		std::format(Fmt, std::forward<T>(Args)...)
	);
}

template <typename T>
AcliveBackendRespMeta parseMeta(const AcliveBackendRespWire<T> &wire) {
	return AcliveBackendRespMeta{
		.requestID = wire.requestID,
		.result = wire.result,
		.error = wire.error,
	};
}

AnyResp parseResp(const WsData &wsData) {
	switch (std::stoi(wsData.event)) {
	case QrCodeLoginResp::type: {
		const auto wire = rfl::json::read<QrCodeLoginRespWire>(wsData.payload).value();
		const auto meta = parseMeta(wire);
		if (meta.result != 1) {
			throw AcliveBackendError{
				meta,
				std::format(
					"Aclive backend request failed. type={}, requestID={}, result={}, error={}",
					QrCodeLoginResp::type,
					meta.requestID,
					meta.result,
					meta.error.value_or("unknown error"))
			};
		}

		if (!wire.data) {
			throw std::runtime_error("QrCodeLoginResp missing data.");
		}

		return QrCodeLoginResp{
			.meta = meta,
			.data = QrCodeLoginRespBody{
				.imageData = wire.data->imageData,
				.expireTime = wire.data->expireTime,
				.extraFields = std::move(wire.data->extraFields),
			}
		};
	}
	case QrCodeScannedResp::type: {
		const auto wire = rfl::json::read<QrCodeScannedRespWire>(wsData.payload).value();
		const auto meta = parseMeta(wire);
		if (meta.result != 1) {
			throw AcliveBackendError{
				meta,
				std::format(
					"Aclive backend request failed. type={}, requestID={}, result={}, error={}",
					QrCodeScannedResp::type,
					meta.requestID,
					meta.result,
					meta.error.value_or("unknown error"))
			};
		}

		return QrCodeScannedResp{
			.meta = meta,
			.data = {}
		};
	}
	case QrCodeLoginTerminatedResp::type: {
		const auto wire = rfl::json::read<QrCodeLoginTerminatedRespWire>(wsData.payload).value();
		const auto meta = parseMeta(wire);
		if (meta.result != 1) {
			throw AcliveBackendError{
				meta,
				std::format(
					"Aclive backend request failed. type={}, requestID={}, result={}, error={}",
					QrCodeLoginTerminatedResp::type,
					meta.requestID,
					meta.result,
					meta.error.value_or("unknown error"))
			};
		}

		return QrCodeLoginTerminatedResp{
			.meta = meta,
			.data = {}
		};
	}
	case QrCodeLoginSuccessResp::type: {
		const auto wire = rfl::json::read<QrCodeLoginSuccessRespWire>(wsData.payload).value();
		const auto meta = parseMeta(wire);
		if (meta.result != 1) {
			throw AcliveBackendError{
				meta,
				std::format(
					"Aclive backend request failed. type={}, requestID={}, result={}, error={}",
					QrCodeLoginSuccessResp::type,
					meta.requestID,
					meta.result,
					meta.error.value_or("unknown error"))
			};
		}

		if (!wire.data) {
			throw std::runtime_error("QrCodeLoginSuccessResp missing data.");
		}

		return QrCodeLoginSuccessResp{
			.meta = meta,
			.data = QrCodeLoginSuccessRespBody{
				.tokenInfo = std::move(wire.data->tokenInfo),
				.extraFields = std::move(wire.data->extraFields),
			}
		};
	}
	default:
		throw std::runtime_error(std::format("Unsupported aclive backend response type: {}", wsData.event));
	}
}

std::string makeRequestID() {
	static std::atomic_uint64_t counter = 0;
	return std::format("req_{}", ++counter);
}

} // namespace

struct AcliveBackendClient::State
{
	Ws ws;
	std::vector<RespHandler> respHandlers;
	std::optional<std::string> pendingQrCodeLoginRequestID;
	bool waitingForReconnectRecovery = false;
	Ws::ConnectionState lastConnectionState = Ws::ConnectionState::Disconnected;

	explicit State(std::string_view url, std::chrono::seconds heartbeatInterval)
		: ws{
			url,
			"type",
			heartbeatInterval,
			[]() -> nlohmann::json {
				return nlohmann::json::parse(rfl::json::write(HeartbeatReqWire{}));
			}
		} {
		ws.on("7", [this](const WsData &data) {
			dispatch(parseResp(data));
		});
		ws.on("8", [this](const WsData &data) {
			dispatch(parseResp(data));
		});
		ws.on("9", [this](const WsData &data) {
			dispatch(parseResp(data));
		});
		ws.on("10", [this](const WsData &data) {
			dispatch(parseResp(data));
		});
	}

	void dispatch(const AnyResp &resp) {
		for (const auto &handler : respHandlers) {
			handler(resp);
		}
	}
};

AcliveBackendError::AcliveBackendError(AcliveBackendRespMeta meta, std::string message)
	: std::runtime_error{ std::move(message) }
	, meta_{ std::move(meta) } {
}

[[nodiscard]] const AcliveBackendRespMeta &AcliveBackendError::meta() const noexcept {
	return meta_;
}

AcliveBackendClient::AcliveBackendClient(std::string_view url, std::chrono::seconds heartbeatInterval)
	: state_{ newBox(State{ url, heartbeatInterval }) } {
}

AcliveBackendClient::~AcliveBackendClient() = default;

AcliveBackendClient::AcliveBackendClient(AcliveBackendClient &&other) noexcept = default;

AcliveBackendClient &AcliveBackendClient::operator=(AcliveBackendClient &&other) noexcept = default;

void AcliveBackendClient::connect() {
	state_->ws.connect();
	state_->lastConnectionState = state_->ws.getConnectionState();
	state_->waitingForReconnectRecovery = false;
}

accoro::result<void> AcliveBackendClient::connectAsync() {
	co_await state_->ws.connectAsync();
	state_->lastConnectionState = state_->ws.getConnectionState();
	state_->waitingForReconnectRecovery = false;
}

void AcliveBackendClient::bindRuntime(accoro::runtime &corort, ESSM::Rc<accoro::executor> mainThreadExecutor) {
	state_->ws.bindRuntime(corort, std::move(mainThreadExecutor));
}

void AcliveBackendClient::disconnect() {
	state_->ws.disconnect();
	state_->lastConnectionState = state_->ws.getConnectionState();
	state_->waitingForReconnectRecovery = false;
}

[[nodiscard]] bool AcliveBackendClient::isConnected() const noexcept {
	return state_ && state_->ws.isConnected();
}

void AcliveBackendClient::exec() {
	state_->ws.execCb();

	const auto connectionState = state_->ws.getConnectionState();
	if (connectionState != state_->lastConnectionState) {
		abclog(
			LogType::TRACE,
			"exec: connection state changed from {} to {}.",
			rfl::enum_to_string(state_->lastConnectionState),
			rfl::enum_to_string(connectionState)
		);

		if (state_->lastConnectionState == Ws::ConnectionState::Connected &&
			connectionState != Ws::ConnectionState::Connected) {
			state_->waitingForReconnectRecovery = true;
		}

		state_->lastConnectionState = connectionState;
	}

	if (state_->waitingForReconnectRecovery &&
		connectionState == Ws::ConnectionState::Connected &&
		state_->pendingQrCodeLoginRequestID) {
		abclog(
			LogType::INFO,
			"exec: connection recovered, re-sending pending qr code login request, requestID={}.",
			*state_->pendingQrCodeLoginRequestID
		);
		state_->waitingForReconnectRecovery = false;
		requestQrCodeLogin(*state_->pendingQrCodeLoginRequestID);
	}
}

void AcliveBackendClient::onResp(RespHandler handler) {
	state_->respHandlers.push_back(std::move(handler));
}

void AcliveBackendClient::onReconnectAttempt(ReconnectHandler handler) {
	state_->ws.onReconnectAttempt(std::move(handler));
}

void AcliveBackendClient::requestQrCodeLogin(std::string_view requestID) {
	if (state_->ws.getConnectionState() != Ws::ConnectionState::Connected) {
		throw std::runtime_error("Aclive backend is not connected.");
	}

	const auto actualRequestID = requestID.empty() ? makeRequestID() : std::string{ requestID };
	state_->pendingQrCodeLoginRequestID = actualRequestID;
	state_->ws.sendText(rfl::json::write(QrCodeLoginReqWire{
		.type = QrCodeLoginResp::type,
		.requestID = actualRequestID,
	}));
}
