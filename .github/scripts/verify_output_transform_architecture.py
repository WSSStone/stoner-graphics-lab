#!/usr/bin/env python3
"""Feature 029 architecture and excluded-scope scanner."""

from __future__ import annotations

import argparse
from pathlib import Path
import re


TEXT_SUFFIXES = {".h", ".hpp", ".cpp", ".c", ".m", ".mm", ".glsl", ".vert", ".frag"}
OUTPUT_SCOPE_NAMES = (
    "OutputTransform", "HDRPostProcess", "HDRSceneColor", "PostProcessInsertion",
)


def text_files(root: Path, relative: str) -> list[Path]:
    base = root / relative
    if not base.exists():
        return []
    return sorted(path for path in base.rglob("*")
                  if path.is_file() and path.suffix in TEXT_SUFFIXES)


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
