窗口系统
========

本文档说明当前项目中 `RmlUIWin` 窗口系统的职责、主循环中的位置、窗口关闭流程，以及与异步操作相关的生命周期约束。


概览
----

当前窗口系统的核心类型是：

- `RmlUIWin::UiWin`
- `RmlUIWin::WinManager`

它们共同负责：

- 创建和持有原生窗口
- 创建和持有 `Rml::Context`
- 驱动每帧的 `update / render`
- 管理窗口关闭与销毁
- 维护 `Rml::Context -> UiWin` 的反查映射

其中：

- `UiWin` 表示一个具体窗口实例
- `WinManager` 负责管理当前所有 `UiWin`

当前实现还额外要求：

- `UiWin` 只能由 `WinManager` 创建
- 业务层只能持有 `UiWin*` / `UiWin&` 这类非拥有引用
- 不允许业务层先自行构造 `UiWin`，再把所有权移交给 `WinManager`


主循环中的角色
--------------

在 `src/main.cpp` 里，窗口系统的典型驱动顺序是：

1. `Backend::ProcessEvents(false)`
2. 主线程协程执行器 `loop_once()`
3. `winMan.updateAll()`
4. `winMan.renderAll()`
5. `winMan.cleanupClosedWindows()`

这意味着：

- 输入和系统事件先被处理
- 协程恢复可能发生在窗口更新之前
- 窗口真正的销毁发生在 `cleanupClosedWindows()`


UiWin 的职责
------------

`UiWin` 目前持有两类数据：

- 原生层数据：`GLFWwindow`、`Rml::Context`、`Rml::ElementDocument`
- 自身状态：窗口名、文档路径、回调、关闭状态、异步操作计数

`UiWin` 对外主要提供：

- `update()`
- `render()`
- `reload()`
- `setShouldClose()`
- `startAsyncOp()`


WinManager 的职责
-----------------

`WinManager` 维护：

- `wins_`：所有窗口
- `context2Win_`：`Rml::Context* -> UiWin*`
- `modalWin_`：当前模态窗口

并且负责：

- 创建 `UiWin` 并立即纳入 `wins_`
- 在窗口销毁时统一撤销 `context -> UiWin` 映射

其中 `context2Win_` 很关键，因为很多 UI 元素需要通过：

- `getWinOfElement(const Rml::Element &)`
- `getWinOfContext(const Rml::Context &)`

反查自己所属的窗口。


关闭流程
--------

当前关闭流程分成两个阶段：

- 请求关闭
- 真正销毁


请求关闭
~~~~~~~~

关闭请求可能来自两类入口：

- 业务代码主动调用 `UiWin::setShouldClose()`
- 原生窗口已经被标记为关闭，`WinManager::cleanupClosedWindows()` 观察到 `Backend::ShouldWindowClose(...)`

应用级退出请求也遵循同一原则：

- 例如主循环收到 `Ctrl-C` 或 backend 请求退出时，应先转成“请求关闭所有窗口”
- 在所有窗口真正销毁前，主循环仍要继续驱动事件处理、协程 executor、窗口更新与清理
- 不能在仍有未完成异步操作时直接停止驱动主循环并析构 `WinManager`

`UiWin::setShouldClose()` 的行为不是立刻销毁窗口，而是：

- 将内部 `_shouldClose` 设为 `true`
- 根据当前异步操作数，决定是否已经允许真正关闭


异步延迟关闭
~~~~~~~~~~~~

为了避免协程恢复后继续访问已经销毁的 `UiWin`，窗口系统引入了异步操作计数。

接口是：

- `UiWin::startAsyncOp()`
- `UiWin::AsyncOpScope`

典型用法是：

- 协程开始时获取一个 `AsyncOpScope`
- `AsyncOpScope` 构造时把窗口异步计数 `+1`
- `AsyncOpScope` 析构时把窗口异步计数 `-1`

因此：

- 如果用户请求关闭窗口，但仍有异步操作未完成，窗口不会立刻进入真正关闭
- 只有当 `_shouldClose == true` 且异步计数归零时，才会调用 `Backend::SetShouldClose(...)`

这保证了：

- 协程执行期间窗口对象不会因为关闭请求而过早析构


真正销毁
~~~~~~~~

窗口真正被移除发生在 `WinManager::cleanupClosedWindows()` 中。

它的逻辑是：

1. 观察原生 backend 是否已经认为窗口可关闭
2. 将其同步为框架内部的关闭请求
3. 仅当 `shouldClose && runningAsyncOpCount == 0` 时，才允许该窗口从 `wins_` 中被删除

`wins_` 中的 `unique_ptr<UiWin>` 被删除后，会进入：

- `UiWin::~UiWin()`
- `UiWin::destroy()`


销毁顺序
--------

`UiWin::destroy()` 的当前顺序是：

1. `detachDocument()`
2. `Rml::RemoveContext(...)`
3. `WinManager::unregisterWindow(*this)`
4. 如果不是主窗口，再 `Backend::DestroyWindow(...)`

这个顺序不能随意改动，尤其是步骤 2 和 3。


为什么必须先 RemoveContext，再 unregisterWindow
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

这是当前窗口系统里一个非常重要的生命周期约束。

在实际运行中，`Rml::RemoveContext(...)` 期间，RmlUi 内部的 Debugger 插件可能触发：

- `Rml::Debugger::DebuggerPlugin::OnContextDestroy`
- `Rml::Debugger::DebuggerPlugin::ReleaseElements`
- `Rml::Context::Update`

这意味着：

- `Context` 销毁过程中，元素树仍可能再经历一次 `Update`
- 某些自定义元素的 `onUpdate()` 仍可能被调用

而这些元素里可能存在这样的代码：

- 通过 `getWinOfElement(*this)` 反查所属 `UiWin`

如果这时候已经提前执行了 `unregisterWindow(*this)`，就会导致：

- `context2Win_` 映射失效
- `getWinOfElement(*this)` 返回空指针
- 元素在最后一轮 `onUpdate()` 中崩溃

因此，正确顺序必须是：

- 先让 `Rml::RemoveContext(...)` 完成
- 再移除 `context -> UiWin` 映射


当前已知边界
------------

当前设计下，以下行为是成立的：

- 业务层不应该假设“收到关闭请求后绝不会再有任何 UI 更新”
- 在 `Context` 的销毁阶段，RmlUi 内部仍可能触发最后一轮元素更新
- 这类更新必须由框架层生命周期顺序保证安全，而不是靠业务层到处判空兜底

也就是说，窗口系统当前的保证方式是：

- 不要求每个业务元素在 `onUpdate()` 中自己防御「窗口反查为空」
- 而是保证在 `Context` 真正销毁完成前，窗口映射仍有效


推荐约束
--------

基于当前实现，建议遵守以下约束：

- 所有长生命周期协程，只要会在恢复后访问窗口对象，都应持有 `UiWin::AsyncOpScope`
- 不要在 `UiWin::destroy()` 中把 `unregisterWindow(*this)` 提前到 `Rml::RemoveContext(...)` 之前
- 如果未来引入新的插件或新的 `Context` 销毁回调，默认假设它们在销毁阶段仍可能触发元素更新


总结
----

当前窗口系统的关键设计点有两个：

- 关闭不是立即销毁，而是「请求关闭 -> 等待异步归零 -> 真正销毁」
- `Rml::Context` 的销毁不是一个完全静态的过程，销毁期间仍可能触发元素更新，因此窗口映射必须保留到 `RemoveContext` 完成之后

理解这两点，基本就能理解当前窗口系统的大部分生命周期行为。
