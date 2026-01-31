cmake_minimum_required(VERSION 3.21)

project(NanoLiveLens_EmergUI LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(unofficial-libui CONFIG REQUIRED)
find_package(reflectcpp CONFIG REQUIRED)
find_package(cxxopts CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS process)

add_executable(nanolivelens_emergui
	crash_dlg.cxx
	sound.cxx
	main.cxx
	fatal.ogg.cxx
)
add_executable(nanolivelens::emergui ALIAS nanolivelens_emergui)

add_library(nanolivelens_emergui_api
	api/private/emerg_ui.cxx
)
add_library(nanolivelens::emergui_api ALIAS nanolivelens_emergui_api)

target_include_directories(nanolivelens_emergui_api PUBLIC
    api/public/
)

target_link_libraries(nanolivelens_emergui_api PRIVATE
	Boost::process
	reflectcpp::reflectcpp
)

target_compile_definitions(nanolivelens_emergui_api PUBLIC
    EMERG_UI_EXE_NAME="$<PATH:GET_FILENAME,$<TARGET_FILE:nanolivelens_emergui>>"
)



set_target_properties(nanolivelens_emergui PROPERTIES VS_DPI_AWARE "PerMonitor")

target_link_libraries(nanolivelens_emergui
	PRIVATE	
	glfw
	unofficial::libui::libui
	reflectcpp::reflectcpp
	cxxopts::cxxopts
	nanolivelens_emergui_api
	Hs::Soloudpp
)


add_subdirectory(test)