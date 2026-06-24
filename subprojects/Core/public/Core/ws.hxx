#ifndef NANOLIVELENS_CORE_WS_HXX
#define NANOLIVELENS_CORE_WS_HXX

struct WsData {
	std::string event;
	std::string payload;
};

class Ws {
public:
	enum class DisconnectReason {
		None,
		Requested,
		Unexpected,
	};

	using Callback = std::function<void(const WsData&)>;
	using HeartbeatGenerator = std::function<nlohmann::json()>;

	Ws(
		std::string_view url,
		std::string_view eventFieldName = "event",
		std::chrono::seconds heartbeatInterval = std::chrono::seconds{0},
		HeartbeatGenerator heartbeatGen = nullptr
	);
	~Ws();

	Ws(Ws&& other) noexcept;
	Ws& operator=(Ws&& other) noexcept;

	Ws(const Ws&) = delete;
	Ws& operator=(const Ws&) = delete;

	void connect();
	accoro::result<void> connectAsync(accoro::runtime &corort, ESSM::Rc<accoro::executor> mainThreadExecutor);
	void disconnect();
	[[nodiscard]] bool isConnected() const noexcept;
	[[nodiscard]] std::chrono::system_clock::time_point getLastHeartbeatSentAt() const noexcept;
	[[nodiscard]] std::chrono::system_clock::time_point getLastMessageReceivedAt() const noexcept;
	[[nodiscard]] DisconnectReason getDisconnectReason() const noexcept;
	[[nodiscard]] std::chrono::system_clock::time_point getDisconnectedAt() const noexcept;
	void clearDisconnectState() noexcept;

	void on(std::string_view event, Callback cb);
	void once(std::string_view event, Callback cb);
	void send(nlohmann::json data);
	void sendText(std::string_view data);

	void execCb();

private:
	struct State;
	ESSM::Box<State> m_state;
};

#endif
