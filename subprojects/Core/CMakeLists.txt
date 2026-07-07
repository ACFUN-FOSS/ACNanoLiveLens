cmake_minimum_required(VERSION 3.21)

project(NanoLiveLens_Core LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Boost REQUIRED COMPONENTS dll process)

add_library(nanolivelens_core
    private/stdafx.cpp
    private/assets.cxx
    private/excp_to_show_to_user.cxx
	private/sound.cxx
    private/aclive_backend_daemon.cxx
    private/aclive_backend_client.cxx
    private/ws.cxx
)

add_library(nanolivelens::core ALIAS nanolivelens_core)

target_include_directories(nanolivelens_core
    PUBLIC public
    PRIVATE .
)

target_precompile_headers(nanolivelens_core PRIVATE public/Core/stdafx.h)

target_link_libraries(nanolivelens_core
    PUBLIC
    EatiEssentials
    nlohmann_json::nlohmann_json
	concurrencpp::concurrencpp
    PRIVATE
    Boost::dll
    Boost::process
    httplib
    cppcodec
	corrected_targets::Miniaudio
	Hs::Soloudpp
	
)

target_compile_definitions(nanolivelens_core PRIVATE
    NANOLIVELENS_CORE_BUILDING
)

# == Tests =================================
add_library(tests_public_dep STATIC test/dummy.cxx)

target_precompile_headers(tests_public_dep PUBLIC test/stdafx.h)
target_link_libraries(tests_public_dep
    PUBLIC
    nanolivelens::core
    httplib
)

add_executable(ws_test test/ws_test.cxx)
target_precompile_headers(ws_test REUSE_FROM tests_public_dep)
target_link_libraries(ws_test
    PRIVATE
    tests_public_dep
)

add_executable(ws_login_test test/ws_login_test.cxx)
target_precompile_headers(ws_login_test REUSE_FROM tests_public_dep)
target_link_libraries(ws_login_test
    PRIVATE
    tests_public_dep
)
 
