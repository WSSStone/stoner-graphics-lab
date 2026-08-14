#!/usr/bin/env python3
"""Verify Asset, Renderer, and native API architecture boundaries."""

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

APPROVED_THIRD_PARTY_INCLUDES = {
    (
        "Source/Asset/Private/FStbImageDecode.cpp",
        "../../../ThirdParty/stb/stb_image.h",
    ),
    (
        "Source/Asset/Private/FWamrEncoderRuntime.cpp",
        "wasm_export.h",
    ),
    (
        "Source/Asset/Private/FKTX2ContainerCodec.cpp",
        "ktx.h",
    ),
    (
        "Source/Asset/Private/FKTX2TextureCodec.cpp",
        "ktx.h",
    ),
    (
        "Source/Asset/Private/FBasisTextureTranscoder.cpp",
        "ktx.h",
    ),
    (
        "Source/Asset/Private/FMaterialShaderJsonCodec.cpp",
        "../../../ThirdParty/yyjson/yyjson.h",
    ),
    (
        "Source/Asset/Private/FGLTFStableKey.cpp",
        "../../../ThirdParty/yyjson/yyjson.h",
    ),
    (
        "Source/Asset/Private/FCgltfDocument.cpp",
        "../../../ThirdParty/cgltf/cgltf.h",
    ),
    *{
        (f"Source/Asset/Private/{name}.cpp", "cgltf/cgltf.h")
        for name in (
            "FGLTFHierarchyBuilder",
            "FGLTFImageTextureBridge",
            "FGLTFMaterialMapper",
            "FGLTFPackageAssembler",
            "FGLTFPackageIdentityPlanner",
            "FGLTFPackageValidator",
            "FGLTFStaticModelImporter",
        )
    },
    (
        "Source/Asset/Private/FStaticMeshTangentGenerator.cpp",
        "mikktspace/mikktspace.h",
    ),
}

THIRD_PARTY_PATTERNS = (
    re.compile(r"(?:^|/)cgltf(?:/|\.h$)"),
    re.compile(r"(?:^|/)mikktspace(?:/|\.h$)"),
    re.compile(r"(?:^|/)stb_image\.h$"),
    re.compile(r"(?:^|/)yyjson(?:/|\.h$)"),
)
NATIVE_TYPE_PATTERN = re.compile(r"\b(?:Vk[A-Z][A-Za-z0-9_]*|GLFW[A-Za-z0-9_]*)\b")


def verify(root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    asset_root = root / "Source" / "Asset"
    for path in sorted(asset_root.rglob("*")):
        if not path.is_file() or path.suffix not in {".h", ".cpp"}:
            continue
        text = path.read_text(encoding="utf-8")
        for include in re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', text, re.MULTILINE):
            relative = path.relative_to(root).as_posix()
            if include.startswith(FORBIDDEN_PREFIXES):
                errors.append(f"{path.relative_to(root)}: forbidden include {include}")
            if "utf8proc" in include.lower():
                errors.append(f"{path.relative_to(root)}: utf8proc leaks into Asset")
            if (
                include in {"ktx.h", "wasm_export.h"}
                and (relative, include)
                not in APPROVED_THIRD_PARTY_INCLUDES
            ):
                errors.append(
                    f"{relative}: {include} may only be included by an "
                    "approved private adapter"
                )
            if any(pattern.search(include.lower()) for pattern in THIRD_PARTY_PATTERNS) and (
                relative, include
            ) not in APPROVED_THIRD_PARTY_INCLUDES:
                errors.append(
                    f"{relative}: third-party include {include} is not an "
                    "approved private adapter dependency"
                )

    for public_root in (
        root / "Source" / "Asset" / "Public",
        root / "Source" / "Renderer" / "Public",
        root / "Source" / "RHI" / "Public",
    ):
        for path in sorted(public_root.rglob("*.h")):
            text = path.read_text(encoding="utf-8")
            includes = re.findall(
                r'^\s*#\s*include\s*[<"]([^>"]+)[>"]', text, re.MULTILINE
            )
            for include in includes:
                if any(
                    pattern.search(include.lower())
                    for pattern in THIRD_PARTY_PATTERNS
                ):
                    errors.append(
                        f"{path.relative_to(root)}: third-party dependency leaks into a public header"
                    )
            if NATIVE_TYPE_PATTERN.search(text):
                errors.append(
                    f"{path.relative_to(root)}: native graphics type leaks into a public header"
                )

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
    if not re.search(
        r"BuildLayer\s*\(\s*env\s*,\s*['\"]Asset['\"]\s*,\s*\[['\"]Core['\"]\]",
        sconscript,
    ):
        errors.append("Source/Asset/SConscript: Asset must declare Core as its only dependency")
    if "utf8proc" in sconscript.lower():
        errors.append("Source/Asset/SConscript: Asset must not compile third-party Unicode sources")
    if "'ThirdParty/stb'" in sconscript:
        errors.append(
            "Source/Asset/SConscript: do not expose the stb directory as an "
            "include search path because VERSION shadows standard <version> "
            "on case-insensitive filesystems"
        )
    if "'ThirdParty/yyjson'" in sconscript:
        errors.append(
            "Source/Asset/SConscript: do not expose the yyjson directory as "
            "an include path because VERSION shadows standard <version>"
        )
    if "ThirdParty/yyjson/yyjson.c" not in sconscript:
        errors.append(
            "Source/Asset/SConscript: yyjson must compile as a private C source"
        )
    for source in (
        "ThirdParty/cgltf/cgltf.c",
        "ThirdParty/mikktspace/mikktspace.c",
    ):
        if source not in sconscript:
            errors.append(
                f"Source/Asset/SConscript: {source} must compile as a private C source"
            )

    backend_root = root / "Source" / "Backend"
    for path in sorted(backend_root.rglob("*")):
        if not path.is_file() or path.suffix not in {".h", ".cpp"}:
            continue
        for include in re.findall(
            r'^\s*#\s*include\s*[<"]([^>"]+)[>"]',
            path.read_text(encoding="utf-8"),
            re.MULTILINE,
        ):
            if include.startswith("Asset/"):
                errors.append(
                    f"{path.relative_to(root)}: Backend must not include Asset"
                )

    renderer_root = root / "Source" / "Renderer"
    renderer_forbidden = (
        re.compile(r"#\s*include\s*[<\"]filesystem[>\"]"),
        re.compile(r"\bstd::filesystem\b"),
        re.compile(r"\bcgltf\b", re.IGNORECASE),
        re.compile(r"\bmikktspace\b", re.IGNORECASE),
        re.compile(r"\byyjson\b", re.IGNORECASE),
        re.compile(r"\bstbi?_[A-Za-z0-9_]+\b"),
        re.compile(r"[\"']\.[gG][lL][tT][fFbB][\"']"),
    )
    for path in sorted(renderer_root.rglob("*")):
        if not path.is_file() or path.suffix not in {".h", ".cpp"}:
            continue
        text = path.read_text(encoding="utf-8")
        if any(pattern.search(text) for pattern in renderer_forbidden):
            errors.append(
                f"{path.relative_to(root)}: Renderer must not parse source formats or access files"
            )

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
    print(
        "Asset architecture boundaries passed: Asset -> Core; third-party "
        "parsers remain private; Renderer owns no files/source parsing; "
        "Backend owns no Asset dependency; public APIs expose no native types"
    )
    if args.stamp:
        args.stamp.parent.mkdir(parents=True, exist_ok=True)
        args.stamp.write_text("passed\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
