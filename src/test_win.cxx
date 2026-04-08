#include "test_win.hxx"
#include "appstate.hxx"
#include "assets.hxx"
#include "rmluipp.hxx"
#include "RmlUIWin/window_manager.hxx"
// #include "utils.hxx"
// #include "sound/sound.hxx"

using namespace RmlUIWin;
using namespace Essentials::Memory;
using namespace Essentials::IO;

class TestWin::Impl
{
public:
	Impl()
		: uiState_{ [] -> UIState {
			auto mainWin = newBox(UiWin{ "main", {}, getAssetsDir() / "test_win.rml", true });
			auto &mainWinRootEle = mainWin->getRootElement();
			SimpleEventListenerManager mainWinRootEleEventMan{ mainWinRootEle };
			auto &win = getAppState().winManager->transferWin(std::move(mainWin));
			return { &win, std::move(mainWinRootEleEventMan) };
		}() } {
		
		// uiState_.mainWinRootEleEventMan_.on("btn", "click", [this](Rml::Event &e) {
		// 	std::print("btn click\n");
		// });

		uiState_.mainWin_->setReloadCb([this]{
			auto &mainWinRootEle = uiState_.mainWin_->getRootElement();
			uiState_.mainWinRootEleEventMan_.reBind(mainWinRootEle);
		});
	
	}

    ~Impl() = default;
	Impl(Impl &&) = delete;
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
    : pImpl{ stdx::pimpl::make_unique<Impl>() }
{
}

TestWin::~TestWin() = default;
