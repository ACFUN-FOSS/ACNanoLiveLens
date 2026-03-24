#ifndef NANOLIVELENS_ACLIVE_CLIENT_HXX
#define NANOLIVELENS_ACLIVE_CLIENT_HXX

#include "aclive_backend.hxx"

namespace ACLive {

class AcliveClient
{
public:
	struct Config
	{
		std::string host;
		int port;
		std::chrono::milliseconds heartbeatInterval;
		std::chrono::milliseconds requestTimeout;

		Config() : host("localhost"), port(15368), 
			heartbeatInterval(std::chrono::milliseconds(5000)), 
			requestTimeout(std::chrono::milliseconds(10000)) {};
	};

	AcliveClient(concurrencpp::runtime& runtime, Config config = Config{});
	~AcliveClient();

	concurrencpp::result<void> connectAsync();
	concurrencpp::result<void> disconnectAsync();

	template <typename Req>
	concurrencpp::result<std::string> sendRequestAsync(Req req, int type);

	template <typename Resp>
	concurrencpp::result<Resp> sendRequestAsync(std::string jsonData);

	concurrencpp::result<LoginResp> loginAsync(LoginReq req);
	concurrencpp::result<QrCodeLoginResp> qrCodeLoginAsync(QrCodeLoginReq req);
	concurrencpp::result<SetTokenResp> setTokenAsync(SetTokenReq req);
	concurrencpp::result<GetDanmakuResp> getDanmakuAsync(GetDanmakuReq req);
	concurrencpp::result<StopGetDanmakuResp> stopGetDanmakuAsync(StopGetDanmakuReq req);

	void setDanmakuCallback(std::function<void(const DanmakuData&)> callback);
	void setSignalCallback(std::function<void(const SignalData&)> callback);

private:
	void startHeartbeat();
	void stopHeartbeat();
	void onMessageReceived(const std::string& message);
	void handleResponse(const std::string& message);
	void handleDanmakuOrSignal(const std::string& message);
	void receiveLoop();

	template <typename T>
	std::string serializeToJson(const T& obj);

	template <typename T>
	T deserializeFromJson(const std::string& json);

	concurrencpp::runtime& m_runtime;
	std::shared_ptr<concurrencpp::thread_pool_executor> m_workerExecutor;
	std::shared_ptr<concurrencpp::manual_executor> m_mainExecutor;

	Config m_config;
	std::unique_ptr<httplib::ws::WebSocketClient> m_websocket;

	std::unordered_map<std::string, concurrencpp::result_promise<std::string>> m_pendingRequests;
	std::mutex m_pendingRequestsMutex;

	std::function<void(const DanmakuData&)> m_danmakuCallback;
	std::function<void(const SignalData&)> m_signalCallback;

	std::atomic<bool> m_isConnected{false};
	std::atomic<bool> m_shouldStopHeartbeat{false};
	std::jthread m_heartbeatThread;
	std::jthread m_receiveThread;

public:
	AcliveClient(const AcliveClient&) = delete;
	AcliveClient& operator=(const AcliveClient&) = delete;
	AcliveClient(AcliveClient&&) = delete;
	AcliveClient& operator=(AcliveClient&&) = delete;
};

template <typename Req>
concurrencpp::result<std::string> AcliveClient::sendRequestAsync(Req req, int type)
{
	co_await concurrencpp::resume_on(m_workerExecutor);

	std::string requestID = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
	std::string jsonData = serializeToJson(req);

	nlohmann::json requestJson;
	requestJson["type"] = type;
	requestJson["requestID"] = requestID;
	requestJson["data"] = nlohmann::json::parse(jsonData);

	std::string requestStr = requestJson.dump();

	{
		std::lock_guard<std::mutex> lock(m_pendingRequestsMutex);
		m_pendingRequests[requestID] = concurrencpp::result_promise<std::string>{};
	}

	if (m_isConnected && m_websocket && m_websocket->is_open())
	{
		m_websocket->send(requestStr);
	}
	else
	{
		throw std::runtime_error("WebSocket not connected");
	}

	auto& promise = m_pendingRequests[requestID];
	std::string response = co_await promise.get_result();

	co_await concurrencpp::resume_on(m_mainExecutor);
	co_return response;
}

template <typename Resp>
concurrencpp::result<Resp> AcliveClient::sendRequestAsync(std::string jsonData)
{
	auto responseStr = co_await sendRequestAsync(jsonData, Resp::type);

	auto responseJson = nlohmann::json::parse(responseStr);

	Resp resp;
	resp.meta.requestID = responseJson["requestID"];
	resp.meta.result = responseJson["result"];
	if (responseJson.contains("error") && !responseJson["error"].is_null())
	{
		resp.meta.error = responseJson["error"];
	}

	if (responseJson.contains("data") && !responseJson["data"].is_null())
	{
		resp.data = responseJson["data"].template get<decltype(resp.data)>();
	}

	co_return resp;
}

template <typename T>
std::string AcliveClient::serializeToJson(const T& obj)
{
	nlohmann::json j = obj;
	return j.dump();
}

template <typename T>
T AcliveClient::deserializeFromJson(const std::string& json)
{
	nlohmann::json j = nlohmann::json::parse(json);
	return j.get<T>();
}

} 

#endif
