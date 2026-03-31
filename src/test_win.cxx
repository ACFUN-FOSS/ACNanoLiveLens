#include "test_win.hxx"
#include "appstate.hxx"
#include "assets.hxx"
#include "rmluipp.hxx"
#include "RmlUIWin/window_manager.hxx"
#include "utils.hxx"
#include "sound/sound.hxx"

using namespace RmlUIWin;
using namespace Essentials::Memory;
using namespace Essentials::IO;

class TestWin::Impl
{
public:
    Impl()
		: uiState_{ [] -> UIState {
			auto mainWin = newBox(UiWin{ "main", {}, getAssetsDir() / "main.rml", true });
			auto &mainWinRootEle = UNWRAP(mainWin->getContext().GetRootElement());
			SimpleEventListenerManager mainWinRootEleEventMan{ mainWinRootEle };
			auto &win = getAppState().winManager.transferWin(std::move(mainWin));
			return { &win, std::move(mainWinRootEleEventMan) };
		}() } {}

    ~Impl() = default;
	Impl(const Impl &) = delete;
	Impl &operator=(const Impl &) = delete;
	Impl &operator=(Impl &&) = delete;
	
private:
	struct UIState
	{
		gsl::not_null<UiWin *> mainWin_;
		SimpleEventListenerManager mainWinRootEleEventMan_;
	} uiState_;
};

TestWin::TestWin()
    : pImpl(std::make_unique<Impl>())
{
}

TestWin::~TestWin() = default;
