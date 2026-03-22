#ifndef NANOLIVELENS_PCH_H
#define NANOLIVELENS_PCH_H

// 标准库头文件
#include <string>
#include <string_view>
#include <functional>
#include <iostream>
#include <ranges>
#include <filesystem>
#include <print>

namespace stdf = std::filesystem;
namespace stdr = std::ranges;
namespace stdv = std::views;

// EATI Essentials 头文件（EATI C++ 支持）
#include <EatiEssentials/memory.hxx>
#include <EatiEssentials/memsafety.hxx>
#include <EatiEssentials/special.hxx>
#include <EatiEssentials/io.hxx>
#include <EatiEssentials/misc.hxx>

// 第三方库头文件
//#include <glad/glad.h>
//#include <glfw/glfw3.h>
#include <RmlUI/Core.h>
#include <RmlUi_Backend.h>

#include <Soloudpp/Soloud.hxx>

#endif