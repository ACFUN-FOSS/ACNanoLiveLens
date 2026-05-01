cmake_minimum_required(VERSION 3.21)
project(ElementEngine_QuickJsBinding LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 创建库
add_library(element_qjs_binding
    private/element_qjs_binding.cxx
)

# 链接依赖
target_link_libraries(element_qjs_binding
    PRIVATE
    Microsoft.GSL::GSL
    metapp::metapp
    quickjspp
	EatiEssentials
)

# 导出头文件
target_include_directories(element_qjs_binding
    PUBLIC public
    PRIVATE ${NLLENS_VENDOR_DIR}
)
