#include "Core/aclive_backend_daemon.hxx"
#include "Core/assets.hxx"

using namespace Essentials::Memory;
using namespace Essentials::ContainerAndView;
using namespace std::literals;
namespace bp = boost::process::v1;

namespace {

constexpr std::size_t kCrashLimit = 3;

stdf::path getBackendExecutablePath() {
	auto acliveBackendOsPostfix = []() {
#if defined(NLLENS_PLATFORM_OS_WIN32)
		return "win";
#elif defined(NLLENS_PLATFORM_OS_ANYLINUX)
		return "linux";
#elif defined(NLLENS_PLATFORM_OS_MACOS)
		return "mac";
#else
#error "Unsupported platform."
#endif
	}();

	auto acliveBackendArchPostfix = []() {
#if defined(NLLENS_PLATFORM_ARCH_AMD64)
		return "x64";
#elif defined(NLLENS_PLATFORM_ARCH_X86)
		return "x86";
#elif defined(NLLENS_PLATFORM_ARCH_LOONGARCH64)
		return "loongarch64";
#elif defined(NLLENS_PLATFORM_ARCH_ARM64)
		return "arm64";
#elif defined(NLLENS_PLATFORM_ARCH_POWERPC)
		return "powerpc";
#endif
	}();

	return getAssetsDir() / "blob" /
		("acbackensd-"s + acliveBackendOsPostfix + "-" + acliveBackendArchPostfix + ".exe");

}

} // namespace

AcliveBackendDaemon::CannotFindBackendExe::CannotFindBackendExe(std::string_view exePath)
	: std::runtime_error{
		 std::format(
			"無法找到當前平臺合適的 AcFun 直播通用後端。其可能遺失或未安装。\n"
			"后端路径：{}；\n操作系统：{}；硬件架构：{}",
			exePath,
			NLLENS_PLATFORM_OS,
			NLLENS_PLATFORM_ARCH
		)
	} { }


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
			throw CannotFindBackendExe{ backendExePath.string() };
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
