#!/usr/bin/env python3
"""
Convert a binary file to a C++ source file with std::array<std::byte> data.

Usage:
    python bin2cxx.py <input_file> [output_file]

If output_file is not specified, it will be <input_file>.cxx
"""

import argparse
import os
import sys
from pathlib import Path


def to_camel_case(name: str) -> str:
    """Convert a string to camelCase, removing non-alphanumeric characters."""
    parts = name.replace("-", "_").replace(".", "_").split("_")
    return parts[0].lower() + "".join(p.capitalize() for p in parts[1:] if p)


def convert(input_path: Path, output_path: Path) -> None:
    with open(input_path, "rb") as f:
        data = f.read()

    size = len(data)
    var_name = to_camel_case(input_path.stem) + "Data"

    lines = [
        "#include <array>",
        "#include <cstddef>",
        "",
        f"// array size is {size}",
        f'extern const std::array<std::byte, {size}> {var_name} = {{',
    ]

    bytes_per_line = 16
    for i in range(0, size, bytes_per_line):
        chunk = data[i : i + bytes_per_line]
        hex_values = ", ".join(f"std::byte{{ 0x{b:02x} }}" for b in chunk)
        lines.append(f"  {hex_values},")

    lines.append("};")
    lines.append("")

    with open(output_path, "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(lines))

    print(f"Generated: {output_path} ({size} bytes)")


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert a binary file to a C++ source file."
    )
    parser.add_argument("input", type=Path, help="Input binary file")
    parser.add_argument(
        "output",
        type=Path,
        nargs="?",
        help="Output .cxx file (default: <input>.cxx)",
    )

    args = parser.parse_args()

    if not args.input.exists():
        print(f"Error: Input file not found: {args.input}", file=sys.stderr)
        sys.exit(1)

    output = args.output or (args.input.with_suffix(".cxx"))
    convert(args.input, output)


if __name__ == "__main__":
    main()
