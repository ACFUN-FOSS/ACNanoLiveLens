#ifndef NANOLIVELENS_WINFRAME_HXX
#define NANOLIVELENS_WINFRAME_HXX
#include <RmlUi/Core/Element.h>
#include <RmlUi/Core/ElementInstancer.h>
#include <RmlUi/Core/Math.h>

#include "rmlui_sys.hxx"
#include "rmluipp.hxx"


/**
 * 自定义一个元素代码太多了，我们必须把它封装一下，未来会有更多自定义元素。把这个类整理一下，
 * 成为所有自定义元素的基类？
 */

/**
 * 自定义元素看来比我想象得要复杂一些：
 * 
 * 我们希望采取「类对象创建即成立不变式」的原则（即「饮料瓶装饮料失败，就不该有饮料瓶，而不是构造
 * 一个「没有饮料的饮料瓶」，并要求每次检查其合法性再使用」），见核心指导方针 C.41 C.42 两条：
 * https://github.com/lynnboy/CppCoreGuidelines-zh-CN/blob/master/CppCoreGuidelines-zh-CN.md#c41-%E6%9E%84%E9%80%A0%E5%87%BD%E6%95%B0%E5%BA%94%E5%BD%93%E5%88%9B%E5%BB%BA%E7%BB%8F%E8%BF%87%E5%AE%8C%E6%95%B4%E5%88%9D%E5%A7%8B%E5%8C%96%E7%9A%84%E5%AF%B9%E8%B1%A1
 * https://github.com/lynnboy/CppCoreGuidelines-zh-CN/blob/master/CppCoreGuidelines-zh-CN.md#c42-%E5%BD%93%E6%9E%84%E9%80%A0%E5%87%BD%E6%95%B0%E6%97%A0%E6%B3%95%E6%9E%84%E9%80%A0%E6%9C%89%E6%95%88%E5%AF%B9%E8%B1%A1%E6%97%B6%E5%BA%94%E5%BD%93%E6%8A%9B%E5%87%BA%E5%BC%82%E5%B8%B8
 *
 * 理想状态是：一个自定义类会假定它所需要的 Rml 元素都存在，且其成员函数以及外部的调用者不需要检查
 * 这些元素是否存在（即「类对象创建即成立不变式」）。因此，如果在构造函数中无法满足这个假定，就抛出
 * 异常，构造失败，不创建对象，不存在一个「存在但非法的对象」。
 * 
 * 但是我们没法让「所有子 Elements 指针都是有效的」成为不变式，因为 hotreload 会更新他们，从而可能
 * 让他们失效。而且，自定义元素内部的逻辑可能会更改 DOM，删除和添加子元素，也会让这些指针失效。
 * 
 * 我现在的想法是：
 * 
 * 1. 如同 web 中一样，不能直接保存子元素的指针，而是每次需要使用时都通过 id 查找一次。这样就算
 *    hotreload 了，或者 DOM 结构改变了，我们也能找到正确的元素。但是这样可能不能及时发现问题，
 *    直到对应的逻辑被执行。不过这个感觉是最简单的。
 * 
 * 2. 创建一个内部结构体 “RequiredElements”，里面装有所有假定存在的子元素的 id。每次 hotreload
 *    或者构造函数被调用时，都会检查这个结构体里列出的元素是否都存在。每当需要使用这些子元素时，都会
 *    调用 getRequiredElements 获取一个 RequiredElements 引用。这个 getRequiredElements 
 *    会检查这些元素是否都存在。但这样有点不现实，因为将假定的文档结构通过 RequiredElements 来描
 *    述，并实现检查的逻辑可能并不容易。
 * 
 * 我们不能使用使用 assert / UNWRAP / EXCEPT 使得不满足条件时让程序崩溃，因为一个自定义元素的
 * RmlUI 是用户可编辑的，不是由开发者在开发时决定的。我们希望当用户写完样式表，按下快捷键
 * hotreload 时，不会一下把整个程序干崩溃。
 * 
 */

/**
 * 还有一个问题，RmlUI 貌似是不使用异常的。如果有其他原因导致自定义元素构造失败，抛出了异常，会直接把
 * RmlUI 内部状态干坏。这个怎么解决？我们要禁止所有自定义元素的构造函数抛出异常吗？还是说再封装一层？
 */

class WinFrame : public Rml::Element
{
public:
	WinFrame(const std::string_view tag);
	WinFrame(const WinFrame &) = delete;
	WinFrame(WinFrame &&) = delete;
	~WinFrame();
	WinFrame &operator=(const WinFrame &) = delete;
	WinFrame &operator=(WinFrame &&) = delete;


	static void reg(RmlUISystem &rmlui);
	
private:
	void OnUpdate() override;
	void ProcessDefaultAction(Rml::Event &event) override;
	void initAfterConstruct();
	void reload();
	void bindEventHandlers();

	bool firstInited = false;

	// Dragging
	bool isDragging_ = false;
	Rml::Vector2i mousePosWhenBeginDrag_;

	SimpleEventListenerManager eventListenerMan_{ *this };

	//TestListener testListener{ this };

};


#endif //NANOLIVELENS_WINFRAME_HXX
