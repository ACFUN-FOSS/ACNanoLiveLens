#ifndef NANOLIVELENS_JS_HOOK_HXX
#define NANOLIVELENS_JS_HOOK_HXX

#include "danmaku_monitor_win.hxx"

// JS 钩子系统。
//
// 借鉴 LuaTeX 的设计哲学：与其提供完整的插件接口、文件结构与打包安装机制，
// 不如在应用的关键时刻定义轻量的触发点，让脚本以最简单的方式挂入自定义逻辑。
// 应用在特定时刻触发某个钩子，脚本侧则通过全局的 addXxxHook 把回调注册到该钩子上，
// 触发时按注册顺序调用全部回调。
//
// 与脚本的接口完全建立在 ElementQuickJsBinding 之上：
//   * 每个钩子在 C++ 侧是一个 addXxxHook(std::function<...>) 函数，由 JsEngine 用
//     Binding::regGlobalFunction 注册为脚本可直接调用的全局函数；
//   * 脚本传入的函数由 ElementQuickJsBinding 的 JS 函数桥接（JsFunctionBridge）
//     转换为 C++ 的 std::function，其形参按声明类型自动编组，返回值同理，
//     因此脚本回调与 C++ 直接注册的回调没有任何区别；
//   * 应用在关键时刻调用 triggerXxx，按注册顺序依次调用该钩子的全部回调，
//     单个回调抛出的异常会被捕获并打印，不影响其余回调。
//
// 回调表是本文件内的单例：一个应用只有一份脚本引擎，注册（脚本侧）与触发
//（C++ 侧）都只是普通的 std::function 操作，因此本文件不依赖脚本引擎本身。
namespace JsHook {

// 传给脚本的时间点。任何 std::chrono::time_point 都由 ElementQuickJsBinding
// 自动翻译为 JavaScript 的 Date（见文档「转换方法：翻译」与
// ElementQuickJsBinding/metapp_chrono_metatypes.hxx），因此这里只是一个别名，
// 不需要注册也不需要辅助访问器。
using TimePoint = std::chrono::system_clock::time_point;

// 回调标识，用于 removeHook。从 1 开始，0 保留为无效值。
using HookId = int;

// 各钩子的回调类型。形参类型同时是脚本侧的编组契约：
//   * DanmakuInfo 按值传入：脚本拿到的是自己的副本（移植体），可以放心保存到回调之外，
//     但改动不会回写到 C++ 侧；
//   * Rml::Element & 以孪生体（twin）传入：脚本可以直接操作 DOM，但不得保存引用，
//     元素会随弹幕清理或 DOM 重建而销毁；
//   * TimePoint 翻译为 Date。

// SysStartUp：系统启动成功，即将显示第一个窗体（登入窗体）时触发。参数：time（当前时间）。
using SysStartUpCallback = std::function<void(TimePoint time)>;

// PostLogin：用户已成功登入，即将显示应用窗体（弹幕监视窗体）时触发。无参数。
using PostLoginCallback = std::function<void()>;

// OnReceivedDanmaku：收到弹幕但尚未被弹幕监视窗口处理时触发。
// 参数：danmakuInfo（弹幕信息）、time（触发时间）。
// 此时弹幕元素尚未创建，适合做统计、过滤、持久化等与界面无关的工作。
using ReceivedDanmakuCallback = std::function<void(DanmakuMonitorWin::DanmakuInfo danmakuInfo, TimePoint time)>;

// PostShowedDanmaku：弹幕元素真正开始出场（出场动画启动）时触发。
// 参数：danmakuInfo（弹幕信息）、danmakuEle（该弹幕的容器元素）。
//
// 这是脚本给弹幕元素加**带过渡/动画的类**的唯一有效时机。RmlUi 只在元素的
// 「上一次样式定义」存在时才启动过渡（ElementStyle::UpdateDefinition 要求新旧定义
// 同时非空），而元素插入 DOM 的当帧还没有上一次定义——此刻加类，类会落入元素的
// 首次定义，属性只会立刻生效、不会播放动画。因此本钩子被推迟到元素已经历过一次
// 样式更新之后触发，与出场动画的 .show 落在同一次样式更新里。
//
// 若只关心「弹幕已入库/已计数」，用 OnReceivedDanmaku 即可；本钩子会晚若干帧。
using ShowedDanmakuCallback = std::function<void(DanmakuMonitorWin::DanmakuInfo danmakuInfo, Rml::Element &danmakuEle)>;

// OnDanmakuDrop：弹幕因过于久远触发自动清理、其元素与结构体即将被销毁时触发。
// 参数：danmakuInfo（弹幕信息）、danmakuEle（该弹幕的容器元素）。
// 注意弹幕量很大时，一条弹幕可能在轮到它出场之前就被清理，此时它不会有
// PostShowedDanmaku，但会有 OnDanmakuDrop。
using DanmakuDropCallback = ShowedDanmakuCallback;

// PostHotreloadStyle：样式（RCSS）热重载执行完毕时触发。无参数。
using HotreloadStyleCallback = std::function<void()>;

// PostHotreloadFull：完整热重载执行完毕时触发。完整热重载过程中 PostHotreloadStyle
// 也会被触发。无参数。
using HotreloadFullCallback = std::function<void()>;

// --- 注册：以下函数同时是脚本侧可以直接调用的全局函数（由 JsEngine 注册）---
// 返回回调 id；注册失败时抛出异常（脚本侧表现为 JS 异常）。

HookId addSysStartUpHook(SysStartUpCallback callback);
HookId addPostLoginHook(PostLoginCallback callback);
HookId addOnReceivedDanmakuHook(ReceivedDanmakuCallback callback);
HookId addPostShowedDanmakuHook(ShowedDanmakuCallback callback);
HookId addOnDanmakuDropHook(DanmakuDropCallback callback);
HookId addPostHotreloadStyleHook(HotreloadStyleCallback callback);
HookId addPostHotreloadFullHook(HotreloadFullCallback callback);

// 注销一个回调，返回是否找到并移除。回调在钩子触发过程中注销自己也是安全的。
bool removeHook(HookId id);

// 释放全部回调。回调可能持有脚本函数的引用，因此必须在脚本引擎的上下文
// 销毁之前调用（JsEngine 的析构函数会调用它）。
void clear();

// --- 触发：应用侧在对应时刻调用 ---

void triggerSysStartUp(TimePoint time);
void triggerPostLogin();
void triggerOnReceivedDanmaku(const DanmakuMonitorWin::DanmakuInfo &danmakuInfo, TimePoint time);
void triggerPostShowedDanmaku(const DanmakuMonitorWin::DanmakuInfo &danmakuInfo, Rml::Element &danmakuEle);
void triggerOnDanmakuDrop(const DanmakuMonitorWin::DanmakuInfo &danmakuInfo, Rml::Element &danmakuEle);
void triggerPostHotreloadStyle();
void triggerPostHotreloadFull();

// 已注册的回调总数（调试与测试用）。
std::size_t hookCount();

}

#endif // NANOLIVELENS_JS_HOOK_HXX
