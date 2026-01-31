cmake_minimum_required(VERSION 3.30)

project(NanoLiveLens LANGUAGES CXX)
option(NLLENS_ASSETS_DIR_OVERRIDE "资产目录重载。若设定，应用程序将强行在指定的目录中寻找资产。")
option(NLLENS_ASSETS_DIR_OVERRIDE_IS_ABS_PATH "资产目录重载是否是绝对路径。" false)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)
list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR}/vendor/Eati/CorrectedCmakeTargetForThirdpartyLibs/)
include(MiniaudioCorrectedTarget)

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

if(NOT CMAKE_RUNTIME_OUTPUT_DIRECTORY)
    # Require a unified runtime output directory.
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
endif()

# == Dependencies ==========================
find_package(Boost REQUIRED COMPONENTS dll stacktrace_from_exception)
find_package(glfw3 CONFIG REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(RmlUi CONFIG REQUIRED)
find_package(portaudio CONFIG REQUIRED)
add_subdirectory(vendor/RmlUiBackend)

set(EESS_ENABLE_CODECVT ON)
set(EESS_ENABLE_MEMSAFETY ON)
add_subdirectory(vendor/Eati/Essentials)

Dep_AddBoostStacktraceImplLib()
find_package(boost_stacktrace_from_exception CONFIG REQUIRED)

AddMiniaudioTarget()
set(SOLOUD_DEP_MINIAUDIO_TARGET corrected_targets::Miniaudio)
add_subdirectory(vendor/Hs/soloud/contrib/simple_with_miniaudio)

add_subdirectory(vendor/Hs/Soloudpp)

# == Subprojects ============================
add_subdirectory(subprojects/RmlUIWin)
add_subdirectory(subprojects/EmergUI)

# == Targets ================================
add_executable(nanolivelens
    src/main.cpp
    src/winframe.cxx
    src/custom_elements.cxx
    src/rmlui_sys.cxx
    src/rmluipp.cxx
    src/platform/crash_handler.cxx
    src/platform/tools.cxx
    src/sound/sound.cxx
    src/assets.cxx
	src/utils.cxx
    src/appstate.cxx
)
target_include_directories(nanolivelens PRIVATE src)

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows" AND MSVC)
    target_sources(nanolivelens PRIVATE
        src/platform/crash_handler_impl_win_msvc.cxx
    )
    target_link_libraries(nanolivelens
        PRIVATE
        psapi.lib
        dbghelp.lib
    )
elseif(UNIX)

else()
    message(WARNING Implement your system's crash handler. Dummy implementation is used.)
    target_sources(nanolivelens PRIVATE
        src/platform/crash_handler_impl_dummy.cxx
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
    EatiEssentials::MemSafety
    corrected_targets::Miniaudio
    Hs::Soloudpp
	nanolivelens::emergui_api
)

if(NLLENS_ASSETS_DIR_OVERRIDE)
    if(NLLENS_ASSETS_DIR_OVERRIDE_IS_ABS_PATH)
        target_compile_definitions(nanolivelens PRIVATE NLLENS_ASSETS_DIR_OVERRIDE_FULL_PATH="${NLLENS_ASSETS_DIR_OVERRIDE}")
    else()
        target_compile_definitions(nanolivelens PRIVATE NLLENS_ASSETS_DIR_OVERRIDE_FULL_PATH="${CMAKE_SOURCE_DIR}/${NLLENS_ASSETS_DIR_OVERRIDE}")
    endif()
endif()

add_dependencies(nanolivelens nanolivelens::emergui)
