#!/usr/bin/env python3

import os
import sys
import argparse
from typing import List, Optional, Set, Tuple

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from meta_gen_types import (
    CXX_STANDARD, ConstructorInfo, FieldInfo, MethodInfo, AccessorInfo, ReflectedClass
)
from meta_gen_gen import generate_class_reflection

import clang.cindex


REFLECT_MACRO = 'METAPP_REFLECT'


def find_reflected_positions(tu: clang.cindex.TranslationUnit, header_abs_paths: Set[str]) -> Set[Tuple[str, int]]:
    """通过 token 扫描找到所有标注了 METAPP_REFLECT 的位置"""
    reflected_positions = set()
    tokens = list(tu.get_tokens(extent=tu.cursor.extent))
    
    for i, tok in enumerate(tokens):
        if tok.spelling == REFLECT_MACRO:
            if tok.location and tok.location.file:
                file_path = os.path.abspath(tok.location.file.name)
                if file_path in header_abs_paths:
                    if i + 1 < len(tokens):
                        next_tok = tokens[i + 1]
                        if next_tok.kind == clang.cindex.TokenKind.KEYWORD and next_tok.spelling in ('struct', 'class'):
                            reflected_positions.add((file_path, tok.location.line))
    
    return reflected_positions


def is_reflected(cursor: clang.cindex.Cursor, reflected_positions: Set[Tuple[str, int]]) -> bool:
    """检查类/结构体是否在标注位置"""
    loc = cursor.location
    if loc.file:
        cursor_file = os.path.abspath(loc.file.name) if isinstance(loc.file.name, str) else os.path.abspath(str(loc.file))
        for attr_line in range(loc.line - 1, max(0, loc.line - 5), -1):
            if (cursor_file, attr_line) in reflected_positions:
                return True
    return False


def get_qualified_name(cursor: clang.cindex.Cursor) -> str:
    """递归拼接命名空间限定的类名"""
    if cursor.kind in (clang.cindex.CursorKind.NAMESPACE, clang.cindex.CursorKind.CLASS_DECL, clang.cindex.CursorKind.STRUCT_DECL):
        parent = cursor.semantic_parent
        if parent and parent.kind != clang.cindex.CursorKind.TRANSLATION_UNIT:
            return get_qualified_name(parent) + '::' + cursor.spelling
    return cursor.spelling


def make_property_name_from_getter(name: str) -> str:
    if name.startswith('get'):
        base = name[3:]
        return base[0].lower() + base[1:] if base else name
    if name.startswith('is'):
        base = name[2:]
        return base[0].lower() + base[1:] if base else name
    return name


def make_property_name_from_setter(name: str) -> str:
    if name.startswith('set'):
        base = name[3:]
        return base[0].lower() + base[1:] if base else name
    return name


def get_public_non_special_methods(class_cursor: clang.cindex.Cursor) -> List[clang.cindex.Cursor]:
    """获取 public 的且非构造/析构/operator 的成员函数"""
    methods = []
    for m in class_cursor.get_children():
        if m.kind == clang.cindex.CursorKind.CXX_METHOD and m.access_specifier == clang.cindex.AccessSpecifier.PUBLIC:
            if m.is_static_method():
                continue
            if m.spelling.startswith('operator') or m.spelling == class_cursor.spelling:
                continue
            methods.append(m)
    return methods


def extract_constructor_info(ctor_cursor: clang.cindex.Cursor) -> ConstructorInfo:
    """提取构造函数信息"""
    args = [arg.type.spelling for arg in ctor_cursor.get_arguments()]
    return ConstructorInfo(args=args)


def extract_field_info(field_cursor: clang.cindex.Cursor) -> FieldInfo:
    """提取成员变量信息"""
    return FieldInfo(
        name=field_cursor.spelling,
        type_name=field_cursor.type.spelling
    )


def extract_method_info(method_cursor: clang.cindex.Cursor) -> MethodInfo:
    """提取方法信息"""
    args = [arg.type.spelling for arg in method_cursor.get_arguments()]
    return MethodInfo(
        name=method_cursor.spelling,
        return_type=method_cursor.result_type.spelling,
        is_const=method_cursor.is_const_method(),
        args=args
    )


def analyze_class(class_cursor: clang.cindex.Cursor) -> ReflectedClass:
    """分析单个类/结构体，提取反射信息"""
    qualified_name = get_qualified_name(class_cursor)
    short_name = class_cursor.spelling
    
    constructors = []
    fields = []
    
    for child in class_cursor.get_children():
        if child.kind == clang.cindex.CursorKind.CONSTRUCTOR and child.access_specifier == clang.cindex.AccessSpecifier.PUBLIC:
            constructors.append(extract_constructor_info(child))
        elif child.kind == clang.cindex.CursorKind.FIELD_DECL and child.access_specifier == clang.cindex.AccessSpecifier.PUBLIC:
            fields.append(extract_field_info(child))
    
    all_methods = get_public_non_special_methods(class_cursor)
    
    getters = []
    setters = []
    remaining_funcs = []
    
    for m in all_methods:
        method_info = extract_method_info(m)
        is_const = m.is_const_method()
        ret = m.result_type
        name = m.spelling
        
        if is_const and ret.kind != clang.cindex.TypeKind.VOID:
            prop = make_property_name_from_getter(name)
            if prop != name:
                getters.append((method_info, prop))
            else:
                remaining_funcs.append(method_info)
        elif not is_const and ret.kind == clang.cindex.TypeKind.VOID:
            prop = make_property_name_from_setter(name)
            if prop != name:
                setters.append((method_info, prop))
            else:
                remaining_funcs.append(method_info)
        else:
            remaining_funcs.append(method_info)
    
    accessors = []
    used_setters = []
    for get_f, prop in getters:
        matching = [(s_f, s_prop) for s_f, s_prop in setters if s_prop == prop and s_f not in used_setters]
        if matching:
            set_f, _ = matching[0]
            accessors.append(AccessorInfo(property_name=prop, getter=get_f, setter=set_f))
            used_setters.append(set_f)
        else:
            remaining_funcs.append(get_f)
    remaining_funcs.extend(s_f for s_f, _ in setters if s_f not in used_setters)
    
    return ReflectedClass(
        qualified_name=qualified_name,
        short_name=short_name,
        constructors=constructors,
        fields=fields,
        methods=remaining_funcs,
        accessors=accessors
    )


def sanitize_flags(flags_str: str) -> str:
    """过滤并转换 MSVC 特有选项，保留 GCC 风格选项"""
    cleaned = []
    for token in flags_str.split():
        if token.startswith('/FI'):
            file_name = token[3:].strip('"')
            cleaned.append(f'-include "{file_name}"')
        elif token.startswith('/D'):
            define = token[2:]
            cleaned.append(f'-D{define}')
        elif token.startswith('/'):
            continue
        else:
            cleaned.append(token)
    return ' '.join(cleaned)


def analyze(headers: List[str], extra_cflags: str = '', auto_include: Optional[List[str]] = None, cxx_standard: str = CXX_STANDARD) -> List[ReflectedClass]:
    """解析所有头文件，返回反射类信息列表"""
    args_list = []
    if extra_cflags:
        sanitized = sanitize_flags(extra_cflags)
        args_list.extend(sanitized.split())
    if not any(a.startswith('-std=') for a in args_list):
        args_list.append(f'-std={cxx_standard}')
    if auto_include:
        for inc in auto_include:
            args_list.append('-include')
            args_list.append(inc)
    
    parse_options = clang.cindex.TranslationUnit.PARSE_SKIP_FUNCTION_BODIES
    
    index = clang.cindex.Index.create()
    tu = index.parse(None, headers + args_list, options=parse_options)
    
    header_abs_paths = set()
    for h in headers:
        abs_path = os.path.abspath(h)
        header_abs_paths.add(abs_path)
    
    reflected_positions = find_reflected_positions(tu, header_abs_paths)
    
    reflected_cursors = []
    def traverse(cursor):
        for child in cursor.get_children():
            if child.kind in (clang.cindex.CursorKind.CLASS_DECL, clang.cindex.CursorKind.STRUCT_DECL):
                loc = child.location
                if loc.file:
                    cursor_file = os.path.abspath(loc.file.name) if isinstance(loc.file.name, str) else os.path.abspath(str(loc.file))
                    if cursor_file in header_abs_paths:
                        if is_reflected(child, reflected_positions):
                            reflected_cursors.append(child)
            if child.kind in (clang.cindex.CursorKind.NAMESPACE, clang.cindex.CursorKind.CLASS_DECL, clang.cindex.CursorKind.STRUCT_DECL):
                traverse(child)
    
    traverse(tu.cursor)
    
    return [analyze_class(cls) for cls in reflected_cursors]


def generate(headers: list, extra_cflags: str = '', auto_include: list = None, cxx_standard: str = CXX_STANDARD) -> str:
    """解析所有头文件，返回反射注册代码"""
    classes = analyze(headers, extra_cflags, auto_include, cxx_standard)
    
    if not classes:
        return '// No reflected classes found.\n'
    
    lines = []
    for h in headers:
        lines.append(f'#include "{h}"')
    lines.append('')
    
    codes = [generate_class_reflection(cls) for cls in classes]
    lines.append('\n\n'.join(codes))
    
    return '\n'.join(lines)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='Generate metapp reflection code using libclang')
    parser.add_argument('headers', nargs='+', help='Input header files')
    parser.add_argument('--compiler-flags-file', default=None, help='File with additional compiler flags')
    parser.add_argument('--output', '-o', required=True, help='Output C++ source file')
    parser.add_argument('--auto-include', action='append', default=[], help='Header to force-include')
    parser.add_argument('--std', default=None, help='Override C++ standard (e.g. c++23)')
    args = parser.parse_args()
    
    std = args.std if args.std else CXX_STANDARD
    
    extra_flags = ''
    if args.compiler_flags_file:
        with open(args.compiler_flags_file, 'r') as f:
            extra_flags = f.read().strip()
    
    code = generate(args.headers, extra_flags, auto_include=args.auto_include, cxx_standard=std)
    
    with open(args.output, 'w') as f:
        f.write(code)
    
    print(f'Reflection code written to {args.output}')
