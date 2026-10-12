/*
 * 示例脚本：关键字高亮（带过渡动画）。
 *
 * 与「直接改元素属性」相比，这里做的是**声明式**的高亮：
 *   1. 脚本在被载入时把一小段 RCSS 交给宿主（addScriptStyle），其中定义了
 *      带 transition 的高亮类；
 *   2. PostShowedDanmaku 钩子里只做一件事——给弹幕元素加上这个类。
 * 于是「高亮长什么样、要不要动画、动画多长」全部写在样式表里，脚本只负责判断
 * 命中与否。改样式不必动脚本，改脚本也不必重复写样式值。
 *
 * ## 为什么必须用 PostShowedDanmaku，而不是 OnReceivedDanmaku
 *
 * RmlUi 只在元素的**上一次样式定义**存在时才启动过渡——ElementStyle::UpdateDefinition()
 * 里的条件是「新旧定义都非空」：
 *     if (definition && new_definition) TransitionPropertyChanges(...);
 * 元素插入 DOM 的当帧还没有上一次定义，此时加类，类会落进元素的**首次**定义里，
 * 属性只会立刻生效、不播放任何动画（表现为「只有静态背景色」）。
 *
 * PostShowedDanmaku 正是被推迟到元素已经历过一次样式更新之后触发的，
 * 与出场动画的 .show 落在同一次样式更新里，因此这时加类才会真正播放过渡。
 * 反过来说：**想让某个类在弹幕上产生动画，就必须在这个钩子里加。**
 *
 * 关于钩子收到的 danmakuEle：它是 C++ 侧 RmlUi 元素的孪生体（twin），脚本可以直接
 * 调用它的方法（见 src/js_bindings.hxx 暴露的方法列表）。孪生体只是「遥控器」，
 * 元素由应用持有：弹幕被清理、或按 Ctrl+R 重建 DOM 之后旧元素即被销毁，
 * 因此不要在回调之外保存 danmakuEle。
 */

import * as std from 'std';

const log = (message) => std.puts(`[高亮] ${message}\n`);

// 需要高亮的关键字。
const KEYWORDS = ['acfun', 'ACFun', '弹幕'];

// 高亮类名。样式表里定义它，脚本只负责加这个类。
const HIGHLIGHT_CLASS = 'script-keyword-highlight';

// 样式表的名字，只用于出错时标识来源。
const STYLE_NAME = 'danmaku-highlight.js';

/* 脚本注入的样式表。
 *
 * 弹幕元素是 <danmaku-item>，内部是 <div class="danmaku-item"> 包着
 * #sender / #content / #time 三个文本节点，因此：
 *   * 背景、过渡这类「元素自身」的属性写在 .script-keyword-highlight 上；
 *   * 文字颜色必须写在后代选择器上——#sender / #content 各自声明了颜色，
 *     只改父元素不会影响它们的文字。
 *
 * 过渡的起点：RmlUi 对「旧定义解析不出该属性」的情况会回退到属性规范的默认值
 *（background-color 默认透明），所以不必为起点额外声明一次 background-color。
 *
 * 第 3 条规则为什么不写在 .script-keyword-highlight 上：RmlUi 里的 transition
 * 取自**新定义**，两种写法都能生效；写在始终匹配的选择器上更稳，不依赖
 *「加类的那一次更新同时带来 transition」这一实现细节。
 */
const HIGHLIGHT_RCSS = `
	.script-keyword-highlight
	{
		transition: background-color .4s quadratic-out;
		background-color: rgb(46, 68, 96);
	}

	/* 高亮期间的文字颜色。 */
	.script-keyword-highlight #sender,
	.script-keyword-highlight #content
	{
		color: #66ccff;
	}

	/* 仅声明过渡、不改变外观，让上面两处文字颜色的变化也有动画可播。
	 * 若想要「亮一下再恢复」的一次性脉冲，可把这里的 transition 换成 animation
	 *（RmlUi 支持 @keyframes），例如：
	 *   animation: 1.2s quadratic-out 1 highlight-pulse;
	 *   @keyframes highlight-pulse { from { color: #66ccff; } to { color: #dc22dc; } } */
	.danmaku-item #sender,
	.danmaku-item #content
	{
		transition: color .4s quadratic-out;
	}
`;

// 加载时注入样式表。解析失败会返回 false（例如 RCSS 有语法错误）。
if (!addScriptStyle(STYLE_NAME, HIGHLIGHT_RCSS)) {
	log('样式表注入失败，高亮不会生效（请检查上面的 RCSS）');
}

let hitCount = 0;

// PostShowedDanmaku：弹幕元素真正开始出场时触发，也是加动画类的唯一有效时机。
addPostShowedDanmakuHook((danmakuInfo, danmakuEle) => {
	const hit = KEYWORDS.find((keyword) => danmakuInfo.content.includes(keyword));
	if (!hit) {
		return;
	}

	// 高亮 = 加一个类。颜色与动画都在脚本注入的样式表里。
	danmakuEle.setClass(HIGHLIGHT_CLASS, true);

	hitCount++;
	// 只打印前几条，避免刷屏。
	if (hitCount <= 5) {
		log(`命中关键字「${hit}」：${danmakuInfo.sender} 的弹幕已高亮`);
	} else if (hitCount % 20 === 0) {
		log(`已高亮 ${hitCount} 条弹幕`);
	}
});

// 完整热重载（Ctrl+R）会重建 DOM：元素全部换成新的，弹幕重新出现时会再次触发
// PostShowedDanmaku，高亮会自动重新应用，所以这里只需要打印提示。
// F5 只是重载脚本：弹幕元素不变，但脚本会重新加载并重新注入样式表。
addPostHotreloadFullHook(() => {
	log('DOM 已重建，弹幕重新出现时会重新执行高亮');
});

log('danmaku-highlight.js 已加载');
