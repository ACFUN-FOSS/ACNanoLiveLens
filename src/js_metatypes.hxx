#ifndef NANOLIVELENS_JS_METATYPES_HXX
#define NANOLIVELENS_JS_METATYPES_HXX

#include "appstate.hxx"
#include "danmaku_monitor_win.hxx"

#include <RmlUi/Core/Element.h>
#include <metapp/interfaces/metaclass.h>
#include <metapp/metatype.h>

// 暴露给 JavaScript 的 C++ 类型的 metapp 元类型声明。
//
// JsTwinObject 桥接完全由 metapp 的元类信息驱动：要让一个类型能被脚本使用，
// 只需在此处特化 DeclareMetaType（声明成员变量与成员方法），并在
// JsEngine::init() 中通过 regTypeStatic<T>() 注册，无需手写任何 JS 绑定代码。
//
// 注意：这些特化必须对所有可能实例化 getMetaType<T>() 的翻译单元可见（ODR），
// 因此定义在头文件中，而不是某个 .cxx 里。

// 弹幕信息。按值语义传给脚本（移植体），脚本可自行持有其副本，
// 不会因为弹幕被清理而失效。
template<>
struct metapp::DeclareMetaType<DanmakuMonitorWin::DanmakuInfo>
	: metapp::DeclareMetaTypeBase<DanmakuMonitorWin::DanmakuInfo>
{
	static void setup() {
		App::getGlobalMetaRepo().registerType<DanmakuMonitorWin::DanmakuInfo>("DanmakuInfo");
	}

	static const metapp::MetaClass *getMetaClass() {
		using DanmakuInfo = DanmakuMonitorWin::DanmakuInfo;
		static const metapp::MetaClass metaClass{
			metapp::getMetaType<DanmakuInfo>(),
			[](metapp::MetaClass &mc) {
				mc.registerVariable("sender", &DanmakuInfo::sender);
				mc.registerVariable("content", &DanmakuInfo::content);
				mc.registerCallable("timestampMs", &DanmakuInfo::timestampMs);
				mc.registerCallable("timestampText", &DanmakuInfo::timestampText);
			}
		};
		return &metaClass;
	}
};

// 弹幕在界面中的 GUI 信息。包含两个 UI 元素的引用，读取时返回继承
// 关联对象生命周期的孪生体（见文档「转换方法：孪生（Twining）」）。
template<>
struct metapp::DeclareMetaType<DanmakuMonitorWin::DanmakuGuiInfo>
	: metapp::DeclareMetaTypeBase<DanmakuMonitorWin::DanmakuGuiInfo>
{
	static void setup() {
		App::getGlobalMetaRepo().registerType<DanmakuMonitorWin::DanmakuGuiInfo>("DanmakuGuiInfo");
	}

	static const metapp::MetaClass *getMetaClass() {
		using DanmakuGuiInfo = DanmakuMonitorWin::DanmakuGuiInfo;
		static const metapp::MetaClass metaClass{
			metapp::getMetaType<DanmakuGuiInfo>(),
			[](metapp::MetaClass &mc) {
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
// 脚本拿到的元素总是孪生体（元素不可移动，也绝不能复制/移植到 JS VM 里），
// 因此这里只暴露一组安全的成员方法：读写样式类、属性与内容、查询尺寸与滚动位置。
// 返回指针的查询接口（GetElementById / QuerySelector）有意不暴露：它们在查找
// 失败时返回空指针，而空指针无法安全地转换为 JS 侧的孪生体。
template<>
struct metapp::DeclareMetaType<Rml::Element>
	: metapp::DeclareMetaTypeBase<Rml::Element>
{
	static void setup() {
		App::getGlobalMetaRepo().registerType<Rml::Element>("RmlUIEle");
	}

	static const metapp::MetaClass *getMetaClass() {
		using Ele = Rml::Element;
		static const metapp::MetaClass metaClass{
			metapp::getMetaType<Ele>(),
			[](metapp::MetaClass &mc) {
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

#endif // NANOLIVELENS_JS_METATYPES_HXX
