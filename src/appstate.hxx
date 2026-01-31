#ifndef NANOLIVELENS_APPSTATE_HXX
#define NANOLIVELENS_APPSTATE_HXX
#include <RmlUIWin/window_manager.hxx>

struct AppState
{
    RmlUIWin::WinManager winManager;
};

AppState &getAppState();

#endif