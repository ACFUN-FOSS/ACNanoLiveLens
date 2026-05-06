#ifndef NANOLIVELENS_APPSTATE_HXX
#define NANOLIVELENS_APPSTATE_HXX
#include "RmlUIWin/window_manager.hxx"
#include "rmlui_sys.hxx"

struct AppState
{
	gsl::not_null<RmlUISystem *const> rmluiSys;
    gsl::not_null<RmlUIWin::WinManager *const> winManager;
	gsl::not_null<qjs::Runtime *const> jsRuntime;
	gsl::not_null<qjs::Context *const> jsCtx;
};

void initAppState(AppState &&appState);
AppState &getAppState();

metapp::MetaRepo &getGlobalMetaRepo();

#endif
