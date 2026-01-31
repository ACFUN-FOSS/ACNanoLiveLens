add_executable(nanolivelens_emergui_test1
	1.cxx
)

target_link_libraries(nanolivelens_emergui_test1 PRIVATE	
	nanolivelens::emergui_api
)

add_dependencies(nanolivelens_emergui_test1 nanolivelens_emergui)

add_executable(nanolivelens_emergui_test2
	2.cxx
)

target_link_libraries(nanolivelens_emergui_test2 PRIVATE	
	nanolivelens::emergui_api
)

add_dependencies(nanolivelens_emergui_test2 nanolivelens_emergui)
