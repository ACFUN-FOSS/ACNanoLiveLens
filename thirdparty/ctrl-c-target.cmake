add_library(ctrl-c 
	${CMAKE_CURRENT_LIST_DIR}/ctrl-c/src/ctrl-c.cpp
)

target_include_directories(ctrl-c PUBLIC ${CMAKE_CURRENT_LIST_DIR}/ctrl-c/src)
