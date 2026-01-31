option(ENABLE_SARIF_DIAG "Enable SARIF diagnostic.")
option(SARIF_DIAG_FILE "SARIF diagnostic file.")

macro(common_compiler_setup)
	# MSVC compiler
	add_compile_options("$<$<C_COMPILER_ID:MSVC>:/utf-8>")
	add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/utf-8>")

	if(ENABLE_SARIF_DIAG)
		if(SARIF_DIAG_FILE)
			add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/experimental:log${SARIF_DIAG_FILE}>")
		else()
			add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/experimental:log${CMAKE_BINARY_DIR}/diags>")
		endif()
	endif()
endmacro(common_compiler_setup)

macro(warn_as_err TGT)
	target_compile_options(${TGT} PRIVATE
		"$<$<CXX_COMPILER_ID:MSVC>:/WX>"
		"$<$<CXX_COMPILER_ID:GNU>:/Werror>"
		"$<$<CXX_COMPILER_ID:Clang>:/Werror>"
	)
endmacro(warn_as_err)


macro(common_installation_setup)
	# https://gitlab.kitware.com/cmake/cmake/-/issues/18312
	if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
		if(CMAKE_SIZEOF_VOID_P MATCHES "8")
			string(REPLACE "Program Files (x86)" "Program Files" 
			CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
		endif()
	endif()
endmacro(common_installation_setup)
