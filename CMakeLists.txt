cmake_minimum_required(VERSION 3.21)

project(NanoLiveLens LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 查找依赖
find_package(glfw3 CONFIG REQUIRED)
find_package(Microsoft.GSL CONFIG REQUIRED)
find_package(RmlUi CONFIG REQUIRED)

# 添加子项目
add_subdirectory(vendor/RmlUiBackend)
add_subdirectory(subprojects/RmlUIWin)

# 创建可执行文件
add_executable(nanolivelens
    src/main.cpp
)

# 链接依赖
target_link_libraries(nanolivelens
    PRIVATE
    rmlui_win
    rmlui_backend_GLFW_GL3
    RmlUi::RmlUi
    glfw
    Microsoft.GSL::GSL
)

# 复制资源文件
add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/assets
        ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets
    COMMENT "Copying assets to runtime directory"
)

add_dependencies(nanolivelens copy_assets)