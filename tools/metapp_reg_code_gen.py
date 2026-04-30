#!/usr/bin/env python3
"""
metapp_reflection_generator.py

Usage:
  python metapp_reflection_generator.py [--compiler-flags-file FILE] --output OUT.cpp header1.hpp [header2.hpp ...]

依赖: pygccxml, castxml
"""

import sys
import argparse
from typing import List
import pygccxml
from pygccxml import declarations, parser, utils

# ----------------------------- 配置 -----------------------------
# C++ 标准（可被命令行覆盖）
CXX_STANDARD = 'c++2b'
# 标记属性名称
REFLECT_ATTRIBUTE = 'annotate'
REFLECT_VALUE = 'metapp_reflect'

# ----------------------------- 辅助函数 -----------------------------

import re

def sanitize_flags(flags_str: str) -> str:
    """将 MSVC 特有选项转换为 Clang 兼容形式，过滤掉无法识别的选项"""
    cleaned = []
    for token in flags_str.split():
        # 跳过 MSVC 特有的 / 选项（例外：/FI 转换为 -include）
        if token.startswith('/FI'):
            # 强制包含文件，提取后面的文件名（可能用引号）
            file_name = token[3:].strip('"')
            cleaned.append(f'-include "{file_name}"')
        elif token.startswith('/D'):
            # MSVC 定义：/DNAME 或 /DNAME=VALUE → 转换为 -D
            define = token[2:]
            cleaned.append(f'-D{define}')
        elif token.startswith('/'):
            # 其他 / 选项直接跳过
            continue
        else:
            # 保留 GCC 风格选项（-I, -D, -std, -f 等）以及路径
            cleaned.append(token)
    return ' '.join(cleaned)

def is_reflected(decl) -> bool:
    """检查 declaration 是否包含反射标记属性"""
    if not hasattr(decl, 'attributes'):
        return False
    for attr in decl.attributes:
        if attr.name == REFLECT_ATTRIBUTE and attr.value == REFLECT_VALUE:
            return True
    return False


def get_qualified_name(decl) -> str:
    """获得全限定名，如 'ns::MyPet'"""
    if decl.parent and hasattr(decl.parent, 'name') and decl.parent.name:
        return f'{get_qualified_name(decl.parent)}::{decl.name}'
    return decl.name


def arg_type_string(arg) -> str:
    """获取函数参数的完整类型字符串，包括 const 和引用"""
    return arg.decl_type.decl_string


def constructor_args_string(ctor) -> str:
    """构造函数的参数列表字符串，如 'const std::string &, int'"""
    return ', '.join(arg_type_string(a) for a in ctor.arguments)


def make_property_name_from_getter(get_name: str) -> str:
    """从 getter 名提取属性名，getAge -> age"""
    if get_name.startswith('get'):
        base = get_name[3:]
        if base:
            return base[0].lower() + base[1:]
    elif get_name.startswith('is'):
        base = get_name[2:]
        if base:
            return base[0].lower() + base[1:]
    return get_name


def make_property_name_from_setter(set_name: str) -> str:
    """从 setter 名提取属性名，setAge -> age"""
    if set_name.startswith('set'):
        base = set_name[3:]
        if base:
            return base[0].lower() + base[1:]
    return set_name


def get_public_member_functions(class_decl):
    """获取所有 public、非构造/析构/operator 的成员函数"""
    funcs = []
    for m in class_decl.member_functions():
        if m.access_type != 'public':
            continue
        if m.name.startswith('operator') or m.name == class_decl.name:
            continue
        funcs.append(m)
    return funcs


def generate_class_reflection(class_decl) -> str:
    """为指定类生成完整的 metapp 反射注册代码"""
    qualified_name = get_qualified_name(class_decl)
    short_name = class_decl.name

    # 1. 收集构造器（仅 public）
    ctors = [c for c in class_decl.constructors() if c.access_type == 'public']
    # 2. 收集成员变量（public）
    public_vars = [v for v in class_decl.variables() if v.access_type == 'public']
    # 3. 收集成员函数（public，非特殊）
    all_funcs = get_public_member_functions(class_decl)

    # 识别 getter/setter 对
    getters = []   # (func, property_name)
    setters = []
    remaining_funcs = []

    for f in all_funcs:
        if f.is_const and f.return_type and not declarations.is_void(f.return_type):
            prop = make_property_name_from_getter(f.name)
            if prop != f.name:
                getters.append((f, prop))
            else:
                remaining_funcs.append(f)
        elif not f.is_const and f.return_type and declarations.is_void(f.return_type):
            prop = make_property_name_from_setter(f.name)
            if prop != f.name:
                setters.append((f, prop))
            else:
                remaining_funcs.append(f)
        else:
            remaining_funcs.append(f)

    # 匹配 getter 和 setter
    getter_setter_pairs = []
    used_setters = []
    for get_f, prop in getters:
        matching_sets = [(s_f, s_prop) for s_f, s_prop in setters
                         if s_prop == prop and s_f not in used_setters]
        if matching_sets:
            set_f, _ = matching_sets[0]
            getter_setter_pairs.append((prop, get_f, set_f))
            used_setters.append(set_f)
        else:
            remaining_funcs.append(get_f)

    remaining_funcs.extend([s_f for s_f, _ in setters if s_f not in used_setters])

    # 生成注册代码块
    lines = []
    indent = '    '
    lines.append(f'template <>')
    lines.append(f'struct metapp::DeclareMetaType<{qualified_name}> : metapp::DeclareMetaTypeBase<{qualified_name}>')
    lines.append('{')
    lines.append(f'{indent}static void setup() {{')
    lines.append(f'{indent}{indent}getGlobalMetaRepo().registerType<{qualified_name}>( "{short_name}" );')
    lines.append(f'{indent}}}')

    lines.append(f'{indent}static const metapp::MetaClass *getMetaClass() {{')
    lines.append(f'{indent}{indent}static const metapp::MetaClass metaClass(')
    lines.append(f'{indent}{indent}{indent}metapp::getMetaType<{qualified_name}>(),')
    lines.append(f'{indent}{indent}{indent}[](metapp::MetaClass &mc) {{')

    # 注册构造函数
    for ctor in ctors:
        args_str = constructor_args_string(ctor)
        func_type = f'{qualified_name} ({args_str})' if args_str else f'{qualified_name} ()'
        lines.append(f'{indent}{indent}{indent}{indent}mc.registerConstructor(metapp::Constructor<{func_type}>());')

    # 注册 public 成员变量
    for var in public_vars:
        name = var.name
        lines.append(f'{indent}{indent}{indent}{indent}mc.registerAccessible("{name}", &{qualified_name}::{name});')

    # 注册 getter/setter 对
    for prop, get_f, set_f in getter_setter_pairs:
        lines.append(f'{indent}{indent}{indent}{indent}mc.registerAccessible("{prop}",')
        lines.append(f'{indent}{indent}{indent}{indent}{indent}metapp::createAccessor(&{qualified_name}::{get_f.name}, &{qualified_name}::{set_f.name}));')

    # 注册剩余成员函数
    for func in remaining_funcs:
        lines.append(f'{indent}{indent}{indent}{indent}mc.registerCallable("{func.name}", &{qualified_name}::{func.name});')

    lines.append(f'{indent}{indent}{indent}}}')
    lines.append(f'{indent}{indent});')
    lines.append(f'{indent}{indent}return &metaClass;')
    lines.append(f'{indent}}}')
    lines.append('};')

    return '\n'.join(lines)


def generate(headers: List[str], extra_cflags: str = '') -> str:
    """解析多个头文件，返回所有反射类的注册代码"""
    # 动态查找生成器（优先使用 castxml）
    generator_path, generator_name = utils.find_xml_generator()

    # 组合 C++ 标准与额外编译选项
    sanitized_flags = sanitize_flags(extra_cflags)
    cflags = f'-std={CXX_STANDARD} {sanitized_flags}'

    # 配置 XML 生成器
    xml_generator_config = parser.xml_generator_configuration_t(
        xml_generator_path=generator_path,
        xml_generator=generator_name,
        cflags=cflags
    )

    # 解析所有头文件
    decls = parser.parse(headers, xml_generator_config)
    global_ns = declarations.get_global_namespace(decls)

    # 遍历所有类，收集标记的
    reflected_classes = []
    def visit(decl):
        if is_reflected(decl) and isinstance(decl, declarations.class_t):
            reflected_classes.append(decl)
        if hasattr(decl, 'declarations'):
            for child in decl.declarations:
                visit(child)

    visit(global_ns)

    if not reflected_classes:
        return '// No reflected classes found.\n'

    codes = [generate_class_reflection(cls) for cls in reflected_classes]
    return '\n\n'.join(codes)


if __name__ == '__main__':
    parser_arg = argparse.ArgumentParser(description='Generate metapp reflection registration code.')
    parser_arg.add_argument('headers', nargs='+', help='Input header files')
    parser_arg.add_argument('--compiler-flags-file', default=None,
                            help='File containing additional compiler flags (e.g., -I, -D)')
    parser_arg.add_argument('--output', '-o', required=True, help='Output C++ source file')
    parser_arg.add_argument('--auto-include', action='append', default=[],
                            help='Header to force-include (equivalent to -include)')  # 新增
    args = parser_arg.parse_args()

    # 读取编译器标志文件内容
    extra_flags = ''
    if args.compiler_flags_file:
        with open(args.compiler_flags_file, 'r') as f:
            extra_flags = f.read().strip()

	# 将 --auto-include 转换为 -include 选项
    auto_include_flags = ' '.join(f'-include "{inc}"' for inc in args.auto_include)

    # 组合所有标志（extra_flags 已经过 sanitize_flags 过滤）
    combined_flags = f'{extra_flags} {auto_include_flags}'

    # 生成代码
    code = generate(args.headers, combined_flags)

    # 写入输出文件
    with open(args.output, 'w') as f:
        f.write(code)

    print(f'Reflection code successfully generated in {args.output}')
