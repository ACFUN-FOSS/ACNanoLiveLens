#ifndef NANOLIVELENS_CORE_PCH_H
#define NANOLIVELENS_CORE_PCH_H

// 标准库头文件
#include <atomic>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <utility>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <stop_token>
#include <future>
#include <vector>
#include <variant>
#include <chrono>
#include <format>

// EATI Essentials 头文件
#include <EatiEssentials/memory/memory.hxx>

// 第三方库头文件
#include <cppcodec/base64_rfc4648.hpp>
#include <nlohmann/json.hpp>
#include <concurrencpp/concurrencpp.h>
#include <rfl.hpp>
#include <rfl/json.hpp>

#include "Core/rfl_custom_type.hxx"

namespace accoro = concurrencpp;
namespace stdf = std::filesystem;

// Private dependencies
#ifdef NANOLIVELENS_CORE_BUILDING
#include <EatiEssentials/container_and_view_and_ranges/container_and_view_and_range.hxx>
#include <EatiEssentials/io.hxx>
#include <httplib.h>
#include <boost/process/v1.hpp>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
#endif

#endif
