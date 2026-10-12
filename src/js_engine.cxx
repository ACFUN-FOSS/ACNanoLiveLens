#include "js_engine.hxx"

#include "appstate.hxx"
#include "js_bindings.hxx"
#include "js_hook.hxx"
#include "script_style.hxx"

#include <EatiEssentials/io.hxx>
#include <quickjs/quickjs-libc.h>

#include <algorithm>
#include <chrono>
#include <print>
#include <stdexcept>
#include <string>
#include <vector>

using ElementEngine::QJSBinding::Binding;

JsEngine::JsEngine()
	: rt_{ } {
}

JsEngine::~JsEngine() {
	if (!binding_)
		return;

	// 回调可能持有脚本函数（引用计数由 JS 函数桥接体维护），必须在上下文销毁前释放，
	// 否则析构期间会向已经销毁的上下文释放 JSValue。
	JsHook::clear();
	// 与 init() 中的 js_std_init_handlers 配对。os 定时器里同样存着 JSValue，
	// 因此这一步也必须早于 ctx_ 的析构。
	js_std_free_handlers(rt_.rt);
}

qjs::Runtime &JsEngine::runtime() {
	return rt_;
}

qjs::Context &JsEngine::context() {
	if (!ctx_)
		throw std::logic_error{ "JsEngine::init() 尚未被调用。" };
	return *ctx_;
}

Binding &JsEngine::binding() {
	if (!binding_)
		throw std::logic_error{ "JsEngine::init() 尚未被调用。" };
	return *binding_;
}

void JsEngine::init() {
	if (binding_)
		return;

	ctx_.emplace(rt_);

	// 与测试一致：安装 libc 的处理器与模块加载器，脚本才能 import 'std' / 'os'。
	js_std_init_handlers(rt_.rt);
	JS_SetModuleLoaderFunc(rt_.rt, nullptr, js_module_loader, nullptr);
	js_init_module_std(ctx_->ctx, "std");
	js_init_module_os(ctx_->ctx, "os");

	binding_.emplace(App::getGlobalMetaRepo(), rt_, *ctx_);
	// 全部绑定细节集中在 src/js_bindings.cxx（元类声明在 js_bindings.hxx）。
	JsBindings::setupAll(*binding_);
}

void JsEngine::hotReload() {
	if (!binding_ || !ctx_) {
		std::println(stderr, "JsEngine::hotReload() 在 init() 之前被调用，已忽略。");
		return;
	}
	if (!scriptDir_) {
		std::println(stderr, "JsEngine::hotReload() 尚无可重载的脚本目录，已忽略。");
		return;
	}

	std::println("正在热重载脚本（目录 {}）……", scriptDir_->string());

	// 1. 释放 C++ 侧持有的全部 JS 引用。桥接体持有的 JS 函数引用由回调对象持有，
	//    必须在上下文销毁前释放，否则会向已销毁的上下文释放 JSValue。
	JsHook::clear();
	// 1.5 丢弃旧脚本注入的样式：脚本会被重新求值并重新注册样式，
	//     被删掉的脚本不能把自己的样式留在界面上。
	ScriptStyle::clear();
	// 2. os/std 的定时器里同样存着 JSValue，也必须早于上下文的析构。
	js_std_free_handlers(rt_.rt);
	// 3. 先销毁绑定：它持有上下文指针，析构时会解除 ctx → Binding 映射。
	binding_.reset();
	// 4. 销毁上下文。这一步才是热重载的关键：模块表、全局变量、全部 JS 对象
	//    都随上下文一起消失，同名脚本的旧模块不会再被 import 命中。
	ctx_.reset();

	// 5. 重建上下文并重新注册全部绑定与全局函数。
	init();
	// 6. 重新求值全部用户脚本（脚本会重新 addXxxHook）。
	loadScripts(*scriptDir_);

	// 7. 重放已经发生过的一次性事件。重载只是换了一份脚本，世界状态并没有变：
	//    引擎仍然就绪、用户仍然处于登入状态，所以把这两个事件补发给新脚本，
	//    否则脚本会以为自己还没启动。
	std::println("重放一次性事件（SysStartUp、PostLogin），供新脚本重建初始状态。");
	JsHook::triggerSysStartUp(std::chrono::system_clock::now());
	JsHook::triggerPostLogin();

	std::println("脚本热重载完成，已注册钩子 {} 个。", JsHook::hookCount());
}

void JsEngine::dumpPendingException(const std::string_view where) {
	ElementEngine::QJSBinding::dumpJsException(*ctx_->ctx, where);
}

std::size_t JsEngine::loadScripts(const std::filesystem::path &scriptDir) {
	if (!binding_)
		throw std::logic_error{ "JsEngine::init() 必须先于 loadScripts()。" };

	std::error_code ec;
	if (!std::filesystem::is_directory(scriptDir, ec)) {
		std::println(stderr, "脚本目录 {} 不存在，跳过脚本加载。", scriptDir.string());
		return 0;
	}

	// 目录有效才记住它，避免用错误路径覆盖掉可用的目录。
	scriptDir_ = scriptDir;

	std::vector<std::filesystem::path> scripts;
	for (const auto &entry : std::filesystem::directory_iterator{ scriptDir, ec }) {
		if (entry.is_regular_file() && entry.path().extension() == ".js")
			scripts.push_back(entry.path());
	}
	// 按文件名排序，让脚本之间的加载顺序可预期。
	std::ranges::sort(scripts);

	std::size_t loadedCount = 0;
	for (const auto &script : scripts) {
		const std::string filename = script.filename().string();
		try {
			const std::string code = Essentials::IO::readFile(script);
			JSValue result = JS_UNDEFINED;
			if (runCode(code, filename, JS_EVAL_TYPE_MODULE, &result)) {
				++loadedCount;
				std::println("已加载脚本 {}（已注册钩子 {} 个）", filename, JsHook::hookCount());
			}
			JS_FreeValue(ctx_->ctx, result);
		} catch (const std::exception &ex) {
			std::println(stderr, "加载脚本 {} 失败：{}", filename, ex.what());
		}
	}

	if (loadedCount == 0)
		std::println("脚本目录 {} 中没有可加载的脚本。", scriptDir.string());
	else
		std::println("共加载 {} 个脚本，已注册钩子 {} 个。", loadedCount, JsHook::hookCount());

	return loadedCount;
}

JSValue JsEngine::evalModule(const std::string_view code, const std::string_view filename) {
	JSValue result = JS_UNDEFINED;
	if (!runCode(code, filename, JS_EVAL_TYPE_MODULE, &result))
		return ctx_->newObject().v;
	return result;
}

JSValue JsEngine::eval(const std::string_view code) {
	JSValue result = JS_UNDEFINED;
	if (!runCode(code, "<eval>", JS_EVAL_TYPE_GLOBAL, &result))
		return ctx_->newObject().v;
	return result;
}

bool JsEngine::runCode(const std::string_view code, const std::string_view filename,
	const int evalFlags, JSValue *resultOut) {

	if (!binding_)
		throw std::logic_error{ "JsEngine::init() 必须先于求值。" };

	// JS_Eval 需要以 '\0' 结尾的源码与文件名，故先落到 std::string。
	const std::string codeStr{ code };
	const std::string filenameStr{ filename };

	JSValue result = JS_Eval(ctx_->ctx, codeStr.c_str(), codeStr.size(),
		filenameStr.c_str(), evalFlags);
	if (JS_IsException(result)) {
		dumpPendingException(filenameStr);
		JS_FreeValue(ctx_->ctx, result);
		return false;
	}

	*resultOut = result;
	return true;
}
