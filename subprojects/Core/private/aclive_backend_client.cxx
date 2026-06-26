#include "Core/aclive_backend_client.hxx"

using namespace std::chrono_literals;
using namespace Essentials::Memory;

namespace {

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
	std::vector<ReconnectHandler> reconnectHandlers;
	std::optional<std::chrono::system_clock::time_point> disconnectAt;
	std::size_t reconnectAttemptCount = 0;
	std::optional<std::string> pendingQrCodeLoginRequestID;
	std::chrono::system_clock::time_point nextReconnectAttemptAt{};

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

	void notifyReconnectAttempt() const {
		if (!disconnectAt) {
			std::println("[AcliveBackendClient] notifyReconnectAttempt skipped: disconnectAt is not set.");
			return;
		}

		std::println(
			"[AcliveBackendClient] notifyReconnectAttempt: disconnectAtMs={}, attemptCount={}, handlerCount={}",
			std::chrono::duration_cast<std::chrono::milliseconds>(
				disconnectAt->time_since_epoch()
			).count(),
			reconnectAttemptCount,
			reconnectHandlers.size()
		);

		for (const auto &handler : reconnectHandlers) {
			handler(*disconnectAt, reconnectAttemptCount);
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
	state_->disconnectAt.reset();
	state_->reconnectAttemptCount = 0;
	state_->nextReconnectAttemptAt = {};
}

accoro::result<void> AcliveBackendClient::connectAsync(accoro::runtime &corort, ESSM::Rc<accoro::executor> mainThreadExecutor) {
	co_await state_->ws.connectAsync(corort, std::move(mainThreadExecutor));
	state_->disconnectAt.reset();
	state_->reconnectAttemptCount = 0;
	state_->nextReconnectAttemptAt = {};
}

void AcliveBackendClient::disconnect() {
	state_->ws.disconnect();
}

[[nodiscard]] bool AcliveBackendClient::isConnected() const noexcept {
	return state_ && state_->ws.isConnected();
}

void AcliveBackendClient::exec() {
	state_->ws.execCb();

	if (!state_->ws.isConnected()) {
		const auto reason = state_->ws.getDisconnectReason();
		const auto disconnectedAt = state_->ws.getDisconnectedAt();
		std::println(
			"[AcliveBackendClient] exec: ws is not connected, reason={}, disconnectedAtMs={}.",
			static_cast<int>(reason),
			std::chrono::duration_cast<std::chrono::milliseconds>(
				disconnectedAt.time_since_epoch()
			).count()
		);

		if (reason != Ws::DisconnectReason::Unexpected) {
			std::println("[AcliveBackendClient] exec: disconnect is not unexpected, skipping reconnect.");
			return;
		}

		if (!state_->disconnectAt) {
			state_->disconnectAt = disconnectedAt;
			state_->reconnectAttemptCount = 0;
			state_->nextReconnectAttemptAt = disconnectedAt;
			std::println(
				"[AcliveBackendClient] exec: unexpected disconnect detected, disconnectAtMs={}.",
				std::chrono::duration_cast<std::chrono::milliseconds>(
					state_->disconnectAt->time_since_epoch()
				).count()
			);
		}

		const auto now = std::chrono::system_clock::now();
		if (now < state_->nextReconnectAttemptAt) {
			std::println(
				"[AcliveBackendClient] exec: waiting for next reconnect slot, remainingMs={}.",
				std::chrono::duration_cast<std::chrono::milliseconds>(
					state_->nextReconnectAttemptAt - now
				).count()
			);
			return;
		}

		++state_->reconnectAttemptCount;
		state_->notifyReconnectAttempt();

		std::println(
			"[AcliveBackendClient] exec: starting reconnect attempt {} after unexpected disconnect.",
			state_->reconnectAttemptCount
		);
		try {
			state_->ws.connect();
			state_->nextReconnectAttemptAt = {};
			std::println(
				"[AcliveBackendClient] exec: reconnect attempt {} succeeded.",
				state_->reconnectAttemptCount
			);
		} catch (const std::exception &e) {
			state_->nextReconnectAttemptAt = now + 1s;
			std::println(
				"[AcliveBackendClient] exec: reconnect attempt {} failed: {}. nextRetryAtMs={}.",
				state_->reconnectAttemptCount,
				e.what(),
				std::chrono::duration_cast<std::chrono::milliseconds>(
					state_->nextReconnectAttemptAt.time_since_epoch()
				).count()
			);
			return;
		}

		if (state_->pendingQrCodeLoginRequestID) {
			std::println(
				"[AcliveBackendClient] exec: re-sending pending qr code login request, requestID={}.",
				*state_->pendingQrCodeLoginRequestID
			);
			requestQrCodeLogin(*state_->pendingQrCodeLoginRequestID);
		}
		return;
	}

	const auto lastHeartbeatSentAt = state_->ws.getLastHeartbeatSentAt();
	if (lastHeartbeatSentAt == std::chrono::system_clock::time_point{}) {
		std::println("[AcliveBackendClient] exec: no heartbeat has been sent yet.");
		return;
	}

	const auto lastMessageReceivedAt = state_->ws.getLastMessageReceivedAt();
	std::println(
		"[AcliveBackendClient] exec: heartbeatSentAtMs={}, lastMessageReceivedAtMs={}",
		std::chrono::duration_cast<std::chrono::milliseconds>(
			lastHeartbeatSentAt.time_since_epoch()
		).count(),
		std::chrono::duration_cast<std::chrono::milliseconds>(
			lastMessageReceivedAt.time_since_epoch()
		).count()
	);
	if (lastMessageReceivedAt >= lastHeartbeatSentAt) {
		if (state_->disconnectAt || state_->reconnectAttemptCount != 0) {
			std::println("[AcliveBackendClient] exec: connection looks healthy again, clearing reconnect state.");
		}
		state_->disconnectAt.reset();
		state_->reconnectAttemptCount = 0;
		state_->nextReconnectAttemptAt = {};
		return;
	}

	const auto now = std::chrono::system_clock::now();
	if (now - lastHeartbeatSentAt <= 3s) {
		std::println(
			"[AcliveBackendClient] exec: heartbeat timeout has not been reached yet, elapsedMs={}",
			std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHeartbeatSentAt).count()
		);
		return;
	}

	if (!state_->disconnectAt) {
		state_->disconnectAt = now;
		state_->reconnectAttemptCount = 0;
		state_->nextReconnectAttemptAt = now;
		std::println(
			"[AcliveBackendClient] exec: heartbeat timeout detected, disconnectAtMs={}",
			std::chrono::duration_cast<std::chrono::milliseconds>(
				state_->disconnectAt->time_since_epoch()
			).count()
		);
	}

	std::println("[AcliveBackendClient] exec: disconnecting ws before reconnect attempt.");
	state_->ws.disconnect();
	state_->nextReconnectAttemptAt = now;
	++state_->reconnectAttemptCount;
	state_->notifyReconnectAttempt();

	std::println(
		"[AcliveBackendClient] exec: starting reconnect attempt {}.",
		state_->reconnectAttemptCount
	);
	try {
		state_->ws.connect();
		state_->nextReconnectAttemptAt = {};
		std::println(
			"[AcliveBackendClient] exec: reconnect attempt {} succeeded.",
			state_->reconnectAttemptCount
		);
	} catch (const std::exception &e) {
		state_->nextReconnectAttemptAt = now + 1s;
		std::println(
			"[AcliveBackendClient] exec: reconnect attempt {} failed: {}. nextRetryAtMs={}.",
			state_->reconnectAttemptCount,
			e.what(),
			std::chrono::duration_cast<std::chrono::milliseconds>(
				state_->nextReconnectAttemptAt.time_since_epoch()
			).count()
		);
		return;
	}

	if (state_->pendingQrCodeLoginRequestID) {
		std::println(
			"[AcliveBackendClient] exec: re-sending pending qr code login request, requestID={}.",
			*state_->pendingQrCodeLoginRequestID
		);
		requestQrCodeLogin(*state_->pendingQrCodeLoginRequestID);
	}
}

void AcliveBackendClient::onResp(RespHandler handler) {
	state_->respHandlers.push_back(std::move(handler));
}

void AcliveBackendClient::onReconnectAttempt(ReconnectHandler handler) {
	state_->reconnectHandlers.push_back(std::move(handler));
	std::println(
		"[AcliveBackendClient] onReconnectAttempt: registered handler, handlerCount={}.",
		state_->reconnectHandlers.size()
	);
}

void AcliveBackendClient::requestQrCodeLogin(std::string_view requestID) {
	if (!isConnected()) {
		throw std::runtime_error("Aclive backend is not connected.");
	}

	const auto actualRequestID = requestID.empty() ? makeRequestID() : std::string{ requestID };
	state_->pendingQrCodeLoginRequestID = actualRequestID;
	state_->ws.sendText(rfl::json::write(QrCodeLoginReqWire{
		.type = QrCodeLoginResp::type,
		.requestID = actualRequestID,
	}));
}
