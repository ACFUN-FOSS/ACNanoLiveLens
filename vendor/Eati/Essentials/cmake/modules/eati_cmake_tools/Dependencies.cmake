function(fetch_header_only_library header_file_url mytarget)
    # 提取文件名
    get_filename_component(header_file_name "${header_file_url}" NAME)

    # 定义目标目录路径
    set(target_include_dir "${CMAKE_CURRENT_BINARY_DIR}/thirdparty_from_web/${target}")

    set(download_path "${target_include_dir}/${header_file_name}")

    # 如果设置了 FORCE_REDOWNLOAD，删除现有文件
    if(DEFINED FETCH_HEADER_ONLY_LIB__FORCE_REDOWNLOAD AND EXISTS "${download_path}")
        message(STATUS "Force redownload is enabled. Removing existing file: ${download_path}")
        file(REMOVE "${download_path}")
    endif()

    # 如果文件不存在，下载文件
    if(NOT EXISTS "${download_path}")
        message(STATUS "Downloading header-only library ${header_file_url} to ${download_path}...")

        # 下载头文件
        file(DOWNLOAD
            "${header_file_url}"
            "${download_path}"
            SHOW_PROGRESS
            STATUS download_status
        )

        # Separate the returned status code, and error message.
        list(GET download_status 0 status_code)
        list(GET download_status 1 err_msg)
        # Check if download was successful.
        if(${status_code} EQUAL 0)
            message(STATUS "Download completed successfully!")
        else()
            # Exit CMake if the download failed, printing the error message.
            message(FATAL_ERROR "Error occurred during download: ${err_msg}")
            file(REMOVE "${download_path}")
        endif()
    else()
        message(STATUS "Header file already exists: ${download_path}")
    endif()

    # 创建接口库
    add_library(${mytarget} INTERFACE)

    # 设置包含目录
    target_include_directories(${mytarget} INTERFACE "${target_include_dir}")
endfunction()

function(copy_dep_runtime_dll mytarget)
    if(NOT DEFINED MAINEXE_BUILD_DIR)
        message(FATAL_ERROR "In order to use `copy_dep_runtime_dll`, define MAINEXE_BUILD_DIR variable first.")
    endif()
    if(${CMAKE_SYSTEM_NAME} STREQUAL "Windows")
        add_custom_command(TARGET ${mytarget} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_RUNTIME_DLLS:${mytarget}> ${MAINEXE_BUILD_DIR}
            COMMAND_EXPAND_LISTS
        )
    endif()
endfunction()

macro(Dep_AddYalantingLibs)
    set(BUILD_EXAMPLES OFF)
    set(BUILD_BENCHMARK OFF)
    set(BUILD_UNIT_TESTS OFF)
    FetchContent_Declare(
        yalantinglibs
        GIT_REPOSITORY https://github.com/alibaba/yalantinglibs.git
        GIT_TAG v0.5.6
        GIT_SHALLOW 1 # optional ( --depth=1 )
    )
    FetchContent_MakeAvailable(yalantinglibs)
endmacro(Dep_AddYalantingLibs)


# Add a alias target boost_stacktrace_impl
macro(Dep_AddBoostStacktraceImplLib)
    if(WIN32)
        find_package(boost_stacktrace_windbg CONFIG REQUIRED)
        add_library(Dep::boost_stacktrace_impl ALIAS Boost::stacktrace_windbg)
    elseif(UNIX)
        find_package(boost_stacktrace_xxxxx CONFIG REQUIRED)
        add_library(Dep::boost_stacktrace_impl ALIAS Boost::stacktrace_backtrace)
    else()
        message(FATAL_ERROR "Unsupported platform for boost_stacktrace")
    endif()
endmacro(Dep_AddBoostStacktraceImplLib)
