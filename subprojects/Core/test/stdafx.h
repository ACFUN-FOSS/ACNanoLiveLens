#ifndef NANOLIVELENS_CORE_TEST_PUBLIC_PCH_H
#define NANOLIVELENS_CORE_TEST_PUBLIC_PCH_H

// Core 测试的预编译头（PCH），由 tests_public_dep 以 PUBLIC 方式提供，
// 因此也会作用于链接它的 ws_test / ws_login_test。
//
// 约定（见 .trae/rules/coding-standard.md）：各编译单元只允许 include 项目内的头文件，
// 标准库 / 第三方库头文件一律集中在这里。本文件已由编译选项自动包含。
//
// 标准库依赖无需在此重复罗列——下面 include 的 <Core/stdafx.h> 已经提供；
// 这里只补它没有覆盖的部分。

// 标准库头文件（<Core/stdafx.h> 未覆盖的部分）
#include <cstdlib>
#include <iostream>

// EATI Essentials 头文件
#include <EatiEssentials/assert.hxx>
#include <EatiEssentials/memory/memory.hxx>

// 第三方库头文件
#include <cppcodec/base64_rfc4648.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>

// 项目内头文件
#include <Core/stdafx.h>
#include <Core/ws.hxx>

#endif
