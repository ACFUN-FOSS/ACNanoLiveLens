#ifndef NANOLIVELENS_PCH_H
#define NANOLIVELENS_PCH_H

// nanolivelens 主程序的预编译头（PCH）。
//
// 约定（见 .trae/rules/coding-standard.md）：
//   * 本项目使用预编译头文件，各编译单元（含它们 include 的 .hxx）**只允许 include
//     项目内的头文件**；任何标准库、第三方库头文件都统一放在本文件里。
//   * 本文件已由编译选项自动包含（CMakeLists.txt 的 target_precompile_headers），
//     各处不要再显式 include 它。
//   * 例外：ElementQuickJsBinding 是模板库性质，其头文件允许在源文件中直接 include。
//
// 平台边界例外：src/platform/ 下与具体平台绑定的实现文件（*_win*.cxx 等）自带的
// <windows.h>、<psapi.h>、<imagehlp.h> 属于该平台专属依赖，留在各自文件里，不进 PCH——
// 否则会污染全部编译单元（参考仓库里让 Soloud 不再传递 windows.h 的那次改动）。

// == 标准库 ==========================================
#include <algorithm>
#include <any>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <limits>
#include <mutex>
#include <optional>
#include <print>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// == EATI Essentials 头文件（EATI C++ 支持）==========
#include <EatiEssentials/assert.hxx>
#include <EatiEssentials/io.hxx>
#include <EatiEssentials/memory/lifetime_informant.hxx>
#include <EatiEssentials/memory/memory.hxx>
#include <EatiEssentials/memory/memsafety.hxx>
#include <EatiEssentials/memory/moved_flag.hxx>
#include <EatiEssentials/misc.hxx>
#include <EatiEssentials/special.hxx>

// == 第三方库 ========================================
#include <RmlUi/Core.h>          // 已聚合 Core/Element.h、Core/ElementDocument.h、Core/StyleSheetContainer.h
#include <RmlUi/Core/StreamMemory.h>  // 不包含在上面那个聚合头里
#include <RmlUi_Backend.h>
#include <RmlUi_Platform_GLFW.h> // 同时带入 <GLFW/glfw3.h>
#include <boost/dll.hpp>
#include <boost/stacktrace.hpp>
#include <concurrencpp/concurrencpp.h>
#include <ctrl-c.h>
#include <httplib.h>
#include <metapp/allmetatypes.h>
#include <metapp/interfaces/metaclass.h>
#include <metapp/metatype.h>
#include <nlohmann/json.hpp>
#include <pimpl.hpp>
#include <quickjs/quickjs-libc.h>
#include <quickjspp.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>

// == 子项目 ==========================================
#include "Core/stdafx.h"
#include "Core/assert.hxx"
#include "Core/aclive_backend_daemon.hxx"
#include "Core/aclive_backend_client.hxx"
#include "Core/aclive_backend_msg.hxx"
#include "Core/ws.hxx"
#include "Core/assets.hxx"
#include "Core/sound.hxx"

namespace stdf = std::filesystem;
namespace coro = concurrencpp;

#if defined(__clang__)
#define METAPP_REFLECT __attribute__((annotate("metapp_reflect")))
#else
#define METAPP_REFLECT
#endif

#endif
