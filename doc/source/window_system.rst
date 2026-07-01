窗口系统
========

本文档说明当前项目中 `RmlUIWin` 窗口系统的职责、所有权模型、主循环中的位置，以及窗口关闭与异步操作之间的约束。


概览
----

当前窗口系统的核心类型是：

- `RmlUIWin::UiWin`
- `RmlUIWin::WinManager`
- 窗口业务逻辑对象，例如 `LoginWin`

它们共同负责：

- 创建并持有原生平台窗口
- 创建并持有 `Rml::Context`
- 驱动每帧的 `update / render`
- 管理窗口显示、隐藏、请求关闭
- 维护 `Rml::Context -> UiWin` 的反查映射


所有权模型
----------

当前模型中，三个客体的生命周期是绑定的：

- 窗口业务逻辑对象
- `UiWin`
- 平台窗口

关系如下：

- 窗口业务逻辑对象直接拥有其主 `UiWin`
- `UiWin` 在构造时创建或接管平台窗口
- `UiWin` 在析构时销毁平台窗口
- `WinManager` 不拥有 `UiWin`，只登记当前存活窗口的非拥有引用

这意味着：

- `UiWin` 不再通过 `WinManager::createWindow(...)` 创建
- 业务层不再只持有 `UiWin*` / `UiWin&` 的弱引用模式
- `UiWin` 的生命周期不应长于所属业务逻辑对象


UiWin 构造与析构
----------------

`UiWin` 的当前职责包括：

- 持有原生层数据：`GLFWwindow`、`Rml::Context`、`Rml::ElementDocument`
- 持有自身状态：窗口名、文档路径、显示状态、`pendingClose` 状态、异步操作状态、回调
- 在构造时向 `WinManager` 自动注册
- 在析构时向 `WinManager` 自动注销

构造过程的语义是：

1. 创建或接管平台窗口
2. 创建 `Rml::Context`
3. 加载 `Rml::ElementDocument`
4. 注册到 `WinManager`
5. 初始状态保持隐藏

其中第 5 点非常重要：

- `UiWin` 构造完成后默认不会自动显示
- 即使是第一次运行，也必须显式调用 `show()`

析构过程的语义是：

1. 断言没有未完成的异步操作
2. 解除文档和 context 相关资源
3. 从 `WinManager` 注销
4. 销毁平台窗口

因此：

- 平台窗口的真正销毁只发生在 `UiWin::~UiWin()`
- `WinManager` 不负责销毁 `UiWin`
- `WinManager` 也不负责销毁平台窗口


WinManager 的职责
-----------------

`WinManager` 当前维护：

- `wins_`：所有已注册 `UiWin` 的非拥有引用
- `context2Win_`：`Rml::Context* -> UiWin*`
- `modalWin_`：当前模态窗口

它负责：

- 维护窗口注册表
- 维护 `context -> window` 反查映射
- 在主循环中统一执行 `updateAll()` / `renderAll()`
- 观察原生关闭请求，并将其转换成框架内部的 `requestClose()`

它不负责：

- 创建 `UiWin`
- 拥有 `UiWin`
- 销毁 `UiWin`

这与旧模型不同。当前 `WinManager` 是“窗口注册与调度器”，不是“窗口所有者”。


主循环中的角色
--------------

在 `src/main.cpp` 中，窗口系统的典型驱动顺序是：

1. `Backend::ProcessEvents(false)`
2. 主线程协程执行器 `loop_once()`
3. `winMan.updateAll()`
4. `winMan.renderAll()`
5. `winMan.cleanupClosedWindows()`

其中：

- `cleanupClosedWindows()` 不再删除窗口
- 它只负责把原生层“用户点了 X / 操作系统请求关闭”的事件，同步成 `UiWin::requestClose()`

当前主循环退出条件也已经变化：

- 不再是“是否还有注册窗口”
- 而是“是否还有可见窗口”

这是因为当前模型下：

- 窗口可以已经隐藏，但对象仍然存活
- 只有当最后一个可见窗口隐藏后，主循环才可以自然退出
- 业务对象析构通常发生在主循环结束后的作用域退出阶段


局部事件循环
------------

除了全局主循环外，当前还存在一种特殊路径：某些窗口会在自身内部运行局部事件循环，例如 `MsgBox::showModal()`。

这类局部事件循环的特点是：

- 它们会自行调用 `Backend::ProcessEvents(false)`
- 会自行驱动所属 `UiWin` 的 `update()` / `render()`
- 它们不是依赖“backend 认为窗口 should close”来退出
- 而是依赖该 `UiWin` 自身是否已经隐藏

这与当前窗口模型是一致的，因为：

- close 的语义已经不是“真正关闭并销毁窗口”
- close 的语义是“请求关闭，最终隐藏”

因此，对这类局部事件循环来说，正确的退出条件是：

- `UiWin::isHidden() == true`

而不是：

- `Backend::ShouldWindowClose(...) == true`

否则会和当前“关闭请求会被转化为 `requestClose()`，然后清除原生 close 标志”的机制冲突。


显示、隐藏与关闭
----------------

当前模型刻意区分三件事：

- `show`
- `hide`
- `close`


show
~~~~

`show()` 是程序化显示窗口：

- 仅在当前窗口已隐藏时生效
- 显示 `document`
- 显示平台窗口
- 触发 `show` 回调


hide
~~~~

`hide()` 是程序化隐藏窗口：

- 只能由程序代码主动调用
- 调用后立即隐藏
- 不检查是否仍有异步操作
- 不存在 `pendingHide` 概念

`hide()` 的语义不是“用户关闭了窗口”，而是“程序决定临时不显示该窗口”。


close
~~~~~

“关闭窗口”在当前模型中的实际效果是“请求关闭，然后最终隐藏”。

对外接口是：

- `requestClose()`
- `isPendingClose()`

关闭请求可能来自两类入口：

- 业务代码主动调用 `UiWin::requestClose()`
- 用户点击窗口 X，或操作系统发出关闭请求，随后由 `WinManager::cleanupClosedWindows()` 转成 `requestClose()`

当前设计中：

- close 不会直接销毁 `UiWin`
- close 也不会直接销毁平台窗口
- close 的最终效果是：窗口在满足条件后被 `hide()`


pendingClose 的语义
-------------------

`pendingClose` 表示：

- 这个窗口已经收到关闭请求
- 但当前还不能真正隐藏，因为仍有未完成的异步操作

当窗口处于 `pendingClose` 时：

- 框架会显示“正在结束未完成的工作”这类视觉提示
- 每帧 `update()` 都会再次检查是否已经允许关闭

一旦异步操作完成：

- `applyCloseRequestState()` 会调用 `hide()`
- 并清除 `pendingClose`

因此，当前 close 的完整语义是：

- 请求关闭
- 若可立即关闭，则立刻隐藏
- 若不可立即关闭，则进入 `pendingClose`
- 异步结束后自动隐藏


异步操作约束
------------

为了防止协程恢复后继续访问一个本应隐藏或即将析构的窗口，当前窗口系统为 `UiWin` 维护“是否存在未完成异步操作”状态。

目前业务层通过 `UiWinBizLogicObjAsyncOpScope` 表达这一约束。

它的语义是：

- 作用域开始时，标记对应 `UiWin` 正在进行异步操作
- 作用域结束时，清除该标记

这会影响两个行为：

- `requestClose()` 后是否可以立即隐藏
- `UiWin::~UiWin()` 是否允许执行

当前必须满足的约束是：

- 任何恢复后仍会访问窗口状态的长生命周期异步操作，都必须正确标记为进行中
- `UiWin` 析构时必须断言没有任何未完成异步操作

这意味着：

- “关闭窗口”不是异步安全边界
- “业务对象析构”才是最终生命周期结束点
- 如果异步作用域管理错误，最迟会在 `UiWin::~UiWin()` 上触发断言


原生关闭请求的处理
------------------

当前平台窗口的关闭按钮并不直接销毁窗口。

流程是：

1. 用户点击 X，或操作系统请求关闭窗口
2. backend 标记该原生窗口收到了关闭请求
3. `WinManager::cleanupClosedWindows()` 观察到该请求
4. 请求被转换为 `UiWin::requestClose()`
5. backend 的原生 `shouldClose` 标志被清除
6. 框架之后按照 `pendingClose -> hide` 的规则继续处理

因此：

- 原生关闭事件只是一种“关闭意图”
- 真正的框架语义仍然由 `UiWin` 内部状态机控制


销毁顺序
--------

`UiWin::destroy()` 的核心顺序仍然重要：

1. 解除文档相关绑定
2. `Rml::RemoveContext(...)`
3. `WinManager::unregisterWindow(*this)`
4. 销毁平台窗口

其中步骤 2 和 3 的顺序仍然不能随意交换。

原因是：

- `Rml::RemoveContext(...)` 期间，RmlUi 内部仍可能触发最后一轮与元素树相关的逻辑
- 这期间仍可能需要通过 `context -> UiWin` 反查所属窗口

所以当前约束仍然是：

- 先完成 `RemoveContext`
- 再移除 `context2Win_` 映射


对业务层的实际含义
------------------

业务层在当前模型下应当这样理解窗口：

- 业务对象拥有窗口
- `show()` 是显式显示
- `hide()` 是程序化立即隐藏
- `requestClose()` 是“用户语义的关闭”，其效果是“异步安全地隐藏”
- 窗口是否最终销毁，不取决于 close，而取决于业务对象是否析构

因此，推荐遵守以下约束：

- 不要把 `requestClose()` 理解成析构请求
- 不要把 `hide()` 和 `requestClose()` 混用
- 只有在明确要无条件立即隐藏时才调用 `hide()`
- 如果是“用户点击关闭按钮”或“程序想模拟用户关闭语义”，应调用 `requestClose()`
- 所有会跨帧恢复并访问窗口状态的异步操作，都必须正确登记异步作用域


总结
----

当前窗口系统的关键设计点有三个：

- 所有权绑定在“业务对象 -> UiWin -> 平台窗口”这条链上
- close 的语义是“请求关闭，最终隐藏”，而不是“立即销毁”
- `UiWin` 析构是平台窗口真正销毁的唯一入口，并且要求不存在未完成异步操作

理解这三点，基本就能理解当前窗口系统的大部分生命周期行为。
