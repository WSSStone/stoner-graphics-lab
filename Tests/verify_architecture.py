#!/usr/bin/env python3
"""Reusable runtime, Asset, and offline-Tool architecture checks."""

from __future__ import annotations

import argparse
import json
import pathlib
import re
import subprocess
import sys


SOURCE_SUFFIXES = {".h", ".hpp", ".c", ".cpp", ".cc", ".mm"}
RUNTIME_LAYERS = ("Core", "Asset", "RHI", "Renderer", "Application", "Backend")
GRAPHICS_PREFIXES = ("RHI/", "Renderer/", "Application/", "Backend/")
NATIVE_OR_PRIVATE_PATTERN = re.compile(
    r"\b(?:Vk[A-Z][A-Za-z0-9_]*|GLFW[A-Za-z0-9_]*|ID3D[A-Za-z0-9_]*|"
    r"IDXGI[A-Za-z0-9_]*|yyjson_[A-Za-z0-9_]*|cgltf_[A-Za-z0-9_]*|"
    r"ktx_[A-Za-z0-9_]*|wasm_[A-Za-z0-9_]*|CAMetalLayer|NSView|"
    r"MTL[A-Z][A-Za-z0-9_]*|id\s*<\s*MTL[A-Za-z0-9_]+\s*>)\b"
)
METAL_NATIVE_PATTERN = re.compile(
    r"\b(?:CAMetalLayer|NSView|MTL[A-Z][A-Za-z0-9_]*|"
    r"id\s*<\s*MTL[A-Za-z0-9_]+\s*>)\b"
)
INCLUDE_PATTERN = re.compile(
    r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', re.MULTILINE
)
BACKEND_NAMESPACE_PATTERN = re.compile(r"\bStoner::Backend::(?:Vulkan|Metal)::")
ASSET_FORBIDDEN_NAMESPACE_PATTERN = re.compile(
    r"\bStoner::(?:RHI|Renderer|Application|Backend)::"
)
DEMO_COMPOSITION_ROOT_MAX_LINES = 1600
FEATURE_SCOPE_RULES = (
    ("new source importer", re.compile(r"(?:^|[^a-z0-9])(?:importer|fbx|obj|usd|tga)(?:[^a-z0-9]|$)")),
    ("skeletal/editor/hot-reload", re.compile(r"(?:skeletal|animation|editor|hot.?reload)")),
    ("package/archive delivery", re.compile(r"(?:package|archive)")),
    ("streaming/residency", re.compile(r"(?:streaming|residency)")),
    ("Meshlet or LOD", re.compile(r"(?:meshlet|(?:^|[^a-z0-9])lod(?:[^a-z0-9]|$))")),
    ("virtual geometry", re.compile(r"virtual.?geometry")),
    ("ray tracing", re.compile(r"ray.?tracing")),
    ("visual-quality redesign", re.compile(r"visual.*redesign|redesign.*visual")),
)


def _source_files(root: pathlib.Path) -> list[pathlib.Path]:
    return sorted(
        path for path in root.rglob("*")
        if path.is_file() and path.suffix.lower() in SOURCE_SUFFIXES
    )


def _scope_search_text(path: str) -> str:
    normalized = path.replace("\\", "/")
    normalized = re.sub(r"([a-z0-9])([A-Z])", r"\1-\2", normalized)
    normalized = re.sub(r"([A-Z]+)([A-Z][a-z])", r"\1-\2", normalized)
    return normalized.lower()


def verify_feature_diff_paths(paths: list[str]) -> list[str]:
    """Reject Feature 028 production-code additions outside FR-044 scope."""
    errors: list[str] = []
    for path in sorted(set(paths)):
        normalized = path.replace("\\", "/")
        if not normalized.startswith(("Source/", "Demo/", "Tools/")):
            continue
        searchable = _scope_search_text(normalized)
        for label, pattern in FEATURE_SCOPE_RULES:
            if pattern.search(searchable):
                errors.append(
                    f"{normalized}: Feature 028 must not add {label} scope"
                )
                break
    return errors


def _feature_added_paths(root: pathlib.Path) -> list[str]:
    feature_state = root / ".specify/feature.json"
    if not feature_state.is_file():
        return []
    try:
        state = json.loads(feature_state.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return []
    feature_directory = state.get("feature_directory", "")
    base_revision = state.get("implementation_base_revision", "")
    if feature_directory != "specs/028-production-content-acceptance":
        return []
    if not re.fullmatch(r"[0-9a-f]{40}", base_revision):
        return [".specify/feature.json: missing valid implementation_base_revision"]

    diff = subprocess.run(
        ["git", "-C", str(root), "diff", "--name-only", "--diff-filter=A",
         base_revision, "--", "Source", "Demo", "Tools"],
        check=False, capture_output=True, text=True,
    )
    untracked = subprocess.run(
        ["git", "-C", str(root), "ls-files", "--others", "--exclude-standard",
         "Source", "Demo", "Tools"],
        check=False, capture_output=True, text=True,
    )
    if diff.returncode != 0:
        return [f".specify/feature.json: Feature 028 base revision is unavailable ({base_revision})"]
    paths = diff.stdout.splitlines()
    if untracked.returncode == 0:
        paths.extend(untracked.stdout.splitlines())
    return paths


def verify(root: pathlib.Path) -> list[str]:
    root = root.resolve()
    errors: list[str] = []
    private_root = (root / "Source/Asset/Private").resolve()
    private_headers = {
        path.name for path in private_root.rglob("*.h") if path.is_file()
    }
    metal_private_root = (root / "Source/Backend/Metal/Private").resolve()

    for layer in RUNTIME_LAYERS:
        layer_root = root / "Source" / layer
        if not layer_root.is_dir():
            continue
        for path in _source_files(layer_root):
            text = path.read_text(encoding="utf-8")
            resolved = path.resolve()
            in_metal_private = (
                resolved == metal_private_root or
                metal_private_root in resolved.parents
            )
            if path.suffix.lower() == ".mm" and not in_metal_private:
                errors.append(
                    f"{path.relative_to(root)}: Objective-C++ is restricted to "
                    "Backend/Metal/Private"
                )
            if not in_metal_private and METAL_NATIVE_PATTERN.search(text):
                errors.append(
                    f"{path.relative_to(root)}: native Metal ownership is restricted "
                    "to Backend/Metal/Private"
                )
            if "spirv_cross" in text:
                errors.append(
                    f"{path.relative_to(root)}: SPIRV-Cross is restricted to Tools"
                )
            if layer == "Asset" and ASSET_FORBIDDEN_NAMESPACE_PATTERN.search(text):
                errors.append(
                    f"{path.relative_to(root)}: Asset must not reference RHI, "
                    "Renderer, Application, or Backend APIs"
                )
            if layer in ("Renderer", "Application") and BACKEND_NAMESPACE_PATTERN.search(text):
                errors.append(
                    f"{path.relative_to(root)}: {layer} must not call native backend APIs"
                )
            if re.search(r"(?:ThirdParty[/\\]flip[/\\]FLIP\.h|#\s*include[^\n]*FLIP\.h)", text):
                errors.append(
                    f"{path.relative_to(root)}: FLIP is validation-only and must not link into runtime"
                )
            for include in INCLUDE_PATTERN.findall(text):
                if include.startswith("Tools/") or "/Tools/" in include:
                    errors.append(
                        f"{path.relative_to(root)}: runtime layer must not include Tools ({include})"
                    )
                if layer == "Asset" and include.startswith(GRAPHICS_PREFIXES):
                    errors.append(
                        f"{path.relative_to(root)}: Asset must not include graphics/runtime layer ({include})"
                    )
                if layer in ("Renderer", "Application") and include.startswith(
                        ("VulkanRHI/", "MetalRHI/")):
                    errors.append(
                        f"{path.relative_to(root)}: {layer} must not call native backend APIs ({include})"
                    )

    demo_root = root / "Demo"
    if demo_root.is_dir():
        for path in _source_files(demo_root):
            text = path.read_text(encoding="utf-8")
            if re.search(r"(?:ThirdParty[/\\]flip[/\\]FLIP\.h|#\s*include[^\n]*FLIP\.h)", text):
                errors.append(
                    f"{path.relative_to(root)}: FLIP is validation-only and must not link into runtime"
                )
        composition_root = demo_root / "StonerDemo/Private/FStonerDemoApplication.cpp"
        if composition_root.is_file():
            line_count = len(composition_root.read_text(encoding="utf-8").splitlines())
            if line_count > DEMO_COMPOSITION_ROOT_MAX_LINES:
                errors.append(
                    f"{composition_root.relative_to(root)}: Demo composition root exceeds "
                    f"{DEMO_COMPOSITION_ROOT_MAX_LINES}-line responsibility budget ({line_count})"
                )
        for sconscript in sorted(demo_root.rglob("SConscript")):
            text = sconscript.read_text(encoding="utf-8")
            if re.search(r"(?:ThirdParty[/\\]flip|['\"]flip['\"])", text, re.IGNORECASE):
                errors.append(
                    f"{sconscript.relative_to(root)}: FLIP is validation-only and must not link into runtime"
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
    feature_paths = _feature_added_paths(root)
    if feature_paths and feature_paths[0].startswith(".specify/feature.json:"):
        errors.extend(feature_paths)
    else:
        errors.extend(verify_feature_diff_paths(feature_paths))
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
        "Tool-to-Asset-private, backend call isolation, Metal/Objective-C++ "
        "ownership, validation-only FLIP, Demo composition budget, Tools-only "
        "SPIRV-Cross, native/private public API leakage, Feature scope)"
    )
    if args.stamp:
        args.stamp.parent.mkdir(parents=True, exist_ok=True)
        args.stamp.write_text("passed\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
