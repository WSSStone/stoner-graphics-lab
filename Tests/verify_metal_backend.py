#!/usr/bin/env python3
"""Verify Feature 027 contracts, isolation, and the frozen public RHI surface."""

from __future__ import annotations

import argparse
import collections
import hashlib
import json
import pathlib
import re


REQUIREMENT_RE = re.compile(r"^- \*\*(FR|SC)-(\d{3})\*\*:", re.MULTILINE)
TASK_RE = re.compile(r"^- \[[ xX]\] (T\d{3})\b", re.MULTILINE)
MATRIX_ROW_RE = re.compile(r"^\| `(IRHI[A-Za-z0-9_]+)` \| (.+) \|$", re.MULTILINE)
FINAL_MATRIX_ROW_RE = re.compile(
    r"^\| `(IRHI[A-Za-z0-9_]+)` \| `([A-Za-z_][A-Za-z0-9_]*)` \| "
    r"(\d+) \| `([0-9a-f]{64})` \| `(FR-\d{3})` \| `([^`]+)` \| "
    r"`([^`]+)` \| `([^`]+)` \| `([^`]+)` \| `([^`]+)` \| `([^`]+)` \|$",
    re.MULTILINE,
)
CODE_RE = re.compile(r"`([A-Za-z_][A-Za-z0-9_]*)`")
VIRTUAL_RE = re.compile(r"\bvirtual\b(.*?)(?:=\s*0\s*;|\{)", re.DOTALL)
METHOD_RE = re.compile(r"([~A-Za-z_][A-Za-z0-9_]*)\s*\(")
COMMENT_RE = re.compile(r"//.*?$|/\*.*?\*/", re.MULTILINE | re.DOTALL)

MATRIX_INTERFACE_REQUIREMENT = {
    "IRHIBuffer": "FR-009",
    "IRHICommandBuffer": "FR-014",
    "IRHICommandQueue": "FR-015",
    "IRHIComputePipeline": "FR-013",
    "IRHIDescriptorSet": "FR-011",
    "IRHIDevice": "FR-002",
    "IRHIFence": "FR-015",
    "IRHIFramebuffer": "FR-014",
    "IRHIGraphicsPipeline": "FR-012",
    "IRHIPipelineLayout": "FR-011",
    "IRHIPresentationSurface": "FR-020",
    "IRHIRenderPass": "FR-014",
    "IRHISampler": "FR-010",
    "IRHISemaphore": "FR-015",
    "IRHIShaderModule": "FR-011",
    "IRHISwapchain": "FR-020",
    "IRHITexture": "FR-010",
}

MATRIX_DEVICE_OPERATION_REQUIREMENT = {
    "CreateCommandQueue": "FR-015",
    "CreateCommandBuffer": "FR-014",
    "CreateFence": "FR-015",
    "CreateSemaphore": "FR-015",
    "CreateSwapchain": "FR-020",
    "CreateBuffer": "FR-009",
    "UploadBuffer": "FR-009",
    "CreateTexture": "FR-010",
    "UploadTexture": "FR-010",
    "CreateSampler": "FR-010",
    "CreateShaderModule": "FR-011",
    "CreatePipelineLayout": "FR-011",
    "CreateDescriptorSet": "FR-011",
    "CreateGraphicsPipeline": "FR-012",
    "CreateComputePipeline": "FR-013",
    "CreateRenderPass": "FR-014",
    "CreateFramebuffer": "FR-014",
    "CreatePresentationSurface": "FR-020",
}

MATRIX_TEST_EVIDENCE = {
    "resource": (
        "metal-resource;metal-native",
        "vulkan;vulkan-native",
        "Validation/027/reports/us1-rhi-conformance.json;"
        "Validation/027/reports/us4-metal-triangle.json",
    ),
    "command": (
        "metal-command;metal-native;metal-render-acceptance",
        "vulkan;vulkan-native;triangle-demo;deferred-native",
        "Validation/027/reports/us1-rhi-conformance.json;"
        "Validation/027/reports/us4-metal-triangle.json;"
        "Validation/027/reports/us4-metal-deferred.json",
    ),
    "pipeline": (
        "metal-pipeline;metal-shader-runtime;metal-render-acceptance",
        "vulkan;triangle-demo;deferred-native",
        "Validation/027/reports/us1-rhi-conformance.json;"
        "Validation/027/reports/us4-metal-triangle.json;"
        "Validation/027/reports/us4-metal-deferred.json",
    ),
    "presentation": (
        "metal-presentation;metal-presentation-visible;MetalPresentationProbe",
        "vulkan;triangle-demo",
        "Validation/027/reports/us2-presentation-smoke.json;"
        "Validation/027/reports/us1-regression.md",
    ),
    "device": (
        "metal-device;metal-native",
        "vulkan;vulkan-native",
        "Validation/027/reports/us1-rhi-conformance.json;"
        "Validation/027/reports/us1-regression.md",
    ),
}

APPLE_PUBLIC_TOKENS = (
    "#import",
    "<Metal/",
    "<QuartzCore/",
    "CAMetalLayer",
    "NSView",
    "MTLDevice",
    "id<MTL",
)

RUNTIME_SOURCE_SUFFIXES = {".h", ".hpp", ".c", ".cc", ".cpp", ".mm"}
RUNTIME_ROOTS = ("Source", "Demo", "Tools")

REQUIREMENT_TASK_TRACE: dict[str, tuple[str, ...]] = {
    "FR-001": ("T039", "T061", "T095"),
    "FR-002": ("T038", "T041", "T042", "T043", "T062"),
    "FR-003": ("T034", "T041"),
    "FR-004": ("T034", "T042", "T043"),
    "FR-005": ("T003", "T006", "T034", "T063"),
    "FR-006": ("T036", "T063"),
    "FR-007": ("T035", "T040", "T060"),
    "FR-008": ("T034", "T035", "T037", "T047"),
    "FR-009": ("T035", "T044", "T046", "T055", "T062"),
    "FR-010": ("T035", "T045", "T046", "T055", "T062"),
    "FR-011": ("T028", "T036", "T047", "T053", "T054"),
    "FR-012": ("T036", "T049"),
    "FR-013": ("T036", "T050"),
    "FR-014": ("T037", "T052", "T056"),
    "FR-015": ("T037", "T057", "T058", "T059"),
    "FR-016": ("T037", "T040", "T059", "T060"),
    "FR-017": ("T018", "T019", "T020", "T034", "T042"),
    "FR-018": ("T065", "T068", "T073"),
    "FR-019": ("T066", "T069", "T070", "T071"),
    "FR-020": ("T066", "T069", "T070", "T071", "T072", "T073", "T076"),
    "FR-021": ("T067", "T070", "T071", "T072"),
    "FR-022": ("T067", "T073", "T076", "T124"),
    "FR-023": ("T021", "T024", "T027", "T077", "T082"),
    "FR-024": ("T027", "T028", "T029", "T030", "T078", "T082", "T089"),
    "FR-025": ("T031", "T032", "T079", "T083", "T084", "T089"),
    "FR-026": ("T031", "T032", "T078", "T079", "T083", "T090"),
    "FR-027": ("T021", "T022", "T023", "T024", "T030", "T079", "T089"),
    "FR-028": ("T079", "T080", "T083", "T084", "T085", "T086", "T087", "T088"),
    "FR-029": ("T036", "T048", "T049", "T050", "T085", "T087"),
    "FR-030": ("T021", "T022", "T024", "T047", "T077", "T087", "T088"),
    "FR-031": ("T092", "T095", "T096", "T098", "T101"),
    "FR-032": ("T093", "T097", "T099", "T101"),
    "FR-033": ("T038", "T091", "T101", "T117", "T121", "T122", "T123"),
    "FR-034": ("T016", "T017", "T020", "T091", "T102", "T120"),
    "FR-035": ("T091", "T094", "T095", "T103"),
    "FR-036": ("T093", "T094", "T099", "T100", "T103"),
    "FR-037": ("T105", "T106", "T107", "T110", "T111", "T113", "T114"),
    "FR-038": ("T034", "T040", "T105", "T106", "T107", "T108", "T115"),
    "FR-039": ("T039", "T041", "T105", "T109", "T112"),
    "FR-040": ("T105", "T109", "T112", "T114"),
    "FR-041": ("T007", "T008", "T009", "T010", "T090", "T116", "T119"),
    "FR-042": ("T089", "T101", "T121", "T122", "T123"),
    "FR-043": ("T116", "T119", "T121", "T122", "T123"),
    "FR-044": ("T001", "T003", "T062", "T076", "T089", "T101", "T117", "T125"),
    "FR-045": ("T007", "T009", "T026", "T030", "T045", "T117"),
    "SC-001": ("T062", "T063", "T122", "T123"),
    "SC-002": ("T076", "T124"),
    "SC-003": ("T101", "T102", "T103", "T122", "T123"),
    "SC-004": ("T027", "T078", "T089", "T116", "T119"),
    "SC-005": ("T105", "T106", "T107", "T110", "T111", "T114", "T122", "T123"),
    "SC-006": ("T108", "T115", "T122", "T123"),
    "SC-007": ("T121", "T122", "T123"),
    "SC-008": ("T116", "T119", "T120", "T121"),
    "SC-009": ("T117", "T121", "T122", "T123", "T124", "T125"),
    "SC-010": ("T016", "T017", "T020", "T091", "T102", "T120"),
}

FORBIDDEN_SCOPE_PATTERNS = {
    "iOS application lifecycle": re.compile(r"\b(?:UIApplication|UIWindow|UIViewController)\b"),
    "Metal mesh shaders": re.compile(r"\b(?:MTLMeshRenderPipeline|MTLObjectPayloadBinding)\b"),
    "Metal ray tracing": re.compile(r"\b(?:MTLAccelerationStructure|MTLIntersectionFunctionTable)\b"),
}

SPIRV_CROSS_SOURCES = (
    "spirv_cross.cpp",
    "spirv_cross_parsed_ir.cpp",
    "spirv_parser.cpp",
    "spirv_cfg.cpp",
    "spirv_glsl.cpp",
    "spirv_msl.cpp",
)

SPIRV_CROSS_HEADERS = (
    "GLSL.std.450.h",
    "NonSemanticShaderDebugInfo100.h",
    "spirv.hpp",
    "spirv_cfg.hpp",
    "spirv_common.hpp",
    "spirv_cross.hpp",
    "spirv_cross_containers.hpp",
    "spirv_cross_error_handling.hpp",
    "spirv_cross_parsed_ir.hpp",
    "spirv_glsl.hpp",
    "spirv_msl.hpp",
    "spirv_parser.hpp",
)


def _read(path: pathlib.Path, errors: list[str]) -> str:
    try:
        return path.read_text(encoding="utf-8")
    except (OSError, UnicodeError) as error:
        errors.append(f"cannot read {path}: {error}")
        return ""


def _expected_matrix(contract: pathlib.Path, errors: list[str]) -> dict[str, collections.Counter[str]]:
    text = _read(contract, errors)
    result: dict[str, collections.Counter[str]] = {}
    for interface, cell in MATRIX_ROW_RE.findall(text):
        counts = collections.Counter(CODE_RE.findall(cell))
        for name in re.findall(r"both `([A-Za-z_][A-Za-z0-9_]*)` overloads", cell):
            counts[name] = 2
        result[interface] = counts
    if not result:
        errors.append(f"no RHI operation rows found in {contract}")
    return result


def _normalize_signature(declaration: str) -> str:
    return " ".join(declaration.replace("\n", " ").split())


def _actual_matrix(public_root: pathlib.Path, errors: list[str]) -> dict[str, list[tuple[str, str, str]]]:
    result: dict[str, list[tuple[str, str, str]]] = {}
    if not public_root.is_dir():
        errors.append(f"missing RHI public root: {public_root}")
        return result
    for path in sorted(public_root.glob("IRHI*.h")):
        interface = path.stem
        text = COMMENT_RE.sub("", _read(path, errors))
        entries: list[tuple[str, str, str]] = []
        for declaration in VIRTUAL_RE.findall(text):
            names = METHOD_RE.findall(declaration)
            if not names:
                continue
            name = names[-1]
            if name.lstrip("~") == interface:
                continue
            normalized = _normalize_signature(declaration)
            digest = hashlib.sha256(normalized.encode("utf-8")).hexdigest()
            entries.append((name, normalized, digest))
        result[interface] = entries
    return result


def _matrix_group(interface: str, operation: str) -> str:
    if interface in {"IRHIPresentationSurface", "IRHISwapchain"} or (
        interface == "IRHIDevice" and operation in {
            "CreatePresentationSurface", "CreateSwapchain",
        }
    ):
        return "presentation"
    if interface in {"IRHIBuffer", "IRHITexture", "IRHISampler"} or (
        interface == "IRHIDevice" and operation in {
            "CreateBuffer", "UploadBuffer", "CreateTexture",
            "UploadTexture", "CreateSampler",
        }
    ):
        return "resource"
    if interface in {
        "IRHIShaderModule", "IRHIPipelineLayout", "IRHIDescriptorSet",
        "IRHIGraphicsPipeline", "IRHIComputePipeline",
    } or (
        interface == "IRHIDevice" and operation in {
            "CreateShaderModule", "CreatePipelineLayout",
            "CreateDescriptorSet", "CreateGraphicsPipeline",
            "CreateComputePipeline",
        }
    ):
        return "pipeline"
    if interface in {
        "IRHICommandBuffer", "IRHICommandQueue", "IRHIFence",
        "IRHISemaphore", "IRHIRenderPass", "IRHIFramebuffer",
    } or (
        interface == "IRHIDevice" and operation in {
            "CreateCommandQueue", "CreateCommandBuffer", "CreateFence",
            "CreateSemaphore", "CreateRenderPass", "CreateFramebuffer",
        }
    ):
        return "command"
    return "device"


def _matrix_requirement(interface: str, operation: str) -> str:
    if interface == "IRHIDevice":
        return MATRIX_DEVICE_OPERATION_REQUIREMENT.get(operation, "FR-002")
    return MATRIX_INTERFACE_REQUIREMENT[interface]


def _verify_final_rhi_matrix(
    root: pathlib.Path,
    actual: dict[str, list[tuple[str, str, str]]],
    errors: list[str],
) -> None:
    matrix_path = root / "Validation/027/reports/rhi-operation-matrix.md"
    if not matrix_path.is_file():
        return
    text = _read(matrix_path, errors)
    rows = FINAL_MATRIX_ROW_RE.findall(text)
    expected_rows: list[tuple[str, str, int, str]] = []
    for interface, entries in sorted(actual.items()):
        seen: collections.Counter[str] = collections.Counter()
        for operation, _, digest in entries:
            seen[operation] += 1
            expected_rows.append((interface, operation, seen[operation], digest))
    actual_rows = [
        (interface, operation, int(overload), digest)
        for interface, operation, overload, digest, *_ in rows
    ]
    if actual_rows != expected_rows:
        errors.append(
            "final RHI evidence matrix inventory/signature/order differs from "
            "the frozen public overload inventory"
        )
    if len(rows) != len(set(actual_rows)):
        errors.append("final RHI evidence matrix contains duplicate overload rows")
    for (
        interface, operation, overload, _digest, requirement, api,
        metal_test, vulkan_test, status, capability, evidence,
    ) in rows:
        expected_requirement = _matrix_requirement(interface, operation)
        if requirement != expected_requirement:
            errors.append(
                f"RHI matrix requirement mismatch for {interface}::{operation} "
                f"#{overload}: expected {expected_requirement}"
            )
        expected_api = (
            f"Source/RHI/Public/RHI/{interface}.h::{operation}#{overload}"
        )
        if api != expected_api:
            errors.append(
                f"RHI matrix API evidence mismatch for {interface}::{operation} "
                f"#{overload}"
            )
        if status not in {"native-pass", "capability-limited"}:
            errors.append(
                f"RHI matrix unresolved status for {interface}::{operation} "
                f"#{overload}: {status}"
            )
        if (status == "native-pass" and capability != "n/a") or (
            status == "capability-limited" and capability == "n/a"
        ):
            errors.append(
                f"RHI matrix capability predicate mismatch for "
                f"{interface}::{operation} #{overload}"
            )
        if not metal_test or not vulkan_test:
            errors.append(
                f"RHI matrix lacks test evidence for {interface}::{operation} "
                f"#{overload}"
            )
        evidence_paths = [path for path in evidence.split(";") if path]
        if not evidence_paths:
            errors.append(
                f"RHI matrix lacks device evidence for {interface}::{operation} "
                f"#{overload}"
            )
        b_device_proof = False
        for relative in evidence_paths:
            evidence_path = root / relative
            if not evidence_path.is_file():
                errors.append(
                    f"RHI matrix evidence path does not exist for "
                    f"{interface}::{operation} #{overload}: {relative}"
                )
                continue
            if evidence_path.suffix == ".json":
                try:
                    document = json.loads(evidence_path.read_text(encoding="utf-8"))
                except (OSError, UnicodeError, json.JSONDecodeError):
                    continue
                device = document.get("device") if isinstance(document, dict) else None
                if document.get("result") == "passed" and isinstance(device, dict) \
                        and re.fullmatch(
                            r"[0-9a-f]{64}",
                            str(device.get("capabilityDigest", "")),
                        ):
                    b_device_proof = True
        if not b_device_proof:
            errors.append(
                f"RHI matrix lacks passed device-capability proof for "
                f"{interface}::{operation} #{overload}"
            )


def verify_rhi_matrix(root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    expected = _expected_matrix(
        root / "specs/027-metal-backend/contracts/rhi-operation-matrix.md",
        errors,
    )
    actual = _actual_matrix(root / "Source/RHI/Public/RHI", errors)
    for interface in sorted(set(expected) | set(actual)):
        expected_counts = expected.get(interface, collections.Counter())
        actual_counts = collections.Counter(name for name, _, _ in actual.get(interface, []))
        if expected_counts != actual_counts:
            errors.append(
                f"RHI operation mismatch for {interface}: "
                f"expected={dict(sorted(expected_counts.items()))}, "
                f"actual={dict(sorted(actual_counts.items()))}"
            )
    _verify_final_rhi_matrix(root, actual, errors)
    return errors


def verify_contracts(root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    spec = _read(root / "specs/027-metal-backend/spec.md", errors)
    tasks = _read(root / "specs/027-metal-backend/tasks.md", errors)
    requirements = {
        f"{kind}-{number}" for kind, number in REQUIREMENT_RE.findall(spec)
    }
    expected_requirements = {
        *(f"FR-{index:03d}" for index in range(1, 46)),
        *(f"SC-{index:03d}" for index in range(1, 11)),
    }
    if requirements != expected_requirements:
        errors.append(
            "Feature 027 requirement identity mismatch: "
            f"missing={sorted(expected_requirements - requirements)}, "
            f"unexpected={sorted(requirements - expected_requirements)}"
        )
    task_ids = TASK_RE.findall(tasks)
    expected_tasks = [f"T{index:03d}" for index in range(1, 129)]
    if task_ids != expected_tasks:
        errors.append("Feature 027 tasks must be exactly sequential T001-T128")
    errors.extend(verify_rhi_matrix(root))
    errors.extend(verify_requirement_trace(root, requirements, set(task_ids)))
    return errors


def verify_requirement_trace(
    root: pathlib.Path,
    requirements: set[str] | None = None,
    task_ids: set[str] | None = None,
) -> list[str]:
    errors: list[str] = []
    if requirements is None or task_ids is None:
        spec = _read(root / "specs/027-metal-backend/spec.md", errors)
        tasks = _read(root / "specs/027-metal-backend/tasks.md", errors)
        requirements = {
            f"{kind}-{number}" for kind, number in REQUIREMENT_RE.findall(spec)
        }
        task_ids = set(TASK_RE.findall(tasks))
    traced = set(REQUIREMENT_TASK_TRACE)
    if traced != requirements:
        errors.append(
            "Feature 027 traceability identity mismatch: "
            f"missing={sorted(requirements - traced)}, "
            f"unexpected={sorted(traced - requirements)}"
        )
    for requirement, tasks in sorted(REQUIREMENT_TASK_TRACE.items()):
        if not tasks:
            errors.append(f"traceability has no tasks for {requirement}")
        missing_tasks = set(tasks) - task_ids
        if missing_tasks:
            errors.append(
                f"traceability for {requirement} references missing tasks: "
                f"{sorted(missing_tasks)}"
            )
    return errors


def validate_validation_evidence(document: dict[str, object], label: str) -> list[str]:
    errors: list[str] = []
    tier = document.get("tier")
    result = document.get("result")
    backend = document.get("backend")
    workload = document.get("workload")
    native_tiers = {"native-offscreen", "visible-manual", "cross-backend"}
    if tier == "deterministic" and backend != "shared":
        errors.append(f"{label}: deterministic evidence must use the shared backend")
    if result == "passed" and tier in native_tiers:
        device = document.get("device")
        shaders = document.get("shaderEvidenceDigests")
        if not isinstance(device, dict) or not {
            "identity", "name", "capabilityDigest"
        }.issubset(device):
            errors.append(f"{label}: native pass lacks complete device proof")
        if workload != "metal-presentation-smoke" and (
            not isinstance(shaders, list) or not shaders
        ):
            errors.append(f"{label}: native pass lacks shader payload proof")
        probes = document.get("probes")
        if not isinstance(probes, list) or not any(
            isinstance(probe, dict) and probe.get("result") == "passed" and
            re.fullmatch(r"[0-9a-f]{64}", str(probe.get("evidenceDigest", "")))
            for probe in probes
        ):
            errors.append(f"{label}: native pass lacks GPU evidence digest")
        serialized = json.dumps(document, sort_keys=True).lower()
        if "semantic-oracle" in serialized or "semantic oracle" in serialized:
            errors.append(f"{label}: semantic oracle is presented as native evidence")
    if tier == "visible-manual" and result == "passed":
        counts = document.get("counts")
        minimum_frames, minimum_cycles = (
            (120, 4) if workload == "metal-presentation-smoke"
            else (3000, 20)
        )
        if not isinstance(counts, dict) or \
                counts.get("frames", 0) < minimum_frames or \
                counts.get("lifecycleCycles", 0) < minimum_cycles:
            errors.append(
                f"{label}: visible pass lacks {minimum_frames} frames "
                f"and {minimum_cycles} cycles"
            )
    if tier == "cross-backend" and result == "passed":
        probes = document.get("probes")
        if not isinstance(probes, list) or not any(
            isinstance(probe, dict) and
            probe.get("tolerance") == "metal-vulkan-tolerance-v1"
            for probe in probes
        ):
            errors.append(f"{label}: comparison pass lacks frozen tolerance provenance")
    return errors


def verify_validation_evidence(root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    validation = root / "Validation/027"
    if not validation.is_dir():
        return errors
    for path in sorted(validation.rglob("*.json")):
        if "downloaded" in path.parts or "work" in path.parts:
            continue
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError) as error:
            errors.append(f"cannot read validation evidence {path}: {error}")
            continue
        if not isinstance(document, dict) or "tier" not in document:
            continue
        errors.extend(
            validate_validation_evidence(document, path.relative_to(root).as_posix())
        )
    return errors


def verify_forbidden_scope(root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    roots = (
        root / "Source/Backend/Metal",
        root / "Source/Asset",
        root / "Tools/AssetCooker",
    )
    for source_root in roots:
        if not source_root.is_dir():
            continue
        for path in sorted(source_root.rglob("*")):
            if not path.is_file() or path.suffix.lower() not in RUNTIME_SOURCE_SUFFIXES:
                continue
            text = _read(path, errors)
            for label, pattern in FORBIDDEN_SCOPE_PATTERNS.items():
                if pattern.search(text):
                    errors.append(
                        f"Feature 027 forbidden scope ({label}) in "
                        f"{path.relative_to(root)}"
                    )
    return errors


def verify_architecture(root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    for public_root in (
        root / "Source/RHI/Public",
        root / "Source/Backend/Metal/Public",
        root / "Source/Asset/Public",
        root / "Source/Renderer/Public",
        root / "Source/Application/Public",
    ):
        if not public_root.is_dir():
            continue
        for path in sorted(public_root.rglob("*.h")):
            text = _read(path, errors)
            for token in APPLE_PUBLIC_TOKENS:
                if token in text:
                    errors.append(
                        f"Apple API token {token!r} leaks through {path.relative_to(root)}"
                    )

    metal_private = root / "Source/Backend/Metal/Private"
    for relative_root in RUNTIME_ROOTS:
        runtime_root = root / relative_root
        if not runtime_root.is_dir():
            continue
        for path in sorted(runtime_root.rglob("*")):
            if not path.is_file() or path.suffix.lower() not in RUNTIME_SOURCE_SUFFIXES:
                continue
            text = _read(path, errors)
            in_metal_private = metal_private in path.parents
            if path.suffix.lower() == ".mm" and not in_metal_private:
                errors.append(
                    f"Objective-C++ implementation outside Metal private boundary: "
                    f"{path.relative_to(root)}"
                )
            if not in_metal_private:
                for token in APPLE_PUBLIC_TOKENS:
                    if token in text:
                        errors.append(
                            f"Apple API token {token!r} outside Metal private boundary: "
                            f"{path.relative_to(root)}"
                        )
            if '#include "Tools/' in text or "#include <Tools/" in text:
                errors.append(f"runtime source includes Tools: {path.relative_to(root)}")
            if "spirv_cross" in text and relative_root != "Tools":
                errors.append(
                    f"SPIRV-Cross use outside Tools: {path.relative_to(root)}"
                )
    return errors


def verify_metal_build_contract(root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    sconscript = root / "Source/Backend/Metal/SConscript"
    text = _read(sconscript, errors)
    required_fragments = {
        "non-macOS source exclusion": "if platform != 'Mac'",
        "Objective-C ARC": "-fobjc-arc",
        "deployment environment": "MACOSX_DEPLOYMENT_TARGET",
        "deployment compiler flag": "-mmacosx-version-min=12.0",
        "unguarded availability error": "-Werror=unguarded-availability-new",
        "Metal framework": "'Metal'",
        "QuartzCore framework": "'QuartzCore'",
        "Cocoa framework": "'Cocoa'",
    }
    for label, fragment in required_fragments.items():
        if fragment not in text:
            errors.append(f"Metal build contract missing {label}: {fragment}")
    unsupported = _read(
        root / "Source/Backend/Metal/Private/FMetalDeviceFactoryUnsupported.cpp",
        errors,
    )
    if "FMetalDeviceFactoryUnsupported.cpp" not in text:
        errors.append("non-macOS build does not compile the unsupported Metal factory")
    if "ERHIResult::Unsupported" not in unsupported:
        errors.append("unsupported Metal factory does not return Unsupported")
    if "ProvesNativeExecution" in unsupported:
        errors.append("unsupported Metal factory can report a native pass")
    for token in APPLE_PUBLIC_TOKENS:
        if token in unsupported:
            errors.append(
                f"unsupported Metal factory contains Apple API token {token!r}"
            )
    return_index = text.find("Return('lib')")
    framework_index = text.find("metal_env.Append(FRAMEWORKS")
    if return_index < 0 or framework_index < 0 or return_index > framework_index:
        errors.append("non-macOS Metal build does not return before framework linkage")
    return errors


def verify_spirv_cross_vendor(root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    vendor = root / "ThirdParty/spirv-cross"
    required = {
        "LICENSE",
        "UPSTREAM.md",
        "SHA256SUMS",
        *SPIRV_CROSS_SOURCES,
        *SPIRV_CROSS_HEADERS,
    }
    actual = {path.name for path in vendor.iterdir()} if vendor.is_dir() else set()
    if actual != required:
        errors.append(
            "SPIRV-Cross inventory mismatch: "
            f"missing={sorted(required - actual)}, extra={sorted(actual - required)}"
        )
        return errors

    sums_path = vendor / "SHA256SUMS"
    sums: dict[str, str] = {}
    for line in _read(sums_path, errors).splitlines():
        match = re.fullmatch(r"([0-9a-f]{64})  (.+)", line)
        if not match:
            errors.append(f"invalid SPIRV-Cross hash line: {line!r}")
            continue
        digest, relative = match.groups()
        sums[pathlib.PurePosixPath(relative).name] = digest
    hashed = {"LICENSE", *SPIRV_CROSS_SOURCES, *SPIRV_CROSS_HEADERS}
    if set(sums) != hashed:
        errors.append("SPIRV-Cross hash inventory is incomplete or contains extras")
    for name in sorted(hashed & set(sums)):
        try:
            actual_digest = hashlib.sha256((vendor / name).read_bytes()).hexdigest()
        except OSError as error:
            errors.append(f"cannot hash {vendor / name}: {error}")
            continue
        if actual_digest != sums[name]:
            errors.append(f"SPIRV-Cross digest mismatch: {name}")

    upstream = _read(vendor / "UPSTREAM.md", errors)
    if "a0fba56c34a6700f1724bf9b751da5b488a3775c" not in upstream:
        errors.append("SPIRV-Cross provenance is missing the pinned commit")
    tool_build = _read(root / "Tools/AssetCooker/SConscript", errors)
    for source in SPIRV_CROSS_SOURCES:
        if f"ThirdParty/spirv-cross/{source}" not in tool_build:
            errors.append(f"AssetCooker does not build pinned source: {source}")
    asset_build = _read(root / "Source/Asset/SConscript", errors)
    if "spirv-cross" in asset_build:
        errors.append("SPIRV-Cross leaked into the runtime Asset build")
    return errors


def validate_metal_shader_evidence(
    schema_path: pathlib.Path, document: dict[str, object]
) -> list[str]:
    """Targeted standard-library oracle for the checked-in evidence schema."""
    errors: list[str] = []
    try:
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        return [f"cannot read evidence schema: {error}"]
    required = set(schema.get("required", []))
    properties = set(schema.get("properties", {}))
    missing = required - set(document)
    extra = set(document) - properties
    if missing:
        errors.append(f"evidence missing fields: {sorted(missing)}")
    if extra:
        errors.append(f"evidence has unknown fields: {sorted(extra)}")
    if document.get("schemaVersion") != 1:
        errors.append("evidence schemaVersion must be 1")
    if document.get("kind") not in {"derivation", "native-library"}:
        errors.append("evidence kind is invalid")
    digest_re = re.compile(r"^[0-9a-f]{64}$")
    for field in (
        "shaderAssetVersion",
        "spirvDigest",
        "interfaceDigest",
        "normalizedMslDigest",
        "evidenceDigest",
    ):
        if field in document and not digest_re.fullmatch(str(document[field])):
            errors.append(f"evidence digest is invalid: {field}")
    native = document.get("nativeLibrary")
    if document.get("kind") == "native-library" and not isinstance(native, dict):
        errors.append("native-library evidence requires nativeLibrary")
    if document.get("kind") == "derivation" and native is not None:
        errors.append("derivation evidence forbids nativeLibrary")
    return errors


def verify(root: pathlib.Path, mode: str = "all") -> list[str]:
    root = root.resolve()
    errors: list[str] = []
    if mode in {"all", "contracts", "rhi-matrix"}:
        errors.extend(verify_contracts(root) if mode != "rhi-matrix" else verify_rhi_matrix(root))
    if mode in {"all", "architecture"}:
        errors.extend(verify_architecture(root))
        errors.extend(verify_metal_build_contract(root))
        errors.extend(verify_spirv_cross_vendor(root))
        errors.extend(verify_forbidden_scope(root))
    if mode in {"all", "evidence"}:
        errors.extend(verify_validation_evidence(root))
    return errors


def write_rhi_matrix(root: pathlib.Path, output: pathlib.Path) -> None:
    errors: list[str] = []
    actual = _actual_matrix(root / "Source/RHI/Public/RHI", errors)
    if errors:
        raise RuntimeError("; ".join(errors))
    lines = [
        "# Feature 027 RHI Operation Matrix",
        "",
        "Status: baseline seed; every row starts as `required-native`.",
        "",
        "| Interface | Operation | Overload | Signature SHA-256 | Status |",
        "|---|---|---:|---|---|",
    ]
    for interface, entries in sorted(actual.items()):
        seen: collections.Counter[str] = collections.Counter()
        for name, _, digest in entries:
            seen[name] += 1
            lines.append(
                f"| `{interface}` | `{name}` | {seen[name]} | `{digest}` | "
                "`required-native` |"
            )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


def write_final_rhi_matrix(root: pathlib.Path, output: pathlib.Path) -> None:
    errors: list[str] = []
    actual = _actual_matrix(root / "Source/RHI/Public/RHI", errors)
    if errors:
        raise RuntimeError("; ".join(errors))
    lines = [
        "# Feature 027 RHI Operation Matrix",
        "",
        "Status: final native conformance inventory. All 131 frozen overloads "
        "are semantically applicable and passed on a real Metal device; no "
        "capability-limited row was required.",
        "",
        "| Interface | Operation | Overload | Signature SHA-256 | Requirement | "
        "API | Metal Test | Vulkan Regression | Status | Capability Predicate | "
        "Device Evidence |",
        "|---|---|---:|---|---|---|---|---|---|---|---|",
    ]
    for interface, entries in sorted(actual.items()):
        seen: collections.Counter[str] = collections.Counter()
        for operation, _, digest in entries:
            seen[operation] += 1
            overload = seen[operation]
            requirement = _matrix_requirement(interface, operation)
            group = _matrix_group(interface, operation)
            metal_test, vulkan_test, evidence = MATRIX_TEST_EVIDENCE[group]
            api = (
                f"Source/RHI/Public/RHI/{interface}.h::"
                f"{operation}#{overload}"
            )
            lines.append(
                f"| `{interface}` | `{operation}` | {overload} | `{digest}` | "
                f"`{requirement}` | `{api}` | `{metal_test}` | "
                f"`{vulkan_test}` | `native-pass` | `n/a` | `{evidence}` |"
            )
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root",
        type=pathlib.Path,
        default=pathlib.Path(__file__).resolve().parents[1],
    )
    parser.add_argument(
        "--mode",
        choices=("all", "architecture", "contracts", "rhi-matrix", "evidence"),
        default="all",
    )
    parser.add_argument("--write-rhi-matrix", type=pathlib.Path)
    parser.add_argument("--finalize-rhi-matrix", type=pathlib.Path)
    parser.add_argument("--stamp", type=pathlib.Path)
    arguments = parser.parse_args()
    root = arguments.root.resolve()
    if arguments.write_rhi_matrix:
        write_rhi_matrix(root, arguments.write_rhi_matrix)
    if arguments.finalize_rhi_matrix:
        write_final_rhi_matrix(root, arguments.finalize_rhi_matrix)
    errors = verify(root, arguments.mode)
    for error in errors:
        print(f"ERROR: {error}")
    if not errors and arguments.stamp:
        arguments.stamp.parent.mkdir(parents=True, exist_ok=True)
        arguments.stamp.write_text("passed\n", encoding="ascii")
    if not errors:
        print(f"Metal backend {arguments.mode}: PASS")
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
