#ifndef NANOLIVELENS_CORE_ACLIVE_BACKEND_CLIENT_HXX
#define NANOLIVELENS_CORE_ACLIVE_BACKEND_CLIENT_HXX

#include "Core/aclive_backend_msg.hxx"
#include "Core/ws.hxx"

class AcliveBackendError : public std::runtime_error
{
public:
	AcliveBackendError(AcliveBackendRespMeta meta, std::string message);

	[[nodiscard]] const AcliveBackendRespMeta &meta() const noexcept;

private:
	AcliveBackendRespMeta meta_;
};

class AcliveBackendClient
{
public:
	using RespHandler = std::function<void(const AnyResp &)>;
	using ReconnectHandler = std::function<void(std::chrono::system_clock::time_point disconnectAt, std::size_t attemptCount)>;

	AcliveBackendClient(
		std::string_view url,
		std::chrono::seconds heartbeatInterval = std::chrono::seconds{3}
	);
	~AcliveBackendClient();

	AcliveBackendClient(AcliveBackendClient &&other) noexcept;
	AcliveBackendClient &operator=(AcliveBackendClient &&other) noexcept;

	AcliveBackendClient(const AcliveBackendClient &) = delete;
	AcliveBackendClient &operator=(const AcliveBackendClient &) = delete;

	void connect();
	accoro::result<void> connectAsync();
	void disconnect();
	[[nodiscard]] bool isConnected() const noexcept;

	void exec();
	void bindRuntime(accoro::runtime &corort, ESSM::Rc<accoro::executor> mainThreadExecutor);

	void onResp(RespHandler handler);
	void onReconnectAttempt(ReconnectHandler handler);
	void requestQrCodeLogin(std::string_view requestID = {});

private:
	struct State;
	ESSM::Box<State> state_;
};

#endif
