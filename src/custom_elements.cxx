#include "custom_elements.hxx"
#include "rmlui_sys.hxx"
#include "winframe.hxx"

void registerCustomElements(RmlUISystem &rmlui) {
    WinFrame::reg(rmlui);
}
