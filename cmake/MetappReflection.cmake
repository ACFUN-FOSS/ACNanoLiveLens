# MetappReflection.cmake
# 提供 gen_metapp_reflection(target) 函数
# 依赖: Python3, pygccxml, CastXML

find_package(Python3 REQUIRED COMPONENTS Interpreter)

# 生成器脚本路径（按你的项目结构调整）
if(NOT METAPP_GENERATOR_SCRIPT)
    set(METAPP_GENERATOR_SCRIPT "${CMAKE_CURRENT_SOURCE_DIR}/tools/metapp_reg_code_gen.py")
endif()

function(gen_metapp_reflection target)
    # ---------------- 解析参数 ----------------
    set(options "")
    set(oneValueArgs OUTPUT)
    set(multiValueArgs HEADERS AUTO_INCLUDE)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # 确定需要反射的头文件列表
    if(ARG_HEADERS)
        set(reflect_headers ${ARG_HEADERS})
    else()
        # 尝试从 target 属性获取
        get_target_property(prop_headers ${target} METAPP_REFLECT_HEADERS)
        if(prop_headers)
            set(reflect_headers ${prop_headers})
        else()
            # 最后从 SOURCES 中筛选头文件（保证 IDE 友好）
            get_target_property(srcs ${target} SOURCES)
            set(reflect_headers "")
            foreach(src IN LISTS srcs)
                get_filename_component(ext "${src}" LAST_EXT)
                if(ext MATCHES "^\\.(h|hpp|hxx|hh|H|h\\+\\+)$")
                    get_filename_component(abs_src "${src}" ABSOLUTE)
                    list(APPEND reflect_headers "${abs_src}")
                endif()
            endforeach()
        endif()
    endif()

	    # 处理 AUTO_INCLUDE 参数
    set(auto_include_args "")
    if(ARG_AUTO_INCLUDE)
        foreach(_inc IN LISTS ARG_AUTO_INCLUDE)
            get_filename_component(_abs_inc "${_inc}" ABSOLUTE)
            list(APPEND auto_include_args "--auto-include" "${_abs_inc}")
        endforeach()
    endif()

    if(NOT reflect_headers)
        message(FATAL_ERROR "gen_metapp_reflection: No headers specified for target ${target}. "
                            "Please provide HEADERS argument or set METAPP_REFLECT_HEADERS property.")
    endif()

	set(abs_headers "")
    foreach(_hdr IN LISTS reflect_headers)
        get_filename_component(_abs_hdr "${_hdr}" ABSOLUTE)
        list(APPEND abs_headers "${_abs_hdr}")
    endforeach()
    set(reflect_headers ${abs_headers})

    # 确定输出文件路径
    if(ARG_OUTPUT)
        set(output_cpp "${ARG_OUTPUT}")
    else()
        set(output_dir "${CMAKE_CURRENT_BINARY_DIR}/metapp_autogen")
        file(MAKE_DIRECTORY "${output_dir}")
        set(output_cpp "${output_dir}/${target}_reflection.cpp")
    endif()

    # ---------------- 生成编译选项响应文件 ----------------
    # 生成完整的编译标志文件（GCC 风格）
	# 因为 CastXML 只理解 GCC 风格选项，需要做转换
	set(flags_file "${output_dir}/${target}_compile_flags.rsp")

	# 1. 包含路径：每个路径前加 -I，并用空格分隔
	set(include_flags "$<JOIN:$<TARGET_PROPERTY:${target},INCLUDE_DIRECTORIES>, -I>")
	# 加上开头的 -I（即使只有一个路径也需要）
	set(include_flags "-I${include_flags}")

	# 2. 宏定义：每个定义前加 -D，并用空格分隔
	set(define_flags "$<JOIN:$<TARGET_PROPERTY:${target},COMPILE_DEFINITIONS>, -D>")
	set(define_flags "-D${define_flags}")

	# 3. 编译选项：过滤出 GCC 兼容的选项（-std, -f, -m, -W 等），丢弃 / 开头的 MSVC 选项
	#    同时提取可能存在的 /FI 选项（MSVC 的强制包含）并转换为 -include
	set(compile_options "$<TARGET_PROPERTY:${target},COMPILE_OPTIONS>")

	# 4. 预编译头：若 target 使用了预编译头，提取头文件并添加 -include
	#    (此处假设 target_precompile_headers 会将 /FI 加入 COMPILE_OPTIONS)
	#    我们需要生成一个 -include <header> 选项，从 /FI 选项中提取
	#    但直接使用生成器表达式提取比较复杂，可以交给 Python 脚本处理
	#    这里简单将 COMPILE_OPTIONS 原样传递，由 Python 脚本过滤和转换
	file(GENERATE
    	OUTPUT "${flags_file}"
    	CONTENT "${include_flags} ${define_flags} $<JOIN:$<TARGET_PROPERTY:${target},COMPILE_OPTIONS>, >"
	)

    # ---------------- 添加自定义命令生成反射代码 ----------------
    add_custom_command(
        OUTPUT "${output_cpp}"
        COMMAND
            ${Python3_EXECUTABLE} "${METAPP_GENERATOR_SCRIPT}"
                ${reflect_headers}
            --compiler-flags-file "${flags_file}"
            --output "${output_cpp}"
			${auto_include_args}
        DEPENDS
            ${reflect_headers}
            "${METAPP_GENERATOR_SCRIPT}"
            "${flags_file}"
        COMMENT "Generating metapp reflection for ${target}"
        VERBATIM
    )

    # 将生成的 cpp 添加到 target 源文件
    target_sources(${target} PRIVATE "${output_cpp}")

    # 确保 target 能找到 metapp 和自身头文件
    # 通常这一部分已由用户负责，这里仅添加生成的 cpp 所在目录
    target_include_directories(${target} PRIVATE "${output_dir}")
endfunction()
