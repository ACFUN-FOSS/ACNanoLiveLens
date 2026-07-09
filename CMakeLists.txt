cmake_minimum_required(VERSION 3.30)

project(NanoLiveLens LANGUAGES CXX)
option(NLLENS_ASSETS_DIR_OVERRIDE "资产目录重载。若设定，应用程序将强行在指定的目录中寻找资产。")
option(NLLENS_ASSETS_DIR_OVERRIDE_IS_ABS_PATH "资产目录重载是否是绝对路径。" false)
option(NLLENS_DEVELOPMENT "是否为开发模式。" false)
set(NLLENS_VENDOR_DIR ${CMAKE_CURRENT_LIST_DIR}/vendor)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
list(APPEND CMAKE_MODULE_PATH
	${CMAKE_CURRENT_LIST_DIR}/cmake
    ${CMAKE_CURRENT_LIST_DIR}/vendor/Eati
    ${CMAKE_CURRENT_LIST_DIR}/vendor/Eati/Essentials/cmake/modules
)
include(FetchContent)
include(CorrectedCmakeTargetForThirdpartyLibs/MiniaudioCorrectedTarget)
include(eati_cmake_tools/Dependencies)
include(MetappReflection)

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    # Require a unified runtime output directory.
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

# == Compiler settings ================
add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/external:anglebrackets>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/external:W0>")
#add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/fsanitize=address>")

# == Platform macros ==================
# This include sets these compile definitions for the whole project:
# - NLLENS_PLATFORM_ARCH_<ARCH>
# - NLLENS_PLATFORM_ARCH="<arch>"
# - NLLENS_PLATFORM_OS_<OS>
# - NLLENS_PLATFORM_OS="<os>"
include(NLLensPlatformDefinitions)

if(NLLENS_ASSETS_DIR_OVERRIDE)
    if(NLLENS_ASSETS_DIR_OVERRIDE_IS_ABS_PATH)
        add_compile_definitions(NLLENS_ASSETS_DIR_OVERRIDE_FULL_PATH="${NLLENS_ASSETS_DIR_OVERRIDE}")
    else()
        add_compile_definitions(NLLENS_ASSETS_DIR_OVERRIDE_FULL_PATH="${CMAKE_SOURCE_DIR}/${NLLENS_ASSETS_DIR_OVERRIDE}")
    endif()
endif()

if(NLLENS_DEVELOPMENT)
    add_compile_definitions(NLLENS_DEVELOPMENT)
endif()


# == Dependencies ==========================
find_package(Boost REQUIRED COMPONENTS dll stacktrace_from_exception)
find_package(glfw3 CONFIG REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(RmlUi CONFIG REQUIRED)
find_package(portaudio CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(concurrencpp CONFIG REQUIRED)
find_package(reflectcpp CONFIG REQUIRED)
find_package(Stb REQUIRED)
fetch_header_only_library(https://raw.githubusercontent.com/yhirose/cpp-httplib/refs/heads/master/httplib.h httplib)
fetch_header_only_library(https://raw.githubusercontent.com/mguludag/cpp-pimpl-helper/refs/heads/main/pimpl.hpp cpp_pimpl_helper)

add_subdirectory(vendor/quickjs)
add_subdirectory(vendor/RmlUiBackend)

include(thirdparty/ctrl-c-target.cmake)

# cppcodec
find_path(CPPCODEC_INCLUDE_DIRS "cppcodec/base32_crockford.hpp" REQUIRED)
add_library(cppcodec INTERFACE)
target_include_directories(cppcodec INTERFACE ${CPPCODEC_INCLUDE_DIRS})

# stb
add_library(stb INTERFACE)
target_include_directories(stb INTERFACE ${Stb_INCLUDE_DIR})

# metapp
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_Declare(
  metapp
  GIT_REPOSITORY https://github.com/wqking/metapp.git
  GIT_TAG        origin/master
)
FetchContent_MakeAvailable(metapp)
unset(CMAKE_POLICY_VERSION_MINIMUM)

# glfw (TheCherno fork)
# FetchContent_Declare(
#   glfw
#   GIT_REPOSITORY https://github.com/TheCherno/glfw.git
#   GIT_TAG        origin/master
# )
# FetchContent_MakeAvailable(glfw)

# Import EatiEssentials via FetchContent in order to allow other dependencies
# require EatiEssentials with `find_package(EatiEssentials)` (if there are)
# e.g.:
#
# find_package(EatiEssentials CONFIG REQUIRED COMPONENT CodeCvt)
# EatiEssentials_EnsureReallyHasComponents(CodeCvt)
set(EESS_ENABLE_CODECVT ON)
FetchContent_Declare(
    EatiEssentials
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/vendor/Eati/Essentials
    OVERRIDE_FIND_PACKAGE
)
FetchContent_MakeAvailable(EatiEssentials)

# boost stacktrace
Dep_AddBoostStacktraceImplLib()
find_package(boost_stacktrace_from_exception CONFIG REQUIRED)

# miniaudio and Hs::Soloudpp
AddMiniaudioTarget()
set(SOLOUD_DEP_MINIAUDIO_TARGET corrected_targets::Miniaudio)
add_subdirectory(vendor/Hs/soloud/contrib/simple_with_miniaudio SYSTEM)
add_subdirectory(vendor/Hs/Soloudpp)

# qjspp
add_subdirectory(vendor/quickjspp)

# == Subprojects ============================
add_subdirectory(subprojects/Core)
add_subdirectory(subprojects/RmlUIWin)
add_subdirectory(subprojects/EmergUI)
# add_subdirectory(subprojects/ElementQuickJsBinding)

# == Targets ================================

add_executable(nanolivelens
    src/stdafx.cpp
    src/main.cpp
    src/rmlui_sys.cxx
    src/rmluipp.cxx
    src/platform/crash_handler.cxx
    src/platform/tools.cxx
	src/utils.cxx
    src/appstate.cxx
    src/safe_element_instancer.cxx
    src/rmlui_element.cxx
	src/uiwin_bizlogic_obj_async_op_scope.cxx
	# src/js_binding.cxx

	# Elements
	src/winframe.cxx
	src/danmaku_item.cxx

	# Windows
	src/test_win.cxx
	src/danmaku_monitor_win.cxx
	src/msg_box.cxx
	src/login_win.cxx

	# JavaScript bindings
	# src/js_bindings.cxx
 )
target_include_directories(nanolivelens PRIVATE src)
target_precompile_headers(nanolivelens PRIVATE src/stdafx.h)

# 平台相关
# 1. 崩溃处理器
if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
	target_sources(nanolivelens PRIVATE
        src/platform/crash_handler_impl_win_msvc.cxx
    )
	target_link_libraries(nanolivelens
        PRIVATE
        psapi.lib
        dbghelp.lib
    )
elseif(UNIX)
	# TODO: 实现 Unix 平台的崩溃处理器
	target_sources(nanolivelens PRIVATE
        src/platform/crash_handler_impl_unix.cxx
    )
else()
	message(WARNING Implement your system's crash handler. Dummy implementation is used.)
	target_sources(nanolivelens PRIVATE
        src/platform/crash_handler_impl_dummy.cxx
    )
endif()

# 模态窗口激活优化
if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    target_sources(nanolivelens PRIVATE
        src/platform/window_activation_optimization_win.cxx
    )
#elseif(UNIX)
	# TODO: 实现 X11, Wayland, 苹果平台的模态窗口激活优化
else()
    message(WARNING Implement your system's window activation optimization. Dummy implementation is used.)
    target_sources(nanolivelens PRIVATE
        src/platform/window_activation_optimization_dummy.cxx
    )
endif()


target_link_libraries(nanolivelens
    PRIVATE
    rmlui_win
    rmlui_backend_GLFW_GL3
    RmlUi::RmlUi
    glfw
    Microsoft.GSL::GSL
    Dep::boost_stacktrace_impl
    Boost::stacktrace_from_exception
    Boost::dll
    EatiEssentials
	nanolivelens::emergui_api
    nlohmann_json::nlohmann_json
    concurrencpp::concurrencpp
    httplib
	cpp_pimpl_helper
    reflectcpp::reflectcpp
    ctrl-c
	nanolivelens_core
	quickjspp
	# element_qjs_binding
	metapp::metapp
    #${CMAKE_CURRENT_BINARY_DIR}/vcpkg_installed/x64-windows/debug/lib/duktape.lib
)
# gen_metapp_reflection(nanolivelens
# 	HEADERS 
# 	src/danmaku_monitor_win.hxx
# 	AUTO_INCLUDE
# 	src/stdafx.h
# )

set_target_properties(nanolivelens PROPERTIES
	VS_GLOBAL_EnableCppCoreCheck true
	VS_GLOBAL_CodeAnalysisRuleSet CppCoreCheckRules.ruleset
	VS_GLOBAL_RunCodeAnalysis true
)

add_dependencies(nanolivelens nanolivelens::emergui)
