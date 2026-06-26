编码规则
###########

总是使用 `LIFETIMEBOUND_MEMBER` 以及 `LIFETIMEBOUND` 标记生命周期绑定的成员变量
================================================

虽然现在还没有 C++ 编译器完整实现支持生命周期分析（Lifetime Analysis），但本项目规定使用 `LIFETIMEBOUND_MEMBER` 以及 `LIFETIMEBOUND` 标记所有需要在对象生命周期内保持存在的变量。这是为了清晰地表达意图以及「生命期依赖线索」，从而让开发者在阅读代码时能够更容易看出生命周期错误（及构架设计不佳）、使得 LLM 能更准确清晰地检查生命周期错误。

被标记为 `LIFETIMEBOUND_MEMBER` 的可以是「视图」语义成员变量（如指针、引用、`std::string_view`）。它的合法使用检查规则如下：

* 如果这个视图所指向的变量 `v` 具有自动存储期，则 `this` 的生命周期与 `v` 绑定。`this` 的生命周期必须大于等于 `v` 的生命周期。
* 如果这个视图所指向的变量 `v` 具有动态存储期，则查找 `v` 的持有者（`Box`, `std::vector` 等）`v_owner`。

  * 如果 `v_owner` 是常数，则 `this` 的生命周期与 `v_owner` 绑定。`this` 的生命周期必须大于等于 `v_owner` 的生命周期。
  * 如果 `v_owner` 是变数，视图不是如 `std::weak_ptr` 的具有检查有效性的视图（如指针、`std::string_view`），则标记这个视图为「危险」。

运用被标记为 `LIFETIMEBOUND_MEMBER` 的视图，必须遵守以下规则：

* 任何此般视图，都必须在类对象被构造时初始化，且是 `gsl::not_null`。
* 任何 `std::weak_ptr` 都必须经过检查有效性后才能使用，直接使用会被标记为「危险」。

标记例子如下：

.. code-block:: cpp
   class MyWin
   {
   public:
      MyWin()
         : guiLibWin([&](){
            return GuiMan::createWin();
         }) {
         guiLibWin.onClick("btn_ok", [this](){
            if (auto guiLibWin_ = guiLibWin.lock()) {
               guiLibWin_->close();
            }
         });
      }

   private:
      std::weak_ptr<GuiLibWin> guiLibWin;
   };
