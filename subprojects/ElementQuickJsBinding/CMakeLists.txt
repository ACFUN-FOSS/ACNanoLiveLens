cmake_minimum_required(VERSION 3.21)
project(ElementEngine_QuickJsBinding LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 创建库
add_library(element_qjs_binding
	private/ejs_obj.cxx
	private/binding.cxx
)

# 链接依赖
# reflectcpp is a public dependency: metapp_enum_metatype.hxx (a public
# header) uses rfl::string_to_enum / rfl::enum_to_string for the enum <->
# script string marshalling.
find_package(reflectcpp CONFIG REQUIRED)

target_link_libraries(element_qjs_binding
    PRIVATE
    Microsoft.GSL::GSL
    metapp::metapp
    quickjspp
	
    PUBLIC
	EatiEssentials
	cpp_pimpl_helper
	reflectcpp::reflectcpp
)

# 导出头文件
target_include_directories(element_qjs_binding
    PUBLIC public
    PRIVATE ${NLLENS_VENDOR_DIR}
)

add_executable(element_qjs_binding__regType_test
    test/regType_test.cxx
)

target_link_libraries(element_qjs_binding__regType_test
    PRIVATE
    element_qjs_binding
    quickjspp
    metapp::metapp
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

 add_executable(element_qjs_binding__property_test
     test/property_test.cxx
     test/test_common.hxx
 )

 target_link_libraries(element_qjs_binding__property_test
     PRIVATE
     element_qjs_binding
     quickjspp
     metapp::metapp
 )

 add_executable(element_qjs_binding__global_function_test
     test/global_function_test.cxx
     test/test_common.hxx
 )

 target_link_libraries(element_qjs_binding__global_function_test
     PRIVATE
     element_qjs_binding
     quickjspp
     metapp::metapp
 )

 add_executable(element_qjs_binding__enum_test
     test/enum_test.cxx
     test/test_common.hxx
 )

 target_link_libraries(element_qjs_binding__enum_test
     PRIVATE
     element_qjs_binding
     quickjspp
     metapp::metapp
 )

 add_executable(element_qjs_binding__chrono_test
     test/chrono_test.cxx
     test/test_common.hxx
 )

 target_link_libraries(element_qjs_binding__chrono_test
     PRIVATE
     element_qjs_binding
     quickjspp
     metapp::metapp
 )
