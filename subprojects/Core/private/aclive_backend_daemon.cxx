#include "Core/aclive_backend_daemon.hxx"
#include "Core/assets.hxx"
#include "Core/sound.hxx"

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
		("acbackend-"s + acliveBackendOsPostfix + "-" + acliveBackendArchPostfix + ".exe");

}

} // namespace

AcliveBackendDaemon::CannotFindBackendExe::CannotFindBackendExe(std::string_view exePath)
	: ExcpToShowToUser{
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
	using Lock = std::scoped_lock<std::mutex>;
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
		{
			std::scoped_lock lock{ mutex };
			startChild(lock);
		}

		monitorThread.emplace([this](std::stop_token stopToken) {
			monitorLoop(stopToken);
		});
	}

	void setCrashLimitExceededHandler(CrashLimitExceededHandler handler) {
		std::scoped_lock lock{ mutex };
		crashLimitExceededHandler = std::move(handler);
	}

	void requestStop() {
		if (monitorThread && monitorThread->joinable()) {
			monitorThread->request_stop();
			monitorThread->join();
		}
		monitorThread.reset();

		std::scoped_lock lock{ mutex };
		child.reset();
	}

	void monitorLoop(std::stop_token stopToken) {
		while (!stopToken.stop_requested()) {
			std::scoped_lock lock{ mutex };
			if (!child)
				return;

			if (stopToken.stop_requested()) {
				child->terminate();
				return;
			}

			if (child->running())
				continue;

			bool giveup = handleChildExit(lock);

			if (!giveup)
				startChild(lock);
			else
				return;
		}
	}


	bool handleChildExit(const Lock &lock) {
		std::println("[AcliveBackendDaemon] 后端已崩溃");
		playSound(Sound::EXTERNAL_SERVICE_CRASH);
		{
			//std::scoped_lock lock{ mutex };
			++crashCount;
			if (crashCount > kCrashLimit) {
				if (!crashLimitNotified) {
					crashLimitNotified = true;
					mainThreadExecutor->post([handler = std::move(crashLimitExceededHandler)]() mutable {
						handler();
					});
					return true;
				}
			}

			return false;
		}
	}

	void startChild(const Lock &lock) {
		std::println("[AcliveBackendDaemon] 启动新的后端");
		const auto backendExePath = getBackendExecutablePath();
		if (!stdf::exists(backendExePath)) {
			throw CannotFindBackendExe{ backendExePath.string() };
		}

		auto newChild = bp::child{ backendExePath.string() };

		//std::scoped_lock lock{ mutex };
		child.emplace(std::move(newChild));
		std::this_thread::sleep_for(500ms);
	}
};

AcliveBackendDaemon::AcliveBackendDaemon(Rc<accoro::executor> mainThreadExecutor)
	: state_{ newBox(State{ std::move(mainThreadExecutor) }) } {
	state_->startMonitorLoop();
}

AcliveBackendDaemon::~AcliveBackendDaemon() {
	std::println("[AcliveBackendDaemon] DECONS");
	if (state_) {
		state_->requestStop();
	}
}

AcliveBackendDaemon::AcliveBackendDaemon(AcliveBackendDaemon &&other) noexcept = default;

AcliveBackendDaemon &AcliveBackendDaemon::operator=(AcliveBackendDaemon &&other) noexcept = default;

void AcliveBackendDaemon::onCrashLimitExceeded(CrashLimitExceededHandler handler) {
	state_->setCrashLimitExceededHandler(std::move(handler));
}
