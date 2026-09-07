#!/usr/bin/env python3
"""Feature 029/030 architecture and excluded-scope scanner."""

from __future__ import annotations

import argparse
from pathlib import Path
import re


TEXT_SUFFIXES = {".h", ".hpp", ".cpp", ".c", ".m", ".mm", ".glsl", ".vert", ".frag"}
OUTPUT_SCOPE_NAMES = (
    "OutputTransform", "HDRPostProcess", "HDRSceneColor", "PostProcessInsertion",
)
FEATURE_030_RUNTIME_ROOTS = (
    "Source/Application", "Source/Renderer", "Source/RHI", "Source/Backend",
    "Source/Core", "Source/Asset", "Demo/StonerDemo",
)
FEATURE_030_BUILD_ROOTS = (
    "Source", "Demo/StonerDemo", "Tests", "ThirdParty/yyjson", "ThirdParty/imgui",
)
FEATURE_030_PRIVATE_IMGUI_ROOT = "Source/Application/Private"
FEATURE_030_SHARED_YYJSON_ROOT = "ThirdParty/yyjson"


def text_files(root: Path, relative: str) -> list[Path]:
    base = root / relative
    if not base.exists():
        return []
    return sorted(path for path in base.rglob("*")
                  if path.is_file() and path.suffix in TEXT_SUFFIXES)


def build_files(root: Path, relative: str) -> list[Path]:
    """Return build scripts in a bounded source/build dependency scope."""
    base = root / relative
    if not base.exists():
        return []
    return sorted(path for path in base.rglob("SConscript") if path.is_file())


def _is_under(path: Path, root: Path, relative: str) -> bool:
    """Whether path is below a repository-relative root."""
    try:
        path.relative_to(root / relative)
        return True
    except ValueError:
        return False


def _feature_030_checks(root: Path, findings: list[str]) -> None:
    """Check Feature 030's private UI and build/runtime ownership boundaries.

    Feature 030 files are intentionally optional while the feature is being
    implemented.  These checks reject an observed boundary violation, but do
    not require planned files, directories, or future build targets to exist.
    """
    renderer_files = text_files(root, "Source/Renderer")
    application_files = text_files(root, "Source/Application")

    imgui_include = re.compile(
        r"#\s*include\s*[<\"](?:[^>\"]*/)?imgui(?:_internal)?\.h[>\"]",
        re.IGNORECASE,
    )
    imgui_api = re.compile(
        r"\b(?:ImGui[A-Za-z0-9_]*|ImVec[234][A-Za-z0-9_]*|ImRect[A-Za-z0-9_]*|"
        r"ImDraw[A-Za-z0-9_]*|ImFont[A-Za-z0-9_]*|ImTexture[A-Za-z0-9_]*|"
        r"IMGUI_[A-Z0-9_]+)\b"
    )
    imgui_official_backend = re.compile(
        r"\bimgui_(?:impl_[A-Za-z0-9_]+|demo(?:\.[A-Za-z0-9_]+)?)\b",
        re.IGNORECASE,
    )

    # ImGui is an Application-private implementation detail.  Keep the
    # allowed root explicit so planned FImGui* adapters can land incrementally
    # without allowing UI types to leak into Renderer, public headers, or Demo.
    for relative in ("Source", "Demo/StonerDemo"):
        for path in text_files(root, relative):
            if _is_under(path, root, FEATURE_030_PRIVATE_IMGUI_ROOT):
                text = path.read_text(encoding="utf-8", errors="replace")
                if imgui_official_backend.search(text):
                    findings.append(
                        f"{path.relative_to(root)}: official Dear ImGui platform/renderer backends are forbidden"
                    )
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            if imgui_include.search(text) or imgui_api.search(text):
                findings.append(
                    f"{path.relative_to(root)}: Dear ImGui must remain behind Application/Private adapters"
                )

    # Keep the same native API vocabulary as the existing Renderer check and
    # add native object/type/header forms that a new Application or Renderer
    # file could otherwise use without spelling a vk* call. GLFW is a window
    # driver dependency and remains intentionally outside this graphics API
    # boundary.
    application_renderer_native = re.compile(
        r"(?:#\s*include\s*[<\"](?:[^>\"]*/)?(?:vulkan/vulkan(?:_core)?\.h|"
        r"Metal/[^>\"]+|QuartzCore/[^>\"]+|VulkanRHI/|MetalRHI/|"
        r"Backend/(?:Vulkan|Metal))|"
        r"\bvk[A-Z]\w*\s*\(|\bVk[A-Z]\w*\b|\bVK_[A-Z0-9_]+\b|"
        r"\bMTL[A-Z]\w*\b|\bCAMetalLayer\b|\bid\s*<\s*MTL|"
        r"\bStoner::Backend::(?:Vulkan|Metal)::)"
    )
    legacy_renderer_native = re.compile(
        r"(?:#\s*include\s*[<\"].*(?:VulkanRHI|MetalRHI|Backend/(?:Vulkan|Metal))|"
        r"\bvk[A-Z]\w*\s*\(|\bMTL[A-Z]\w*|\bCAMetalLayer\b|\bid<MTL)"
    )
    for layer, files in (("Application", application_files),
                         ("Renderer", renderer_files)):
        for path in files:
            text = path.read_text(encoding="utf-8", errors="replace")
            if application_renderer_native.search(text):
                # Preserve the original Renderer finding and avoid reporting
                # the same legacy match twice.
                if layer == "Renderer" and legacy_renderer_native.search(text):
                    continue
                findings.append(
                    f"{path.relative_to(root)}: {layer} calls or includes a native backend API"
                )

    yyjson_include = re.compile(
        r"#\s*include\s*[<\"](?:[^>\"]*/)?yyjson(?:[/\\]yyjson)?\.h[>\"]",
        re.IGNORECASE,
    )
    yyjson_api = re.compile(r"\b(?:yyjson_|YYJSON_)[A-Za-z0-9_]+\b")
    for relative in FEATURE_030_RUNTIME_ROOTS:
        for path in text_files(root, relative):
            # The vendored implementation/header is the dependency itself,
            # not a consumer boundary to validate.
            if _is_under(path, root, FEATURE_030_SHARED_YYJSON_ROOT):
                continue
            text = path.read_text(encoding="utf-8", errors="replace")
            if not (yyjson_include.search(text) or yyjson_api.search(text)):
                continue
            if _is_under(path, root, "Source/Asset/Private") or _is_under(
                path, root, FEATURE_030_PRIVATE_IMGUI_ROOT
            ):
                continue
            findings.append(
                f"{path.relative_to(root)}: yyjson is build-only and private to Asset/Application implementations"
            )

    legacy_runtime_compile = re.compile(
        r"\b(?:glslang|shaderc|glslc|dxc|CreateLibraryWithSource|"
        r"newLibraryWithSource|compileShaderSource)\b", re.IGNORECASE)
    runtime_compile = re.compile(
        r"\b(?:glslang(?:Validator)?|shaderc|glslc|dxc|CreateLibraryWithSource|"
        r"newLibraryWithSource|compileShaderSource|compileShader)\b",
        re.IGNORECASE,
    )
    # Keep the original Renderer/Backend diagnostics and coverage intact;
    # these additions cover runtime owners that were outside the 029 scan.
    for relative in ("Source/Application", "Source/Renderer", "Source/RHI", "Source/Core", "Source/Asset",
                     "Source/Backend", "Demo/StonerDemo"):
        for path in text_files(root, relative):
            text = path.read_text(encoding="utf-8", errors="replace")
            if not runtime_compile.search(text):
                continue
            # The original 029 loops already report legacy tokens for
            # Renderer/Vulkan/Metal. Preserve those messages exactly, while
            # covering the newly forbidden tokens on every runtime root.
            original_loop_covers_path = (
                _is_under(path, root, "Source/Renderer") or
                _is_under(path, root, "Source/Backend/Vulkan") or
                _is_under(path, root, "Source/Backend/Metal")
            )
            if original_loop_covers_path and legacy_runtime_compile.search(text):
                continue
            findings.append(
                f"{path.relative_to(root)}: runtime shader compilation is forbidden"
            )

    build_scripts: list[Path] = []
    for relative in FEATURE_030_BUILD_ROOTS:
        build_scripts.extend(build_files(root, relative))
    build_scripts = sorted(set(build_scripts))

    # The shared library may be absent until T081.  Before that migration the
    # existing Asset-private C source is valid; after it appears, only the
    # dedicated yyjson build script may compile the implementation.
    yyjson_c = re.compile(r"(?:ThirdParty[/\\]yyjson[/\\])?yyjson\.c")
    yyjson_compilers = [
        path for path in build_scripts
        if yyjson_c.search(path.read_text(encoding="utf-8", errors="replace"))
    ]
    shared_yyjson_script = root / FEATURE_030_SHARED_YYJSON_ROOT / "SConscript"
    if len(yyjson_compilers) > 1:
        for path in yyjson_compilers:
            findings.append(
                f"{path.relative_to(root)}: yyjson implementation must be compiled once as a shared build-only library"
            )
    elif shared_yyjson_script.is_file() and yyjson_compilers and yyjson_compilers[0] != shared_yyjson_script:
        findings.append(
            f"{yyjson_compilers[0].relative_to(root)}: yyjson implementation must be compiled by ThirdParty/yyjson/SConscript"
        )
    elif yyjson_compilers and yyjson_compilers[0] not in (
        shared_yyjson_script, root / "Source/Asset/SConscript"
    ):
        findings.append(
            f"{yyjson_compilers[0].relative_to(root)}: yyjson implementation may only be compiled by Asset or the shared yyjson build script"
        )

    # Official ImGui backends or the demo translation unit would introduce a
    # second platform/UI ownership path even when they are mentioned only by
    # a build script. The four core files remain free to be listed here.
    for path in build_scripts:
        text = path.read_text(encoding="utf-8", errors="replace")
        if imgui_official_backend.search(text):
            findings.append(
                f"{path.relative_to(root)}: official Dear ImGui backends/demo sources are forbidden"
            )


def scan(root: Path) -> list[str]:
    findings: list[str] = []
    renderer_files = text_files(root, "Source/Renderer")
    output_files = [path for path in renderer_files
                    if any(name in path.name for name in OUTPUT_SCOPE_NAMES)]
    output_files += text_files(root, "Content/Shaders/PostProcess")

    renderer_native = re.compile(
        r"(?:#\s*include\s*[<\"].*(?:VulkanRHI|MetalRHI|Backend/(?:Vulkan|Metal))|"
        r"\bvk[A-Z]\w*\s*\(|\bMTL[A-Z]\w*|\bCAMetalLayer\b|\bid<MTL)")
    backend_color_policy = re.compile(
        r"\b(?:khronosPbrNeutral|narkowiczAcesFit|extendedReinhard|"
        r"applyAces2|toneMap|tonemap|manualExposure|ExposureScale)\b",
        re.IGNORECASE)
    runtime_compile = re.compile(
        r"\b(?:glslang|shaderc|glslc|dxc|CreateLibraryWithSource|"
        r"newLibraryWithSource|compileShaderSource)\b", re.IGNORECASE)
    excluded_scope = re.compile(
        r"\b(?:TAA|FXAA|DLSS|FSR|XeSS|temporal\s+reconstruction|"
        r"motion\s*vector|camera\s*history|jitter\s*sequence|bloom|"
        r"depth\s*of\s*field|DoF|motion\s*blur|auto(?:matic)?\s*exposure|"
        r"vendor\s*upscal|post[- ]process\s*editor)\b", re.IGNORECASE)

    for path in renderer_files:
        text = path.read_text(encoding="utf-8", errors="replace")
        if renderer_native.search(text):
            findings.append(f"{path.relative_to(root)}: Renderer calls or includes a native backend API")
        if runtime_compile.search(text):
            findings.append(f"{path.relative_to(root)}: runtime shader compilation is forbidden")

    for backend in ("Source/Backend/Vulkan", "Source/Backend/Metal"):
        for path in text_files(root, backend):
            text = path.read_text(encoding="utf-8", errors="replace")
            if backend_color_policy.search(text):
                findings.append(f"{path.relative_to(root)}: backend-private tone/exposure policy is forbidden")
            if runtime_compile.search(text):
                findings.append(f"{path.relative_to(root)}: backend runtime shader compilation is forbidden")

    for path in output_files:
        text = path.read_text(encoding="utf-8", errors="replace")
        if excluded_scope.search(text):
            findings.append(f"{path.relative_to(root)}: excluded temporal/AA/effect/editor scope detected")

    graph_definition_count = 0
    formal_output_creation_count = 0
    for path in renderer_files:
        text = path.read_text(encoding="utf-8", errors="replace")
        graph_definition_count += text.count(
            "FOutputTransformGraphDeclaration FHDRPostProcessPipeline::DeclareGraph(")
        formal_output_creation_count += text.count(
            '"Output.FinalOutput"')
    if graph_definition_count != 1:
        findings.append(
            f"Source/Renderer: expected one output-graph definition, found {graph_definition_count}")
    if formal_output_creation_count != 1:
        findings.append(
            f"Source/Renderer: expected one formal-output graph resource, found {formal_output_creation_count}")
    _feature_030_checks(root, findings)
    return findings


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", default=".")
    args = parser.parse_args()
    findings = scan(Path(args.root).resolve())
    for finding in findings:
        print(f"finding: {finding}")
    print(f"output-transform architecture: {'FAILED' if findings else 'PASS'} "
          f"({len(findings)} findings)")
    return 1 if findings else 0


if __name__ == "__main__":
    raise SystemExit(main())
