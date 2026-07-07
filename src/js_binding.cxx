#include "js_binding.hxx"
#include "appstate.hxx"
#include "danmaku_monitor_win.hxx"

void setupAllJsBinding(qjs::Context &ctx) {
	DanmakuMonitorWin::setupJsBinding(ctx);
}

qjs::Value
try_eval_module(std::string_view code) {
	try {
		return App::getState().jsCtx->eval(code, "<import>", JS_EVAL_TYPE_MODULE);
	} catch (const qjs::exception &ex) {
		//js_std_dump_error(ctx);
		auto exc = App::getState().jsCtx->getException();
		std::cerr << (exc.isError() ? "Error: " : "Throw: ") << (std::string)exc << std::endl;
		if ((bool)exc["stack"])
			std::cerr << (std::string)exc["stack"] << std::endl;

		js_std_free_handlers(App::getState().jsRuntime->rt);
		return App::getState().jsCtx->newObject();
	}
}

qjs::Value
try_eval(std::string_view code) {
	try {
		return App::getState().jsCtx->eval(code);
	} catch (const qjs::exception &ex) {
		js_std_dump_error(App::getState().jsCtx->ctx);
		auto exc = App::getState().jsCtx->getException();
		std::cerr << (exc.isError() ? "Error: " : "Throw: ") << (std::string)exc << std::endl;
		if ((bool)exc["stack"])
			std::cerr << (std::string)exc["stack"] << std::endl;

		js_std_free_handlers(App::getState().jsRuntime->rt);
		return App::getState().jsCtx->newObject();
	}

}
