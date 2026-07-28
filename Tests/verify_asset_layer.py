#!/usr/bin/env python3
"""Verify Feature 020's Asset -> Core production dependency boundary."""

from __future__ import annotations

import argparse
import pathlib
import re
import sys


FORBIDDEN_PREFIXES = (
    "Application/",
    "Backend/",
    "Renderer/",
    "RHI/",
    "Tools/",
    "vulkan/",
    "GLFW/",
)


def verify(root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    asset_root = root / "Source" / "Asset"
    for path in sorted(asset_root.rglob("*")):
        if not path.is_file() or path.suffix not in {".h", ".cpp"}:
            continue
        text = path.read_text(encoding="utf-8")
        for include in re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', text, re.MULTILINE):
            if include.startswith(FORBIDDEN_PREFIXES):
                errors.append(f"{path.relative_to(root)}: forbidden include {include}")
            if "utf8proc" in include.lower():
                errors.append(f"{path.relative_to(root)}: utf8proc leaks into Asset")

    core_public = root / "Source" / "Core" / "Public"
    for path in sorted(core_public.rglob("*.h")):
        if "utf8proc" in path.read_text(encoding="utf-8").lower():
            errors.append(f"{path.relative_to(root)}: utf8proc leaks into Core public API")

    for path in sorted((root / "Source" / "Core").rglob("*")):
        if not path.is_file() or path.suffix not in {".h", ".cpp"}:
            continue
        if "utf8proc" in path.read_text(encoding="utf-8").lower():
            relative = path.relative_to(root).as_posix()
            if relative != "Source/Core/Private/FUnicode.cpp":
                errors.append(f"{relative}: utf8proc may only be used by FUnicode.cpp")

    sconscript = (asset_root / "SConscript").read_text(encoding="utf-8")
    if not re.search(r"BuildLayer\s*\(\s*env\s*,\s*['\"]Asset['\"]\s*,\s*\[['\"]Core['\"]\]\s*\)", sconscript):
        errors.append("Source/Asset/SConscript: Asset must declare Core as its only dependency")
    if "ThirdParty" in sconscript or "utf8proc" in sconscript.lower():
        errors.append("Source/Asset/SConscript: Asset must not compile third-party Unicode sources")

    core_sconscript = (root / "Source" / "Core" / "SConscript").read_text(encoding="utf-8")
    if "ThirdParty/utf8proc/utf8proc.c" not in core_sconscript:
        errors.append("Source/Core/SConscript: Core must compile utf8proc privately")
    if "UTF8PROC_STATIC" not in core_sconscript:
        errors.append("Source/Core/SConscript: utf8proc declarations must use static linkage")

    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root",
        type=pathlib.Path,
        default=pathlib.Path(__file__).resolve().parents[1],
    )
    parser.add_argument("--stamp", type=pathlib.Path)
    args = parser.parse_args()
    errors = verify(args.root.resolve())
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print("Asset architecture boundary passed: Asset -> Core; utf8proc remains Core-private")
    if args.stamp:
        args.stamp.parent.mkdir(parents=True, exist_ok=True)
        args.stamp.write_text("passed\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
