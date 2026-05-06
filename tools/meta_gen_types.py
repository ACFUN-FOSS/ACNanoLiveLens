#!/usr/bin/env python3

from dataclasses import dataclass, field
from typing import List


CXX_STANDARD = 'c++20'


@dataclass
class ConstructorInfo:
    args: List[str]


@dataclass
class FieldInfo:
    name: str
    type_name: str


@dataclass
class MethodInfo:
    name: str
    return_type: str
    is_const: bool
    args: List[str]


@dataclass
class AccessorInfo:
    property_name: str
    getter: 'MethodInfo'
    setter: 'MethodInfo'


@dataclass
class ReflectedClass:
    qualified_name: str
    short_name: str
    constructors: List[ConstructorInfo] = field(default_factory=list)
    fields: List[FieldInfo] = field(default_factory=list)
    methods: List[MethodInfo] = field(default_factory=list)
    accessors: List[AccessorInfo] = field(default_factory=list)
