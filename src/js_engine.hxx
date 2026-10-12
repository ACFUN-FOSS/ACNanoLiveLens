#ifndef NANOLIVELENS_JS_ENGINE_HXX
#define NANOLIVELENS_JS_ENGINE_HXX

#include "js_hook.hxx"

#include <EatiEssentials/memory/memsafety.hxx>
#include <ElementQuickJsBinding/binding.hxx>
#include <quickjspp.hpp>

#include <cstddef>
#include <filesystem>
#include <optional>
#include <string_view>

// 内嵌的 JavaScript 脚本引擎，即「脚本扩展模块」的宿主。它负责：
//   * QuickJS 运行时（Runtime）与上下文（Context）的生命周期；
//   * 通过 ElementQuickJsBinding 把 C++ 类型自动暴露给脚本（JsTwinObject 双生对象机制）；
//   * 把 JsHook 的 addXxxHook 注册成脚本侧的全局函数（钩子系统的脚本入口）；
//   * 以模块语义加载用户脚本，并求值。
//
// 本类只管运行时与脚本加载；**具体暴露了什么**（哪些类型、翻译器、函数桥接签名、
// 全局函数）全部集中在 src/js_bindings.hxx / js_bindings.cxx。
//
// 引擎只在主线程上使用：QuickJS 的上下文与 JSValue 都不是线程安全的，而钩子的
// 所有触发点（窗口更新、热重载、弹幕收发）本来就在主线程上。
class JsEngine
{
public:
	JsEngine();
	~JsEngine();

	JsEngine(const JsEngine &) = delete;
	JsEngine(JsEngine &&) = delete;
	JsEngine &operator=(const JsEngine &) = delete;
	JsEngine &operator=(JsEngine &&) = delete;

	// 创建运行时与上下文、注册全部 C++ 绑定与钩子注册函数。
	// 必须在加载任何脚本之前调用；重复调用无副作用（已初始化时直接返回）。
	void init();

	// 加载脚本目录下的全部 .js 用户脚本（模块语义，按文件名排序），
	// 返回成功加载的脚本数。单个脚本出错只会打印错误，不影响其余脚本。
	// 目录会被记住，供 hotReload() 使用。
	std::size_t loadScripts(const std::filesystem::path &scriptDir);

	// 重新加载上一次 loadScripts() 的目录。
	//
	// 实现方式是「重建上下文」而不是「重新求值」：QuickJS 的模块表按名字挂在
	// 上下文上，且 JS_Eval(MODULE) 不会检查重名（quickjs.c 的 js_new_module_def
	// 无条件追加），模块解析又总是返回第一个同名模块——所以同名文件二次求值会
	// 留下旧模块，`import './x.js'` 仍会命中它。销毁上下文是唯一能真正清干净的做法。
	//
	// 因此这是「干净重启」而非「热补丁」：脚本的顶层状态（计数、缓存、模块级变量）
	// 全部清零，脚本需要重新 addXxxHook；已发生过的一次性事件（SysStartUp、
	// PostLogin）会在重载后重放，让新脚本能重建初始状态。
	void hotReload();

	// 以模块语义求值一段脚本。出错时打印错误并返回一个空对象。
	JSValue evalModule(std::string_view code, std::string_view filename);
	// 以脚本（非模块）语义求值一段代码。出错时打印错误并返回一个空对象。
	JSValue eval(std::string_view code);

	[[nodiscard]] qjs::Runtime &runtime() LIFETIMEBOUND;
	[[nodiscard]] qjs::Context &context() LIFETIMEBOUND;
	[[nodiscard]] ElementEngine::QJSBinding::Binding &binding() LIFETIMEBOUND;

private:
	// 求值入口：evalFlags 为 JS_EVAL_TYPE_*。成功时把结果写入 resultOut 并返回 true；
	// 失败时打印错误、消费挂起的 JS 异常并返回 false。
	bool runCode(std::string_view code, std::string_view filename, int evalFlags, JSValue *resultOut);

	// 打印并消费当前挂起的 JS 异常。
	void dumpPendingException(std::string_view where);

	qjs::Runtime rt_;
	// 用 optional 而不是直接持有：qjs::Context 既不可拷贝也没有移动构造，
	// 而热重载必须销毁并重建它。运行时（rt_）可以复用——模块表在上下文上，
	// 不在运行时上。
	std::optional<qjs::Context> ctx_;
	// 声明在 ctx_ 之后：析构顺序与之相反，绑定会先于上下文销毁。
	std::optional<ElementEngine::QJSBinding::Binding> binding_;

	// 上一次 loadScripts() 的目录，hotReload() 用它重新加载。
	std::optional<std::filesystem::path> scriptDir_;
};

#endif // NANOLIVELENS_JS_ENGINE_HXX
