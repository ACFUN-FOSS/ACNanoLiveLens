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

	concurrencpp::result<LoginResp> loginAsync(const LoginReq& req);
	concurrencpp::result<QrCodeLoginResp> qrCodeLoginAsync(const QrCodeLoginReq& req);
	concurrencpp::result<SetTokenResp> setTokenAsync(const SetTokenReq& req);
	concurrencpp::result<GetDanmakuResp> getDanmakuAsync(const GetDanmakuReq& req);
	concurrencpp::result<StopGetDanmakuResp> stopGetDanmakuAsync(const StopGetDanmakuReq& req);

	void setDanmakuCallback(std::function<void(const DanmakuData&)> callback);
	void setSignalCallback(std::function<void(const SignalData&)> callback);

private:
	void startHeartbeat();
	void stopHeartbeat();
	void onMessageReceived(const std::string& message);
	void handleResponse(const std::string& message);
	void handleDanmakuOrSignal(const std::string& message);
	void receiveLoop();

	template <typename Req, typename Resp>
	concurrencpp::result<Resp> sendRequestAsync(const Req& req);

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

template <typename Req, typename Resp>
concurrencpp::result<Resp> AcliveClient::sendRequestAsync(const Req& req)
{
	co_await concurrencpp::resume_on(m_workerExecutor);

	std::string requestID = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());

	RequestWrapper<Req> wrapper{
		.type = Req::type,
		.requestID = requestID,
		.data = req
	};

	std::string requestStr = rfl::json::write(wrapper);

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
	std::string responseStr = co_await promise.get_result();

	auto result = rfl::json::read<ResponseWrapper<typename Resp::data_type>>(responseStr);
	if (!result)
	{
		throw std::runtime_error("Failed to parse response: " + result.error().what());
	}

	auto& respWrapper = result.value();
	Resp resp;
	resp.meta.requestID = respWrapper.requestID;
	resp.meta.result = respWrapper.result;
	resp.meta.error = respWrapper.error;
	resp.data = respWrapper.data;

	co_await concurrencpp::resume_on(m_mainExecutor);
	co_return resp;
}

}

#endif
