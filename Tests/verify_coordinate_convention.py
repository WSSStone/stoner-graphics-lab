#!/usr/bin/env python3
"""Verify the active Feature 024 coordinate-convention migration evidence."""

from __future__ import annotations

import argparse
from pathlib import Path


CONVENTION = "UnrealLH_ZUp_XForward_YRight_Meters_CW"
AMENDMENT = "## Feature 024 Coordinate Convention Amendment (2026-07-31)"
ACTIVE_FILES = {
    "Source/Core/Public/Core/FCoordinateConvention.h": (
        "FVector3::UnitX()",
        "FVector3::UnitY()",
        "FVector3::UnitZ()",
    ),
    "Source/RHI/Public/RHI/FRHIGraphicsPipelineDesc.h": ("FrontFace = ERHIFrontFace::Clockwise",),
    "Source/Renderer/Public/Renderer/FForwardViewData.h": (
        "TransformWorldPositionToView",
        "ComputeViewSpaceForwardDepth",
    ),
    "Source/Renderer/Private/FForwardViewData.cpp": (
        "return TransformWorldPositionToView(ViewMatrix, WorldPosition).X;",
        "return ComputeViewSpaceForwardDepth(ViewMatrix, WorldPosition);",
    ),
    "Source/Renderer/Public/Renderer/FShaderMatrixPacking.h": ("GLSL default-layout mat4 values are column-major",),
}
HISTORICAL_FILES = (
    "specs/004-core-math-library/plan.md",
    "specs/004-core-math-library/research.md",
    "specs/004-core-math-library/tasks.md",
    "specs/004-core-math-library/contracts/core-math-api.md",
    "specs/017-scene-graph-ecs/spec.md",
    "specs/018-triangle-demo-integration/spec.md",
    "specs/019-deferred-rendering-pipeline/spec.md",
)
FORBIDDEN_ACTIVE_TEXT = ("right-handed", "right handed", "+Z forward", "Z-forward")


def _read(root: Path, relative: str, errors: list[str]) -> str:
    path = root / relative
    if not path.is_file():
        errors.append(f"missing required file: {relative}")
        return ""
    return path.read_text(encoding="utf-8")


def verify(root: Path) -> list[str]:
    errors: list[str] = []
    convention_header = _read(root, "Source/Core/Public/Core/FCoordinateConvention.h", errors)
    if CONVENTION not in convention_header:
        errors.append("coordinate convention name is missing or stale")

    for relative, required_fragments in ACTIVE_FILES.items():
        contents = convention_header if relative.endswith("FCoordinateConvention.h") else _read(root, relative, errors)
        for fragment in required_fragments:
            if fragment not in contents:
                errors.append(f"{relative}: missing active-convention evidence: {fragment}")
        lowered = contents.lower()
        for forbidden in FORBIDDEN_ACTIVE_TEXT:
            if forbidden.lower() in lowered:
                errors.append(f"{relative}: stale active-convention text: {forbidden}")

    for relative in HISTORICAL_FILES:
        contents = _read(root, relative, errors)
        if AMENDMENT not in contents:
            errors.append(f"{relative}: missing Feature 024 historical amendment")
        elif CONVENTION not in contents:
            errors.append(f"{relative}: amendment does not name the active convention")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    errors = verify(args.root.resolve())
    if errors:
        print("\n".join(f"ERROR: {error}" for error in errors))
        return 1
    print("Feature 024 coordinate convention: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
