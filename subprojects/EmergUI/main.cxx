#include "EmergUI/crash_dlg.hxx"
#include <rfl/json.hpp>
#include <cxxopts.hpp>
#include <ui.h>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

void crashDlg(const CrashDlgData data);

int main(int argc, char *argv[]) {
#ifdef WIN32
	SetProcessDPIAware();
	system("chcp 65001");
#endif

	try {
		std::cout << "[EmergUI] HI" << std::endl;

		for (int i = 0; i < argc; i++)
			std::cout << "[EmergUI] " << argv[i] << std::endl;

		cxxopts::Options options{ "", "" };
		options.add_options()
			("dlg_crashDlg", "", cxxopts::value<bool>())
			("dlg_data", "", cxxopts::value<std::string>());

		auto result = options.parse(argc, argv);

		uiInitOptions o{ };
		if (auto msg = uiInit(&o)) {
			// 如果初始化失败，至少打印一下
			std::cerr << "[EmergUI] libui init failed: " << msg << std::endl;
			return -2;
		}

		if (result["dlg_crashDlg"].as<bool>()) {
			auto data = rfl::json::read<CrashDlgData>(result["dlg_data"].as<std::string>()).value();
			crashDlg(data);
			return 0;
		} else {
			uiWindow *win = uiNewWindow("死了", 0, 0, 0);
			//uiMsgBoxError(win, "建议远离当前设备。", "");
			auto text = 
				#include "quran.inc"
			std::cerr << text << std::endl;
			uiMsgBoxError(win, "", text);
		}
	} catch (const std::exception &ex) {
		std::cout << "[EmergUI] " << ex.what() << std::endl;
	}

}
