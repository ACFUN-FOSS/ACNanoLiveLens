/*
 * 示例脚本：关键字高亮。
 *
 * 演示 PostShowedDanmaku 钩子里的 DOM 操作：钩子收到的 danmakuEle 是 C++ 侧
 * RmlUi 元素的孪生体（twin），脚本可以直接调用它的方法（见 src/js_metatypes.hxx
 * 中暴露的方法列表）。
 *
 * 注意：孪生体只是一个「遥控器」，元素由应用持有。弹幕被清理、或按 Ctrl+R 重建
 * DOM 之后，旧元素即被销毁，因此不要在回调之外保存 danmakuEle。
 */

import * as std from 'std';

// 需要高亮的关键字，以及高亮时使用的样式属性。
const KEYWORDS = ['acfun', 'ACFun', '弹幕'];
const HIGHLIGHT_COLOR = '#66ccff';

const log = (message) => std.puts(`[高亮] ${message}\n`);

addPostShowedDanmakuHook((danmakuInfo, danmakuEle) => {
	const hit = KEYWORDS.find((keyword) => danmakuInfo.content.includes(keyword));
	if (!hit) {
		return;
	}

	// 直接操作元素：加一个类，并覆盖文字颜色。
	danmakuEle.setClass('script-keyword-highlight', true);
	danmakuEle.setProperty('color', HIGHLIGHT_COLOR);

	if (danmakuInfo.content.length > 0) {
		log(`命中关键字「${hit}」：${danmakuInfo.sender} 的弹幕已高亮`);
	}
});

// 元素会被销毁，这里演示在弹幕清理时做脚本侧的收尾工作。
addOnDanmakuDropHook((danmakuInfo, danmakuEle) => {
	// 此时元素仍然有效，但马上就要被销毁：只能做「最后一次」读写，不要保存引用。
	if (danmakuEle.isClassSet('script-keyword-highlight')) {
		danmakuEle.setClass('script-keyword-highlight', false);
	}
});

// 完整热重载会重建 DOM：元素全部换成新的，样式类需要重新应用（弹幕重新出现时
// PostShowedDanmaku 会再次触发，故这里只需要打印提示）。
addPostHotreloadFullHook(() => {
	log('DOM 已重建，弹幕重新出现时会重新执行高亮');
});

log('danmaku-highlight.js 已加载');
