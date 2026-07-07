#ifndef NANOLIVELENS_APPSTATE_HXX
#define NANOLIVELENS_APPSTATE_HXX
#include "RmlUIWin/window_manager.hxx"
#include "rmlui_sys.hxx"

namespace App {

struct State
{
	gsl::not_null<RmlUISystem *const> rmluiSys;
    gsl::not_null<RmlUIWin::WinManager *const> winManager;

	gsl::not_null<coro::runtime *const> coroRuntime;
	ESSM::Rc<coro::executor> mainThreadExecutor;
};

void initState(State &&state);
State &getState();

metapp::MetaRepo &getGlobalMetaRepo();

void die(int code);

}

#endif