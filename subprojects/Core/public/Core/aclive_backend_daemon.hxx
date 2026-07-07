#ifndef NANOLIVELENS_CORE_ACLIVE_BACKEND_DAEMON_HXX
#define NANOLIVELENS_CORE_ACLIVE_BACKEND_DAEMON_HXX
#include "Core/excp_to_show_to_user.hxx"

class AcliveBackendDaemon
{
public:
	struct CannotFindBackendExe : public ExcpToShowToUser
	{
		CannotFindBackendExe(std::string_view exePath);
	};

	using CrashLimitExceededHandler = std::function<void()>;

	explicit AcliveBackendDaemon(
		ESSM::Rc<accoro::executor> mainThreadExecutor
	);
	~AcliveBackendDaemon();

	AcliveBackendDaemon(AcliveBackendDaemon &&other) noexcept;
	AcliveBackendDaemon &operator=(AcliveBackendDaemon &&other) noexcept;

	AcliveBackendDaemon(const AcliveBackendDaemon &) = delete;
	AcliveBackendDaemon &operator=(const AcliveBackendDaemon &) = delete;

	void onCrashLimitExceeded(CrashLimitExceededHandler handler);

private:
	struct State;
	ESSM::Box<State> state_;
};

#endif
