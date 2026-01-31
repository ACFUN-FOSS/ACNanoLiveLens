#include "EmergUI/emerg_ui.hxx"
#include "EmergUI/crash_dlg.hxx"

int main() {
    #if 0
    spawnCrashDlg({
        CrashReason::ILLEGAL_OP,
        "Test message"
    });
    #endif

    #if 1
    spawnCrashDlg({
        CrashReason::UNHANDLED_EXCP,
        "Test message"
    });
    #endif
}