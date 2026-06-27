# Repository Guidelines

## 项目简介

`AcfunNanoLiveLens` 是一个为 AcFun 主播打造的小巧简单的直播动态查看小工具。它的用途是将直播间的包括弹幕、礼物等动态反馈给主播，便于主播掌握直播间情况。

它的设计目标是：

- 占用资源低，适合一直开着，为游戏留出性能空间。
- 界面简单，「给我展示关键信息，不碍事」。
- 不需要配置就能运行，但也可以根据需要进行界面美化。
- 不论在何种平台上，均不需要安装。
- 理论上跨平台。

## 技术栈

- 语言：C++23，CMake
- 音频：SoLoudpp
- 界面：GLFW, RmlUi, libui
- 网络：httplib
- 协程支持：concurrencpp
- 数据处理：nlohmann\_json, reflectcpp

## 项目结构与模块组织

业务逻辑并不直接基于 RmlUi 等库。为了方便开发，项目具有一些封装作为上层逻辑的地基。这主要包括：

- 窗口管理（`subprojects/RmlUIWin`）。
- RmlUI 组件及功能封装（`src/rmlui_element` ,`src/rmlui_sys`，`src/rmluipp`）。
- WebSocket 客户端以及在其基础上建立的 AcliveBackend 客户端（`subprojects/Core`）。
- 用于崩溃后显示错误信息的 EmergUI 模块（`subprojects/EmergUI`）。

主程序位于 `src/`，采用 `.hxx` / `.cxx` 配对组织；界面与运行资源位于 `assets/`；可复用子项目位于 `subprojects/`。第三方代码集中在 `vendor/` 与 `thirdparty/`；设计与规则文档位于 `doc/source/`。新增代码优先放入现有模块，不要把通用基础设施散落到业务目录。

## 构建、测试与开发命令

推荐使用 CMake 预设：

- `cmake --preset develop`：生成 `BUILD/`，默认 Debug，并启用 `compile_commands.json`。
- `cmake --build BUILD`：构建主程序 `nanolivelens` 及其依赖。
- `cmake --build BUILD --target ws_test ws_login_test`：仅构建 `subprojects/Core/test/` 下的测试目标。
- `ctest --test-dir BUILD --output-on-failure`：执行已注册到 CTest 的测试；若目标未注册，则直接运行 `BUILD/bin/` 下测试可执行文件。

`CMakePresets.json` 提供的预设都是依赖 vcpkg 的，使用他们时，请确认 vcpkg 工具链可用。项目不是紧依赖 vcpkg 的，只要保证 `find_package` 能找到对应的库即可。
开发期默认通过 `NLLENS_ASSETS_DIR_OVERRIDE=assets` 从仓库内资源目录加载资产；新增资源时保持相对路径稳定，避免写死绝对路径。

## 编码风格与命名

本项目使用 `C++23`、UTF-8 编码、CRLF 换行、制表符缩进。类型名使用 PascalCase，如 `AppState`；接口常用 `I` 前缀；使用 EatiEssentials 库提供的工具，涉及生命周期的视图成员应使用 `LIFETIMEBOUND` / `LIFETIMEBOUND_MEMBER` 标记；优先复用现有薄封装，如 `Box`、`Rc`、`Weak`、`UNWRAP`。

## 测试要求

测试代码主要位于 `subprojects/Core/test/`，命名模式为 `*_test.cxx`。现有测试偏行为驱动和集成验证，允许日志输出，但必须能明确判断通过或失败。新增网络、异步或回调相关逻辑时，优先补充端到端测试，并覆盖移动语义、生命周期和异常路径。
