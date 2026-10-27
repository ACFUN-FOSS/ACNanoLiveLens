/**
 * ACNanoLiveLens 脚本 API 类型声明。
 *
 * 把本文件与脚本放在一起（assets/scripts/），用编辑器打开脚本目录即可获得补全、
 * 参数提示与类型检查。它以全局声明（declare）的方式描述宿主注册到脚本里的
 * 全局函数与类型，因此脚本不需要 import 任何东西就能用上。
 *
 * 命名空间速览：
 *   * 全局函数 addXxxHook / removeHook —— JS 钩子系统，见 src/js_hook.hxx；
 *   * 全局函数 addScriptStyle            —— 脚本注入 RCSS 样式，见 src/script_style.hxx；
 *   * 全局函数 playOgg / playSound       —— 播放音频，见 subprojects/Core/public/Core/sound.hxx；
 *   * 类型 DanmakuInfo / DanmakuGuiInfo / RmlUIEle —— 见 src/js_bindings.hxx；
 *   * 全局 console / print               —— QuickJS 自带（宿主调用 js_std_add_helpers 装上的）；
 *   * 模块 'std' / 'os'                  —— QuickJS 自带（quickjs-libc），此处只声明常用部分。
 *
 * 输出：脚本里有 console.log 与 print，两者等价，都写到标准输出——宿主装的是
 * QuickJS 自带的 js_std_add_helpers，没有自己另造一套。它们只有 log 这一个方法，
 * 且实参按 JS 的 String() 转换（对象打印成 `[object Object]`、数组打印成
 * `1,two`），想看清结构请自己 JSON.stringify；需要写标准错误时用 std.err.puts。
 */

/**
 * 钩子回调的标识，由 addXxxHook 返回。从 1 开始，0 表示无效值。
 * 传给 removeHook 即可注销对应回调。
 */
type HookId = number;

/* -------------------------------------------------------------------------
 * 输出：QuickJS 自带的全局辅助（宿主调用 js_std_add_helpers 装上）
 * ---------------------------------------------------------------------- */

/**
 * 输出到标准输出：实参用空格连接，末尾自动换行。
 *
 * 它是 QuickJS 自带的，与 console.log 是同一个函数。值按 JS 的 String() 转换，
 * 因此对象是 `[object Object]`、数组是逗号连接的 `1,two`——要看清结构请自己
 * JSON.stringify：
 *
 * @example
 * ```js
 * print('收到', 3, '条弹幕');
 * console.log(JSON.stringify({ sender: 'A', content: 'B' }));
 * ```
 */
declare function print(...args: unknown[]): void;

/**
 * QuickJS 自带的 console，**只有 log 一个方法**。
 *
 * 常与 print 混用（两者等价）。需要写标准错误时没有 console.error，改用
 * `import * as std from 'std'` 后的 `std.err.puts`；需要 printf 风格格式化时用
 * `std.printf`。
 *
 * @example
 * ```js
 * console.log('命中关键字：%s', hit);   // 注意：这不支持 %s，会原样打印
 * console.log(`命中关键字：${hit}`);     // 想插值就用模板串
 * ```
 */
declare const console: {
	log(...args: unknown[]): void;
};

/* -------------------------------------------------------------------------
 * 宿主暴露给脚本的 C++ 类型
 * ---------------------------------------------------------------------- */

/**
 * 弹幕信息。
 *
 * 以**副本**（移植体）传给脚本：可以放心保存到回调之外，但修改它不会影响 C++ 侧。
 */
declare class DanmakuInfo {
	/** 发送者昵称。 */
	sender: string;
	/** 弹幕正文。 */
	content: string;
	/**
	 * 发送时间。
	 *
	 * C++ 的 `std::chrono::time_point` 由宿主统一翻译为 JS `Date`；写回时同样
	 * 接受一个 `Date`。
	 */
	timestamp: Date;
}

/**
 * 弹幕在界面中的 GUI 信息。
 *
 * 其中的元素引用是孪生体，只在当次回调期间有效。
 */
declare class DanmakuGuiInfo {
	/** 弹幕出现动画的容器元素。 */
	danmakuItemAppearAnimContainerEle: RmlUIEle;
	/** 弹幕条目元素。 */
	danmakuEle: RmlUIEle;
}

/**
 * RmlUi 元素。
 *
 * 脚本拿到的元素**总是孪生体（twin）**：它只是 C++ 侧真实元素的「遥控器」，
 * 元素本身仍由应用持有。因此：
 *   * 可以直接读写它（改动会立刻作用到界面上）；
 *   * **不要**把它保存到回调之外——弹幕被清理或按 Ctrl+R 重建 DOM 后，
 *     真实元素随之销毁，保存下来的孪生体也就失效了。
 */
declare class RmlUIEle {
	/** 元素标签名，例如 "danmaku-item"、"div"。 */
	getTagName(): string;
	/** 元素的 id 属性，未设置时为空串。 */
	getId(): string;

	/** 增删一个样式类。高亮这类「带动画的外观变化」应当写成类，再在这里添加。 */
	setClass(className: string, activate: boolean): void;
	/** 是否含有某个样式类。 */
	isClassSet(className: string): boolean;
	/** 覆盖元素的整个 class 属性（空格分隔）。 */
	setClassNames(classNames: string): void;

	/**
	 * 设置一条样式属性，等价于 RML 上的 style 属性。
	 * 返回值表示是否设置成功（属性名或值非法时为 false）。
	 * 需要多个属性或需要过渡动画时，优先用 addScriptStyle 注入样式表 + setClass。
	 */
	setProperty(name: string, value: string): boolean;

	/** 用 RML 片段替换元素的全部内容。 */
	setInnerRML(rml: string): void;
	/** 取元素当前的内容（RML 片段）。 */
	getInnerRML(): string;

	/** 元素盒子的高度（含内边距与边框，dp）。 */
	getOffsetHeight(): number;
	/** 元素内容区的高度（dp）。 */
	getClientHeight(): number;
	/** 内容总高度（含被 overflow 隐藏的部分，dp）。 */
	getScrollHeight(): number;
	/** 当前纵向滚动位置（dp）。 */
	getScrollTop(): number;
	/** 设置纵向滚动位置（dp）。 */
	setScrollTop(scrollTop: number): void;
}

/* -------------------------------------------------------------------------
 * 钩子：参数说明见 src/js_hook.hxx
 * ---------------------------------------------------------------------- */

/**
 * SysStartUp：系统启动成功、即将显示第一个窗体时触发。
 * @param callback 参数 time 为触发时刻的当前时间。
 */
declare function addSysStartUpHook(callback: (time: Date) => void): HookId;

/** PostLogin：用户已成功登入、即将显示应用窗体（弹幕监视窗体）时触发。 */
declare function addPostLoginHook(callback: () => void): HookId;

/**
 * OnReceivedDanmaku：收到弹幕、尚未被弹幕监视窗口处理时触发。
 * 适合做统计、过滤、改写前的观察。此时还没有对应的元素。
 */
declare function addOnReceivedDanmakuHook(
	callback: (danmaku: DanmakuInfo, time: Date) => void,
): HookId;

/**
 * PostShowedDanmaku：弹幕元素真正开始出场（出场动画启动）时触发。
 *
 * **这是给弹幕元素加「带 transition / animation 的类」的唯一有效时机。**
 * RmlUi 只在元素的「上一次样式定义」存在时才启动过渡，而元素插入 DOM 的当帧还
 * 没有上一次定义——在那时加类，类会落进元素的首次定义里，样式只会立刻生效、
 * 不播放动画。本回调被推迟到元素已经历过一次样式更新之后触发，与出场动画落在
 * 同一次样式更新里，因此这时加类才能看到动画。
 *
 * 只关心计数/入库时用 addOnReceivedDanmakuHook 即可，本回调会晚若干帧；
 * 弹幕量很大时，一条弹幕甚至可能在轮到它出场前就被清理（则本次回调不触发，
 * addOnDanmakuDropHook 仍会触发）。
 *
 * @param callback 参数 element 为该弹幕的元素孪生体，可在本次回调中任意操作。
 */
declare function addPostShowedDanmakuHook(
	callback: (danmaku: DanmakuInfo, element: RmlUIEle) => void,
): HookId;

/**
 * OnDanmakuDrop：弹幕因过于久远被自动清理、其元素与信息即将被销毁时触发。
 * 元素此刻仍然有效，可以做「最后一次」读写，但不要保存引用。
 */
declare function addOnDanmakuDropHook(
	callback: (danmaku: DanmakuInfo, element: RmlUIEle) => void,
): HookId;

/** PostHotreloadStyle：样式（RCSS）热重载执行完毕时触发（按 F6）。 */
declare function addPostHotreloadStyleHook(callback: () => void): HookId;

/**
 * PostHotreloadFull：完整热重载执行完毕时触发（按 Ctrl+R）。
 * 此时 DOM 已重建、弹幕已重新生成；完整重载过程中 PostHotreloadStyle 也会被触发。
 */
declare function addPostHotreloadFullHook(callback: () => void): HookId;

/**
 * 注销一个钩子回调。
 * @returns 是否找到并移除。回调在自身执行过程中调用它也是安全的。
 */
declare function removeHook(id: HookId): boolean;

/* -------------------------------------------------------------------------
 * 样式注入：见 src/script_style.hxx
 * ---------------------------------------------------------------------- */

/**
 * 注册一段由脚本提供的 RCSS 样式表，宿主会把它挂到窗口文档上。
 *
 * 用于让脚本自带样式（例如带 transition 的高亮类），从而只靠 setClass 就完成
 * 带动画的外观变化，不必把样式写进 assets 下的 RCSS 文件。
 *
 * 脚本样式总是叠加在文档自身样式表之后，同等特异性下脚本样式胜出；
 * 它会被自动重新挂载，因此按 F6（重载样式）、Ctrl+R（重建文档）、F5（重载脚本）
 * 之后依然有效——不需要脚本做任何事。
 *
 * @param name 样式表名，仅用于报错时标识来源（例如写成脚本文件名）。
 * @param rcss RCSS 全文。后注册的样式表排在后面，优先级更高。
 * @returns 是否解析成功。**脚本应当检查它**，否则样式写错时会静默失效。
 *
 * @example
 * ```js
 * if (!addScriptStyle('my-highlight.js', `
 *     .my-highlight { transition: background-color .5s quadratic-out; background-color: #2e4460; }
 *     .my-highlight #content { color: #66ccff; }
 * `)) {
 *     console.log('样式注入失败，检查上面的 RCSS');
 * }
 * // 之后在 PostShowedDanmaku 里：element.setClass('my-highlight', true)
 * ```
 */
declare function addScriptStyle(name: string, rcss: string): boolean;

/* -------------------------------------------------------------------------
 * 音频：见 subprojects/Core/public/Core/sound.hxx
 * ---------------------------------------------------------------------- */

/**
 * 内置音效的名字，对应 C++ 侧 Sound 枚举的枚举值。
 *
 * 目前只有 'ERRR'（information_2.ogg）、'EXTERNAL_SERVICE_CRASH'
 * 与 'RELOAD'（hot_reload.ogg）真正接上了音频文件，其余在 C++ 侧是空操作。
 */
type SoundName =
	| 'CLICK'
	| 'LOGIN'
	| 'LOGOUT'
	| 'INFO'
	| 'ERRR'
	| 'EXTERNAL_SERVICE_CRASH'
	| 'RELOAD';

/**
 * 播放 assets 目录下的一个音频文件。
 *
 * 路径是**相对 assets 目录**的。脚本自带的音频一般放在 assets/scripts/ 下，
 * 因此写成 'scripts/my-sound.ogg'。
 *
 * 支持 ogg / wav / flac / mp3（按文件内容识别，与扩展名无关）。同一个路径只在
 * 首次播放时读盘并解码，之后重复播放复用缓存，因此可以在高频回调里调用。
 * 多个声音叠加混音，不会互相打断——弹幕密集时需要节流请自行在脚本里做。
 *
 * 注意它**会抛异常**（文件缺失、解码失败、音频设备不可用时），并且这个异常会
 * 冒泡成脚本侧的 TypeError。音效通常只是附属效果，建议按需要 try/catch：
 *
 * @example
 * ```js
 * try {
 *     playOgg('scripts/danmaku-highlight.ogg');
 * } catch (error) {
 *     console.log(`音效播放失败：${error.message}`);
 * }
 * ```
 */
declare function playOgg(oggName: string): void;

/**
 * 播放宿主内置的音效，用 SoundName 指定。
 * 名字写错会抛异常（脚本侧的 TypeError）。
 *
 * @example
 * ```js
 * playSound('RELOAD');
 * ```
 */
declare function playSound(sound: SoundName): void;

/* -------------------------------------------------------------------------
 * QuickJS 自带模块（只声明常用部分）
 * ---------------------------------------------------------------------- */

/**
 * std 模块：标准输出与文件 I/O。宿主只注册了 'std' 与 'os' 两个模块。
 *
 * 日常输出直接用全局的 console.log / print 即可；std 的用处是它们没覆盖的部分：
 * 写**标准错误**（std.err.puts）、printf 风格格式化（std.printf / std.sprintf）、
 * 读写文件与环境变量。
 *
 * @example
 * ```js
 * import * as std from 'std';
 * std.puts('hello\n');                 // 等价于 console.log('hello')
 * std.err.puts('这条走标准错误\n');
 * std.printf('%d 条弹幕\n', count);
 * ```
 */
declare module 'std' {
	/** 标准输出、标准错误、标准输入（File 对象）。 */
	const out: StdFile;
	const err: StdFile;
	const in: StdFile;

	/** 向标准输出写字符串。 */
	function puts(text: string): void;
	/** 按 printf 风格格式化并输出。 */
	function printf(format: string, ...args: unknown[]): void;
	/** 按 printf 风格格式化并返回字符串。 */
	function sprintf(format: string, ...args: unknown[]): string;

	/** 读文件并返回其内容（文本）。 */
	function loadFile(path: string): string | null;
	/** 打开文件，mode 同 fopen（例如 "r"、"w"）。 */
	function open(path: string, mode: string): StdFile | null;
	/** 读取环境变量。 */
	function getenv(name: string): string | undefined;
	/** 退出进程。 */
	function exit(code: number): void;
	/** 以脚本语义求值一段代码。 */
	function evalScript(code: string): unknown;
	/** 加载并执行一个脚本文件。 */
	function loadScript(path: string): void;
	/** 触发一次垃圾回收。 */
	function gc(): void;

	/** 错误码表，见 std.Error.*。 */
	const Error: Record<string, number>;
}

/** std.open / std.tmpfile 返回的文件对象。 */
declare class StdFile {
	close(): void;
	flush(): void;
	puts(text: string): number;
	printf(format: string, ...args: unknown[]): void;
	/** 读取 size 字节；无参数时读取全部。 */
	read(size?: number): string | ArrayBuffer | null;
	write(data: string | ArrayBuffer): number;
	/** 文件末尾的下一个位置。 */
	tell(): number;
	eof(): boolean;
	/** 当前是否处于错误状态。 */
	error(): boolean;
	clearerr(): void;
	seek(offset: number, whence: number): number;
}

/**
 * os 模块：进程与文件系统。宿主没有运行事件循环，因此 setTimeout 回调
 * 不会被触发——不要依赖它做延迟逻辑（需要延迟时用钩子：下一个弹幕、
 * 下一次热重载等都是天然的时机）。
 */
declare module 'os' {
	/** 平台标识，例如 "win32"。 */
	const platform: string;

	/** 文件属性常量，配合 stat 的返回值使用。 */
	const S_IFDIR: number;
	const S_IFREG: number;

	function open(path: string, flags: number, mode?: number): number;
	function close(fd: number): number;
	function read(fd: number, buffer: ArrayBuffer, offset: number, length: number): number;
	function write(fd: number, buffer: ArrayBuffer, offset: number, length: number): number;
	function remove(path: string): number;
	function rename(oldPath: string, newPath: string): number;
	function getcwd(): string;
	function chdir(path: string): number;
	function mkdir(path: string, mode?: number): number;
	function readdir(path: string): string[];
	function realpath(path: string): string;
	/** 取文件信息（属性、大小、时间等），失败时返回 null。 */
	function stat(path: string): Record<string, number> | null;
	/** 阻塞指定毫秒。会卡住整个 UI，慎用。 */
	function sleep(milliseconds: number): void;

	/** 注册一个定时器。注意：宿主没有运行 QuickJS 事件循环，回调不会被执行。 */
	function setTimeout(callback: () => void, delayMs: number): unknown;
	/** 取消 setTimeout 注册的定时器。 */
	function clearTimeout(timer: unknown): void;
}
