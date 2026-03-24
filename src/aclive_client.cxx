#include "aclive_client.hxx"

namespace ACLive {

AcliveClient::AcliveClient(concurrencpp::runtime& runtime, Config config)
	: m_runtime(runtime)
	, m_workerExecutor(runtime.thread_pool_executor())
	, m_mainExecutor(runtime.make_manual_executor())
	, m_config(std::move(config))
	, m_websocket(nullptr)
	, m_isConnected(false)
	, m_shouldStopHeartbeat(false)
{
}

AcliveClient::~AcliveClient()
{
	disconnectAsync().get();
}

concurrencpp::result<void> AcliveClient::connectAsync()
{
	co_await concurrencpp::resume_on(m_workerExecutor);

	std::string url = "ws://" + m_config.host + ":" + std::to_string(m_config.port) + "/";

	m_websocket = std::make_unique<httplib::ws::WebSocketClient>(url);

	m_websocket->set_read_timeout(
		std::chrono::duration_cast<std::chrono::seconds>(m_config.requestTimeout).count(),
		std::chrono::duration_cast<std::chrono::microseconds>(m_config.requestTimeout % std::chrono::seconds(1)).count()
	);

	if (!m_websocket->is_valid())
	{
		throw std::runtime_error("Invalid WebSocket URL");
	}

	if (!m_websocket->connect())
	{
		throw std::runtime_error("Failed to connect to WebSocket server");
	}

	m_isConnected = true;

	startHeartbeat();

	m_receiveThread = std::jthread([this](std::stop_token) {
		receiveLoop();
	});

	co_await concurrencpp::resume_on(m_mainExecutor);
}

concurrencpp::result<void> AcliveClient::disconnectAsync()
{
	co_await concurrencpp::resume_on(m_workerExecutor);

	stopHeartbeat();

	if (m_isConnected && m_websocket && m_websocket->is_open())
	{
		m_websocket->close();
	}

	m_isConnected = false;

	{
		std::lock_guard<std::mutex> lock(m_pendingRequestsMutex);
		for (auto& [id, promise] : m_pendingRequests)
		{
			promise.set_exception(std::make_exception_ptr(std::runtime_error("Connection closed")));
		}
		m_pendingRequests.clear();
	}

	co_await concurrencpp::resume_on(m_mainExecutor);
}

concurrencpp::result<LoginResp> AcliveClient::loginAsync(LoginReq req)
{
	co_return co_await sendRequestAsync<LoginResp>(serializeToJson(req));
}

concurrencpp::result<QrCodeLoginResp> AcliveClient::qrCodeLoginAsync(QrCodeLoginReq req)
{
	co_return co_await sendRequestAsync<QrCodeLoginResp>(serializeToJson(req));
}

concurrencpp::result<SetTokenResp> AcliveClient::setTokenAsync(SetTokenReq req)
{
	co_return co_await sendRequestAsync<SetTokenResp>(serializeToJson(req));
}

concurrencpp::result<GetDanmakuResp> AcliveClient::getDanmakuAsync(GetDanmakuReq req)
{
	co_return co_await sendRequestAsync<GetDanmakuResp>(serializeToJson(req));
}

concurrencpp::result<StopGetDanmakuResp> AcliveClient::stopGetDanmakuAsync(StopGetDanmakuReq req)
{
	co_return co_await sendRequestAsync<StopGetDanmakuResp>(serializeToJson(req));
}

void AcliveClient::setDanmakuCallback(std::function<void(const DanmakuData&)> callback)
{
	m_danmakuCallback = std::move(callback);
}

void AcliveClient::setSignalCallback(std::function<void(const SignalData&)> callback)
{
	m_signalCallback = std::move(callback);
}

void AcliveClient::startHeartbeat()
{
	m_shouldStopHeartbeat = false;
	m_heartbeatThread = std::jthread([this](std::stop_token stopToken) {
		while (!stopToken.stop_requested() && m_isConnected)
		{
			nlohmann::json heartbeat;
			heartbeat["type"] = 1;

			if (m_websocket && m_websocket->is_open())
			{
				m_websocket->send(heartbeat.dump());
			}

			std::this_thread::sleep_for(m_config.heartbeatInterval);
		}
	});
}

void AcliveClient::stopHeartbeat()
{
	if (m_heartbeatThread.joinable())
	{
		m_heartbeatThread.request_stop();
		m_heartbeatThread.join();
	}
}

void AcliveClient::receiveLoop()
{
	std::string message;
	while (m_isConnected && m_websocket)
	{
		auto result = m_websocket->read(message);
		if (result == httplib::ws::ReadResult::Fail)
		{
			m_isConnected = false;
			break;
		}
		else if (result == httplib::ws::ReadResult::Text)
		{
			onMessageReceived(message);
		}
	}
}

void AcliveClient::onMessageReceived(const std::string& message)
{
	try
	{
		auto json = nlohmann::json::parse(message);

		if (json.contains("liverUID"))
		{
			handleDanmakuOrSignal(message);
		}
		else if (json.contains("requestID"))
		{
			handleResponse(message);
		}
	}
	catch (const std::exception& e)
	{
		std::println("Error parsing message: {}", e.what());
	}
}

void AcliveClient::handleResponse(const std::string& message)
{
	try
	{
		auto json = nlohmann::json::parse(message);
		std::string requestID = json["requestID"];

		std::lock_guard<std::mutex> lock(m_pendingRequestsMutex);
		auto it = m_pendingRequests.find(requestID);
		if (it != m_pendingRequests.end())
		{
			it->second.set_result(message);
			m_pendingRequests.erase(it);
		}
	}
	catch (const std::exception& e)
	{
		std::println("Error handling response: {}", e.what());
	}
}

void AcliveClient::handleDanmakuOrSignal(const std::string& message)
{
	try
	{
		auto json = nlohmann::json::parse(message);

		if (json.contains("liverUID") && json.contains("type"))
		{
			int64_t liverUID = json["liverUID"];
			int type = json["type"];

			if (type >= 1000 && type < 2000 && m_danmakuCallback)
			{
				DanmakuData danmaku;
				danmaku.liverUID = liverUID;
				danmaku.type = type;
				danmaku.jsonData = message;
				m_danmakuCallback(danmaku);
			}
			else if (type >= 2000 && m_signalCallback)
			{
				SignalData signal;
				signal.liverUID = liverUID;
				signal.type = type;
				signal.jsonData = message;
				m_signalCallback(signal);
			}
		}
	}
	catch (const std::exception& e)
	{
		std::println("Error handling danmaku/signal: {}", e.what());
	}
}

} 
