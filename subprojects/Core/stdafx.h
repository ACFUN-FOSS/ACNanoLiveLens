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

// EATI Essentials 头文件
#include <EatiEssentials/memory.hxx>

// 第三方库头文件
#include <cppcodec/base64_rfc4648.hpp>
#include <nlohmann/json.hpp>

// Private dependencies
#ifdef NANOLIVELENS_CORE_BUILDING
#include <httplib.h>
#endif

#endif
