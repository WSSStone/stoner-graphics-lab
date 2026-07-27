#!/usr/bin/env python3
"""Compile a synthetic target matrix against Core/SGPlatform.h."""

import argparse
from pathlib import Path
import shlex
import subprocess
import sys
import tempfile


PREDEFINED_TARGET_MACROS = (
    "_WIN32",
    "__APPLE__",
    "__MACH__",
    "__linux__",
    "__ANDROID__",
)

SUPPORTED_CASES = (
    ("windows", ("_WIN32",), (1, 0, 0), True),
    ("macos", ("__APPLE__", "__MACH__"), (0, 1, 0), True),
    ("linux", ("__linux__",), (0, 0, 1), True),
)

UNSUPPORTED_CASES = (
    ("android", ("__ANDROID__", "__linux__"), (0, 0, 0), False),
    ("ios", ("__APPLE__", "__MACH__"), (0, 0, 0), False),
    ("unknown", (), (0, 0, 0), False),
)


def parse_args(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--compiler", required=True)
    parser.add_argument("--style", choices=("posix", "msvc"), required=True)
    parser.add_argument("--include", type=Path, required=True)
    parser.add_argument("--stamp", type=Path)
    return parser.parse_args(argv)


def write_target_conditionals(directory, macos):
    (directory / "TargetConditionals.h").write_text(
        "#pragma once\n"
        f"#define TARGET_OS_OSX {1 if macos else 0}\n"
        f"#define TARGET_OS_IPHONE {0 if macos else 1}\n",
        encoding="utf-8",
    )


def write_source(path, expected):
    windows, macos, linux = expected
    path.write_text(
        '#include "Core/SGPlatform.h"\n'
        f"static_assert(SG_PLATFORM_WINDOWS == {windows});\n"
        f"static_assert(SG_PLATFORM_MAC == {macos});\n"
        f"static_assert(SG_PLATFORM_LINUX == {linux});\n"
        "int main() { return 0; }\n",
        encoding="utf-8",
    )


def compile_case(args, directory, name, definitions, expected, supported):
    source = directory / f"{name}.cpp"
    output = directory / (f"{name}.obj" if args.style == "msvc" else f"{name}.o")
    write_target_conditionals(directory, name == "macos")
    write_source(source, expected)

    command = shlex.split(args.compiler)
    if args.style == "msvc":
        command.extend(
            (
                "/nologo",
                "/TP",
                "/std:c++20",
                "/c",
                str(source),
                f"/Fo{output}",
                f"/I{args.include}",
                f"/I{directory}",
            )
        )
        command.extend(f"/U{macro}" for macro in PREDEFINED_TARGET_MACROS)
        command.extend(f"/D{macro}" for macro in definitions)
    else:
        command.extend(
            (
                "-std=c++20",
                "-c",
                str(source),
                "-o",
                str(output),
                "-I",
                str(args.include),
                "-I",
                str(directory),
            )
        )
        command.extend(f"-U{macro}" for macro in PREDEFINED_TARGET_MACROS)
        command.extend(f"-D{macro}" for macro in definitions)

    result = subprocess.run(command, capture_output=True, text=True, check=False)
    combined_output = result.stdout + result.stderr
    passed = result.returncode == 0 if supported else (
        result.returncode != 0 and "Unsupported platform for Stoner Graphics Lab" in combined_output
    )
    if not passed:
        expectation = "compile" if supported else "fail with the unsupported-platform diagnostic"
        print(f"ERROR: {name} did not {expectation}", file=sys.stderr)
        print(combined_output, file=sys.stderr)
    return passed


def main(argv=None):
    args = parse_args(argv)
    if not args.include.is_dir():
        print(f"ERROR: public include directory not found: {args.include}", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="stoner-platform-matrix-") as temp:
        directory = Path(temp)
        cases = SUPPORTED_CASES + UNSUPPORTED_CASES
        results = [
            compile_case(args, directory, name, definitions, expected, supported)
            for name, definitions, expected, supported in cases
        ]
        passed = all(results)
    if not passed:
        return 1
    if args.stamp:
        args.stamp.parent.mkdir(parents=True, exist_ok=True)
        args.stamp.write_text("platform identity matrix passed\n", encoding="utf-8")
    print("Platform identity matrix passed: Windows, macOS, Linux; Android, iOS, unknown rejected")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
