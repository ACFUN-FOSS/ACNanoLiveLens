#include "assets.hxx"
#include "appstate.hxx"
#include "js_binding.hxx"
#include "platform/crash_handler.hxx"
#include "sound/sound.hxx"
#include "rmlui_sys.hxx"
#include "rmluipp.hxx"
#include "RmlUIWin/window_manager.hxx"
#include "rmlui_element.hxx"

#include "danmaku_monitor_win.hxx"
#include "test_win.hxx"
#include "login_win.hxx"
//#include "js_bindings.hxx"

using namespace RmlUIWin;
using namespace Essentials::Memory;
using namespace Essentials::IO;

static bool ctrlCPressed = false;

static void initJs(qjs::Runtime &rt, qjs::Context &ctx) {
	js_std_init_handlers(rt.rt);
	JS_SetModuleLoaderFunc(rt.rt, nullptr, js_module_loader, nullptr);
	js_init_module_std(ctx.ctx, "std");
	js_init_module_os(ctx.ctx, "os");
	
	setupAllJsBinding(ctx);

}

METAPP_REFLECT
struct TestStruct
{
	std::string name;
	int age;
	std::string foo() {
		return "foo";
	}
	LifetimeInformant lifetimeInformant;
};

template<>
struct metapp::DeclareMetaType<TestStruct> : metapp::DeclareMetaTypeBase<TestStruct>
{
	static void setup() {
		getGlobalMetaRepo().registerType<TestStruct>("TestStruct");
	}
	static const metapp::MetaClass *getMetaClass() {
		static const metapp::MetaClass metaClass {
			metapp::getMetaType<TestStruct>(),
			[](metapp::MetaClass &mc) {
				mc.registerVariable("name", &TestStruct::name);
				mc.registerVariable("age", &TestStruct::age);
				mc.registerCallable("foo", &TestStruct::foo);
			}
		};
		return &metaClass;
	}
};

static void loadJsScript() {
	auto script = readFile(getAssetsDir() / "testuserscript.js");
	try_eval_module(script);


	auto metaType = metapp::getMetaType<TestStruct>();
	regClass(*getAppState().jsCtx, UNWRAP(metaType));

	auto test = getAppState().jsCtx->global()["test"];


	std::optional<TestStruct> testStruct = TestStruct{};


	auto twin = makeJsTwinObject(
		*getAppState().jsCtx,
		UNWRAP(metapp::getMetaType<TestStruct>()),
		testStruct.value(),
		testStruct.value().lifetimeInformant.info
	);

	testStruct.reset();

	auto res = JS_Call(getAppState().jsCtx->ctx, qjs::Value{ test }.v, JS_UNDEFINED, 1, &twin.v);
	if (JS_IsException(res))
		js_std_dump_error(getAppState().jsCtx->ctx);
	
}

static void rmluiMain() {

    RmlUISystem rmlui{ *Backend::GetSystemInterface(), *Backend::GetRenderInterface() };

    // 載入字體
    Rml::LoadFontFace((getAssetsDir() / "NokiaSans-Regular.ttf").string());
	Rml::LoadFontFace((getAssetsDir() / "wqy-zenhei.ttc").string(), true);

    registerCustomElements(rmlui);

    //Essentials::Special::callNullptr();

    {
        // 使用窗口管理器管理所有窗口
		RmlUIWin::WinManager winMan;

        RmlUIWin::onReloadTriggered = [](Rml::Context &context) {
            playSound(Sound::RELOAD);
            auto children = getAllChildrenRecursively(UNWRAP(context.GetRootElement()));

            auto windowEles = [&]() {
                std::vector<Refw<RmlUIElement>> windowEles;
                for (auto &child : children) {
                    if (auto ele = dynamic_cast<RmlUIElement *>(&child.get())) {
                        if (ele->getIsWindowElement())
                            windowEles.emplace_back(UNWRAP(ele));
                    }
                }
                return windowEles;
            }();

            for (auto &child : windowEles) {
				child.get().reload();
            }

			//winMan.requestReloadToAllWins();
        };

		qjs::Runtime rt;
		qjs::Context ctx(rt);

		initJs(rt, ctx);
		
		initAppState({ &rmlui, &winMan, &rt, &ctx });

		loadJsScript();

        //TestWin testWin;
		//DanmakuMonitorWin danmakuMonitorWin;
		//LoginWin loginWin;

        //assert(false);

		//JSBindings::init(rmlui, danmakuMonitorWin);

		bool shouldExit = false;

        while (!ctrlCPressed && !shouldExit && winMan.hasOpenWins()) {
            // 处理输入和窗口事件
            shouldExit = !Backend::ProcessEvents(false);

            // 更新所有窗口
            winMan.updateAll();

            // 渲染所有窗口
            winMan.renderAll();

            // 清理已關閉的窗口
            winMan.cleanupClosedWindows();
        }

		//JSBindings::shutdown();
    }
}




int crashHandlerProtectedMain() {
#ifdef WIN32
    system("chcp 65001");
#endif

	CtrlCLibrary::SetCtrlCHandler([](enum CtrlCLibrary::CtrlSignal signal) {
		if (signal == CtrlCLibrary::kCtrlCSignal)
			ctrlCPressed = true;
		return true;
	});

    initSound();

    // 初始化后端
    Backend::Initialize("RmlUi App", 360, 450, true);

    //Essentials::Special::callNullptr();
    rmluiMain();

    // 关闭后端
    Backend::Shutdown();

	
    return 0;
}


int main() {
    runProtectedMain();
}

