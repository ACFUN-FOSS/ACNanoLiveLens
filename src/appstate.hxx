#ifndef NANOLIVELENS_APPSTATE_HXX
#define NANOLIVELENS_APPSTATE_HXX
#include "RmlUIWin/window_manager.hxx"
#include "rmlui_sys.hxx"

struct AppState
{
	gsl::not_null<RmlUISystem *const> rmluiSys;
    gsl::not_null<RmlUIWin::WinManager *const> winManager;

	gsl::not_null<coro::runtime *const> coroRuntime;
	ESSM::Rc<coro::executor> mainThreadExecutor;
};

void initAppState(AppState &&appState);
AppState &getAppState();

metapp::MetaRepo &getGlobalMetaRepo();

#endif
