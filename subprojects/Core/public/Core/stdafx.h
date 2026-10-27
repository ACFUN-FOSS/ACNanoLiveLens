#ifndef NANOLIVELENS_CORE_PCH_H
#define NANOLIVELENS_CORE_PCH_H

// NanoLiveLens_Core 的预编译头（PCH）。
//
// 约定（见 .trae/rules/coding-standard.md）：本 target 的各编译单元（含其 .hxx）
// 只允许 include 项目内的头文件，标准库 / 第三方库头文件一律集中在这里。
// 本文件已由编译选项自动包含，各处不要再显式 include 它。

// 标准库头文件
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <exception>
#include <filesystem>
#include <format>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <optional>
#include <print>
#include <stop_token>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <variant>
#include <vector>

// EATI Essentials 头文件
#include <EatiEssentials/memory/memory.hxx>

// 第三方库头文件
#include <cppcodec/base64_rfc4648.hpp>
#include <nlohmann/json.hpp>
#include <concurrencpp/concurrencpp.h>
#include <rfl.hpp>
#include <rfl/json.hpp>

#include "Core/rfl_custom_type.hxx"
#include "Core/assert.hxx"

namespace accoro = concurrencpp;
namespace stdf = std::filesystem;

// Private dependencies
#ifdef NANOLIVELENS_CORE_BUILDING
// EATI Essentials 头文件
#include <EatiEssentials/container_and_view_and_ranges/container_and_view_and_range.hxx>
#include <EatiEssentials/io.hxx>

// 第三方库头文件
#include <httplib.h>
#include <boost/process/v1.hpp>
#include <Soloudpp/Soloud.hxx>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#endif

#endif
