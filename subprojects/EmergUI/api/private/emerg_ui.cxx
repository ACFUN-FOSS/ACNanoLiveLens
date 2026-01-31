#include "EmergUI/emerg_ui.hxx"
#include <iostream>
#include <boost/process/v1.hpp>
#include <rfl/json.hpp>

using namespace std::string_literals;
namespace bp = boost::process::v1;

void spawnCrashDlg(const CrashDlgData data) {
	try {
		bp::child child{
			EMERG_UI_EXE_NAME,
			"--dlg_crashDlg",
			"--dlg_data="s + rfl::json::write(data)
		};
		child.wait();
	} catch (const std::exception &ex) {
		std::cout << "[EmergUIAPI] spawnCrashDlg exception: " << ex.what() << std::endl;
	}
}
