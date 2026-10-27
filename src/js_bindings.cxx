#include "js_bindings.hxx"

#include "js_hook.hxx"
#include "script_style.hxx"

#include "Core/sound.hxx"

using ElementEngine::QJSBinding::Binding;

namespace JsBindings {

// 新增 JS Bindings 时，修改此函数。
void setupAll(Binding &b) {
	// 1. 暴露给脚本的 C++ 类型。它们的 metapp 元类在 js_bindings.hxx 中声明，
	//    这里只需要注册；注册后 JsTwinObject 机制会自动生成 JS 侧的代理。
	b.regTypeStatic<DanmakuMonitorWin::DanmakuInfo>();
	b.regTypeStatic<DanmakuMonitorWin::DanmakuGuiInfo>();
	b.regTypeStatic<Rml::Element>();

	// 2. 时间点与时间长度不需要注册：任何 std::chrono::time_point 都会被翻译为
	//    JS Date，任何 std::chrono::duration 都会被翻译为毫秒数（见
	//    ElementQuickJsBinding/metapp_chrono_metatypes.hxx）。DanmakuInfo 的
	//    timestamp 成员、以及钩子回调里的 TimePoint 形参都因此自动得到 Date。

	// 3. JS 函数桥接：脚本函数只有在对应签名被注册之后才能作为实参传给 C++。
	//    注意同一签名只能注册一次——PostLogin、PostHotreloadStyle 与 PostHotreloadFull
	//    的回调类型相同（都是 std::function<void()>），OnDanmakuDrop 与 PostShowedDanmaku
	//    也相同，故这里只注册去重后的四个签名。
	b.regTypeStatic<std::function<void(JsHook::TimePoint)>>();
	b.regTypeStatic<std::function<void()>>();
	b.regTypeStatic<std::function<void(DanmakuMonitorWin::DanmakuInfo, JsHook::TimePoint)>>();
	b.regTypeStatic<std::function<void(DanmakuMonitorWin::DanmakuInfo, Rml::Element &)>>();

	// 4. 钩子注册函数：JsHook 的自由函数被注册成脚本的全局函数，
	//    脚本调用它们即可把回调挂到对应钩子上，并拿到回调 id。
	b.regGlobalFunction("addSysStartUpHook",
		std::function<JsHook::HookId(JsHook::SysStartUpCallback)>{ JsHook::addSysStartUpHook });
	b.regGlobalFunction("addPostLoginHook",
		std::function<JsHook::HookId(JsHook::PostLoginCallback)>{ JsHook::addPostLoginHook });
	b.regGlobalFunction("addOnReceivedDanmakuHook",
		std::function<JsHook::HookId(JsHook::ReceivedDanmakuCallback)>{ JsHook::addOnReceivedDanmakuHook });
	b.regGlobalFunction("addPostShowedDanmakuHook",
		std::function<JsHook::HookId(JsHook::ShowedDanmakuCallback)>{ JsHook::addPostShowedDanmakuHook });
	b.regGlobalFunction("addOnDanmakuDropHook",
		std::function<JsHook::HookId(JsHook::DanmakuDropCallback)>{ JsHook::addOnDanmakuDropHook });
	b.regGlobalFunction("addPostHotreloadStyleHook",
		std::function<JsHook::HookId(JsHook::HotreloadStyleCallback)>{ JsHook::addPostHotreloadStyleHook });
	b.regGlobalFunction("addPostHotreloadFullHook",
		std::function<JsHook::HookId(JsHook::HotreloadFullCallback)>{ JsHook::addPostHotreloadFullHook });
	b.regGlobalFunction("removeHook",
		std::function<bool(JsHook::HookId)>{ JsHook::removeHook });

	// 5. 样式注入：脚本把自己写的 RCSS 交给宿主，宿主负责把它挂到窗口文档上。
	//    这样脚本可以在被载入时定义自己的类（例如带 transition 的高亮类），
	//    不必把样式写进 assets 里的 RCSS 文件，也就不会与宿主样式互相冲突。
	b.regGlobalFunction("addScriptStyle",
		std::function<bool(std::string, std::string)>{
			[](std::string name, std::string rcss) { return ScriptStyle::add(name, rcss); }
		});

	// 6. 音频。
	//    playOgg：播放 assets 目录下的音频文件，路径相对 assets 目录
	//    （脚本自带的音频放在 scripts/ 下，写成 'scripts/xxx.ogg'）。
	//    playSound：播放内置音效，脚本用名字指定（'ERRR' / 'RELOAD' / ...），
	//    字符串到 Sound 的转换由 Sound 的元类负责（见 js_bindings.hxx）。
	//    两个函数都是自由函数，参数已经能被 metapp 从 JS 值转换过来，
	//    因此直接注册即可，不需要转发用的 lambda。
	b.regGlobalFunction("playOgg", &playOgg);
	b.regGlobalFunction("playSound", &playSound);
}

}
