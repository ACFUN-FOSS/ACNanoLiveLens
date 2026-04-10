project(quickjspp LANGUAGES CXX)

add_library(quickjspp INTERFACE)
target_include_directories(quickjspp INTERFACE .)
target_link_libraries(quickjspp INTERFACE quickjs_koush)
