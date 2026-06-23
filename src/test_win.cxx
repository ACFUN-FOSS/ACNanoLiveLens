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
			auto &win = getAppState().winManager->createWindow("main", {}, getAssetsDir() / "test_win.rml", true);
			auto &mainWinRootEle = win.getRootElement();
			SimpleEventListenerManager mainWinRootEleEventMan{ mainWinRootEle };
			return { &win, std::move(mainWinRootEleEventMan) };
		}() } {
		
		// uiState_.mainWinRootEleEventMan_.on("btn", "click", [this](Rml::Event &e) {
		// 	std::print("btn click\n");
		// });

		uiState_.mainWin_->setDocumentChangedCb([this]{
			uiState_.mainWinRootEleEventMan_.reBind(uiState_.mainWin_->getRootElement());

			// Don't do below: will cause crash: you should't unbind / clear event handler during reload.
			//uiState_.mainWinRootEleEventMan_.clear();

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
