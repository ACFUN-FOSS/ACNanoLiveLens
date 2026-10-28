/* ===========================================================================
 * 教程脚本：关键字高亮
 * ===========================================================================
 *
 * 应用启动时会加载 scripts 目录下所有 .js（按文件名顺序），脚本在加载时注册回调，
 * 应用运行到相应时刻再回头调用它们。改完脚本按 F5 重新加载即可。
 *
 * 一个最小的脚本长这样：
 *
 *     console.log('hello');                            // 加载时立刻执行
 *     addPostLoginHook(() => console.log('已登入'));    // 注册一个回调，稍后会被调用
 *
 * 不需要 import 任何东西：钩子函数与 console 都是宿主直接装到全局上的。
 *
 * 本文件是一个完整例子：给命中关键字的弹幕加一个高亮类，并播放提示音。
 *
 * API 的完整说明在同目录的 nanolivelens.d.ts —— 钩子的确切时机、元素的方法与
 * 属性、音频、std / os 模块都在那里；用编辑器打开这一整个文件夹即可获得补全、
 * 参数提示与类型检查。
 * =========================================================================== */

/* ---------------------------------------------------------------------------
 * 一、输出：console.log
 *
 * console.log("你好！");
 *
 * 它有两点要留意：只有 log 一个方法（没有 warn / error）；实参按 JS 的 String()
 * 转换，所以对象会打印成 [object Object]、数组是逗号连接的。要看清结构就得结合
 * JSON.stringify：
 *
 *     console.log(JSON.stringify({ sender: 'A', content: 'B' }));
 *
 * 日志一多不容易认出是哪来的，包一层带前缀的 log 是个不错的习惯：
 *
 *     const log = (message) => console.log(`[高亮] ${message}`);
 *
 * 可以写现代 JavaScript 语法。nanolivelens 的 JS 环境中，没有 Node、也没有浏览器的 DOM。
 * ------------------------------------------------------------------------ */

const log = (message) => console.log(`[高亮] ${message}`);

/* ---------------------------------------------------------------------------
 * 二、钩子：脚本的入口
 *
 * 脚本没有 main，也不会被循环调用 —— 它是「注册回调，然后结束」，应用在恰当的
 * 时刻回头调用这些回调。宿主提供的钩子（各自的确切时机见 nanolivelens.d.ts）：
 *
 *     addSysStartUpHook          系统启动完成
 *     addPostLoginHook           用户已登入、即将显示弹幕窗体
 *     addOnReceivedDanmakuHook   收到弹幕（此时还没有对应的元素）
 *     addPostShowedDanmakuHook   弹幕元素开始出场        ← 本脚本用这个
 *     addOnDanmakuDropHook       弹幕过于久远被清理
 *     addPostHotreloadStyleHook  按 F6 重载样式完成
 *     addPostHotreloadFullHook   按 Ctrl+R 完整重载完成
 *
 * addXxxHook 返回一个 id（从 1 开始），传给 removeHook(id) 即可注销；不注销则
 * 一直有效。同一个脚本可以注册任意多个钩子，它们彼此独立。
 * ------------------------------------------------------------------------ */


// 命中这些关键字中的任意一个，就把这条弹幕高亮。
const KEYWORDS = ['acfun', 'ACFun', '弹幕'];

// 高亮用的样式类名。它长什么样由下面的 HIGHLIGHT_RCSS 决定，这里只是名字。
const HIGHLIGHT_CLASS = 'script-keyword-highlight';

// 命中时播放的提示音。路径**相对 assets 目录**，脚本自带的音频放在 scripts/ 下。
const HIGHLIGHT_AUDIO = 'scripts/danmaku-highlight.ogg';

/* ---------------------------------------------------------------------------
 * 四三、把样式交给宿主：addScriptStyle
 *
 * 「高亮长什么样、要不要动画、动画多长」全写在 RCSS 里，脚本只负责判断是否命中。
 * 这样改外观不必动脚本逻辑，改判断也不必重复写样式值。
 *
 * 两点容易踩：
 *   * 弹幕是 <danmaku-item>，内部的 #sender / #content 各自声明了文字颜色，
 *     所以改文字颜色必须写后代选择器 —— 只改父元素不会影响它们；
 *   * addScriptStyle 返回 false 表示 RCSS 有语法错误，**一定要检查返回值**，
 *     否则样式静默失效，看起来就像钩子没触发。
 *
 * 宿主会在 F5 / F6 / Ctrl+R 之后自动重新挂载脚本样式，脚本不需要做任何事。
 * ------------------------------------------------------------------------ */
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

	/* 只声明过渡、不改外观，好让上面那处文字颜色也有动画可播。 */
	.danmaku-item #sender,
	.danmaku-item #content
	{
		transition: color .4s quadratic-out;
	}
`;

if (!addScriptStyle('danmaku-highlight.js', HIGHLIGHT_RCSS)) {
	log('样式注入失败，高亮不会生效（检查上面的 RCSS 有没有写错）');
}

/* ---------------------------------------------------------------------------
 * 四、播放声音：会抛异常，按需要 try / catch
 *
 * playOgg 播放 assets 目录下的音频文件，首次播放时读盘并解码，之后复用缓存，
 * 所以放在高频的弹幕回调里也不会反复读盘。文件缺失、解码失败或没有可用音频
 * 设备时它会抛异常。
 * ------------------------------------------------------------------------ */
let audioFailed = false;

function playHighlightSound() {
	try {
		playOgg(HIGHLIGHT_AUDIO);
	} catch (error) {
		if (audioFailed)
			return;
		audioFailed = true;
		log(`提示音播放失败：${error.message ?? error}（后续不再提示）`);
	}
}

/* ---------------------------------------------------------------------------
 * 五、处理弹幕：PostShowedDanmaku
 *
 * 回调收到两样东西：
 *   * danmakuInfo —— 弹幕内容的**副本**（移植体）。可以放心存到回调之外，
 *     但改它不会影响 C++ 侧；
 *   * danmakuEle  —— 弹幕元素的**孪生体**，可以随意读写，改动立刻作用到界面上。
 *     元素本身由应用持有：弹幕被清理、或按 Ctrl+R 重建 DOM 之后它就失效了，
 *     所以不要把孪生体存到回调之外。
 *
 * 「高亮」本身只有一行 —— 加一个类，颜色和动画都在样式表里。
 *
 * 注意：想让类产生动画，就**必须**在这个钩子里加。RmlUi 只在元素「上一次样式
 * 定义」存在时才启动过渡，而元素刚插入 DOM 的当帧还没有上一次定义，那时加类
 * 只会立刻生效、不播动画；本回调被推迟到元素已经历过一次样式更新之后，与出场
 * 动画落在同一次更新里。
 *
 * 只做计数、统计这类不需要元素的活儿，用 addOnReceivedDanmakuHook 更早也更省。
 * ------------------------------------------------------------------------ */
let hitCount = 0;

addPostShowedDanmakuHook((danmakuInfo, danmakuEle) => {
	const hit = KEYWORDS.find((keyword) => danmakuInfo.content.includes(keyword));
	if (!hit)
		return;

	danmakuEle.setClass(HIGHLIGHT_CLASS, true);
	playHighlightSound();

	hitCount++;
	// 只打印前几条，之后每 20 条报一次，避免刷屏。
	if (hitCount <= 5)
		log(`命中关键字「${hit}」：${danmakuInfo.sender} 的弹幕已高亮`);
	else if (hitCount % 20 === 0)
		log(`已高亮 ${hitCount} 条弹幕`);
});

/* ---------------------------------------------------------------------------
 * 六、再来一个钩子：热重载之后
 *
 * 同一个脚本可以注册多个钩子，互不影响；这里顺便演示一个「不碰元素」的钩子。
 * Ctrl+R 会重建 DOM，弹幕重新出现时会再次触发 PostShowedDanmaku，高亮自动重新
 * 应用，所以这里只需要打印一行提示。
 * ------------------------------------------------------------------------ */
addPostHotreloadFullHook(() => {
	log('DOM 已重建，弹幕重新出现时会重新执行高亮');
});

/* ---------------------------------------------------------------------------
 * 八、接下来动手改改看
 *
 *   1. 改关键字：把 KEYWORDS 换成你自己的名字，保存后按 F5。
 *   2. 改外观：改 HIGHLIGHT_RCSS 里的 background-color 和 .4s，按 F6 只重载样式。
 *   3. 换成一次性脉冲动画：把样式表最后一条规则的 transition 换成
 *          animation: 1.2s quadratic-out 1 highlight-pulse;
 *          @keyframes highlight-pulse { from { color: #66ccff; } to { color: #dc22dc; } }
 *   4. 只高亮某个人：把命中条件改成 danmakuInfo.sender === '昵称'。
 *   5. 换成内置音效：把 playOgg(HIGHLIGHT_AUDIO) 换成 playSound('RELOAD')，
 *      可选值见 nanolivelens.d.ts 里的 SoundName。
 *   6. 自己写一个：用 addOnDanmakuDropHook 统计「被清理的弹幕里有多少条命中过
 *      关键字」—— 命中计数已经在 hitCount 里了，只差一个被清理的计数。
 *
 * 热重载：F5 重载脚本（改了逻辑按它）、F6 只重载样式（改了 RCSS 按它）、
 * Ctrl+R 完整重载（重建 DOM，弹幕会重新出现）。脚本出错时终端会打印报错，
 * 行号对得上本文件，照着改即可。
 * ------------------------------------------------------------------------ */

log('danmaku-highlight.js 已加载');
