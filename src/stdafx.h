#ifndef NANOLIVELENS_PCH_H
#define NANOLIVELENS_PCH_H

// 标准库头文件
#include <chrono>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <print>
#include <string>
#include <thread>
#include <unordered_map>
#include <variant>



// EATI Essentials 头文件（EATI C++ 支持）
#include <EatiEssentials/memory.hxx>
#include <EatiEssentials/memsafety.hxx>
#include <EatiEssentials/special.hxx>
#include <EatiEssentials/io.hxx>
#include <EatiEssentials/misc.hxx>

// 第三方库头文件
//#include <glad/glad.h>
//#include <glfw/glfw3.h>// 第三方库头文件
#include <pimpl.hpp>
#include <RmlUI/Core.h>
#include <RmlUi_Backend.h>
#include <Soloudpp/Soloud.hxx>
#include <concurrencpp/concurrencpp.h>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <rfl.hpp>
#include <rfl/json.hpp>
#include <ctrl-c.h>

namespace stdf = std::filesystem;

#endif
