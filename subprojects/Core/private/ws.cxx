#include "Core/ws.hxx"

using namespace std::chrono_literals;
using namespace Essentials::Memory;


namespace {

constexpr auto heartbeatTimeout = std::chrono::seconds{ 3 };

} // namespace

struct Ws::State
{
	struct ReconnectState
	{
		std::optional<std::chrono::system_clock::time_point> disconnectAt;
		std::size_t attemptCount = 0;
		std::chrono::system_clock::time_point nextAttemptAt{};
		std::optional<accoro::result<void>> result;
		std::jthread thread;
		std::vector<ReconnectHandler> handlers;
	};

	// Immutable configuration.
	std::string url;
	std::string eventFieldName;
	std::chrono::seconds heartbeatInterval;
	HeartbeatGenerator heartbeatGen;

	// Control plane: protected by controlMutex.
	mutable std::mutex controlMutex;
	ConnectionState connectionState = ConnectionState::Disconnected;
	std::shared_ptr<httplib::ws::WebSocketClient> ws;
	std::unordered_map<std::string, Callback> eventCallbacks;
	std::unordered_map<std::string, Callback> onceCallbacks;
	std::jthread receiveThread;
	std::jthread heartbeatThread;
	DisconnectReason disconnectReason = DisconnectReason::None;
	std::mutex connectMutex;
	bool connectInProgress = false;
	std::chrono::system_clock::time_point disconnectedAt{};
	std::chrono::system_clock::time_point lastHeartbeatSentAt{};
	std::chrono::system_clock::time_point lastMessageReceivedAt{};
	accoro::runtime *runtime = nullptr;
	ESSM::Rc<accoro::executor> mainThreadExecutor;
	ReconnectState reconnect;

	// Message queue: protected separately because it is consumed on the main thread
	// and produced by the receive thread.
	std::deque<WsData> messageQueue;
	std::mutex messageQueueMutex;

	// Heartbeat thread coordination.
	std::condition_variable heartbeatCv;
	std::mutex heartbeatMutex;

	State(
		std::string url,
		std::string eventFieldName,
		std::chrono::seconds heartbeatInterval,
		HeartbeatGenerator heartbeatGen
	)
		: url{ std::move(url) }
		, eventFieldName{ std::move(eventFieldName) }
		, heartbeatInterval{ heartbeatInterval }
		, heartbeatGen{ std::move(heartbeatGen) } {
	}

	void receiveLoop(std::shared_ptr<httplib::ws::WebSocketClient> ws);
	void heartbeatLoop(std::stop_token stopToken, std::shared_ptr<httplib::ws::WebSocketClient> ws);
	void connectBlocking();
	void clearDisconnectState() noexcept;
	void enqueueMessage(std::string_view message);
	void markDisconnected(Ws::DisconnectReason reason);
	[[nodiscard]] bool isConnectedNoLock() const noexcept;
	void resetReconnectState();
	void noteDisconnect(std::chrono::system_clock::time_point disconnectedAt);
	void notifyReconnectAttempt() const;
};

Ws::Ws(
	std::string_view url,
	std::string_view eventFieldName,
	std::chrono::seconds heartbeatInterval,
	HeartbeatGenerator heartbeatGen
)
	: m_state{ std::make_shared<State>(
		std::string{ url },
		std::string{ eventFieldName },
		heartbeatInterval,
		std::move(heartbeatGen)
	) } {}

Ws::~Ws() {
	disconnect();
}

Ws::Ws(Ws&& other) noexcept
	: m_state{ std::move(other.m_state) } {}

Ws& Ws::operator=(Ws&& other) noexcept {
	if (this != &other) {
		disconnect();
		m_state = std::move(other.m_state);
	}
	return *this;
}

void Ws::connect() {
	if (!m_state) {
		return;
	}

	m_state->connectBlocking();
}

accoro::result<void> Ws::connectAsync() {
	accoro::result_promise<void> promise;
	auto result = promise.get_result();
	auto state = m_state;
	if (!state) {
		co_return;
	}

	{
		std::lock_guard lock{ state->connectMutex };
		if (state->connectInProgress) {
			throw std::runtime_error("WebSocket connection attempt is already in progress");
		}
		state->connectInProgress = true;
	}

	state->reconnect.thread = std::jthread(
		[state, promise = std::move(promise)]() mutable {
			try {
				state->connectBlocking();
				{
					std::lock_guard lock{ state->connectMutex };
					state->connectInProgress = false;
				}

				bool connected = false;
				{
					std::lock_guard lock{ state->controlMutex };
					connected = state->connectionState == Ws::ConnectionState::Connected;
				}
				if (!connected) {
					throw std::runtime_error("WebSocket connection attempt was cancelled");
				}

				state->resetReconnectState();
				promise.set_result();
				std::println("Ws::connectAsync: Connected");
			} catch (...) {
				{
					std::lock_guard lock{ state->connectMutex };
					state->connectInProgress = false;
				}
				std::println("Ws::connectAsync: Exception in connectBlocking");
				promise.set_exception(std::current_exception());
				
			}
		}
	);

	std::exception_ptr excp;

	try {
		co_await result;
	} catch (...) {
		excp = std::current_exception();
	}

	auto mainThreadExecutor = [&](){
		std::lock_guard lock{ state->controlMutex };
		return state->mainThreadExecutor;
	}();

	if (mainThreadExecutor) {
		co_await accoro::resume_on(mainThreadExecutor);
	}

	if (excp) {
		std::rethrow_exception(excp);
	}
	co_return;

}

void Ws::disconnect() {
	if (!m_state) {
		return;
	}

	std::shared_ptr<httplib::ws::WebSocketClient> ws;
	std::jthread receiveThread;
	std::jthread heartbeatThread;
	std::jthread reconnectThread;
	bool shouldFinalize = false;

	{
		std::lock_guard lock(m_state->controlMutex);
		if (m_state->connectionState == ConnectionState::Disconnected) {
			m_state->markDisconnected(DisconnectReason::Requested);
			receiveThread = std::move(m_state->receiveThread);
			heartbeatThread = std::move(m_state->heartbeatThread);
			reconnectThread = std::move(m_state->reconnect.thread);
			shouldFinalize = true;

		}
		else if (m_state->connectionState == ConnectionState::Connecting) {
			m_state->markDisconnected(DisconnectReason::Requested);
			m_state->connectionState = ConnectionState::Disconnecting;
			receiveThread = std::move(m_state->receiveThread);
			heartbeatThread = std::move(m_state->heartbeatThread);
			reconnectThread = std::move(m_state->reconnect.thread);
			shouldFinalize = true;
		}
		else if (m_state->connectionState == ConnectionState::Disconnecting) {
			receiveThread = std::move(m_state->receiveThread);
			heartbeatThread = std::move(m_state->heartbeatThread);
			reconnectThread = std::move(m_state->reconnect.thread);
			shouldFinalize = true;
		}
		else {
			m_state->connectionState = ConnectionState::Disconnecting;
			m_state->markDisconnected(DisconnectReason::Requested);
			ws = std::move(m_state->ws);
			receiveThread = std::move(m_state->receiveThread);
			heartbeatThread = std::move(m_state->heartbeatThread);
			reconnectThread = std::move(m_state->reconnect.thread);
			shouldFinalize = true;
		}
	}

	if (!shouldFinalize) {
		return;
	}

	m_state->heartbeatCv.notify_all();

	// if (m_state->ws && m_state->ws->is_open()) {
	// 	std::println("Closing Ws Connection");
	// 	m_state->ws->close();
	// 	std::println("Closing Ws Connection: OK");
	// }
	if (heartbeatThread.joinable()) {
		heartbeatThread.request_stop();
		heartbeatThread.join();
	}

	if (receiveThread.joinable()) {
		receiveThread.request_stop();
		// httplib websocket read is a blocking call with no cooperative stop API.
		// Once the thread is blocked inside that call, the only reliable shutdown path
		// available here is to terminate the thread at the platform layer.
		#ifdef WIN32
		HANDLE hNative = receiveThread.native_handle();
		TerminateThread(hNative, 1);
		#endif
		receiveThread.detach();
	}

	if (reconnectThread.joinable()) {
		reconnectThread.request_stop();
		// The reconnect thread can be blocked inside httplib's connect path, which also
		// has no stop_token-aware cancellation hook. We therefore mirror the receive
		// thread shutdown strategy and force termination during teardown.
		#ifdef WIN32
		HANDLE hNative = reconnectThread.native_handle();
		TerminateThread(hNative, 1);
		#endif
		reconnectThread.detach();
	}

	std::lock_guard lock(m_state->controlMutex);
	m_state->ws.reset();
	m_state->connectionState = ConnectionState::Disconnected;
}

[[nodiscard]] bool Ws::isConnected() const noexcept {
	if (!m_state) {
		return false;
	}

	std::lock_guard lock(m_state->controlMutex);
	return m_state->connectionState == ConnectionState::Connected;
}

[[nodiscard]] Ws::ConnectionState Ws::getConnectionState() const noexcept {
	if (!m_state) {
		return ConnectionState::Disconnected;
	}

	std::lock_guard lock(m_state->controlMutex);
	return m_state->connectionState;
}

[[nodiscard]] std::chrono::system_clock::time_point Ws::getLastHeartbeatSentAt() const noexcept {
	if (!m_state) {
		return {};
	}

	std::lock_guard lock(m_state->controlMutex);
	return m_state->lastHeartbeatSentAt;
}

[[nodiscard]] std::chrono::system_clock::time_point Ws::getLastMessageReceivedAt() const noexcept {
	if (!m_state) {
		return {};
	}

	std::lock_guard lock(m_state->controlMutex);
	return m_state->lastMessageReceivedAt;
}

[[nodiscard]] Ws::DisconnectReason Ws::getDisconnectReason() const noexcept {
	if (!m_state) {
		return DisconnectReason::None;
	}

	std::lock_guard lock(m_state->controlMutex);
	return m_state->disconnectReason;
}

[[nodiscard]] std::chrono::system_clock::time_point Ws::getDisconnectedAt() const noexcept {
	if (!m_state) {
		return {};
	}

	std::lock_guard lock(m_state->controlMutex);
	return m_state->disconnectedAt;
}

void Ws::bindRuntime(accoro::runtime &corort, ESSM::Rc<accoro::executor> mainThreadExecutor) {
	m_state->runtime = &corort;
	m_state->mainThreadExecutor = std::move(mainThreadExecutor);
}

void Ws::clearDisconnectState() noexcept {
	if (!m_state) {
		return;
	}

	m_state->clearDisconnectState();
}

void Ws::on(std::string_view event, Callback cb) {
	std::lock_guard lock(m_state->controlMutex);
	m_state->eventCallbacks[std::string(event)] = std::move(cb);
}

void Ws::once(std::string_view event, Callback cb) {
	std::lock_guard lock(m_state->controlMutex);
	m_state->onceCallbacks[std::string(event)] = std::move(cb);
}

void Ws::onReconnectAttempt(ReconnectHandler handler) {
	m_state->reconnect.handlers.push_back(std::move(handler));
}

void Ws::send(nlohmann::json data) {
	std::shared_ptr<httplib::ws::WebSocketClient> ws;
	{
		std::lock_guard lock(m_state->controlMutex);
		if (m_state->connectionState != ConnectionState::Connected) {
			return;
		}
		ws = m_state->ws;
	}

	if (ws && ws->is_open()) {
		ws->send(data.dump());
	}
}

void Ws::sendText(std::string_view data) {
	std::shared_ptr<httplib::ws::WebSocketClient> ws;
	{
		std::lock_guard lock(m_state->controlMutex);
		if (m_state->connectionState != ConnectionState::Connected) {
			return;
		}
		ws = m_state->ws;
	}

	if (ws && ws->is_open()) {
		ws->send(std::string{ data });
	}
}


void Ws::execCb() {
	std::deque<WsData> localQueue;
	{
		std::lock_guard<std::mutex> lock(m_state->messageQueueMutex);
		std::swap(localQueue, m_state->messageQueue);
	}

	for (const auto& msg : localQueue) {
		std::optional<Callback> onceCb;
		std::optional<Callback> eventCb;
		{
			std::lock_guard lock(m_state->controlMutex);
			auto onceIt = m_state->onceCallbacks.find(msg.event);
			if (onceIt != m_state->onceCallbacks.end()) {
				onceCb = onceIt->second;
				m_state->onceCallbacks.erase(onceIt);
			}

			auto it = m_state->eventCallbacks.find(msg.event);
			if (it != m_state->eventCallbacks.end()) {
				eventCb = it->second;
			}
		}

		if (onceCb) {
			(*onceCb)(msg);
		}

		if (eventCb) {
			(*eventCb)(msg);
		}
	}

	const auto connectionState = getConnectionState();
	if (connectionState == ConnectionState::Connecting ||
		connectionState == ConnectionState::Disconnecting) {
		return;
	}

	if (connectionState == ConnectionState::Disconnected) {
		const auto reason = getDisconnectReason();
		if (reason != DisconnectReason::Unexpected) {
			return;
		}

		const auto disconnectedAt = getDisconnectedAt();
		if (!m_state->reconnect.disconnectAt) {
			m_state->noteDisconnect(disconnectedAt);
		}

		const auto now = std::chrono::system_clock::now();
		if (now < m_state->reconnect.nextAttemptAt) {
			return;
		}

		if (m_state->reconnect.result) {
			switch (m_state->reconnect.result->status()) {
			case accoro::result_status::idle:
				return;
			case accoro::result_status::value:
				m_state->reconnect.result->get();
				m_state->reconnect.result.reset();
				m_state->reconnect.nextAttemptAt = {};
				return;
			case accoro::result_status::exception:
				try {
					m_state->reconnect.result->get();
				} catch (...) {
					m_state->reconnect.nextAttemptAt = now + 1s;
				}
				m_state->reconnect.result.reset();
				return;
			}
		}

		if (!m_state->runtime || !m_state->mainThreadExecutor) {
			m_state->reconnect.nextAttemptAt = now + 1s;
			return;
		}

		++m_state->reconnect.attemptCount;
		m_state->notifyReconnectAttempt();
		m_state->reconnect.result.emplace(connectAsync());
		return;
	}

	if (connectionState != ConnectionState::Connected) {
		return;
	}

	const auto lastHeartbeatSentAt = getLastHeartbeatSentAt();
	if (lastHeartbeatSentAt == std::chrono::system_clock::time_point{}) {
		return;
	}

	const auto lastMessageReceivedAt = getLastMessageReceivedAt();
	if (lastMessageReceivedAt >= lastHeartbeatSentAt) {
		m_state->resetReconnectState();
		return;
	}

	const auto now = std::chrono::system_clock::now();
	if (now - lastHeartbeatSentAt <= heartbeatTimeout) {
		return;
	}

	if (!m_state->reconnect.disconnectAt) {
		m_state->noteDisconnect(now);
	}

	disconnect();
	m_state->reconnect.nextAttemptAt = now;
}

void Ws::State::receiveLoop(std::shared_ptr<httplib::ws::WebSocketClient> ws) {
	std::string message;
	while (ws && isConnectedNoLock()) {
		auto result = ws->read(message);
		if (result == httplib::ws::ReadResult::Fail) {
			{
				std::lock_guard lock(controlMutex);
				markDisconnected(Ws::DisconnectReason::Unexpected);
				connectionState = ConnectionState::Disconnected;
			}
			heartbeatCv.notify_all();
			break;
		}
		else if (result == httplib::ws::ReadResult::Text) {
			{
				std::lock_guard lock(controlMutex);
				lastMessageReceivedAt = std::chrono::system_clock::now();
			}
			enqueueMessage(message);
		}
	}
}

void Ws::State::connectBlocking() {
	std::shared_ptr<httplib::ws::WebSocketClient> newWs;
	{
		std::lock_guard lock(controlMutex);
		if (connectionState != ConnectionState::Disconnected) {
			return;
		}
		disconnectReason = DisconnectReason::None;
		disconnectedAt = {};
		connectionState = ConnectionState::Connecting;
	}

	newWs = std::make_shared<httplib::ws::WebSocketClient>(url);

	if (!newWs->is_valid()) {
		std::lock_guard lock(controlMutex);
		markDisconnected(DisconnectReason::Unexpected);
		connectionState = ConnectionState::Disconnected;
		throw std::runtime_error("Invalid WebSocket URL: " + url);
	}

	if (!newWs->connect()) {
		std::lock_guard lock(controlMutex);
		markDisconnected(DisconnectReason::Unexpected);
		connectionState = ConnectionState::Disconnected;
		throw std::runtime_error("Failed to connect to WebSocket server: " + url);
	}

	{
		std::lock_guard lock(controlMutex);
		if (connectionState != ConnectionState::Connecting ||
			disconnectReason == DisconnectReason::Requested) {
			connectionState = ConnectionState::Disconnected;
			return;
		}

		ws = newWs;
		clearDisconnectState();
	}

	std::jthread newReceiveThread{ [state = this, newWs](std::stop_token) {
		state->receiveLoop(newWs);
	} };

	std::optional<std::jthread> newHeartbeatThread;
	if (heartbeatInterval.count() > 0 && heartbeatGen) {
		newHeartbeatThread.emplace([state = this, newWs](std::stop_token stopToken) {
			state->heartbeatLoop(stopToken, newWs);
		});
	}

	{
		std::lock_guard lock(controlMutex);
		if (connectionState != ConnectionState::Connecting ||
			disconnectReason == DisconnectReason::Requested) {
			connectionState = ConnectionState::Disconnected;
			return;
		}

		receiveThread = std::move(newReceiveThread);
		if (newHeartbeatThread) {
			heartbeatThread = std::move(*newHeartbeatThread);
		}
		connectionState = ConnectionState::Connected;
	}
}

void Ws::State::markDisconnected(Ws::DisconnectReason reason) {
	disconnectReason = reason;
	disconnectedAt = std::chrono::system_clock::now();
}

void Ws::State::clearDisconnectState() noexcept {
	disconnectReason = DisconnectReason::None;
	disconnectedAt = {};
}

[[nodiscard]] bool Ws::State::isConnectedNoLock() const noexcept {
	std::lock_guard lock(controlMutex);
	return connectionState == ConnectionState::Connected;
}

void Ws::State::resetReconnectState() {
	reconnect.disconnectAt.reset();
	reconnect.attemptCount = 0;
	reconnect.nextAttemptAt = {};
	reconnect.result.reset();
}

void Ws::State::noteDisconnect(std::chrono::system_clock::time_point disconnectedAt) {
	if (reconnect.disconnectAt) {
		return;
	}

	reconnect.disconnectAt = disconnectedAt;
	reconnect.attemptCount = 0;
	reconnect.nextAttemptAt = disconnectedAt;
}

void Ws::State::notifyReconnectAttempt() const {
	if (!reconnect.disconnectAt) {
		return;
	}

	for (const auto &handler : reconnect.handlers) {
		handler(*reconnect.disconnectAt, reconnect.attemptCount);
	}
}

void Ws::State::heartbeatLoop(std::stop_token stopToken, std::shared_ptr<httplib::ws::WebSocketClient> ws) {
	std::unique_lock lock(heartbeatMutex);
	while (isConnectedNoLock()) {
		const bool shouldStop = heartbeatCv.wait_for(lock, heartbeatInterval, [this, stopToken] {
			return stopToken.stop_requested() || !isConnectedNoLock();
		});
		if (shouldStop || !ws || !ws->is_open()) {
			break;
		}

		lock.unlock();
		auto packet = heartbeatGen();
		{
			std::lock_guard controlLock(controlMutex);
			lastHeartbeatSentAt = std::chrono::system_clock::now();
		}
		ws->send(packet.dump());
		lock.lock();
	}
}

void Ws::State::enqueueMessage(std::string_view message) {
	auto json = nlohmann::json::parse(message);

	WsData data;
	if (json.contains(eventFieldName)) {
		auto& value = json[eventFieldName];
		if (value.is_string()) {
			data.event = value.get<std::string>();
		} else if (value.is_number_integer()) {
			data.event = std::to_string(value.get<int64_t>());
		}
		else if (value.is_boolean()) {
			data.event = value.get<bool>() ? "true" : "false";
		} else {
			throw std::runtime_error{
				"Invalid event value type: " + value.dump()
			};
		}
	}
	data.payload = message;

	{
		std::lock_guard<std::mutex> lock(messageQueueMutex);
		messageQueue.push_back(std::move(data));
	}
}
