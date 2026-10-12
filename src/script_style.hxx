#ifndef NANOLIVELENS_SCRIPT_STYLE_HXX
#define NANOLIVELENS_SCRIPT_STYLE_HXX

#include <string_view>

namespace Rml { class ElementDocument; }

// 脚本注入 RCSS 样式的注册表。脚本侧的入口是全局函数 addScriptStyle。
//
// 为什么需要一份注册表，而不是直接把样式合并进文档：
// RmlUI 的「文档 → 样式容器」是**整体替换**的，两条热重载路径都会换掉它——
//   * F6：ElementDocument::ReloadStyleSheet() 按 RML 的 <link> 重新生成容器；
//   * Ctrl+R：LoadDocument 加载出一个全新文档（连同新容器）。
// 每次替换后，先前合并进文档的脚本样式都会随之消失；而在旧容器上再合并一次又会
// 重复叠加。所以文档容器不能作为脚本样式的存放地。
//
// 这里保存的是「脚本样式原文」这一唯一事实来源，syncAll() 负责对账：
// 记住每个文档**自身**的样式容器作为基座，每次都用「基座 + 当前脚本样式」重新组合，
// 于是无论文档容器被替换多少次，脚本样式都只叠加一层。
//
// syncAll() 是幂等的：没有变化时只做几次指针比较，可以每帧调用。
// 文档的创建与替换都发生在主线程，主循环里的对账点足以覆盖全部路径
//（首次加载、F5 重载脚本、F6 重载样式、Ctrl+R 重建文档、新窗口）。
namespace ScriptStyle {

// 注册一个脚本样式表。name 只用于报错时标识来源，rcss 是样式表全文。
// 解析失败时返回 false 并打印原因，不影响其它已注册的样式表。
bool add(std::string_view name, std::string_view rcss);

// 丢弃全部已注册的脚本样式，并让各文档退回「只有自身样式表」的状态。
// 必须在重建 JS 上下文之前调用（见 JsEngine::hotReload）：脚本会被重新求值并重新
// 注册样式，旧脚本留下的样式不能再继续生效。
void clear();

// 对账：把当前注册的脚本样式同步到所有窗口文档上。幂等，可在主循环里每帧调用。
void syncAll();

}

#endif // NANOLIVELENS_SCRIPT_STYLE_HXX
