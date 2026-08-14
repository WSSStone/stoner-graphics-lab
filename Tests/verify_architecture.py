#!/usr/bin/env python3
"""Reusable runtime, Asset, and offline-Tool architecture checks."""

from __future__ import annotations

import argparse
import pathlib
import re
import subprocess
import sys


SOURCE_SUFFIXES = {".h", ".hpp", ".c", ".cpp", ".cc"}
RUNTIME_LAYERS = ("Core", "Asset", "RHI", "Renderer", "Application", "Backend")
GRAPHICS_PREFIXES = ("RHI/", "Renderer/", "Application/", "Backend/")
NATIVE_OR_PRIVATE_PATTERN = re.compile(
    r"\b(?:Vk[A-Z][A-Za-z0-9_]*|GLFW[A-Za-z0-9_]*|ID3D[A-Za-z0-9_]*|"
    r"IDXGI[A-Za-z0-9_]*|yyjson_[A-Za-z0-9_]*|cgltf_[A-Za-z0-9_]*|"
    r"ktx_[A-Za-z0-9_]*|wasm_[A-Za-z0-9_]*)\b"
)
INCLUDE_PATTERN = re.compile(
    r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE
)


def _source_files(root: pathlib.Path) -> list[pathlib.Path]:
    return sorted(
        path for path in root.rglob("*")
        if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES
    )


def verify(root: pathlib.Path) -> list[str]:
    root = root.resolve()
    errors: list[str] = []
    private_root = (root / "Source/Asset/Private").resolve()
    private_headers = {
        path.name for path in private_root.rglob("*.h") if path.is_file()
    }

    for layer in RUNTIME_LAYERS:
        layer_root = root / "Source" / layer
        if not layer_root.is_dir():
            continue
        for path in _source_files(layer_root):
            text = path.read_text(encoding="utf-8")
            for include in INCLUDE_PATTERN.findall(text):
                if include.startswith("Tools/") or "/Tools/" in include:
                    errors.append(
                        f"{path.relative_to(root)}: runtime layer must not include Tools ({include})"
                    )
                if layer == "Asset" and include.startswith(GRAPHICS_PREFIXES):
                    errors.append(
                        f"{path.relative_to(root)}: Asset must not include graphics/runtime layer ({include})"
                    )

    tools_root = root / "Tools"
    if tools_root.is_dir():
        for path in _source_files(tools_root):
            text = path.read_text(encoding="utf-8")
            for include in INCLUDE_PATTERN.findall(text):
                candidate = (path.parent / include).resolve()
                private_by_path = candidate == private_root or private_root in candidate.parents
                private_by_name = pathlib.PurePosixPath(include).name in private_headers
                if (
                    "Source/Asset/Private" in include.replace("\\", "/")
                    or private_by_path
                    or private_by_name
                ):
                    errors.append(
                        f"{path.relative_to(root)}: Tool must use Asset public APIs, not {include}"
                    )
        for sconscript in sorted(tools_root.rglob("SConscript")):
            text = sconscript.read_text(encoding="utf-8")
            normalized = text.replace("\\", "/")
            if "Source/Asset/Private" in normalized:
                errors.append(
                    f"{sconscript.relative_to(root)}: Tool exposes Asset private include paths"
                )

    for layer in RUNTIME_LAYERS:
        public_root = root / "Source" / layer / "Public"
        if not public_root.is_dir():
            continue
        for path in sorted(public_root.rglob("*.h")):
            text = path.read_text(encoding="utf-8")
            if NATIVE_OR_PRIVATE_PATTERN.search(text):
                errors.append(
                    f"{path.relative_to(root)}: native or private parser type leaks into public API"
                )

    for layer in RUNTIME_LAYERS:
        sconscript = root / "Source" / layer / "SConscript"
        if not sconscript.is_file():
            continue
        text = sconscript.read_text(encoding="utf-8").replace("\\", "/")
        if "Tools/AssetCooker" in text or re.search(
                r"['\"]AssetCooker['\"]", text):
            errors.append(
                f"{sconscript.relative_to(root)}: runtime build must not link AssetCooker"
            )
        if layer == "Asset" and any(
                f"'{dependency}'" in text or f'"{dependency}"' in text
                for dependency in ("RHI", "Renderer", "Application", "Backend")
        ):
            errors.append(
                f"{sconscript.relative_to(root)}: Asset build must depend on Core only"
            )

    git = subprocess.run(
        ["git", "-C", str(root), "ls-files"], check=False,
        capture_output=True, text=True,
    )
    if git.returncode == 0:
        forbidden_roots = (
            "Saved/DerivedDataCache/", "Saved/Cooked/", "Saved/Feature025",
        )
        for tracked in git.stdout.splitlines():
            if tracked.startswith(forbidden_roots):
                errors.append(f"tracked generated AssetCooker output: {tracked}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root", type=pathlib.Path,
        default=pathlib.Path(__file__).resolve().parents[1],
    )
    parser.add_argument("--stamp", type=pathlib.Path)
    args = parser.parse_args()
    errors = verify(args.root)
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print(
        "Architecture: PASS (Asset graphics boundary, runtime-to-Tools, "
        "Tool-to-Asset-private, native/private public API leakage)"
    )
    if args.stamp:
        args.stamp.parent.mkdir(parents=True, exist_ok=True)
        args.stamp.write_text("passed\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
