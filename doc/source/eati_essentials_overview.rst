EatiEssentials 总览
===================

本文档总结 `EatiEssentials` 在当前代码库中的定位、公开能力、设计取向与当前边界。

`EatiEssentials` 不是一个「大而全」的通用基础库，而更像是围绕当前项目实际需求逐步生长出来的一层项目级基础设施。它关注的是：让上层代码以更统一、更具语义的方式处理内存、生命周期、容器能力、文件读取和少量调试辅助。


定位
----

从工程角色上看，`EatiEssentials` 处在「标准库 / 第三方库」与「业务代码」之间。

它的主要任务不是替代标准库，而是：

- 提供少量稳定、统一的项目内语义包装
- 吸收一部分样板代码
- 将常见生命周期和所有权约束前置到接口层
- 为上层模块提供更适合当前项目风格的基础抽象

因此，理解 `EatiEssentials` 的最好方式不是把它看成「工具函数集合」，而是把它看成「这个项目自己的基础语言层」。


整体风格
--------

`EatiEssentials` 的整体风格有几个明显特征：

- 使用现代 C++：`C++23`、`concept`、`requires`、ranges、移动语义
- 强调语义包装：更倾向于暴露项目自己的轻量接口，而不是直接把原始 STL 和第三方类型摊开到业务层
- 偏生命周期导向：很多抽象都围绕“谁拥有谁、何时注册、何时清理”展开
- 接受工程务实：允许宏、轻量接口类、类型擦除，只要能改善可读性或调用体验

这说明它不是学术化、纯粹化的库，而是一个以工程实用为中心的基础设施层。


公开模块概览
------------

目前 `EatiEssentials` 的公开头主要包括：

- `memory.hxx`
- `memsafety.hxx`
- `io.hxx`
- `misc.hxx`
- `special.hxx`
- `container_and_view_and_ranges/container_and_view_and_range.hxx`
- `container_and_view_and_ranges/addandremoveable_container_ref.hxx`
- `container_and_view_and_ranges/container_lease.hxx`

此外，它还提供一个可选附加模块：

- `additional/CodeCvt/public/EatiEssentials/codecvt.hxx`

下面分别概括它们的职责。


memory.hxx
~~~~~~~~~~

这一层提供的是项目内统一的所有权命名和构造入口。

主要内容包括：

- `Box`：独占所有权，基于 `std::unique_ptr`
- `Rc`：共享所有权，基于 `std::shared_ptr`
- `Weak`：弱引用，基于 `std::weak_ptr`
- `Refw`：引用包装，基于 `std::reference_wrapper`
- `newBox` / `newRc`：简化构造调用

它的核心意义不在于增加功能，而在于：

- 让项目内部对所有权的表达更统一
- 降低上层代码直接暴露底层指针模板细节的频率

这类封装很轻，但在大型工程里有明显的语义收敛价值。


memsafety.hxx
~~~~~~~~~~~~~

这一层体现的是 `EatiEssentials` 对生命周期和空值安全的关注。

主要内容包括：

- `LIFETIMEBOUND`
- `UNWRAP`
- `EXCEPT`

它们解决的问题并不复杂，但方向很明确：

- 让借用关系更容易被工具链理解
- 让空指针解引用尽量在靠近调用处暴露
- 让“这里逻辑上不该为空”成为显式语义

这部分很能代表 `EatiEssentials` 的气质：  
不是重型静态分析框架，而是一些贴近日常开发的低成本安全语义增强。


io.hxx
~~~~~~

`io.hxx` 目前非常聚焦，核心就是文件读取：

- `readFile`
- `readFileRaw`

它的价值在于统一最常见的文本/二进制读取路径，而不是构建完整 IO 抽象层。

从风格上看，这种接口设计说明 `EatiEssentials` 倾向于：

- 做窄而明确的工具
- 避免过早引入复杂的流抽象或策略层
- 只在项目频繁重复出现的需求上建立公共接口


misc.hxx
~~~~~~~~

`misc.hxx` 当前承载的是一些暂时不值得单开模块、但又有稳定复用价值的小工具。

例如：

- `ptrToHex`

这类工具的特点通常是：

- 很轻
- 依赖少
- 面向调试或小型辅助需求

从模块边界上说，`misc.hxx` 更像一个「保守的杂项容器」，适合放那些尚未形成完整子领域的零散但通用的小功能。


special.hxx
~~~~~~~~~~~

`special.hxx` 提供的不是常规业务能力，而是偏调试、故障注入或特殊控制行为的入口，例如：

- `callNullptr()`
- `triggerDebugger()`

这类接口说明 `EatiEssentials` 并不只服务于正常路径，也考虑到：

- 调试
- 异常场景复现
- 平台相关问题排查

它在整个库中占比不大，但体现了一个工程化特征：基础库需要为“坏情况”提供工具，而不只是服务“好情况”。


additional/CodeCvt
~~~~~~~~~~~~~~~~~~

`additional/CodeCvt` 是 `EatiEssentials` 的可选附加模块，需要通过 `EESS_ENABLE_CODECVT` 显式启用。

从构建结构看，它被单独拆成：

- `EatiEssentials_CodeCvt`
- `EatiEssentials::CodeCvt`

这说明它并不被视为核心最小集的一部分，而是一个按需接入的扩展能力。

公开接口位于：

- `EatiEssentials/codecvt.hxx`

主要提供：

- 编码枚举 `Encoding`
- `encodingEnumToStr`
- `convertToUTF8`
- `convertToAnyMultibyteEncoding`
- `utf8ToUTF16`
- `utf8ToSysApiEncoding`
- `UnsupportedCodepageExcp`

从能力上看，这个模块解决的是“字符串编码在不同运行环境、不同系统 API、不同历史字符集之间转换”的问题，尤其面向：

- UTF-8 与多字节编码互转
- 中文、日文、韩文等东亚编码集
- Windows 系统 API 与跨平台字符串处理之间的桥接

它依赖 `Iconv` 和 `iconvpp`，说明这部分功能被明确视为：

- 外部依赖较强
- 平台与系统环境相关
- 不应强行并入核心最小基础库

这个拆分是合理的，因为编码转换常常是高价值能力，但又不是所有使用方都必须承担的依赖。


container_and_view_and_range.hxx
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

这一层体现的是 `EatiEssentials` 对 ranges 风格和异常语义的结合。

目前最代表性的能力是：

- `findOrThrow`

它把“在范围中查找对象，不存在就抛异常”这个常见模式做成了统一接口。

这说明该模块的目标不是包装全部 ranges，而是：

- 在项目常见数据访问模式上提供更具业务语义的辅助

也就是从「原始算法调用」提升为「可直接表达意图的访问原语」。


addandremoveable_container_ref.hxx
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

这是当前 `EatiEssentials` 中最有设计意味的模块之一。

它主要做两件事：

- 用 `concept` 描述“容器可插入、可遍历、可删除”的能力
- 用轻量接口对象擦除具体容器类型

当前核心构件包括：

- `AddandremoveableContainer`
- `AddandremoveableAssoContainer`
- `IAddandremoveableContainerRef`
- `IAddandremoveableAssoContainerRef`
- `AddandremoveableContainerRef`
- `AddandremoveableAssoContainerRef`

它的设计价值在于：

- 上层逻辑不必关心容器的精确类型
- 运行时可以绑定具体容器实例
- 编译期仍可校验容器是否满足所需能力

这正是 `EatiEssentials` 一个非常核心的抽象风格：  
先表达能力，再决定实现；先定义语义，再处理底层容器。


container_lease.hxx
~~~~~~~~~~~~~~~~~~~

这一层是在前一层基础上，进一步把“容器成员关系”抽象成生命周期对象。

目前主要包括：

- `ContainerLease`
- `AssoContainerLease`

它们采用典型的 `RAII` 模式：

- 构造时向容器注册元素或键值对
- 析构时自动移除

这类抽象的价值很高，因为它表达的不是普通数据结构操作，而是一种作用域绑定语义：

- 某个对象在某个作用域内「临时属于」某个集合
- 离开作用域后自动撤销该关系

这让很多“先 add 再 remove”的场景可以写得更安全、更清晰。


设计取向总结
------------

如果把 `EatiEssentials` 的设计理念压缩成几句话，我会这样总结：

- 它试图给项目提供一层统一的基础语义，而不是直接暴露底层库细节
- 它重视所有权、生命周期、注册关系这些工程里最容易出错的部分
- 它倾向先表达能力，再绑定具体实现
- 它接受轻量类型擦除和宏，只要能换来更顺手的 API

所以它不是「算法型工具库」，而是「工程语义型基础库」。


当前成熟度与边界
----------------

`EatiEssentials` 现在已经有明确风格，但整体上更像一个正在成长的内部基础库，而不是已经完全稳定的独立通用库。

目前能看到的特点包括：

- 模块数量不多，但方向已经清晰
- 某些命名仍偏早期风格，例如 `Addandremoveable*`
- 一些模块非常轻，甚至更像明确职责的小包装
- 抽象层次已有雏形，但尚未完全规范化或成体系展开

这意味着它的价值主要在于：

- 作为当前项目的内部基础设施
- 为后续抽象演进提供连续的设计起点

而不是立即承担“面向外部用户的大型公共库”职责。


适合继续演进的方向
------------------

基于当前状态，`EatiEssentials` 后续比较值得推进的方向包括：

- 统一公开 API 的命名和拼写风格
- 为每个公开模块补齐职责说明和使用边界
- 继续梳理哪些工具应上升为稳定基础设施，哪些只应停留在业务层局部辅助
- 对生命周期型抽象补充更多示例和测试
- 减少“轻量包装不断叠加但职责不清”的风险


总结
----

`EatiEssentials` 是一个偏现代 C++、偏工程语义导向的项目级基础设施库。

它最重要的价值不在于“提供了多少功能”，而在于它试图统一以下几件事：

- 所有权表达
- 生命周期管理
- 容器能力抽象
- 项目内部的基础语义接口

它最像的是「这个项目自己的底层语言层」，而不是「一个泛用工具箱」。  
后续如果继续演进，重点应放在 API 一致性、抽象边界清晰度和生命周期语义的稳定表达上。


附：VENDOR.md 中的诗
--------------------

下面附上 `vendor/Eati/VENDOR.md` 中收录的诗句，作为该 vendor 目录随附文本的一部分保留于此：

    反民抗霸二三载，血性从来不可违。

    寒窗磨剑锋芒出，破除乌云见星辉。

    舍身敢裂金枷锁，众志能熔万仞扉。

    谁道书生无骨气，一脚踹死神皇帝。

并附原文末句：

    当笔墨无法纠正倾斜的公正天平时，身体将成为最后的语言。
