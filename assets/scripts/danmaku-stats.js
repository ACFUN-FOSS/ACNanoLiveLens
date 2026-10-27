/*
 * 示例脚本：弹幕统计。
 *
 * 把本文件放在 assets/scripts/ 目录下即可生效，无需重新编译主程序：
 * 脚本在加载时把回调注册到钩子上，应用运行到对应时刻就会调用它们。
 *
 * 脚本可以用钩子函数与全局的 console，它们由宿主直接装在全局上，无需 import。
 * 需要写标准错误或读写文件时，再 `import * as std from 'std'` 使用 QuickJS 的 std 模块。
 */

const log = (message) => console.log(`[统计] ${message}`);

// 把 Date 格式化为 HH:MM:SS。
const formatTime = (date) => {
	const pad = (value) => String(value).padStart(2, '0');
	return `${pad(date.getHours())}:${pad(date.getMinutes())}:${pad(date.getSeconds())}`;
};

let receivedCount = 0;
let shownCount = 0;
let droppedCount = 0;

// addXxxHook 返回回调 id（从 1 开始），可用于 removeHook(id) 注销。
const startUpHookId = addSysStartUpHook((time) => {
	log(`系统启动完成，当前时间 ${time.toISOString()}`);
	log(`本脚本的启动回调 id = ${startUpHookId}`);
});

addPostLoginHook(() => {
	log('用户已登入，即将显示弹幕监视窗体');
});

addOnReceivedDanmakuHook((danmakuInfo, time) => {
	receivedCount++;

	// danmakuInfo 是脚本自己的副本（移植体）：可以放心保存到回调之外。
	// timestamp 是 JS 的 Date（宿主把任何 C++ 时间点都翻译成 Date），
	// 要毫秒数就用 getTime()。
	if (receivedCount <= 3) {
		log(`收到弹幕 #${receivedCount}：${formatTime(danmakuInfo.timestamp)} ` +
			`${danmakuInfo.sender}: ${danmakuInfo.content}`);
	}
	// 每 20 条弹幕打印一次统计。
	if (receivedCount % 20 === 0) {
		log(`已收到 ${receivedCount} 条弹幕，其中 ${shownCount} 条已显示、` +
			`${droppedCount} 条已被清理`);
	}
});

// 弹幕元素真正开始出场时触发。它比 OnReceivedDanmaku 晚若干帧——
// 应用要等到元素已经历过一次样式更新（此时加类才会播放动画）才通知脚本。
addPostShowedDanmakuHook((danmakuInfo) => {
	shownCount++;
});

// 弹幕因过于久远被自动清理时，它的元素与结构体即将被销毁。
addOnDanmakuDropHook((danmakuInfo) => {
	droppedCount++;
});

// 热重载：F6 只重载 RCSS 样式，Ctrl+R 完整重载样式与 RML 文档。
// 完整重载的过程中 PostHotreloadStyle 也会被调用。
addPostHotreloadStyleHook(() => {
	log('样式热重载完成');
});

addPostHotreloadFullHook(() => {
	log('完整热重载完成：文档已重建，弹幕会重新出现');
});

log('danmaku-stats.js 已加载');
