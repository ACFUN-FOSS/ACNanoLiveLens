#!/usr/bin/env python3

import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from meta_gen_types import ReflectedClass


def generate_class_reflection(cls: ReflectedClass) -> str:
    """为指定类生成完整的 metapp 反射注册代码"""
    lines = []
    indent = '    '
    lines.append(f'#include "appstate.hxx"')

    lines.append(f'template <>')
    lines.append(f'struct metapp::DeclareMetaType<{cls.qualified_name}> : metapp::DeclareMetaTypeBase<{cls.qualified_name}>')
    lines.append('{')
    lines.append(f'{indent}static void setup() {{')
    lines.append(f'{indent}{indent}getGlobalMetaRepo().registerType<{cls.qualified_name}>( "{cls.short_name}" );')
    lines.append(f'{indent}}}')
    
    lines.append(f'{indent}static const metapp::MetaClass *getMetaClass() {{')
    lines.append(f'{indent}{indent}static const metapp::MetaClass metaClass(')
    lines.append(f'{indent}{indent}{indent}metapp::getMetaType<{cls.qualified_name}>(),')
    lines.append(f'{indent}{indent}{indent}[](metapp::MetaClass &mc) {{')
    
    for ctor in cls.constructors:
        args_str = ', '.join(ctor.args)
        func_type = f'{cls.qualified_name} ({args_str})' if args_str else f'{cls.qualified_name} ()'
        lines.append(f'{indent}{indent}{indent}{indent}mc.registerConstructor(metapp::Constructor<{func_type}>());')
    
    for field in cls.fields:
        lines.append(f'{indent}{indent}{indent}{indent}mc.registerAccessible("{field.name}", &{cls.qualified_name}::{field.name});')
    
    for accessor in cls.accessors:
        lines.append(f'{indent}{indent}{indent}{indent}mc.registerAccessible("{accessor.property_name}",')
        lines.append(f'{indent}{indent}{indent}{indent}{indent}metapp::createAccessor(&{cls.qualified_name}::{accessor.getter.name}, &{cls.qualified_name}::{accessor.setter.name}));')
    
    for method in cls.methods:
        lines.append(f'{indent}{indent}{indent}{indent}mc.registerCallable("{method.name}", &{cls.qualified_name}::{method.name});')
    
    lines.append(f'{indent}{indent}{indent}}}')
    lines.append(f'{indent}{indent});')
    lines.append(f'{indent}{indent}return &metaClass;')
    lines.append(f'{indent}}}')
    lines.append('};')
    
    return '\n'.join(lines)