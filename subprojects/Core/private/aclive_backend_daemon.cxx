#include "Core/aclive_backend_daemon.hxx"

#include <boost/dll.hpp>
#include <boost/process/v1.hpp>

using namespace Essentials::Memory;
namespace bp = boost::process::v1;

namespace {

constexpr std::size_t kCrashLimit = 3;

[[nodiscard]] stdf::path getBackendExecutablePath() {
	const auto execDir = stdf::path{ boost::dll::program_location().parent_path().string() };
#ifdef WIN32
	return execDir / "acbackend-win-x64.exe";
#else
	return execDir / "acbackend";
#endif
}

} // namespace

struct AcliveBackendDaemon::State
{
	ESSM::Rc<accoro::executor> mainThreadExecutor;
	std::optional<bp::child> child;
	std::optional<std::jthread> monitorThread;
	std::mutex mutex;
	CrashLimitExceededHandler crashLimitExceededHandler;
	std::size_t crashCount = 0;
	bool crashLimitNotified = false;

	explicit State(Rc<accoro::executor> executor)
		: mainThreadExecutor{ std::move(executor) } {
	}

	void startMonitorLoop() {
		startChild();

		monitorThread.emplace([this](std::stop_token stopToken) {
			monitorLoop(stopToken);
		});
	}

	void stopMonitorLoop() {
		if (monitorThread && monitorThread->joinable()) {
			monitorThread->request_stop();
			requestStop();
			monitorThread->join();
		}
		monitorThread.reset();

		std::lock_guard lock{ mutex };
		child.reset();
	}

	void setCrashLimitExceededHandler(CrashLimitExceededHandler handler) {
		std::lock_guard lock{ mutex };
		crashLimitExceededHandler = std::move(handler);
	}

	void requestStop() {
		std::lock_guard lock{ mutex };
		if (child && child->running()) {
			child->terminate();
		}
	}

	void monitorLoop(std::stop_token stopToken) {
		while (!stopToken.stop_requested()) {
			auto currentChild = takeChild();
			if (!currentChild)
				return;

			currentChild->wait();
			if (stopToken.stop_requested())
				return;

			if (!handleChildExit())
				return;

			startChild();
		}
	}

	[[nodiscard]] std::optional<bp::child> takeChild() {
		std::lock_guard lock{ mutex };
		if (!child) {
			return { };
		}

		auto currentChild = std::move(child);
		child.reset();
		return currentChild;
	}

	[[nodiscard]] bool handleChildExit() {
		CrashLimitExceededHandler handlerToCall;

		{
			std::lock_guard lock{ mutex };
			++crashCount;
			if (crashCount > kCrashLimit) {
				if (!crashLimitNotified) {
					crashLimitNotified = true;
					handlerToCall = crashLimitExceededHandler;
				}
			} else {
				return true;
			}
		}

		if (handlerToCall) {
			mainThreadExecutor->post([handler = std::move(handlerToCall)]() mutable {
				handler();
			});
		}

		return false;
	}

	void startChild() {
		const auto backendExePath = getBackendExecutablePath();
		if (!stdf::exists(backendExePath)) {
			throw std::runtime_error(std::format(
				"Aclive backend executable not found: {}",
				backendExePath.string()
			));
		}

		auto newChild = bp::child{ backendExePath.string() };

		std::lock_guard lock{ mutex };
		child.emplace(std::move(newChild));
	}
};

AcliveBackendDaemon::AcliveBackendDaemon(Rc<accoro::executor> mainThreadExecutor)
	: state_{ newBox(State{ std::move(mainThreadExecutor) }) } {
	state_->startMonitorLoop();
}

AcliveBackendDaemon::~AcliveBackendDaemon() {
	if (state_) {
		state_->stopMonitorLoop();
	}
}

AcliveBackendDaemon::AcliveBackendDaemon(AcliveBackendDaemon &&other) noexcept = default;

AcliveBackendDaemon &AcliveBackendDaemon::operator=(AcliveBackendDaemon &&other) noexcept = default;

void AcliveBackendDaemon::onCrashLimitExceeded(CrashLimitExceededHandler handler) {
	state_->setCrashLimitExceededHandler(std::move(handler));
}
