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
}

accoro::result<void> AcliveBackendClient::connectAsync(accoro::runtime &corort, ESSM::Rc<accoro::executor> mainThreadExecutor) {
	co_await state_->ws.connectAsync(corort, std::move(mainThreadExecutor));
}

void AcliveBackendClient::disconnect() {
	state_->ws.disconnect();
}

[[nodiscard]] bool AcliveBackendClient::isConnected() const noexcept {
	return state_ && state_->ws.isConnected();
}

void AcliveBackendClient::exec() {
	state_->ws.execCb();
}

void AcliveBackendClient::onResp(RespHandler handler) {
	state_->respHandlers.push_back(std::move(handler));
}

void AcliveBackendClient::requestQrCodeLogin(std::string_view requestID) {
	if (!isConnected()) {
		throw std::runtime_error("Aclive backend is not connected.");
	}

	const auto actualRequestID = requestID.empty() ? makeRequestID() : std::string{ requestID };
	state_->ws.sendText(rfl::json::write(QrCodeLoginReqWire{
		.type = QrCodeLoginResp::type,
		.requestID = actualRequestID,
	}));
}
