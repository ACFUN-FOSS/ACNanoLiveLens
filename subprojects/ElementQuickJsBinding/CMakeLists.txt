cmake_minimum_required(VERSION 3.21)
project(ElementEngine_QuickJsBinding LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 创建库
add_library(element_qjs_binding
    #private/element_qjs_binding.cxx
	private/ejs_obj.cxx
	private/lifetime_informant.cxx
	private/binding.cxx
)

# 链接依赖
target_link_libraries(element_qjs_binding
    PRIVATE
    Microsoft.GSL::GSL
    metapp::metapp
    quickjspp
    PUBLIC
	EatiEssentials
)

# 导出头文件
target_include_directories(element_qjs_binding
    PUBLIC public
    PRIVATE ${NLLENS_VENDOR_DIR}
)

add_executable(element_qjs_binding__lifetime_test
    test/lifetime_test.cxx
)

target_link_libraries(element_qjs_binding__lifetime_test
    PRIVATE
    element_qjs_binding
    quickjspp
    metapp::metapp
)

add_executable(element_qjs_binding__value_to_js_test
    test/value_to_js_test.cxx
    test/test_common.hxx
)

target_link_libraries(element_qjs_binding__value_to_js_test
    PRIVATE
    element_qjs_binding
    quickjspp
    metapp::metapp
)
