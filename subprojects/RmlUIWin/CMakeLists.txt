cmake_minimum_required(VERSION 3.21)

project(RmlUIWin LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 创建库
add_library(rmlui_win
    private/window_manager.cpp
)

# 链接依赖
target_link_libraries(rmlui_win
    PRIVATE
    rmlui_backend_GLFW_GL3
    RmlUi::RmlUi
    glfw
    Microsoft.GSL::GSL
)

# 导出头文件
target_include_directories(rmlui_win
    PUBLIC public
)


