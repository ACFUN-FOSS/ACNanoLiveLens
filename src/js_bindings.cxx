#include "js_bindings.hxx"

#include "js_hook.hxx"
#include "script_style.hxx"

#include "Core/sound.hxx"

#include <quickjspp.hpp>

#include <any>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <format>
#include <functional>
#include <stdexcept>
#include <string>

using ElementEngine::QJSBinding::Binding;
using ElementEngine::QJSBinding::Translator;

namespace {

// --- 时间点的翻译：C++ 的 time_point ↔ 脚本的 Date ---

JSValue makeJsDate(qjs::Context &ctx, const JsHook::TimePoint timePoint) {
	const auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(
		timePoint.time_since_epoch()).count();

	JSValue dateCtor = JS_GetPropertyStr(ctx.ctx, ctx.global().v, "Date");
	JSValue args[]{ JS_NewInt64(ctx.ctx, milliseconds) };
	JSValue date = JS_CallConstructor(ctx.ctx, dateCtor, 1, args);
	JS_FreeValue(ctx.ctx, dateCtor);
	JS_FreeValue(ctx.ctx, args[0]);
	return date;
}

// detranslate 必须校验输入并在不符合要求时抛出异常（见文档「转换方法：翻译」）。
JsHook::TimePoint readJsDate(qjs::Context &ctx, JSValue jsVal) {
	const auto fail = [](const char *reason) {
		throw std::invalid_argument{ std::format("期望一个 Date：{}", reason) };
	};

	if (JS_IsUndefined(jsVal) || JS_IsNull(jsVal))
		fail("收到 null 或 undefined");

	JSValue getTime = JS_GetPropertyStr(ctx.ctx, jsVal, "getTime");
	if (!JS_IsFunction(ctx.ctx, getTime)) {
		JS_FreeValue(ctx.ctx, getTime);
		fail("收到的值没有 getTime() 方法");
	}

	JSValue millisecondsVal = JS_Call(ctx.ctx, getTime, jsVal, 0, nullptr);
	JS_FreeValue(ctx.ctx, getTime);
	if (JS_IsException(millisecondsVal)) {
		// 消费掉挂起的异常，避免它泄漏到调用方的错误信息里。
		JS_FreeValue(ctx.ctx, JS_GetException(ctx.ctx));
		JS_FreeValue(ctx.ctx, millisecondsVal);
		fail("调用 getTime() 抛出了异常");
	}

	double milliseconds = 0;
	const int toFloatResult = JS_ToFloat64(ctx.ctx, &milliseconds, millisecondsVal);
	JS_FreeValue(ctx.ctx, millisecondsVal);
	if (toFloatResult < 0)
		fail("getTime() 的返回值不是数值");
	if (std::isnan(milliseconds))
		fail("时间点无效（NaN）");

	return JsHook::TimePoint{
		std::chrono::milliseconds{ static_cast<std::int64_t>(milliseconds) }
	};
}

}

namespace JsBindings {

// 新增 JS Bindings 时，修改此函数。
void setupAll(Binding &b) {
	// 1. 暴露给脚本的 C++ 类型。它们的 metapp 元类在 js_bindings.hxx 中声明，
	//    这里只需要注册；注册后 JsTwinObject 机制会自动生成 JS 侧的代理。
	b.regTypeStatic<DanmakuMonitorWin::DanmakuInfo>();
	b.regTypeStatic<DanmakuMonitorWin::DanmakuGuiInfo>();
	b.regTypeStatic<Rml::Element>();

	// 2. 时间点 → Date。time_point 没有 metapp 的 TypeKind，因此显式提供 translator。
	b.regType({
		.type = metapp::getMetaType<JsHook::TimePoint>(),
		.shareLifetimeInfoFunc = { },
		.moveable = true,
		.translator = Translator{
			.makeTranslateInput = [](const metapp::Variant &cppVal) -> std::any {
				return cppVal.cast<JsHook::TimePoint>().get<JsHook::TimePoint>();
			},
			.translateToJS = [](qjs::Context &ctx, const std::any &cppVal) -> JSValue {
				return makeJsDate(ctx, std::any_cast<JsHook::TimePoint>(cppVal));
			},
			.detranslate = [](qjs::Context &ctx, JSValue jsVal) -> metapp::Variant {
				return metapp::Variant{ readJsDate(ctx, jsVal) };
			}
		}
	});

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
