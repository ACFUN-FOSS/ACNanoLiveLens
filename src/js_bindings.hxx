#ifndef NANOLIVELENS_JS_BINDINGS_HXX
#define NANOLIVELENS_JS_BINDINGS_HXX

#include "danmaku_monitor_win.hxx"

// 例外：ElementQuickJsBinding 是模板库性质，其头文件允许直接 include（不进 PCH）。
#include <ElementQuickJsBinding/binding.hxx>

// ============================================================================
// 本应用暴露给 JavaScript 的全部绑定。
//
// 这是「想让脚本看见什么」的唯一入口。新增或修改脚本可见的 C++ 类型、成员变量、
// 成员方法、翻译器、JS 函数桥接签名、全局函数，都只改本文件与 js_bindings.cxx；
// js_engine.cxx 只负责运行时 / 上下文 / 脚本加载，不含任何绑定细节。
//
// 绑定分两层：
//   1. metapp 元类声明（本文件下半部分）：声明「类型长什么样」——有哪些成员变量与
//      成员方法。JsTwinObject 桥接完全由这些元信息驱动，无需手写任何 JS 绑定代码。
//   2. 注册（js_bindings.cxx 的 setupAll）：把这些类型、翻译器、桥接签名与全局函数
//      装进 Binding。
//
// 注意：元类特化必须对所有可能实例化 getMetaType<T>() 的翻译单元可见（ODR），
// 因此定义在头文件中，而不是某个 .cxx 里。
// ============================================================================

namespace JsBindings {

// 把本应用的全部 JS 绑定注册到 binding 上。
// 每次重建 JS 上下文（init() 与 hotReload()）都必须调用一次。
void setupAll(ElementEngine::QJSBinding::Binding &binding);

}

// ---------------------------------------------------------------------------
// 元类声明
// ---------------------------------------------------------------------------
//
// 通则：类型名要登记在自己 MetaClass 的回调里（mc.registerType<T>("名字")）。
// binding.cxx 的 getTypeName 走的是 MetaClass::getType，它只查该 MetaClass 自身的
// typeData；名字登记在别处会查不到，结果是类型名取到空串——JS 侧的 class name 与
// 转换失败时的报错都会退化成地址。
//
// 枚举例外：绑定层已经为**所有**枚举统一提供了「字符串 ↔ 枚举」，见
// ElementQuickJsBinding/metapp_enum_metatype.hxx —— 枚举值在脚本侧就是它的名字
// （playSound('RELOAD')），类型名也会自动带上。因此这里不需要为枚举写任何东西。

// 弹幕信息。按值语义传给脚本（移植体），脚本可自行持有其副本，
// 不会因为弹幕被清理而失效。
template<>
struct metapp::DeclareMetaType<DanmakuMonitorWin::DanmakuInfo>
	: metapp::DeclareMetaTypeBase<DanmakuMonitorWin::DanmakuInfo>
{
	static const metapp::MetaClass *getMetaClass() {
		using DanmakuInfo = DanmakuMonitorWin::DanmakuInfo;
		static const metapp::MetaClass metaClass{
			metapp::getMetaType<DanmakuInfo>(),
			[](metapp::MetaClass &mc) {
				mc.registerType<DanmakuInfo>("DanmakuInfo");
				mc.registerVariable("sender", &DanmakuInfo::sender);
				mc.registerVariable("content", &DanmakuInfo::content);
				// 时间点成员按原样登记：脚本读到的是 JS Date，写回时接受 Date。
				// 时间点与时间长度由绑定层统一转换（见
				// ElementQuickJsBinding/metapp_chrono_metatypes.hxx），因此不需要
				// 为它提供 timestampMs()/timestampText() 之类的辅助访问器。
				mc.registerVariable("timestamp", &DanmakuInfo::timestamp);
			}
		};
		return &metaClass;
	}
};

// 弹幕在界面中的 GUI 信息。包含两个 UI 元素的引用。
template<>
struct metapp::DeclareMetaType<DanmakuMonitorWin::DanmakuGuiInfo>
	: metapp::DeclareMetaTypeBase<DanmakuMonitorWin::DanmakuGuiInfo>
{
	static const metapp::MetaClass *getMetaClass() {
		using DanmakuGuiInfo = DanmakuMonitorWin::DanmakuGuiInfo;
		static const metapp::MetaClass metaClass{
			metapp::getMetaType<DanmakuGuiInfo>(),
			[](metapp::MetaClass &mc) {
				mc.registerType<DanmakuGuiInfo>("DanmakuGuiInfo");
				mc.registerVariable("danmakuItemAppearAnimContainerEle",
					&DanmakuGuiInfo::danmakuItemAppearAnimContainerEle);
				mc.registerVariable("danmakuEle", &DanmakuGuiInfo::danmakuEle);
			}
		};
		return &metaClass;
	}
};

// RmlUi 元素。
//
// 脚本拿到的元素总是孪生体（元素不可移动，也不能复制/移植到 JS VM 里），
// 返回指针的查询接口（GetElementById / QuerySelector）有意不暴露：它们在查找
// 失败时返回空指针，而空指针无法安全地转换为 JS 侧的孪生体。需要额外包装。
template<>
struct metapp::DeclareMetaType<Rml::Element>
	: metapp::DeclareMetaTypeBase<Rml::Element>
{
	static const metapp::MetaClass *getMetaClass() {
		using Ele = Rml::Element;
		static const metapp::MetaClass metaClass{
			metapp::getMetaType<Ele>(),
			[](metapp::MetaClass &mc) {
				mc.registerType<Ele>("RmlUIEle");
				mc.registerCallable("getTagName", &Ele::GetTagName);
				mc.registerCallable("getId", &Ele::GetId);
				mc.registerCallable("setClass", &Ele::SetClass);
				mc.registerCallable("isClassSet", &Ele::IsClassSet);
				mc.registerCallable("setClassNames", &Ele::SetClassNames);
				// SetProperty 有重载，这里只取字符串形式的那一个。
				mc.registerCallable("setProperty",
					static_cast<bool (Ele::*)(const Rml::String &, const Rml::String &)>(&Ele::SetProperty));
				mc.registerCallable("setInnerRML", &Ele::SetInnerRML);
				// GetInnerRML 同样有重载（String& 出参版 / 返回值版），取返回值版。
				mc.registerCallable("getInnerRML",
					static_cast<Rml::String (Ele::*)() const>(&Ele::GetInnerRML));
				mc.registerCallable("getOffsetHeight", &Ele::GetOffsetHeight);
				mc.registerCallable("getClientHeight", &Ele::GetClientHeight);
				mc.registerCallable("getScrollHeight", &Ele::GetScrollHeight);
				mc.registerCallable("getScrollTop", &Ele::GetScrollTop);
				mc.registerCallable("setScrollTop", &Ele::SetScrollTop);
			}
		};
		return &metaClass;
	}
};

#endif // NANOLIVELENS_JS_BINDINGS_HXX
