#include "Core/ws.hxx"

struct Ws::State
{
	std::string url;
	std::string eventFieldName;
	std::chrono::seconds heartbeatInterval;
	HeartbeatGenerator heartbeatGen;
	std::unique_ptr<httplib::ws::WebSocketClient> ws;

	std::unordered_map<std::string, Callback> eventCallbacks;
	std::unordered_map<std::string, Callback> onceCallbacks;

	std::deque<WsData> messageQueue;
	std::mutex messageQueueMutex;

	std::jthread receiveThread;
	std::jthread heartbeatThread;
	std::atomic<bool> isConnected{false};

	void receiveLoop();
	void heartbeatLoop();
	void enqueueMessage(std::string_view message);
};

Ws::Ws(
	std::string_view url,
	std::string_view eventFieldName,
	std::chrono::seconds heartbeatInterval,
	HeartbeatGenerator heartbeatGen
)
	: m_state{ newBox(State{
		std::string{ url },
		std::string{ eventFieldName },
		heartbeatInterval,
		std::move(heartbeatGen)
	}) } {}

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
	if (m_state->isConnected) {
		return;
	}

	m_state->ws = std::make_unique<httplib::ws::WebSocketClient>(m_state->url);

	if (!m_state->ws->is_valid()) {
		throw std::runtime_error("Invalid WebSocket URL: " + m_state->url);
	}

	if (!m_state->ws->connect()) {
		throw std::runtime_error("Failed to connect to WebSocket server: " + m_state->url);
	}

	m_state->isConnected = true;

	m_state->receiveThread = std::jthread([state = m_state.get()](std::stop_token) {
		state->receiveLoop();
	});

	if (m_state->heartbeatInterval.count() > 0 && m_state->heartbeatGen) {
		m_state->heartbeatThread = std::jthread([state = m_state.get()](std::stop_token stopToken) {
			state->heartbeatLoop();
		});
	}
}

void Ws::disconnect() {
	if (!m_state || !m_state->isConnected) {
		return;
	}

	m_state->isConnected = false;

	if (m_state->heartbeatThread.joinable()) {
		m_state->heartbeatThread.request_stop();
		m_state->heartbeatThread.join();
	}

	if (m_state->ws && m_state->ws->is_open()) {
		m_state->ws->close();
	}

	if (m_state->receiveThread.joinable()) {
		m_state->receiveThread.request_stop();
		m_state->receiveThread.join();
	}

	m_state->ws.reset();
}

void Ws::on(std::string_view event, Callback cb) {
	m_state->eventCallbacks[std::string(event)] = std::move(cb);
}

void Ws::once(std::string_view event, Callback cb) {
	m_state->onceCallbacks[std::string(event)] = std::move(cb);
}

void Ws::send(std::string_view data) {
	if (m_state->isConnected && m_state->ws && m_state->ws->is_open()) {
		m_state->ws->send(std::string(data));
	}
}

void Ws::execCb() {
	std::deque<WsData> localQueue;
	{
		std::lock_guard<std::mutex> lock(m_state->messageQueueMutex);
		std::swap(localQueue, m_state->messageQueue);
	}

	for (const auto& msg : localQueue) {
		auto onceIt = m_state->onceCallbacks.find(msg.event);
		if (onceIt != m_state->onceCallbacks.end()) {
			onceIt->second(msg);
			m_state->onceCallbacks.erase(onceIt);
		}

		auto it = m_state->eventCallbacks.find(msg.event);
		if (it != m_state->eventCallbacks.end()) {
			it->second(msg);
		}
	}
}

void Ws::State::receiveLoop() {
	std::string message;
	while (isConnected && ws) {
		auto result = ws->read(message);
		if (result == httplib::ws::ReadResult::Fail) {
			isConnected = false;
			break;
		}
		else if (result == httplib::ws::ReadResult::Text) {
			enqueueMessage(message);
		}
	}
}

void Ws::State::heartbeatLoop() {
	while (isConnected) {
		std::this_thread::sleep_for(heartbeatInterval);
		if (!isConnected || !ws || !ws->is_open()) {
			break;
		}
		auto packet = heartbeatGen();
		ws->send(packet.dump());
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
