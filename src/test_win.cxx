#include "test_win.hxx"
#include "appstate.hxx"
#include "Core/assets.hxx"
#include "rmluipp.hxx"
#include "RmlUIWin/window_manager.hxx"
// #include "utils.hxx"
// #include "Core/sound.hxx"

using namespace RmlUIWin;
using namespace Essentials::Memory;
using namespace Essentials::IO;

class TestWin::Impl
{
public:
	Impl()
		: uiState_{ *App::getState().winManager, getAssetsDir() / "test_win.rml" } {
		
		// uiState_.mainWinRootEleEventMan_.on("btn", "click", [this](Rml::Event &e) {
		// 	std::print("btn click\n");
		// });

		uiState_.mainWin_.setDocumentChangedCb([this]{
			uiState_.mainWinRootEleEventMan_.reBind(uiState_.mainWin_.getRootElement());

			// Don't do below: will cause crash: you should't unbind / clear event handler during reload.
			//uiState_.mainWinRootEleEventMan_.clear();

		});

		uiState_.mainWin_.show();
	
	}

    ~Impl() = default;
	Impl(Impl &&) = delete;
	Impl(const Impl &) = delete;
	Impl &operator=(const Impl &) = delete;
	Impl &operator=(Impl &&) = delete;
	
private:
	struct UIState
	{
		UiWin mainWin_;
		SimpleEventListenerManager mainWinRootEleEventMan_;

		UIState(WinManager &winManager, std::filesystem::path documentPath)
			: mainWin_{ "main", {}, std::move(documentPath), winManager, true }
			, mainWinRootEleEventMan_{ mainWin_.getRootElement() } {
		}
	} uiState_;
};

TestWin::TestWin()
    : pImpl{ stdx::pimpl::make_unique<Impl>() }
{
}

TestWin::~TestWin() = default;
