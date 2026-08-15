#!/usr/bin/env python3
"""Verify Feature 026 contract presence and the Asset-layer boundary."""

from __future__ import annotations

import argparse
import pathlib
import re


PUBLIC_HEADERS = (
    "FAssetManagerConfig.h",
    "FAssetRequestHandle.h",
    "TAssetHandle.h",
    "FAssetManager.h",
    "FAssetRuntimeExecutionContext.h",
    "FGenerationReaderLease.h",
    "FAssetManagerInspection.h",
)

PRIVATE_SOURCES = (
    "FAssetCompletionQueue.cpp",
    "FAssetDependencyScheduler.cpp",
    "FAssetNodeLoadCoordinator.cpp",
    "FAssetLoadOperationTable.cpp",
    "FAssetManagerInspection.cpp",
    "FAssetRuntimeCache.cpp",
    "FAssetWorkerExecutor.cpp",
    "FAssetRequestTable.cpp",
    "FGenerationReaderLease.cpp",
    "FBoundCookedGeneration.cpp",
    "FDevelopmentAssetLoadingStrategy.cpp",
    "FCookedAssetLoadingStrategy.cpp",
    "FAssetManager.cpp",
)

FORBIDDEN_PUBLIC_TOKENS = (
    "#include \"RHI/",
    "#include \"Renderer/",
    "#include \"Application/",
    "#include \"Backend/",
    "#include <vulkan/",
    "VkInstance",
    "VkDevice",
)

REQUIREMENT_RE = re.compile(r"^- \*\*(FR|SC)-(\d{3})\*\*:", re.MULTILINE)
TASK_RE = re.compile(r"^- \[[ xX]\] (T\d{3})\b", re.MULTILINE)

EXPECTED_REQUIREMENTS = {
    *(f"FR-{index:03d}" for index in range(1, 47)),
    *(f"SC-{index:03d}" for index in range(1, 14)),
}

TRACE_GROUPS = (
    (range(1, 11), ("Source/Asset/Private/FAssetManager.cpp",
                    "Source/Asset/Private/FAssetRequestTable.cpp",
                    "Tests/AssetManagerContractTests.cpp")),
    (range(11, 17), ("Source/Asset/Private/FAssetLoadOperationTable.cpp",
                     "Tests/AssetManagerCoalescingTests.cpp",
                     "Tests/AssetManagerCancellationTests.cpp")),
    (range(17, 25), ("Source/Asset/Private/FAssetDependencyScheduler.cpp",
                     "Source/Asset/Private/FDevelopmentAssetLoadingStrategy.cpp",
                     "Source/Asset/Private/FCookedAssetLoadingStrategy.cpp",
                     "Tests/AssetManagerDependencyTests.cpp")),
    (range(25, 33), ("Source/Asset/Private/FAssetRuntimeCache.cpp",
                     "Source/Asset/Public/Asset/TAssetHandle.h",
                     "Tests/AssetManagerLifetimeTests.cpp",
                     "Tests/AssetManagerShutdownTests.cpp")),
    (range(33, 36), ("Source/Asset/Private/FGenerationReaderLease.cpp",
                     "Source/Asset/Private/FBoundCookedGeneration.cpp",
                     "Tests/AssetManagerGenerationLeaseProcessTests.cpp")),
    (range(36, 37), ("Source/Asset/Private/FAssetCompletionQueue.cpp",
                     "Tests/AssetManagerCompletionTests.cpp")),
    (range(37, 42), ("Source/Asset/Private/FAssetManagerInspection.cpp",
                     "Tests/AssetManagerInspectionTests.cpp",
                     "Tests/AssetManagerStressTests.cpp")),
    (range(42, 43), ("Source/Asset/Public/Asset/AssetMinimal.h",
                     "Tests/verify_runtime_asset_manager.py")),
    (range(43, 44), ("specs/026-runtime-asset-manager/spec.md",)),
    (range(44, 45), ("Tests/AssetTests.cpp",
                     "Tests/AssetManagerStressTests.cpp")),
    (range(45, 46), (".github/workflows/feature-026-runtime-asset-manager.yml",)),
    (range(46, 47), ("Source/Asset/Private/FDevelopmentAssetLoadingStrategy.cpp",
                     "Tests/AssetManagerDevelopmentTests.cpp")),
)

SC_TRACE = {
    "SC-001": ("Tests/AssetManagerEquivalenceTests.cpp",),
    "SC-002": ("Tests/AssetManagerCoalescingTests.cpp",),
    "SC-003": ("Tests/AssetManagerCancellationTests.cpp",),
    "SC-004": ("Tests/AssetManagerDependencyTests.cpp",),
    "SC-005": ("Tests/AssetManagerCacheTests.cpp",),
    "SC-006": ("Tests/AssetManagerShutdownTests.cpp",),
    "SC-007": ("Tests/AssetManagerCookedTests.cpp",),
    "SC-008": ("Tests/AssetManagerGenerationLeaseProcessTests.cpp",),
    "SC-009": ("Tests/AssetManagerStressTests.cpp",
               "Validation/026/reports/determinism.txt"),
    "SC-010": ("Tests/AssetManagerStressTests.cpp",),
    "SC-011": ("Tests/verify_asset_layer.py",),
    "SC-012": (".github/workflows/feature-026-runtime-asset-manager.yml",),
    "SC-013": ("Tests/AssetManagerDevelopmentTests.cpp",),
}


def _read(path: pathlib.Path, errors: list[str]) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        errors.append(f"cannot read {path}: {error}")
        return ""


def verify(root: pathlib.Path) -> list[str]:
    root = root.resolve()
    errors: list[str] = []
    public = root / "Source/Asset/Public/Asset"
    private = root / "Source/Asset/Private"

    for name in PUBLIC_HEADERS:
        path = public / name
        if not path.is_file():
            errors.append(f"missing runtime public contract: {path.relative_to(root)}")
            continue
        text = _read(path, errors)
        for token in FORBIDDEN_PUBLIC_TOKENS:
            if token in text:
                errors.append(
                    f"runtime public contract leaks forbidden token {token!r}: "
                    f"{path.relative_to(root)}"
                )

    for name in PRIVATE_SOURCES:
        path = private / name
        if not path.is_file():
            errors.append(f"missing runtime implementation: {path.relative_to(root)}")

    spec = _read(root / "specs/026-runtime-asset-manager/spec.md", errors)
    tasks = _read(root / "specs/026-runtime-asset-manager/tasks.md", errors)
    requirements = {
        f"{kind}-{number}" for kind, number in REQUIREMENT_RE.findall(spec)
    }
    task_ids = TASK_RE.findall(tasks)
    if requirements != EXPECTED_REQUIREMENTS:
        missing = sorted(EXPECTED_REQUIREMENTS - requirements)
        unexpected = sorted(requirements - EXPECTED_REQUIREMENTS)
        errors.append(
            "Feature 026 requirement identity mismatch: "
            f"missing={missing}, unexpected={unexpected}"
        )
    if len(task_ids) != 71 or len(set(task_ids)) != len(task_ids):
        errors.append(
            f"expected 71 unique Feature 026 tasks, found {len(task_ids)} entries"
        )

    asset_minimal = _read(public / "AssetMinimal.h", errors)
    for name in PUBLIC_HEADERS:
        include = f'#include "Asset/{name}"'
        if (public / name).is_file() and include not in asset_minimal:
            errors.append(f"AssetMinimal.h does not expose {name}")

    trace: dict[str, tuple[str, ...]] = {}
    for indexes, paths in TRACE_GROUPS:
        for index in indexes:
            trace[f"FR-{index:03d}"] = paths
    trace.update(SC_TRACE)
    if set(trace) != EXPECTED_REQUIREMENTS:
        errors.append("internal FR/SC trace map is incomplete")
    for requirement, paths in sorted(trace.items()):
        for relative in paths:
            evidence = root / relative
            if not evidence.is_file() or evidence.stat().st_size == 0:
                errors.append(
                    f"{requirement} missing non-empty evidence: {relative}"
                )

    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, required=True)
    parser.add_argument("--stamp", type=pathlib.Path)
    arguments = parser.parse_args()
    errors = verify(arguments.root)
    for error in errors:
        print(f"ERROR: {error}")
    if not errors and arguments.stamp:
        arguments.stamp.parent.mkdir(parents=True, exist_ok=True)
        arguments.stamp.write_text("passed\n", encoding="ascii")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
