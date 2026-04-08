project(quickjs_koush LANGUAGES C)

file(STRINGS VERSION version)

set(CMAKE_C_STANDARD 99)
set(CMAKE_C_STANDARD_REQUIRED TRUE)

# If we're embedding in a top-level project via `add_subdirectory`
if(PROJECT_IS_TOP_LEVEL)
	set(QJS_IS_EMBEDDING_IN_OTHER_PROJ false)
else()
	set(QJS_IS_EMBEDDING_IN_OTHER_PROJ true)
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    set(QUICKJS_DEBUGGER_TRANSPORT_SRC "quickjs-debugger-transport-win.c")
elseif(UNIX)
    set(QUICKJS_DEBUGGER_TRANSPORT_SRC "quickjs-debugger-transport-unix.c")
else()
    message(FATAL_ERROR "Unsupported target OS.")
endif()

set(quickjs_src quickjs.c libbf.c libunicode.c libregexp.c cutils.c quickjs-libc.c quickjs-debugger.c ${QUICKJS_DEBUGGER_TRANSPORT_SRC})

set(quickjs_private_def CONFIG_VERSION="${version}" _GNU_SOURCE)
set(quickjs_public_def JS_STRICT_NAN_BOXING JS_NAN_BOXING USE_BF_DEC)

add_library(quickjs_koush SHARED ${quickjs_src})
set_target_properties(quickjs_koush PROPERTIES
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)
target_compile_definitions(quickjs_koush PRIVATE ${quickjs_private_def} PUBLIC ${quickjs_public_def})
set_target_properties(quickjs_koush PROPERTIES PUBLIC_HEADER "quickjs.h;quickjs-libc.h")

add_library(quickjs_koush-dumpleaks SHARED ${quickjs_src})
target_compile_definitions(quickjs_koush-dumpleaks PRIVATE ${quickjs_private_def} PUBLIC ${quickjs_public_def} DUMP_LEAKS=1)

#target_include_directories(quickjs PUBLIC .)
set(QUICKJS_LINK_LIBS)
if(UNIX OR MINGW)
  find_package(Threads)
  list(APPEND QUICKJS_LINK_LIBS ${CMAKE_DL_LIBS} m Threads::Threads)
endif()

if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
    list(APPEND QUICKJS_LINK_LIBS ws2_32)
endif()

target_link_libraries(quickjs_koush ${QUICKJS_LINK_LIBS})
target_link_libraries(quickjs_koush-dumpleaks ${QUICKJS_LINK_LIBS})

# install
if(NOT QJS_IS_EMBEDDING_IN_OTHER_PROJ)
    include(GNUInstallDirs)
    install(TARGETS quickjs
        PUBLIC_HEADER DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/quickjs
        ARCHIVE       DESTINATION ${CMAKE_INSTALL_LIBDIR}/quickjs
        LIBRARY       DESTINATION ${CMAKE_INSTALL_LIBDIR}/quickjs
    )
endif()
