#include "Core/assert.hxx"
#include "EmergUI/emerg_ui.hxx"
#include "EmergUI/crash_dlg.hxx"
#include <EatiEssentials/assert.hxx>
#include <boost/stacktrace.hpp>
#include <cstdlib>
#include <format>
#include <sstream>
#include <string_view>

namespace {

[[noreturn]] void showAssertFailThenAbort(const std::string_view source, const char *expr, const char *file, const int line) {
	boost::stacktrace::stacktrace stacktrace;
	std::stringstream ss;
	ss << std::format("{} assert failed: {}\n{}:{}\n\n", source, expr, file, line);
	ss << stacktrace;
	spawnCrashDlg({
		.crashReason = CrashReason::ASSERT_FAIL,
		.msg = ss.str()
	});
	std::abort();
}

}

[[noreturn]] void assertHandler(const char *expr, const char *file, const int line) {
	showAssertFailThenAbort("NanoLiveLens", expr, file, line);
}

[[noreturn]] void eatiEssentialsAssertHandler(const char *expr, const char *file, const int line) {
	showAssertFailThenAbort("EatiEssentials", expr, file, line);
}
