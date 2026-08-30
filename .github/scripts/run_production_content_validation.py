#!/usr/bin/env python3
"""Run Feature 028 source-to-cooked production-content validation."""

from __future__ import annotations

import argparse
import concurrent.futures
import hashlib
import importlib.util
import json
import math
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import threading
import time
from dataclasses import dataclass
from typing import Iterable, Sequence


SCRIPT_DIR = Path(__file__).parent
REPOSITORY_ROOT = SCRIPT_DIR.parents[1]
CORPUS_MANIFEST = Path("Content/ProductionAcceptance/Corpus/corpus-v1.json")
VALIDATION_PROFILES = Path("Config/Validation/ProductionContent")
SHADER_SOURCE_FILES = (
    "Composition.frag",
    "Composition.frag.spv",
    "Composition.shader.json",
    "DirectionalLight.frag",
    "DirectionalLight.frag.spv",
    "DirectionalLight.shader.json",
    "Fullscreen.vert",
    "Fullscreen.vert.spv",
    "PointLight.frag",
    "PointLight.frag.spv",
    "PointLight.shader.json",
    "PointLight.vert",
    "PointLight.vert.spv",
    "SpotLight.frag",
    "SpotLight.frag.spv",
    "SpotLight.shader.json",
    "SpotLight.vert",
    "SpotLight.vert.spv",
    "Surface.shader.json",
    "Surface.vert",
    "Surface.frag",
    "Surface.vert.spv",
    "Surface.frag.spv",
)
DEFERRED_SHADER_ROOTS = (
    "ShaderProgram:Engine/Shaders/Deferred/Surface",
    "ShaderProgram:Engine/Shaders/Deferred/DirectionalLight",
    "ShaderProgram:Engine/Shaders/Deferred/PointLight",
    "ShaderProgram:Engine/Shaders/Deferred/SpotLight",
    "ShaderProgram:Engine/Shaders/Deferred/Composition",
)
FAILURE_CATALOG = Path(
    "Tests/Fixtures/ProductionContent/Failures/failure-catalog.json"
)
WORKLOAD_REVISIONS = {
    "khronos-lantern-glb": "production-content-lantern-v2",
    "khronos-sponza-gltf": "production-content-sponza-v2",
}
FAILURE_STAGES = (
    "corpus", "import", "cook", "publication", "strict-load",
    "realization", "native", "image", "lifecycle", "timeout",
    "unsupported",
)
FAILURE_CASE_FIELDS = {
    "caseId", "stage", "expectedCategory", "reproductionProfile",
}

PROFILE_FIELDS = {
    "schema", "schemaVersion", "profileId", "corpusRevision", "packageIds",
    "targetProfiles", "lifecycleCycles", "warmupCycles", "maxRssGrowthBytes",
    "timeBudgetSeconds", "profileTimeBudgetSeconds",
    "nativeTimeBudgetSeconds", "cadence",
    "requiredGates", "authorityPolicy",
}
EXECUTION_CLASSES = (
    "github-hosted", "maintainer-local-metal",
    "maintainer-local-windows-vulkan", "local-diagnostic",
)
MEASUREMENT_DISPOSITIONS = (
    "required", "operational", "observed", "not-required",
)
PHYSICAL_PREFLIGHT_NAMES = (
    "native-registered-target",
    "registered-device-class",
    "exclusive-local-device-session",
    "clean-frozen-revision-and-software",
    "default-production-allocator",
    "declared-sample-and-presentation-protocol",
)
LOCAL_AUTHORITY_ALLOCATOR_OVERRIDES = (
    "MallocMediumZone", "MallocNanoZone", "MallocMaxMagazines",
    "MallocSpaceEfficient", "MallocXzone", "MALLOC_ARENA_MAX",
)
LOCAL_METAL_TARGET = Path(
    "Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json"
)
LOCAL_METAL_DEVICE_CLASS = "macos.apple8.metal.rgba8"
LOCAL_WINDOWS_VULKAN_TARGET = Path(
    "Config/AssetCooker/Profiles/Production/Windows-Vulkan.json"
)
LOCAL_WINDOWS_VULKAN_DEVICE_CLASS = "windows.discrete-vulkan.rgba8"
_LOCAL_HARDWARE_AUTHORITY_LOCKS: dict[str, object] = {}
PROFILE_CONTRACTS = {
    "regular": {
        "cycles": 20,
        "warmup": 2,
        "budget": 600,
        "profile_budget": 600,
        "native_budget": 600,
        "cadence": ["relevant-pull-request", "relevant-push"],
        "gates": [
            "corpus", "import", "clean-cook", "warm-cook",
            "strict-runtime", "semantic-equivalence",
            "transactional-realization", "platform-applicable-native",
            "lifecycle", "report",
        ],
    },
    "medium": {
        "cycles": 1000,
        "warmup": 20,
        "budget": 5400,
        "profile_budget": 5400,
        "native_budget": 4800,
        "cadence": [
            "weekly-default-branch", "feature-closeout", "release-closeout"
        ],
        "gates": [
            "corpus", "all-package-import", "clean-cook",
            "warm-cook-100-percent-reuse", "strict-no-source-runtime",
            "complete-semantic-equivalence", "transactional-realization",
            "lifecycle", "report",
        ],
    },
    "hardware": {
        "cycles": 1000,
        "warmup": 20,
        "budget": 3600,
        "profile_budget": 7800,
        "native_budget": 3600,
        "cadence": [
            "feature-closeout", "reference-image-change",
            "production-render-path-change",
        ],
        "gates": [
            "strict-runtime", "transactional-realization", "deferred-native",
            "forward-native-smoke", "semantic-readback",
            "accepted-image-baseline", "window-only-capture", "lifecycle",
            "report",
        ],
    },
}


def expected_authority_policy(profile_name: str) -> dict:
    hosted = {
        "rss": "observed",
        "timing": "operational",
        "image": "not-required",
    }
    local = {
        "rss": "observed",
        "timing": "operational",
        "image": "not-required",
    }
    physical_metal = {
        "rss": "required",
        "timing": "operational",
        "image": "required",
    }
    physical_windows_vulkan = {
        "rss": "observed",
        "timing": "operational",
        "image": "required",
    }
    if profile_name in ("regular", "medium"):
        return {
            "allowedExecutionClasses": [
                "github-hosted", "local-diagnostic",
            ],
            "executionClasses": {
                "github-hosted": hosted,
                "local-diagnostic": local,
            },
            "physicalPreflight": [],
        }
    if profile_name == "hardware":
        return {
            "allowedExecutionClasses": [
                "maintainer-local-metal",
                "maintainer-local-windows-vulkan",
                "local-diagnostic",
            ],
            "executionClasses": {
                "maintainer-local-metal": physical_metal,
                "maintainer-local-windows-vulkan": physical_windows_vulkan,
                "local-diagnostic": local,
            },
            "physicalPreflight": list(PHYSICAL_PREFLIGHT_NAMES),
        }
    raise ValueError("production validation profile is invalid")


def _git_local_authority_revision(repository_root: Path) -> str:
    revision = subprocess.run(
        ["git", "rev-parse", "HEAD"], cwd=repository_root,
        check=True, capture_output=True, text=True, timeout=10,
    ).stdout.strip()
    dirty = subprocess.run(
        ["git", "status", "--porcelain", "--untracked-files=all"],
        cwd=repository_root, check=True, capture_output=True, text=True,
        timeout=10,
    ).stdout.strip()
    if not re.fullmatch(r"[0-9a-f]{40}", revision) or dirty:
        raise StageFailure(
            "unsupported", "local-authority-revision-not-frozen",
            "maintainer local authority requires a clean committed HEAD",
        )
    return revision


def _acquire_local_authority_lock(
    repository_root: Path, authority_token: str,
) -> str:
    system = platform.system().lower()
    if system not in ("darwin", "windows"):
        raise StageFailure(
            "unsupported", "local-authority-lock-unavailable",
            "exclusive local authority locking requires macOS or Windows",
        )

    token = hashlib.sha256(
        str(repository_root.resolve()).encode("utf-8")
    ).hexdigest()[:16]
    lock_path = Path(tempfile.gettempdir()) / (
        f"stoner-feature-028-{authority_token}-{token}.lock"
    )
    key = str(lock_path)
    if key in _LOCAL_HARDWARE_AUTHORITY_LOCKS:
        return hashlib.sha256(key.encode("utf-8")).hexdigest()
    handle = lock_path.open("a+b")
    try:
        if system == "darwin":
            import fcntl
            fcntl.flock(handle.fileno(), fcntl.LOCK_EX | fcntl.LOCK_NB)
        else:
            import msvcrt
            if lock_path.stat().st_size == 0:
                handle.write(b"\0")
                handle.flush()
            handle.seek(0)
            msvcrt.locking(handle.fileno(), msvcrt.LK_NBLCK, 1)
    except OSError as error:
        handle.close()
        raise StageFailure(
            "unsupported", "local-authority-session-not-exclusive",
            "another Feature 028 local authority session owns the device",
        ) from error
    _LOCAL_HARDWARE_AUTHORITY_LOCKS[key] = handle
    return hashlib.sha256(key.encode("utf-8")).hexdigest()


def _validate_local_metal_preflight(
    repository_root: Path,
    target_profile: Path,
    target_contract: dict,
    environment: dict[str, str],
) -> dict:
    expected_target = (repository_root / LOCAL_METAL_TARGET).resolve()
    if (
        platform.system().lower() != "darwin"
        or platform.machine().lower() not in ("arm64", "aarch64")
        or rosetta_translated("macos")
        or target_profile.resolve() != expected_target
        or target_contract.get("platform") != "macos"
        or target_contract.get("cpuArchitecture") != "arm64"
        or target_contract.get("graphicsBackend") != "metal"
    ):
        raise StageFailure(
            "unsupported", "local-metal-host-mismatch",
            "authority requires native arm64 macOS and Mac-Metal-Arm64.json",
        )
    forbidden = [
        key for key in LOCAL_AUTHORITY_ALLOCATOR_OVERRIDES
        if environment.get(key) is not None
    ]
    if forbidden:
        raise StageFailure(
            "unsupported", "local-metal-allocator-overridden",
            "default production allocator required; remove " +
            ",".join(forbidden),
        )
    revision = _git_local_authority_revision(repository_root)
    lock_digest = _acquire_local_authority_lock(repository_root, "metal")
    evidence = {
        "authority": "maintainer-local-metal",
        "deviceClass": LOCAL_METAL_DEVICE_CLASS,
        "exclusiveRunnerDeviceDisplay": True,
        "frozenRevision": revision,
        "targetProfile": LOCAL_METAL_TARGET.as_posix(),
        "allocator": "default-production",
        "sampleProtocol": "warmup20-terminal1000",
        "presentation": "window-readback",
        "lockDigest": lock_digest,
        "passed": True,
    }
    evidence["evidenceDigest"] = hashlib.sha256(
        json.dumps(evidence, sort_keys=True, separators=(",", ":")).encode(
            "utf-8"
        )
    ).hexdigest()
    return evidence


def _validate_local_windows_vulkan_preflight(
    repository_root: Path,
    target_profile: Path,
    target_contract: dict,
    environment: dict[str, str],
) -> dict:
    expected_target = (
        repository_root / LOCAL_WINDOWS_VULKAN_TARGET
    ).resolve()
    if (
        platform.system().lower() != "windows"
        or platform.machine().lower() not in ("amd64", "x86_64")
        or target_profile.resolve() != expected_target
        or target_contract.get("platform") != "windows"
        or target_contract.get("cpuArchitecture") != "x86_64"
        or target_contract.get("graphicsBackend") != "vulkan"
    ):
        raise StageFailure(
            "unsupported", "local-windows-vulkan-host-mismatch",
            "authority requires native x86_64 Windows and Windows-Vulkan.json",
        )
    forbidden = [
        key for key in LOCAL_AUTHORITY_ALLOCATOR_OVERRIDES
        if environment.get(key) is not None
    ]
    if forbidden:
        raise StageFailure(
            "unsupported", "local-windows-vulkan-allocator-overridden",
            "default production allocator required; remove " +
            ",".join(forbidden),
        )
    revision = _git_local_authority_revision(repository_root)
    lock_digest = _acquire_local_authority_lock(
        repository_root, "windows-vulkan"
    )
    evidence = {
        "authority": "maintainer-local-windows-vulkan",
        "deviceClass": LOCAL_WINDOWS_VULKAN_DEVICE_CLASS,
        "exclusiveRunnerDeviceDisplay": True,
        "frozenRevision": revision,
        "targetProfile": LOCAL_WINDOWS_VULKAN_TARGET.as_posix(),
        "allocator": "default-production",
        "sampleProtocol": "warmup20-terminal1000",
        "presentation": "window-readback",
        "lockDigest": lock_digest,
        "passed": True,
    }
    evidence["evidenceDigest"] = hashlib.sha256(
        json.dumps(evidence, sort_keys=True, separators=(",", ":")).encode(
            "utf-8"
        )
    ).hexdigest()
    return evidence


def classify_execution_environment(
    profile_name: str,
    environment: dict[str, str] | None = None,
    *,
    local_metal_authority: bool = False,
    local_windows_vulkan_authority: bool = False,
    repository_root: Path | None = None,
    target_profile: Path | None = None,
    target_contract: dict | None = None,
) -> dict:
    environment = dict(os.environ if environment is None else environment)
    if "STONER_PRODUCTION_EXECUTION_CLASS" in environment:
        raise ValueError("caller-selected execution class is forbidden")
    policy = expected_authority_policy(profile_name)
    execution_class = "local-diagnostic"
    preflight = None
    source = "local-default"
    if local_metal_authority and local_windows_vulkan_authority:
        raise ValueError("local authority assertions are mutually exclusive")
    if local_metal_authority:
        if profile_name != "hardware" or environment.get("GITHUB_ACTIONS") == "true":
            raise StageFailure(
                "unsupported", "local-metal-authority-scope-mismatch",
                "local Metal authority is limited to a non-GitHub hardware run",
            )
        if repository_root is None or target_profile is None or target_contract is None:
            raise ValueError(
                "local Metal authority requires repository and target evidence"
            )
        preflight = _validate_local_metal_preflight(
            repository_root, target_profile, target_contract, environment
        )
        execution_class = "maintainer-local-metal"
        source = "explicit-maintainer-local"
    if local_windows_vulkan_authority:
        if profile_name != "hardware" or environment.get("GITHUB_ACTIONS") == "true":
            raise StageFailure(
                "unsupported", "local-windows-vulkan-authority-scope-mismatch",
                "local Windows Vulkan authority is limited to a non-GitHub hardware run",
            )
        if repository_root is None or target_profile is None or target_contract is None:
            raise ValueError(
                "local Windows Vulkan authority requires repository and target evidence"
            )
        preflight = _validate_local_windows_vulkan_preflight(
            repository_root, target_profile, target_contract, environment
        )
        execution_class = "maintainer-local-windows-vulkan"
        source = "explicit-maintainer-local"
    if environment.get("GITHUB_ACTIONS") == "true":
        workflow = environment.get("GITHUB_WORKFLOW_REF", "")
        runner_environment = environment.get("RUNNER_ENVIRONMENT")
        if (
            runner_environment == "github-hosted"
            and profile_name in ("regular", "medium")
            and "/.github/workflows/feature-028-production-content.yml@"
                in workflow
        ):
            execution_class = "github-hosted"
        else:
            raise StageFailure(
                "unsupported", "workflow-authority-mismatch",
                "GitHub workflow cannot own the requested execution class",
            )
    if execution_class not in policy["allowedExecutionClasses"]:
        raise StageFailure(
            "unsupported", "execution-class-not-allowed",
            f"{execution_class} is not allowed by {profile_name}",
        )
    dispositions = policy["executionClasses"][execution_class]
    if any(value not in MEASUREMENT_DISPOSITIONS for value in dispositions.values()):
        raise ValueError("measurement disposition is invalid")
    return {
        "executionClass": execution_class,
        "source": "workflow" if execution_class == "github-hosted" else source,
        "dispositions": dict(dispositions),
        "preflight": preflight,
    }


def profile_package_concurrency(profile: str, package_count: int) -> int:
    if profile not in PROFILE_CONTRACTS:
        raise ValueError("production validation profile is invalid")
    if package_count < 1:
        raise ValueError("production validation package count is invalid")
    return min(2, package_count) if profile == "medium" else 1


def profile_package_clean_runs(
    profile: str, package_tier: str, requested_runs: int
) -> int:
    if profile not in PROFILE_CONTRACTS:
        raise ValueError("production validation profile is invalid")
    if package_tier not in ("regular", "medium"):
        raise ValueError("production validation package tier is invalid")
    if requested_runs < 1 or requested_runs > 20:
        raise ValueError("production validation clean run count is invalid")
    # Per-target clean determinism belongs to the regular gate. Medium and
    # hardware exercise one clean plus one 100%-reuse warm cook for every
    # selected package and must not duplicate the regular gate's 20 clean
    # cooks inside their lifecycle time budget.
    return requested_runs if profile == "regular" else 1


def select_profile_packages(
    profile: str, packages: list[dict], package_id: str | None
) -> list[dict]:
    if package_id is None:
        return packages
    if profile != "medium":
        raise ValueError("package sharding is supported only by medium")
    selected = [
        package for package in packages
        if package["packageId"] == package_id
    ]
    if len(selected) != 1:
        raise ValueError("medium shard package is not declared by the profile")
    return selected


def run_with_optional_lock(action, lock):
    if lock is None:
        return action()
    with lock:
        return action()


LIFECYCLE_EVIDENCE_PREFIX = "[EVIDENCE] "
LIFECYCLE_EVIDENCE_FIELDS = {
    "backend", "cycles", "warmup-cycle", "warmup-rss", "terminal-rss",
    "peak-rss", "growth", "captures", "readbacks", "counters", "stale",
}
IMAGE_EVIDENCE_PREFIX = "[IMAGE] "
IMAGE_EVIDENCE_FIELDS = {
    "backend", "device-class", "baseline", "matched-reference",
    "frame-token", "semantic-probes", "probe-ids", "mean", "p95",
    "maximum", "bad-fraction", "result",
}
IMAGE_REFERENCE_EVIDENCE_PREFIX = "[IMAGE-REFERENCE] "
IMAGE_REFERENCE_EVIDENCE_FIELDS = {
    "reference", "mean", "p95", "maximum", "bad-fraction", "result",
}
FRAME_FINGERPRINT_PREFIX = "[FRAME-FINGERPRINT] "
FRAME_FINGERPRINT_FIELDS = {
    "frame-token", "snapshot", "uniform", "shader", "pipeline",
    "descriptor", "device",
}
FRAME_BUNDLE_PREFIX = "[FRAME-BUNDLE] "
FRAME_BUNDLE_ATTACHMENTS = {
    "BaseColorAO", "Depth", "EmissiveMetallic", "FinalOutput",
    "LightingAccumulation", "NormalRoughness",
}
EXPECTED_SEMANTIC_PROBE_IDS = [
    "color-image", "current-frame", "nonblank", "coverage",
    "region-background", "region-orientation",
    "region-primitive-material", "region-base-color",
    "region-normal-response", "region-metallic-roughness",
    "region-emissive", "normal-semantic", "depth-semantic",
    "base-color-attachment", "normal-attachment-direction",
    "emissive-metallic-attachment", "depth-attachment",
    "lighting-attachment", "final-output-readback",
    "presented-window-capture",
]


def load_local_module(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def remove_validation_tree(path: Path, ignore_errors: bool = False) -> None:
    resolved = path.resolve()
    native_path = (
        Path("\\\\?\\" + str(resolved))
        if platform.system().lower() == "windows" else resolved
    )
    shutil.rmtree(native_path, ignore_errors=ignore_errors)


@dataclass(frozen=True)
class CommandResult:
    seconds: float
    stdout: str
    stderr: str


class CommandFailure(RuntimeError):
    def __init__(
        self,
        command: Sequence[str],
        returncode: int,
        result: CommandResult,
    ):
        super().__init__(
            f"command failed ({returncode}): {' '.join(command)}\n"
            f"stdout:\n{result.stdout}\nstderr:\n{result.stderr}"
        )
        self.returncode = returncode
        self.result = result


class StageFailure(RuntimeError):
    def __init__(
        self,
        stage: str,
        category: str,
        detail: str,
        result: CommandResult | None = None,
        returncode: int | None = None,
    ):
        super().__init__(f"{stage}: {category}: {detail}")
        self.stage = stage
        self.category = category
        self.detail = detail
        self.result = result
        self.returncode = returncode


def wait_for_optional_barrier(barrier, timeout: int) -> None:
    if barrier is None:
        return
    try:
        barrier.wait(timeout=timeout)
    except threading.BrokenBarrierError as error:
        raise StageFailure(
            "native", "barrier-broken",
            "a medium package failed before isolated native execution",
        ) from error


def remaining_stage_timeout(deadline: float, configured_timeout: int) -> int:
    remaining = deadline - time.monotonic()
    if remaining <= 0:
        raise StageFailure(
            "profile", "timeout", "profile time budget is exhausted"
        )
    return min(configured_timeout, max(1, math.ceil(remaining)))


def bounded_package_deadline(
    profile_deadline: float,
    package_budget_seconds: int,
    now: float | None = None,
) -> float:
    started = time.monotonic() if now is None else now
    return min(profile_deadline, started + package_budget_seconds)


def native_stage_timeout(
    deadline: float, configured_timeout: int, native_budget: int
) -> int:
    return min(
        native_budget,
        remaining_stage_timeout(deadline, configured_timeout),
    )


def equivalence_request_timeout_seconds(stage_timeout: int) -> int:
    """Reserve process-exit slack while bounding one extension execution."""
    return max(30, min(300, stage_timeout - 5))


def run_command(
    command: Sequence[str],
    root: Path,
    timeout: int,
    environment: dict[str, str] | None = None,
) -> CommandResult:
    started = time.monotonic()
    process_environment = os.environ.copy()
    if environment:
        process_environment.update(environment)
    result = subprocess.run(
        list(command),
        cwd=root,
        env=process_environment,
        check=False,
        capture_output=True,
        text=True,
        timeout=timeout,
    )
    elapsed = time.monotonic() - started
    command_result = CommandResult(elapsed, result.stdout, result.stderr)
    if result.returncode != 0:
        raise CommandFailure(command, result.returncode, command_result)
    return command_result


def run_stage(
    stage: str,
    command: Sequence[str],
    root: Path,
    timeout: int,
    environment: dict[str, str] | None = None,
) -> CommandResult:
    print(
        f"[production-validation] stage={stage} status=started",
        file=sys.stderr,
        flush=True,
    )
    try:
        result = run_command(command, root, timeout, environment)
        print(
            f"[production-validation] stage={stage} status=passed",
            file=sys.stderr,
            flush=True,
        )
        return result
    except subprocess.TimeoutExpired as error:
        def bounded_partial_output(value: object) -> str:
            if value is None:
                return ""
            if isinstance(value, bytes):
                text = value.decode("utf-8", errors="replace")
            else:
                text = str(value)
            return text[-64 * 1024:]

        partial_stdout = bounded_partial_output(error.stdout)
        partial_stderr = bounded_partial_output(error.stderr)
        print(
            f"[production-validation] stage={stage} status=timeout",
            file=sys.stderr,
            flush=True,
        )
        detail = f"exceeded {timeout} seconds"
        if partial_stdout:
            detail += f"\npartial stdout:\n{partial_stdout}"
        if partial_stderr:
            detail += f"\npartial stderr:\n{partial_stderr}"
        raise StageFailure(stage, "timeout", detail) from error
    except CommandFailure as error:
        print(
            f"[production-validation] stage={stage} status=failed",
            file=sys.stderr,
            flush=True,
        )
        raise StageFailure(
            stage, "command-failed", str(error), error.result,
            error.returncode,
        ) from error
    except (OSError, RuntimeError) as error:
        print(
            f"[production-validation] stage={stage} status=failed",
            file=sys.stderr,
            flush=True,
        )
        raise StageFailure(stage, "command-failed", str(error)) from error


def canonical_json(value: object) -> str:
    return json.dumps(
        value, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    )


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def extended_length_path(path: Path) -> Path:
    resolved = path.resolve()
    if os.name != "nt":
        return resolved
    value = str(resolved)
    if value.startswith("\\\\?\\"):
        return resolved
    if value.startswith("\\\\"):
        return Path("\\\\?\\UNC\\" + value[2:])
    return Path("\\\\?\\" + value)


def iter_regular_files(root: Path) -> Iterable[Path]:
    resolved_root = root.resolve()
    walk_root = extended_length_path(resolved_root)
    discovered = []
    for directory, directory_names, file_names in os.walk(
        walk_root, followlinks=False
    ):
        directory_names.sort()
        relative_directory = Path(directory).relative_to(walk_root)
        for file_name in sorted(file_names):
            discovered.append(
                resolved_root / relative_directory / file_name
            )
    return iter(sorted(
        discovered,
        key=lambda item: item.relative_to(resolved_root).as_posix(),
    ))


def read_file_bytes(path: Path) -> bytes:
    return extended_length_path(path).read_bytes()


def tree_digest(root: Path) -> str:
    records = []
    resolved_root = root.resolve()
    for path in iter_regular_files(resolved_root):
        relative = path.relative_to(resolved_root).as_posix()
        payload = read_file_bytes(path)
        records.append((relative, len(payload), sha256_bytes(payload)))
    return sha256_bytes(canonical_json(records).encode("utf-8"))


def artifact_record(path: Path, root: Path) -> dict:
    resolved_path = extended_length_path(path).resolve()
    resolved_root = extended_length_path(root).resolve()
    try:
        token = resolved_path.relative_to(resolved_root).as_posix()
    except ValueError as error:
        raise ValueError("artifact is outside its declared root") from error
    payload = resolved_path.read_bytes()
    return {
        "path": token,
        "sha256": sha256_bytes(payload),
        "sizeBytes": len(payload),
    }


def revalidate_artifact(record: dict, root: Path) -> None:
    if set(record) != {"path", "sha256", "sizeBytes"}:
        raise ValueError("artifact record fields are invalid")
    token = record["path"]
    if not isinstance(token, str) or not token or "\\" in token:
        raise ValueError("artifact path token is invalid")
    resolved_root = extended_length_path(root).resolve()
    path = (resolved_root / token).resolve()
    try:
        path.relative_to(resolved_root)
    except ValueError as error:
        raise ValueError("artifact path escapes its root") from error
    if not path.is_file():
        raise ValueError("artifact is missing")
    payload = path.read_bytes()
    if len(payload) != record["sizeBytes"]:
        raise ValueError("artifact size differs")
    if sha256_bytes(payload) != record["sha256"]:
        raise ValueError("artifact digest differs")


def write_artifact_manifest(output: Path) -> Path:
    manifest_path = output / "artifact-manifest.json"
    records = [
        artifact_record(path, output)
        for path in iter_regular_files(output)
        if path != manifest_path.resolve()
    ]
    manifest = {
        "schema": "stoner.production-validation-artifacts",
        "schemaVersion": 1,
        "artifacts": records,
    }
    manifest_path.write_text(
        json.dumps(manifest, sort_keys=True, separators=(",", ":")) + "\n",
        encoding="utf-8",
    )
    return manifest_path


def _safe_package_token(value: object) -> str:
    if (
        not isinstance(value, str) or not value
        or not re.fullmatch(r"[a-z0-9]+(?:-[a-z0-9]+)*", value)
    ):
        raise ValueError("validation package identity is invalid")
    return value


def package_workload_revision(package: dict) -> str:
    package_id = _safe_package_token(package.get("packageId"))
    revision = WORKLOAD_REVISIONS.get(package_id)
    if revision is None:
        raise ValueError("production workload revision is not declared")
    return revision


def verify_validation_output(output: Path, target_profile: Path) -> dict:
    output = output.resolve()
    artifact_manifest_path = output / "artifact-manifest.json"
    if not artifact_manifest_path.is_file():
        raise ValueError("artifact manifest is missing")
    manifest = json.loads(artifact_manifest_path.read_text(encoding="utf-8"))
    if (
        not isinstance(manifest, dict)
        or set(manifest) != {"schema", "schemaVersion", "artifacts"}
        or manifest.get("schema") != "stoner.production-validation-artifacts"
        or manifest.get("schemaVersion") != 1
        or not isinstance(manifest.get("artifacts"), list)
    ):
        raise ValueError("artifact manifest contract is invalid")
    paths = [record.get("path") for record in manifest["artifacts"]]
    if paths != sorted(paths) or len(paths) != len(set(paths)):
        raise ValueError("artifact manifest ordering or uniqueness is invalid")
    for record in manifest["artifacts"]:
        revalidate_artifact(record, output)
    expected = {
        path.relative_to(output).as_posix()
        for path in iter_regular_files(output)
        if path != artifact_manifest_path
    }
    if set(paths) != expected:
        raise ValueError("artifact manifest inventory differs")
    summary_path = output / "summary.json"
    if not summary_path.is_file():
        raise ValueError("validation summary is missing")
    summary = json.loads(summary_path.read_text(encoding="utf-8"))
    if summary.get("passed") is not True:
        raise ValueError("validation summary did not pass")
    target_profile = target_profile.resolve()
    if not target_profile.is_file():
        raise ValueError("target profile is missing during artifact verification")
    target_profile_digest = sha256_bytes(target_profile.read_bytes())
    if summary.get("targetProfileDigest") != target_profile_digest:
        raise ValueError("target profile digest differs")
    packages = summary.get("packages")
    if not isinstance(packages, list) or not packages:
        raise ValueError("validation summary package evidence is missing")
    for package in packages:
        if not isinstance(package, dict):
            raise ValueError("validation package evidence is invalid")
        native_lifecycle = package.get("nativeLifecycle")
        if native_lifecycle is not None and not isinstance(native_lifecycle, dict):
            raise ValueError("validation native lifecycle evidence is invalid")
        if (
            isinstance(native_lifecycle, dict)
            and native_lifecycle.get("result") != "Passed"
        ):
            target_contract = load_native_target_contract(target_profile)
            validate_native_deferral(
                summary.get("profile"), target_contract, True
            )
            if native_lifecycle != deferred_native_result(target_contract):
                raise ValueError("validation native deferral evidence differs")
        package_id = _safe_package_token(package.get("packageId"))
        generation_id = package.get("generationId")
        if not isinstance(generation_id, str) or not re.fullmatch(
            r"[0-9a-f]{64}", generation_id
        ):
            raise ValueError("validation generation identity is invalid")
        if (
            not isinstance(package.get("currentPointerDigest"), str)
            or not re.fullmatch(
                r"[0-9a-f]{64}", package["currentPointerDigest"]
            )
            or not isinstance(package.get("generationManifestDigest"), str)
            or not re.fullmatch(
                r"[0-9a-f]{64}", package["generationManifestDigest"]
            )
        ):
            raise ValueError("published generation digest evidence is invalid")
    return {
        "result": "Passed",
        "passed": True,
        "artifactCount": len(paths),
        "manifestSha256": sha256_bytes(artifact_manifest_path.read_bytes()),
        "targetProfileDigest": target_profile_digest,
    }


def unsupported_result(stage: str, prerequisite: str, replacement_lane: str) -> dict:
    return {
        "result": "Unsupported",
        "firstFailure": {
            "stage": stage,
            "category": "unsupported-prerequisite",
            "missingPrerequisite": prerequisite,
            "replacementLane": replacement_lane,
        },
    }


def aggregate_results(results: Sequence[dict]) -> bool:
    return bool(results) and all(item.get("result") == "Passed" for item in results)


def deferred_native_result(contract: dict) -> dict:
    return {
        "result": "NotRun",
        "reason": "deferred-to-required-hardware",
        "missingPrerequisite": (
            f"physical {contract['platform']} {contract['cpuArchitecture']} "
            f"{contract['graphicsBackend']} device"
        ),
        "replacementLane": "-".join((
            contract["platform"], contract["graphicsBackend"],
            contract["cpuArchitecture"], "hardware",
        )),
        "targetProfileDigest": contract["targetProfileDigest"],
    }


def aggregate_native_results(
    results: Sequence[dict],
    allow_deferred_to_hardware: bool,
) -> bool:
    if aggregate_results(results):
        return True
    if not allow_deferred_to_hardware or not results:
        return False
    return all(
        item.get("result") == "NotRun"
        and item.get("reason") == "deferred-to-required-hardware"
        and item.get("replacementLane") == "windows-vulkan-x86_64-hardware"
        and isinstance(item.get("targetProfileDigest"), str)
        and re.fullmatch(r"[0-9a-f]{64}", item["targetProfileDigest"])
        for item in results
    )


def run_serial_packages_collect_all(
    packages: Sequence[dict],
    run_selected_package,
    after_each=None,
) -> list[dict]:
    reports = []
    for package in packages:
        report = run_selected_package(package)
        reports.append(report)
        if after_each is not None:
            after_each(report)
    return reports


def revalidate_local_authority_session(
    repository_root: Path,
    authority: dict,
    package_report: dict | None = None,
) -> None:
    if not authority.get("executionClass", "").startswith(
        "maintainer-local-"
    ):
        return
    preflight = authority.get("preflight")
    if not isinstance(preflight, dict):
        raise RuntimeError("local authority preflight evidence is missing")
    if _git_local_authority_revision(repository_root) != preflight.get(
        "frozenRevision"
    ):
        raise RuntimeError("maintainer-local authority revision changed")
    expected_lock = preflight.get("lockDigest")
    live_locks = {
        hashlib.sha256(key.encode("utf-8")).hexdigest()
        for key, handle in _LOCAL_HARDWARE_AUTHORITY_LOCKS.items()
        if not handle.closed
    }
    if expected_lock not in live_locks:
        raise RuntimeError("maintainer-local authority lock was lost")
    if package_report is None:
        return
    native = package_report.get("nativeLifecycle")
    image = native.get("imageAcceptance") if isinstance(native, dict) else None
    if (
        isinstance(image, dict)
        and image.get("deviceClass") != preflight.get("deviceClass")
    ):
        raise RuntimeError("maintainer-local authority device class changed")


def validate_native_deferral(
    profile_name: str,
    contract: dict,
    defer_native_to_hardware: bool,
) -> None:
    if not defer_native_to_hardware:
        return
    if (
        profile_name != "regular"
        or contract["platform"] != "windows"
        or contract["cpuArchitecture"] != "x86_64"
        or contract["graphicsBackend"] != "vulkan"
    ):
        raise ValueError(
            "native deferral is restricted to hosted Windows regular Vulkan"
        )


def load_failure_catalog(repository_root: Path) -> list[dict]:
    value = json.loads(
        (repository_root / FAILURE_CATALOG).read_text(encoding="utf-8")
    )
    if (
        not isinstance(value, dict)
        or set(value) != {"schema", "schemaVersion", "cases"}
        or value.get("schema") != "stoner.production-content.failure-catalog"
        or value.get("schemaVersion") != 1
        or not isinstance(value.get("cases"), list)
        or len(value["cases"]) < 30
    ):
        raise ValueError("production failure catalog contract is invalid")
    stage_order = {stage: index for index, stage in enumerate(FAILURE_STAGES)}
    identifiers: set[str] = set()
    previous: tuple[int, str] | None = None
    for case in value["cases"]:
        if not isinstance(case, dict) or set(case) != FAILURE_CASE_FIELDS:
            raise ValueError("production failure case fields are invalid")
        if case["stage"] not in stage_order:
            raise ValueError("production failure case stage is invalid")
        for field in FAILURE_CASE_FIELDS:
            token = case[field]
            if (
                not isinstance(token, str)
                or not token
                or len(token) > 128
                or any(character not in
                    "abcdefghijklmnopqrstuvwxyz0123456789.-"
                    for character in token)
            ):
                raise ValueError("production failure case token is invalid")
        if case["caseId"] in identifiers:
            raise ValueError("production failure case identity is duplicated")
        identifiers.add(case["caseId"])
        key = (stage_order[case["stage"]], case["caseId"])
        if previous is not None and key <= previous:
            raise ValueError("production failure catalog ordering is invalid")
        previous = key
    return value["cases"]


def failure_from_catalog_case(case: dict) -> dict:
    if not isinstance(case, dict) or set(case) != FAILURE_CASE_FIELDS:
        raise ValueError("production failure case fields are invalid")
    failure = {
        "stage": case["stage"],
        "category": case["expectedCategory"],
        "subject": case["caseId"],
        "expected": "rejected-at-declared-stage",
        "observed": "targeted-negative-case",
        "reproductionProfile": case["reproductionProfile"],
    }
    if case["stage"] == "unsupported":
        failure.update({
            "missingPrerequisite": case["expectedCategory"],
            "replacementLane": case["reproductionProfile"],
        })
    return failure


def _require_string_list(value: object, field: str) -> list[str]:
    if (
        not isinstance(value, list) or not value
        or any(not isinstance(item, str) or not item for item in value)
        or len(set(value)) != len(value)
    ):
        raise ValueError(f"validation profile {field} must be a non-empty unique string list")
    return value


def load_validation_profile(repository_root: Path, profile_name: str) -> dict:
    if profile_name not in PROFILE_CONTRACTS:
        raise ValueError("unknown validation profile")
    path = repository_root / VALIDATION_PROFILES / f"{profile_name.title()}.json"
    profile = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(profile, dict) or set(profile) != PROFILE_FIELDS:
        raise ValueError("validation profile fields are invalid")
    if profile.get("schema") != "stoner.production-validation-profile" or profile.get("schemaVersion") != 3:
        raise ValueError("validation profile schema is invalid")
    if profile.get("profileId") != profile_name:
        raise ValueError("validation profile ID mismatch")
    if not isinstance(profile.get("corpusRevision"), str) or not profile["corpusRevision"]:
        raise ValueError("validation profile corpus revision is invalid")
    _require_string_list(profile.get("packageIds"), "packageIds")
    target_profiles = _require_string_list(profile.get("targetProfiles"), "targetProfiles")
    if any(Path(item).is_absolute() or "\\" in item or not item.endswith(".json") for item in target_profiles):
        raise ValueError("validation profile target path is invalid")
    contract = PROFILE_CONTRACTS[profile_name]
    if profile.get("lifecycleCycles") != contract["cycles"]:
        raise ValueError("validation profile cycle count is invalid")
    if profile.get("warmupCycles") != contract["warmup"]:
        raise ValueError("validation profile warm-up boundary is invalid")
    if profile.get("maxRssGrowthBytes") != 16 * 1024 * 1024:
        raise ValueError("validation profile RSS limit is invalid")
    if profile.get("timeBudgetSeconds") != contract["budget"]:
        raise ValueError("validation profile time budget is invalid")
    if profile.get("profileTimeBudgetSeconds") != contract["profile_budget"]:
        raise ValueError("validation profile aggregate time budget is invalid")
    if profile.get("nativeTimeBudgetSeconds") != contract["native_budget"]:
        raise ValueError("validation profile native time budget is invalid")
    if profile.get("cadence") != contract["cadence"]:
        raise ValueError("validation profile cadence is invalid")
    if profile.get("requiredGates") != contract["gates"]:
        raise ValueError("validation profile required gates are invalid")
    if profile.get("authorityPolicy") != expected_authority_policy(
        profile_name
    ):
        raise ValueError("validation profile authority policy is invalid")
    return profile


def load_native_target_contract(path: Path) -> dict:
    payload = path.read_bytes()
    value = json.loads(payload.decode("utf-8"))
    if (
        not isinstance(value, dict)
        or value.get("schema") != "stoner.asset-target-profile"
        or value.get("schemaVersion") not in (1, 2)
        or value.get("platform") not in ("windows", "linux", "macos")
        or value.get("cpuArchitecture") not in ("x86_64", "arm64")
        or value.get("graphicsBackend") not in ("vulkan", "metal")
    ):
        raise ValueError("native target profile contract is invalid")
    if value["graphicsBackend"] == "metal" and value["platform"] != "macos":
        raise ValueError("Metal target profile must use macOS")
    return {
        "platform": value["platform"],
        "cpuArchitecture": value["cpuArchitecture"],
        "graphicsBackend": value["graphicsBackend"],
        "targetProfileDigest": sha256_bytes(payload),
    }


def build_native_lifecycle_stage(
    tests: Path,
    backend: str,
    publication: Path,
    lease_root: Path,
    generation: str,
    target_profile: Path,
    production_root: str,
    workload_revision: str,
    cycles: int,
    warmup_cycles: int,
    require_visible: bool = False,
    require_image_acceptance: bool = False,
) -> tuple[list[str], dict[str, str]]:
    if backend not in ("vulkan", "metal"):
        raise ValueError("native lifecycle backend is invalid")
    if (cycles, warmup_cycles) not in ((20, 2), (1000, 20)):
        raise ValueError("native lifecycle boundary is invalid")
    if require_image_acceptance and not require_visible:
        raise ValueError("image acceptance requires visible presentation")
    prefix = backend.upper()
    suite = f"production-content-{backend}-native"
    environment = {
        f"STONER_REQUIRE_{prefix}_PRODUCTION": "1",
        f"STONER_PRODUCTION_{prefix}_PUBLICATION_ROOT": str(publication),
        f"STONER_PRODUCTION_{prefix}_LEASE_ROOT": str(lease_root),
        f"STONER_PRODUCTION_{prefix}_GENERATION": generation,
        f"STONER_PRODUCTION_{prefix}_TARGET_PROFILE": str(target_profile),
        "STONER_PRODUCTION_ROOT": production_root,
        "STONER_PRODUCTION_WORKLOAD_REVISION": workload_revision,
        "STONER_PRODUCTION_LIFECYCLE_CYCLES": str(cycles),
        "STONER_PRODUCTION_WARMUP_CYCLES": str(warmup_cycles),
    }
    if require_visible:
        environment["STONER_PRODUCTION_VISIBLE"] = "1"
    if require_image_acceptance:
        environment["STONER_REQUIRE_PRODUCTION_IMAGE_ACCEPTANCE"] = "1"
    return [str(tests), "--suite", suite], environment


def native_allocator_authority_environment(
    contract: dict,
    cycles: int,
    warmup_cycles: int,
    require_visible: bool,
) -> dict[str, str]:
    # Acceptance must exercise the production allocator. Hosted RSS is an
    # observation. Maintainer-local Metal owns calibrated RSS/image gates;
    # Windows owns the image gate and records working-set RSS as observed.
    # No class may rewrite allocator topology to manufacture a result.
    return {}


def parse_native_lifecycle_evidence(
    output: str,
    expected_cycles: int,
    expected_warmup: int,
    rss_disposition: str = "observed",
) -> dict:
    if rss_disposition not in ("required", "observed"):
        raise ValueError("native lifecycle RSS disposition is invalid")
    matching = [
        line[len(LIFECYCLE_EVIDENCE_PREFIX):]
        for line in output.splitlines()
        if line.startswith(LIFECYCLE_EVIDENCE_PREFIX)
    ]
    if len(matching) != 1:
        raise ValueError("native lifecycle evidence count is invalid")
    fields: dict[str, str] = {}
    for token in matching[0].split():
        if "=" not in token:
            raise ValueError("native lifecycle evidence token is invalid")
        key, value = token.split("=", 1)
        if key in fields:
            raise ValueError("native lifecycle evidence field is duplicated")
        fields[key] = value
    if set(fields) != LIFECYCLE_EVIDENCE_FIELDS:
        raise ValueError("native lifecycle evidence fields are invalid")
    if fields["backend"] not in ("vulkan", "metal"):
        raise ValueError("native lifecycle backend is invalid")
    numeric = {}
    for key in LIFECYCLE_EVIDENCE_FIELDS - {"backend"}:
        if not re.fullmatch(r"0|[1-9][0-9]*", fields[key]):
            raise ValueError("native lifecycle numeric evidence is invalid")
        numeric[key] = int(fields[key])
    if (
        numeric["cycles"] != expected_cycles
        or numeric["warmup-cycle"] != expected_warmup
        or numeric["warmup-rss"] <= 0
        or numeric["terminal-rss"] <= 0
        or numeric["peak-rss"] < max(
            numeric["warmup-rss"], numeric["terminal-rss"]
        )
        or numeric["captures"] != expected_cycles * 2
        or numeric["readbacks"] != 7
        or numeric["counters"] != 0
        or numeric["stale"] != 1
    ):
        raise ValueError("native lifecycle evidence failed its exact contract")
    rss_within_limit = numeric["growth"] <= 16 * 1024 * 1024
    if rss_disposition == "required" and not rss_within_limit:
        raise ValueError("native lifecycle required RSS gate failed")
    return {
        "backend": fields["backend"],
        "lifecycleCycles": numeric["cycles"],
        "warmupCycles": numeric["warmup-cycle"],
        "warmupRssBytes": numeric["warmup-rss"],
        "terminalRssBytes": numeric["terminal-rss"],
        "peakRssBytes": numeric["peak-rss"],
        "rssGrowthBytes": numeric["growth"],
        "rssWithinLimit": rss_within_limit,
        "rssDisposition": rss_disposition,
        "captureCount": numeric["captures"],
        "readbackCount": numeric["readbacks"],
        "ownersAtTerminal": numeric["counters"],
        "staleHandleRejected": True,
    }


def parse_native_image_fields(output: str) -> dict[str, str]:
    matching = [
        line[len(IMAGE_EVIDENCE_PREFIX):]
        for line in output.splitlines()
        if line.startswith(IMAGE_EVIDENCE_PREFIX)
    ]
    if len(matching) != 1:
        raise ValueError("native image evidence count is invalid")
    fields: dict[str, str] = {}
    for token in matching[0].split():
        if "=" not in token:
            raise ValueError("native image evidence token is invalid")
        key, value = token.split("=", 1)
        if key in fields:
            raise ValueError("native image evidence field is duplicated")
        fields[key] = value
    if set(fields) != IMAGE_EVIDENCE_FIELDS:
        raise ValueError("native image evidence fields are invalid")
    return fields


def _parse_image_metrics(fields: dict[str, str]) -> dict[str, float]:
    observations = {}
    for key in ("mean", "p95", "maximum", "bad-fraction"):
        try:
            value = float(fields[key])
        except ValueError as error:
            raise ValueError("native image metric is invalid") from error
        if not math.isfinite(value) or value < 0.0 or value > 1.0:
            raise ValueError("native image metric is invalid")
        observations[key] = value
    return observations


def parse_native_reference_evidence(output: str) -> list[dict]:
    comparisons = []
    for line in output.splitlines():
        if not line.startswith(IMAGE_REFERENCE_EVIDENCE_PREFIX):
            continue
        fields = {}
        for token in line[len(IMAGE_REFERENCE_EVIDENCE_PREFIX):].split():
            if "=" not in token:
                raise ValueError("native reference evidence token is invalid")
            key, value = token.split("=", 1)
            if key in fields:
                raise ValueError("native reference evidence field is duplicated")
            fields[key] = value
        if (
            set(fields) != IMAGE_REFERENCE_EVIDENCE_FIELDS
            or not re.fullmatch(r"[a-z0-9][a-z0-9.-]{0,127}",
                                fields["reference"])
            or fields["result"] not in ("passed", "failed")
        ):
            raise ValueError("native reference evidence is invalid")
        metrics = _parse_image_metrics(fields)
        comparisons.append({
            "referenceId": fields["reference"],
            "mean": metrics["mean"],
            "p95": metrics["p95"],
            "maximum": metrics["maximum"],
            "badPixelFraction": metrics["bad-fraction"],
            "passed": fields["result"] == "passed",
        })
    if len(comparisons) > 3:
        raise ValueError("native reference evidence exceeds its bound")
    identifiers = [item["referenceId"] for item in comparisons]
    if identifiers != sorted(set(identifiers)):
        raise ValueError("native reference evidence order is invalid")
    return comparisons


def parse_frame_fingerprint(output: str, expected_backend: str) -> dict:
    matching = [
        line[len(FRAME_FINGERPRINT_PREFIX):]
        for line in output.splitlines()
        if line.startswith(FRAME_FINGERPRINT_PREFIX)
    ]
    if len(matching) != 1:
        raise ValueError("authoritative frame fingerprint count is invalid")
    fields = {}
    for token in matching[0].split():
        if "=" not in token:
            raise ValueError("authoritative frame fingerprint token is invalid")
        key, value = token.split("=", 1)
        if key in fields:
            raise ValueError("authoritative frame fingerprint is duplicated")
        fields[key] = value
    if (
        set(fields) != FRAME_FINGERPRINT_FIELDS
        or not re.fullmatch(r"[1-9][0-9]*", fields["frame-token"])
        or any(not re.fullmatch(r"[0-9a-f]{64}", fields[field])
               for field in FRAME_FINGERPRINT_FIELDS - {"frame-token"})
    ):
        raise ValueError("authoritative frame fingerprint is invalid")
    bundles = [
        line[len(FRAME_BUNDLE_PREFIX):]
        for line in output.splitlines()
        if line.startswith(FRAME_BUNDLE_PREFIX)
    ]
    if len(bundles) != 1:
        raise ValueError("authoritative frame bundle fingerprint count is invalid")
    try:
        bundle = json.loads(bundles[0])
    except json.JSONDecodeError as error:
        raise ValueError("authoritative frame bundle fingerprint is invalid") from error
    cpu_fields = {
        "snapshot", "uniform", "shader", "pipeline", "descriptor", "device",
    }
    if (
        not isinstance(bundle, dict)
        or set(bundle) != {
            "attachments", "cpu", "frameToken", "vulkanDriver",
            "windowCapture",
        }
        or bundle.get("frameToken") != int(fields["frame-token"])
        or not isinstance(bundle.get("cpu"), dict)
        or set(bundle["cpu"]) != cpu_fields
        or any(bundle["cpu"].get(field) != fields[field]
               for field in cpu_fields)
        or not isinstance(bundle.get("attachments"), dict)
        or set(bundle["attachments"]) != FRAME_BUNDLE_ATTACHMENTS
        or any(not re.fullmatch(r"[0-9a-f]{64}", value)
               for value in bundle["attachments"].values())
        or not isinstance(bundle.get("windowCapture"), str)
        or not re.fullmatch(r"[0-9a-f]{64}", bundle["windowCapture"])
        or bundle["attachments"]["FinalOutput"] != bundle["windowCapture"]
    ):
        raise ValueError("authoritative frame bundle fingerprint is invalid")
    driver = bundle["vulkanDriver"]
    if expected_backend == "vulkan":
        if (
            not isinstance(driver, dict)
            or set(driver) != {
                "driverId", "driverVersion", "floatControlsSha256",
                "pipelineCacheUuid",
            }
            or not isinstance(driver["driverId"], int)
            or isinstance(driver["driverId"], bool)
            or driver["driverId"] < 0
            or not isinstance(driver["driverVersion"], int)
            or isinstance(driver["driverVersion"], bool)
            or driver["driverVersion"] < 0
            or not re.fullmatch(
                r"[0-9a-f]{64}", driver["floatControlsSha256"]
            )
            or not re.fullmatch(
                r"[0-9a-f]{32}", driver["pipelineCacheUuid"]
            )
        ):
            raise ValueError(
                "authoritative Vulkan driver fingerprint is invalid"
            )
    elif driver is not None:
        raise ValueError("non-Vulkan frame bundle contains Vulkan driver data")
    return bundle


def _parse_image_identity_fields(fields: dict[str, str]) -> tuple[int, list[str]]:
    if not re.fullmatch(r"(?:0|[1-9][0-9]*)", fields["frame-token"]):
        raise ValueError("native image frame token is invalid")
    frame_token = int(fields["frame-token"])
    if not re.fullmatch(r"(?:0|[1-9][0-9]*)", fields["semantic-probes"]):
        raise ValueError("native semantic probe count is invalid")
    count = int(fields["semantic-probes"])
    probe_ids = [] if not fields["probe-ids"] else fields["probe-ids"].split(",")
    if (
        len(probe_ids) != count or len(set(probe_ids)) != len(probe_ids)
        or any(not re.fullmatch(r"[a-z0-9][a-z0-9.-]{0,95}", item)
               for item in probe_ids)
    ):
        raise ValueError("native semantic probe identities are invalid")
    return frame_token, probe_ids


def parse_native_image_evidence(output: str, expected_backend: str) -> dict:
    fields = parse_native_image_fields(output)
    frame_token, probe_ids = _parse_image_identity_fields(fields)
    comparisons = parse_native_reference_evidence(output)
    if (
        fields["backend"] != expected_backend
        or fields["result"] != "passed"
        or not re.fullmatch(r"[a-z0-9][a-z0-9.-]{0,95}", fields["device-class"])
        or not re.fullmatch(r"[a-z0-9][a-z0-9.-]{0,127}", fields["baseline"])
        or frame_token == 0 or probe_ids != EXPECTED_SEMANTIC_PROBE_IDS
        or not re.fullmatch(r"[a-z0-9][a-z0-9.-]{0,127}",
                            fields["matched-reference"])
    ):
        raise ValueError("native image identity evidence is invalid")
    observations = _parse_image_metrics(fields)
    matching = [item for item in comparisons
                if item["referenceId"] == fields["matched-reference"]]
    if len(matching) != 1 or matching[0]["passed"] is not True:
        raise ValueError("native matched reference evidence is invalid")
    return {
        "backend": expected_backend,
        "deviceClass": fields["device-class"],
        "baselineId": fields["baseline"],
        "matchedReferenceId": fields["matched-reference"],
        "frameToken": frame_token,
        "semanticProbeCount": len(probe_ids),
        "semanticProbeIds": probe_ids,
        "referenceComparisons": comparisons,
        "flip": {
            "mean": observations["mean"],
            "p95": observations["p95"],
            "maximum": observations["maximum"],
            "badPixelFraction": observations["bad-fraction"],
            "passed": True,
        },
    }


def parse_native_image_failure_evidence(
    stdout: str,
    stderr: str,
    expected_backend: str,
) -> dict:
    fields = parse_native_image_fields(stdout)
    frame_token, probe_ids = _parse_image_identity_fields(fields)
    comparisons = parse_native_reference_evidence(stdout)
    if (
        fields["backend"] != expected_backend
        or fields["result"] != "failed"
        or not re.fullmatch(
            r"[a-z0-9][a-z0-9.-]{0,95}", fields["device-class"]
        )
        or (fields["matched-reference"] and not re.fullmatch(
            r"[a-z0-9][a-z0-9.-]{0,127}", fields["matched-reference"]))
    ):
        raise ValueError("native failed image identity evidence is invalid")
    prefix = f"{expected_backend.capitalize()} production image failure: "
    failures = [
        line[len(prefix):]
        for line in stderr.splitlines()
        if line.startswith(prefix)
    ]
    if (
        len(failures) != 1
        or not re.fullmatch(r"[a-z0-9][a-z0-9.-]{0,127}", failures[0])
    ):
        raise ValueError("native failed image reason is invalid")
    if failures[0] in {
        "baseline-missing", "baseline-state-not-accepted",
        "reference-set-no-match", "reference-image-digest",
    } and (frame_token == 0 or probe_ids != EXPECTED_SEMANTIC_PROBE_IDS):
        raise ValueError(
            "native failed image did not complete the semantic probe set"
        )
    return {
        "backend": expected_backend,
        "deviceClass": fields["device-class"],
        "baselineId": fields["baseline"] or None,
        "matchedReferenceId": fields["matched-reference"] or None,
        "frameToken": frame_token,
        "semanticProbeCount": len(probe_ids),
        "semanticProbeIds": probe_ids,
        "referenceComparisons": comparisons,
        "flip": ({"state": "measured", "passed": False}
                 if comparisons else
                 {"state": "not-run", "reason": failures[0]}),
        "passed": False,
        "firstFailure": failures[0],
    }


def native_host_support(
    contract: dict,
    host_system: str | None = None,
    host_machine: str | None = None,
    host_translated: bool | None = None,
) -> dict | None:
    system = (host_system or platform.system()).lower()
    machine = (host_machine or platform.machine()).lower()
    normalized_system = {
        "darwin": "macos", "macos": "macos", "linux": "linux",
        "windows": "windows",
    }.get(system, system)
    normalized_machine = {
        "amd64": "x86_64", "x86_64": "x86_64",
        "aarch64": "arm64", "arm64": "arm64",
    }.get(machine, machine)
    if (
        normalized_system == contract["platform"]
        and normalized_machine == contract["cpuArchitecture"]
    ):
        if host_translated is None:
            host_translated = rosetta_translated(normalized_system)
        if not (
            host_translated
            and contract["graphicsBackend"] == "metal"
            and normalized_system == "macos"
        ):
            return None
        return unsupported_result(
            "native",
            f"physical {contract['platform']} {contract['cpuArchitecture']} "
            "Metal host (Rosetta translation is cook-only)",
            "-".join((
                contract["platform"], contract["graphicsBackend"],
                contract["cpuArchitecture"], "hardware",
            )),
        )
    lane = "-".join((
        contract["platform"], contract["graphicsBackend"],
        contract["cpuArchitecture"], "hardware",
    ))
    return unsupported_result(
        "native",
        f"{contract['platform']} {contract['cpuArchitecture']} "
        f"{contract['graphicsBackend']} host",
        lane,
    )


def cook_host_support(
    contract: dict,
    host_system: str | None = None,
    host_machine: str | None = None,
    host_translated: bool | None = None,
) -> dict | None:
    if contract["graphicsBackend"] != "metal":
        return None
    unsupported = native_host_support(
        contract, host_system=host_system, host_machine=host_machine,
        host_translated=False,
    )
    if unsupported is None:
        return None
    lane = "-".join((
        contract["platform"], contract["graphicsBackend"],
        contract["cpuArchitecture"], "hardware",
    ))
    return unsupported_result(
        "cook",
        f"{contract['platform']} {contract['cpuArchitecture']} "
        "Metal offline finalizer",
        lane,
    )


def rosetta_translated(normalized_system: str | None = None) -> bool:
    system = normalized_system or {
        "darwin": "macos", "macos": "macos",
    }.get(platform.system().lower(), platform.system().lower())
    if system != "macos":
        return False
    explicit = os.environ.get("STONER_ROSETTA_TRANSLATED")
    if explicit is not None:
        return explicit.strip().lower() in {"1", "true", "yes", "on"}
    try:
        result = subprocess.run(
            ["/usr/sbin/sysctl", "-in", "sysctl.proc_translated"],
            check=False,
            capture_output=True,
            text=True,
            timeout=5,
        )
    except (OSError, subprocess.SubprocessError):
        return False
    return result.returncode == 0 and result.stdout.strip() == "1"


def run_native_lifecycle(
    repository_root: Path,
    tests: Path,
    target_profile: Path,
    publication: Path,
    lease_root: Path,
    generation: str,
    production_root: str,
    workload_revision: str,
    cycles: int,
    warmup_cycles: int,
    report_path: Path,
    timeout: int,
    require_visible: bool = False,
    authority: dict | None = None,
) -> dict:
    authority = authority or {
        "executionClass": "local-diagnostic",
        "dispositions": {
            "rss": "observed", "timing": "operational",
            "image": "not-required",
        },
        "preflight": None,
    }
    contract = load_native_target_contract(target_profile)
    unsupported = native_host_support(contract)
    if unsupported is not None:
        report_path.write_text(
            json.dumps(unsupported, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        return unsupported
    command, environment = build_native_lifecycle_stage(
        tests, contract["graphicsBackend"], publication, lease_root,
        generation, target_profile, production_root, workload_revision,
        cycles, warmup_cycles, require_visible,
        authority["dispositions"]["image"] == "required",
    )
    environment.update(native_allocator_authority_environment(
        contract, cycles, warmup_cycles, require_visible
    ))
    if require_visible:
        environment["STONER_PRODUCTION_CAPTURE_ROOT"] = str(
            report_path.parent / "captures"
        )
    try:
        result = run_stage(
            "native", command, repository_root, timeout, environment
        )
    except StageFailure as error:
        if (
            error.result is None
            or authority["dispositions"]["image"] != "required"
        ):
            raise
        report_path.write_text(
            error.result.stdout + error.result.stderr, encoding="utf-8"
        )
        combined_output = error.result.stdout + "\n" + error.result.stderr
        if (
            error.returncode != 1
            or re.search(
                r"(?:VK_ERROR_DEVICE_LOST|device[- ]lost|authority[- ]lock)",
                combined_output,
                re.IGNORECASE,
            )
        ):
            raise error
        try:
            evidence = parse_native_lifecycle_evidence(
                error.result.stdout, cycles, warmup_cycles, "observed"
            )
        except ValueError:
            raise error
        evidence["rssDisposition"] = authority["dispositions"]["rss"]
        image_lines = any(
            line.startswith(IMAGE_EVIDENCE_PREFIX)
            for line in error.result.stdout.splitlines()
        )
        if not image_lines:
            capture_evidence = compact_completed_native_captures(
                report_path, contract["graphicsBackend"]
            )
            return {
                "result": "Failed",
                "seconds": error.result.seconds,
                "executionClass": authority["executionClass"],
                "timingDisposition": authority["dispositions"]["timing"],
                "imageDisposition": authority["dispositions"]["image"],
                "targetProfileDigest": contract["targetProfileDigest"],
                **evidence,
                "frameFingerprint": None,
                "imageAcceptance": None,
                "captureEvidence": capture_evidence,
                "firstFailure": {
                    "stage": "lifecycle",
                    "category": "native-lifecycle-gate",
                },
            }
        try:
            image_evidence = parse_native_image_failure_evidence(
                error.result.stdout,
                error.result.stderr,
                contract["graphicsBackend"],
            )
            frame_fingerprint = parse_frame_fingerprint(
                error.result.stderr, contract["graphicsBackend"]
            )
        except ValueError:
            raise error
        if (
            authority.get("preflight") is not None
            and image_evidence["deviceClass"] !=
                authority["preflight"]["deviceClass"]
        ):
            raise ValueError(
                "native failed image device class differs from authority preflight"
            )
        capture_evidence = compact_completed_native_captures(
            report_path, contract["graphicsBackend"]
        )
        return {
            "result": "Failed",
            "seconds": error.result.seconds,
            "executionClass": authority["executionClass"],
            "timingDisposition": authority["dispositions"]["timing"],
            "imageDisposition": authority["dispositions"]["image"],
            "targetProfileDigest": contract["targetProfileDigest"],
            **evidence,
            "frameFingerprint": frame_fingerprint,
            "imageAcceptance": image_evidence,
            "captureEvidence": capture_evidence,
            "firstFailure": {
                "stage": "image",
                "category": image_evidence["firstFailure"],
            },
        }
    report_path.write_text(
        result.stdout + result.stderr, encoding="utf-8"
    )
    evidence = parse_native_lifecycle_evidence(
        result.stdout, cycles, warmup_cycles,
        authority["dispositions"]["rss"],
    )
    image_evidence = (
        parse_native_image_evidence(
            result.stdout, contract["graphicsBackend"]
        )
        if authority["dispositions"]["image"] == "required" else None
    )
    frame_fingerprint = (
        parse_frame_fingerprint(
            result.stderr, contract["graphicsBackend"]
        )
        if authority["dispositions"]["image"] == "required" else None
    )
    capture_evidence = (
        compact_completed_native_captures(
            report_path, contract["graphicsBackend"]
        )
        if require_visible else None
    )
    if (
        image_evidence is not None
        and authority.get("preflight") is not None
        and image_evidence["deviceClass"] !=
            authority["preflight"]["deviceClass"]
    ):
        raise ValueError(
            "native image device class differs from authority preflight"
        )
    return {
        "result": "Passed",
        "seconds": result.seconds,
        "executionClass": authority["executionClass"],
        "timingDisposition": authority["dispositions"]["timing"],
        "imageDisposition": authority["dispositions"]["image"],
        "targetProfileDigest": contract["targetProfileDigest"],
        **evidence,
        "frameFingerprint": frame_fingerprint,
        "imageAcceptance": image_evidence,
        "captureEvidence": capture_evidence,
    }


def compact_completed_native_captures(
    report_path: Path,
    backend: str,
) -> dict:
    capture_root = report_path.parent / "captures"
    calibration = load_local_module(
        "run_production_image_calibration_compaction",
        SCRIPT_DIR / "run_production_image_calibration.py",
    )
    try:
        process = calibration.collect_process(capture_root, backend, 1)
        decoded_digest = process["decodedPixelSha256"]
        png_path = report_path.parent / (
            f"authoritative-frame-{decoded_digest[:12]}.png"
        )
        png_digest = calibration.write_png(png_path, process["rgb"])
        evidence = {
            "schema": "stoner.production-frame-capture",
            "schemaVersion": 1,
            "backend": backend,
            "captureCount": calibration.CAPTURES_PER_PROCESS,
            "decodedPixelSha256": decoded_digest,
            "firstFrameToken": process["firstFrameToken"],
            "lastFrameToken": process["lastFrameToken"],
            "staleFrameMutation": process["staleFrameMutation"],
            "png": png_path.name,
            "pngSha256": png_digest,
        }
        (report_path.parent / "capture-summary.json").write_text(
            json.dumps(evidence, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        return evidence
    finally:
        remove_validation_tree(capture_root, ignore_errors=True)


def retain_bounded_validation_evidence(
    output: Path,
    package_reports: Sequence[dict],
) -> None:
    evidence_root = output / "evidence"
    for report in package_reports:
        package_id = _safe_package_token(report.get("packageId"))
        package_root = output / package_id
        native = report.get("nativeLifecycle")
        capture = native.get("captureEvidence") \
            if isinstance(native, dict) else None
        if isinstance(capture, dict):
            report_root = package_root / "clean-00/reports"
            png_name = capture.get("png")
            source_png = report_root / str(png_name)
            source_summary = report_root / "capture-summary.json"
            if (
                not isinstance(png_name, str)
                or not re.fullmatch(
                    r"authoritative-frame-[0-9a-f]{12}\.png", png_name
                )
                or not source_png.is_file()
                or not source_summary.is_file()
                or sha256_bytes(source_png.read_bytes()) !=
                    capture.get("pngSha256")
            ):
                raise ValueError("bounded capture evidence is incomplete")
            destination = evidence_root / package_id
            destination.mkdir(parents=True, exist_ok=True)
            shutil.move(source_png, destination / png_name)
            shutil.move(
                source_summary, destination / "capture-summary.json"
            )
            capture["png"] = (
                Path("evidence") / package_id / png_name
            ).as_posix()
        try:
            package_root.resolve().relative_to(output.resolve())
        except ValueError as error:
            raise ValueError(
                "package evidence cleanup escapes validation output"
            ) from error
        remove_validation_tree(package_root)
    if evidence_root.exists() and not any(evidence_root.iterdir()):
        evidence_root.rmdir()


def load_report(path: Path, expected_command: str) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if (
        value.get("schema") != "stoner.asset-cook-report"
        or value.get("result") != "success"
        or value.get("command") != expected_command
        or "telemetry" in value
    ):
        raise ValueError(f"invalid normalized report: {path}")
    return value


def assert_clean_determinism(reports: Iterable[Path]) -> str:
    paths = list(reports)
    if not paths:
        raise ValueError("no clean reports")
    expected = paths[0].read_bytes()
    for path in paths[1:]:
        if path.read_bytes() != expected:
            raise ValueError(
                f"normalized clean reports differ: {paths[0]} vs {path}"
            )
    return sha256_bytes(expected)


def assert_acceptance_correctness_determinism(
    reports: Iterable[Path],
) -> str:
    paths = list(reports)
    if not paths:
        raise ValueError("no acceptance reports")
    report_module = load_local_module(
        "production_acceptance_report",
        SCRIPT_DIR / "production_acceptance_report.py",
    )
    expected = report_module.canonical_correctness_bytes(
        json.loads(paths[0].read_text(encoding="utf-8"))
    )
    for path in paths[1:]:
        candidate = report_module.canonical_correctness_bytes(
            json.loads(path.read_text(encoding="utf-8"))
        )
        if candidate != expected:
            raise ValueError(
                "acceptance correctness differs across equivalent runs"
            )
    return sha256_bytes(expected)


def assert_full_reuse(report: dict) -> None:
    summary = report.get("summary", {})
    if (
        summary.get("reachable", 0) <= 0
        or summary.get("reused") != summary.get("reachable")
        or summary.get("cooked") != 0
        or summary.get("failed") != 0
    ):
        raise ValueError("unchanged warm cook did not reuse every reachable asset")


def build_cook_command(
    cooker: Path,
    package_root: Path,
    shader_root: Path,
    root_asset_ids: Sequence[str],
    target_profile: Path,
    publication: Path,
    ddc: Path,
    report: Path,
    clean: bool,
    workers: int,
) -> list[str]:
    command = [
        str(cooker),
        "cook",
        "--source-root",
        str(package_root),
        "--source-root",
        str(shader_root),
        "--target-profile",
        str(target_profile),
        "--output",
        str(publication),
        "--ddc",
        str(ddc),
        "--workers",
        str(workers),
        "--lease-timeout-ms",
        "30000",
        "--normalized-report",
        "--report",
        str(report),
    ]
    root_arguments = []
    for root_asset_id in root_asset_ids:
        root_arguments.extend(("--root", root_asset_id))
    command[6:6] = root_arguments
    if clean:
        command.append("--clean")
    return command


def build_metal_doctor_command(
    cooker: Path,
    target_profile: Path,
    report: Path,
) -> list[str]:
    return [
        str(cooker),
        "doctor",
        "--target-profile",
        str(target_profile),
        "--normalized-report",
        "--report",
        str(report),
    ]


def clean_cook_concurrency(
    determinism_runs: int,
    graphics_backend: str,
    cpu_architecture: str,
) -> int:
    is_intel_metal = (
        graphics_backend == "metal" and cpu_architecture == "x86_64"
    )
    limit = 4 if graphics_backend != "metal" or is_intel_metal else 2
    cpu_divisor = 1 if is_intel_metal else 2
    return min(
        determinism_runs,
        limit,
        max(1, (os.cpu_count() or 1) // cpu_divisor),
    )


def copy_shader_source(repository_root: Path, destination: Path) -> None:
    source = repository_root / "Content/Shaders/Deferred"
    destination.mkdir(parents=True)
    for name in SHADER_SOURCE_FILES:
        shutil.copy2(source / name, destination / name)


def executable(build_root: Path, relative: str) -> Path:
    path = build_root / relative
    if os.name == "nt":
        path = path.with_suffix(".exe")
    if not path.is_file():
        raise FileNotFoundError(path)
    return path


def package_source(
    repository_root: Path,
    content_root: Path,
    package: dict,
) -> Path:
    path = content_root / package["packageRoot"]
    if not path.is_dir():
        raise FileNotFoundError(
            f"verified package is not staged: {package['packageId']} at {path}"
        )
    return path


def acquire_missing_external_packages(
    repository_root: Path,
    content_root: Path,
    packages: Sequence[dict],
) -> list[dict]:
    acquisition = load_local_module(
        "acquire_production_corpus",
        SCRIPT_DIR / "acquire_production_corpus.py",
    )
    results = []
    manifest_path = repository_root / CORPUS_MANIFEST
    for package in packages:
        if package.get("tier") != "medium":
            continue
        source = content_root / package["packageRoot"]
        if source.is_dir():
            continue
        result = acquisition.acquire_package(
            manifest_path, package["packageId"], content_root
        )
        results.append(result)
    return results


def validate_profile_selection(
    repository_root: Path,
    profile_name: str,
    target_profile: Path,
    manifest: dict,
) -> tuple[dict, list[dict]]:
    profile = load_validation_profile(repository_root, profile_name)
    if profile.get("corpusRevision") != manifest.get("corpusRevision"):
        raise ValueError("validation profile corpus revision mismatch")
    canonical_target = target_profile.resolve()
    allowed = {
        (repository_root / item).resolve()
        for item in profile.get("targetProfiles", [])
    }
    if canonical_target not in allowed:
        raise ValueError("target profile is not declared by validation profile")
    package_ids = set(profile.get("packageIds", []))
    selected = [
        package for package in manifest["packages"]
        if package["packageId"] in package_ids
    ]
    if {package["packageId"] for package in selected} != package_ids:
        raise ValueError("validation profile references an unknown package")
    return profile, selected


def run_package(
    repository_root: Path,
    cooker: Path,
    tests: Path,
    target_profile: Path,
    package: dict,
    source: Path,
    output: Path,
    determinism_runs: int,
    lifecycle_cycles: int,
    warmup_cycles: int,
    timeout: int,
    native_timeout: int,
    graphics_backend: str,
    cpu_architecture: str,
    require_visible: bool = False,
    defer_native_to_hardware: bool = False,
    deadline: float | None = None,
    native_lifecycle_lock=None,
    native_lifecycle_barrier=None,
    authority: dict | None = None,
) -> dict:
    package_output = output / package["packageId"]
    package_output.mkdir(parents=True)
    source_digest = tree_digest(source)
    package_deadline = deadline or (time.monotonic() + timeout)

    def RunClean(index: int) -> dict:
        run_root = package_output / f"clean-{index:02d}"
        source_root = run_root / "sources/package"
        shader_root = run_root / "sources/shaders"
        shutil.copytree(source, source_root)
        copy_shader_source(repository_root, shader_root)
        publication = run_root / "publication"
        ddc = run_root / "ddc"
        report_root = run_root / "reports"
        report_root.mkdir(parents=True)
        clean_report = report_root / "clean.json"
        result = run_stage(
            "clean-cook",
            build_cook_command(
                cooker,
                source_root,
                shader_root,
                (package["rootAssetId"], *DEFERRED_SHADER_ROOTS),
                target_profile,
                publication,
                ddc,
                clean_report,
                True,
                1 if index % 2 == 0 else 2,
            ),
            repository_root,
            remaining_stage_timeout(package_deadline, timeout),
        )
        clean = load_report(clean_report, "cook")
        if tree_digest(source_root) != source_digest:
            raise RuntimeError("authoritative source changed during clean cook")
        return {
            "index": index,
            "report": clean_report,
            "generationId": clean["generationId"],
            "seconds": result.seconds,
            "paths": {
                "source": source_root,
                "shader": shader_root,
                "publication": publication,
                "ddc": ddc,
                "reports": report_root,
            },
        }

    concurrency = clean_cook_concurrency(
        determinism_runs, graphics_backend, cpu_architecture
    )
    if concurrency == 1:
        clean_runs = [RunClean(index) for index in range(determinism_runs)]
    else:
        with concurrent.futures.ThreadPoolExecutor(
            max_workers=concurrency,
            thread_name_prefix="production-clean-cook",
        ) as executor:
            clean_runs = list(executor.map(RunClean, range(determinism_runs)))
    clean_runs.sort(key=lambda item: item["index"])
    clean_reports = [item["report"] for item in clean_runs]
    generation_ids = [item["generationId"] for item in clean_runs]
    clean_seconds = [item["seconds"] for item in clean_runs]
    first_run = clean_runs[0]["paths"] if clean_runs else None

    clean_report_digest = assert_clean_determinism(clean_reports)
    if len(set(generation_ids)) != 1:
        raise RuntimeError("clean generation identity differs across runs")
    assert first_run is not None

    warm_report = first_run["reports"] / "warm.json"
    warm_result = run_stage(
        "warm-cook",
        build_cook_command(
            cooker,
            first_run["source"],
            first_run["shader"],
            (package["rootAssetId"], *DEFERRED_SHADER_ROOTS),
            target_profile,
            first_run["publication"],
            first_run["ddc"],
            warm_report,
            False,
            8,
        ),
        repository_root,
        remaining_stage_timeout(package_deadline, timeout),
    )
    warm = load_report(warm_report, "cook")
    assert_full_reuse(warm)
    if warm["generationId"] != generation_ids[0]:
        raise RuntimeError("warm generation differs from clean generation")

    validate_report = first_run["reports"] / "validate.json"
    validate_result = run_stage(
        "publication",
        [
            str(cooker),
            "validate",
            "--output",
            str(first_run["publication"]),
            "--strict-files",
            "--normalized-report",
            "--report",
            str(validate_report),
        ],
        repository_root,
        remaining_stage_timeout(package_deadline, timeout),
    )
    load_report(validate_report, "validate")
    current_pointer = first_run["publication"] / "Current.json"
    current_digest_before_runtime = sha256_bytes(current_pointer.read_bytes())
    current_record = json.loads(current_pointer.read_text(encoding="utf-8"))
    generation_manifest = (
        first_run["publication"] / current_record["manifestLocator"]
    )
    generation_manifest_digest = sha256_bytes(generation_manifest.read_bytes())
    if (
        current_record.get("generationId") != generation_ids[0]
        or current_record.get("manifestDigest") != generation_manifest_digest
    ):
        raise RuntimeError("published generation evidence is inconsistent")

    lease_root = package_output / "lease-coordination"
    equivalence_timeout = remaining_stage_timeout(package_deadline, timeout)
    equivalence_result = run_stage(
        "semantic-equivalence",
        [str(tests), "--suite", "production-content-equivalence"],
        repository_root,
        equivalence_timeout,
        {
            "STONER_PRODUCTION_PUBLICATION_ROOT": str(
                first_run["publication"]
            ),
            "STONER_PRODUCTION_LEASE_ROOT": str(lease_root),
            "STONER_PRODUCTION_TARGET_PROFILE": str(target_profile),
            "STONER_PRODUCTION_PACKAGE_ROOT": str(first_run["source"]),
            "STONER_PRODUCTION_SHADER_ROOT": str(first_run["shader"]),
            "STONER_PRODUCTION_REQUEST_TIMEOUT_SECONDS": str(
                equivalence_request_timeout_seconds(equivalence_timeout)
            ),
        },
    )
    (first_run["reports"] / "equivalence.txt").write_text(
        equivalence_result.stdout + equivalence_result.stderr,
        encoding="utf-8",
    )
    remove_validation_tree(first_run["source"])
    remove_validation_tree(first_run["shader"])
    if first_run["source"].exists() or first_run["shader"].exists():
        raise RuntimeError("source roots remained available before strict runtime")
    strict_runtime_result = run_stage(
        "strict-runtime",
        [str(tests), "--suite", "production-content-strict-runtime"],
        repository_root,
        remaining_stage_timeout(package_deadline, timeout),
        {
            "STONER_PRODUCTION_PUBLICATION_ROOT": str(
                first_run["publication"]
            ),
            "STONER_PRODUCTION_TARGET_PROFILE": str(target_profile),
        },
    )
    (first_run["reports"] / "strict-runtime.txt").write_text(
        strict_runtime_result.stdout + strict_runtime_result.stderr,
        encoding="utf-8",
    )
    if sha256_bytes(current_pointer.read_bytes()) != current_digest_before_runtime:
        raise RuntimeError("runtime validation changed the published current pointer")
    if tree_digest(source) != source_digest:
        raise RuntimeError("repository or staged source changed during validation")

    native_report = first_run["reports"] / "native-lifecycle.txt"
    workload_revision = package_workload_revision(package)
    wait_for_optional_barrier(
        native_lifecycle_barrier,
        remaining_stage_timeout(package_deadline, timeout),
    )
    if defer_native_to_hardware:
        native_lifecycle = deferred_native_result(
            load_native_target_contract(target_profile)
        )
        native_report.write_text(
            json.dumps(native_lifecycle, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
    else:
        native_lifecycle = run_with_optional_lock(
            lambda: run_native_lifecycle(
                repository_root,
                tests,
                target_profile,
                first_run["publication"],
                lease_root,
                generation_ids[0],
                package["rootAssetId"],
                workload_revision,
                lifecycle_cycles,
                warmup_cycles,
                native_report,
                native_stage_timeout(
                    package_deadline, timeout, native_timeout
                ),
                require_visible,
                authority,
            ),
            native_lifecycle_lock,
        )

    return {
        "packageId": package["packageId"],
        "rootAssetId": package["rootAssetId"],
        "workloadRevision": workload_revision,
        "generationId": generation_ids[0],
        "currentPointerDigest": current_digest_before_runtime,
        "generationManifestDigest": generation_manifest_digest,
        "cleanRuns": determinism_runs,
        "cleanReportDigest": clean_report_digest,
        "maximumCleanSeconds": max(clean_seconds),
        "warmSeconds": warm_result.seconds,
        "validateSeconds": validate_result.seconds,
        "equivalenceSeconds": equivalence_result.seconds,
        "strictRuntimeSeconds": strict_runtime_result.seconds,
        "reachableAssets": warm["summary"]["reachable"],
        "reusedAssets": warm["summary"]["reused"],
        "sourceTreeDigest": source_digest,
        "nativeLifecycle": native_lifecycle,
    }


def run_profile(args: argparse.Namespace) -> dict:
    repository_root = args.root.resolve()
    target_profile = (
        args.target_profile
        if args.target_profile.is_absolute()
        else repository_root / args.target_profile
    ).resolve()
    build_root = (
        args.build_root
        if args.build_root.is_absolute()
        else repository_root / args.build_root
    ).resolve()
    output = (
        args.output if args.output.is_absolute() else repository_root / args.output
    ).resolve()
    content_root = (
        args.content_root
        if args.content_root.is_absolute()
        else repository_root / args.content_root
    ).resolve()
    if not target_profile.is_file():
        raise FileNotFoundError(target_profile)
    if output.exists() and any(output.iterdir()):
        raise FileExistsError(f"validation output must be empty: {output}")
    cooker = executable(build_root, "Tools/AssetCooker/StonerAssetCooker")
    tests = executable(build_root, "Tests/StonerTest")
    output.mkdir(parents=True, exist_ok=True)

    verifier = load_local_module(
        "verify_production_corpus",
        SCRIPT_DIR / "verify_production_corpus.py",
    )
    manifest_path = repository_root / CORPUS_MANIFEST
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    validation_profile, packages = validate_profile_selection(
        repository_root,
        args.profile,
        target_profile,
        manifest,
    )
    packages = select_profile_packages(
        args.profile, packages, args.package_id
    )
    target_contract = load_native_target_contract(target_profile)
    try:
        authority = classify_execution_environment(
            args.profile,
            local_metal_authority=args.local_metal_authority,
            local_windows_vulkan_authority=
                args.local_windows_vulkan_authority,
            repository_root=repository_root,
            target_profile=target_profile,
            target_contract=target_contract,
        )
    except StageFailure as error:
        if error.stage != "unsupported":
            raise
        requested_authority = (
            "maintainer-local-windows-vulkan"
            if args.local_windows_vulkan_authority
            else "maintainer-local-metal"
        )
        unsupported = unsupported_result(
            "unsupported", error.detail,
            requested_authority,
        )
        result = {
            "schema": "stoner.production-cook-runtime-summary",
            "schemaVersion": 1,
            "profile": args.profile,
            "corpusRevision": manifest["corpusRevision"],
            "corpusDigest": sha256_bytes(manifest_path.read_bytes()),
            "targetProfile": target_profile.relative_to(
                repository_root
            ).as_posix(),
            "targetProfileDigest": target_contract["targetProfileDigest"],
            "determinismRuns": profile_package_clean_runs(
                args.profile, "regular", args.determinism_runs
            ),
            "packages": [],
            "acquisitions": [],
            "timeBudgetSeconds": validation_profile["timeBudgetSeconds"],
            "profileTimeBudgetSeconds":
                validation_profile["profileTimeBudgetSeconds"],
            "nativeTimeBudgetSeconds":
                validation_profile["nativeTimeBudgetSeconds"],
            "elapsedSeconds": 0.0,
            "unsupported": unsupported,
            "passed": False,
        }
        (output / "summary.json").write_text(
            json.dumps(result, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        write_artifact_manifest(output)
        return result
    validate_native_deferral(
        args.profile, target_contract, args.defer_native_to_hardware
    )
    cook_unsupported = cook_host_support(target_contract)
    if cook_unsupported is not None:
        result = {
            "schema": "stoner.production-cook-runtime-summary",
            "schemaVersion": 1,
            "profile": args.profile,
            "executionClass": authority["executionClass"],
            "authoritySource": authority["source"],
            "authorityPreflight": authority["preflight"],
            "measurementDispositions": authority["dispositions"],
            "corpusRevision": manifest["corpusRevision"],
            "corpusDigest": sha256_bytes(manifest_path.read_bytes()),
            "targetProfile": target_profile.relative_to(
                repository_root
            ).as_posix(),
            "targetProfileDigest": target_contract["targetProfileDigest"],
            "determinismRuns": profile_package_clean_runs(
                args.profile, "regular", args.determinism_runs
            ),
            "packages": [],
            "acquisitions": [],
            "timeBudgetSeconds": validation_profile["timeBudgetSeconds"],
            "profileTimeBudgetSeconds":
                validation_profile["profileTimeBudgetSeconds"],
            "nativeTimeBudgetSeconds":
                validation_profile["nativeTimeBudgetSeconds"],
            "elapsedSeconds": 0.0,
            "unsupported": cook_unsupported,
            "passed": False,
        }
        (output / "summary.json").write_text(
            json.dumps(result, indent=2, sort_keys=True) + "\n",
            encoding="utf-8",
        )
        write_artifact_manifest(output)
        return result
    acquisition_results = []
    if args.acquire_missing:
        acquisition_results = acquire_missing_external_packages(
            repository_root, content_root, packages
        )
    verification = verifier.verify_manifest(
        manifest_path,
        content_root,
        package_ids=[package["packageId"] for package in packages],
    )
    if verification["result"] != "Passed":
        raise RuntimeError(
            f"corpus verification failed: {verification['firstFailure']}"
        )

    started = time.monotonic()
    deadline = started + validation_profile["profileTimeBudgetSeconds"]
    if target_contract["graphicsBackend"] == "metal":
        doctor_report = output / "metal-toolchain.json"
        run_stage(
            "metal-toolchain",
            build_metal_doctor_command(
                cooker, target_profile, doctor_report
            ),
            repository_root,
            remaining_stage_timeout(deadline, args.timeout_seconds),
        )
        load_report(doctor_report, "doctor")
    package_concurrency = profile_package_concurrency(
        args.profile, len(packages)
    )
    native_lifecycle_lock = (
        threading.Lock()
        if args.profile == "medium" and package_concurrency > 1 and
        target_contract["graphicsBackend"] == "metal"
        else None
    )
    native_lifecycle_barrier = (
        threading.Barrier(package_concurrency)
        if native_lifecycle_lock is not None
        else None
    )

    def run_selected_package(package: dict) -> dict:
        runs = profile_package_clean_runs(
            args.profile, package["tier"], args.determinism_runs
        )
        try:
            return run_package(
                repository_root,
                cooker,
                tests,
                target_profile,
                package,
                package_source(repository_root, content_root, package),
                output,
                runs,
                validation_profile["lifecycleCycles"],
                validation_profile["warmupCycles"],
                args.timeout_seconds,
                validation_profile["nativeTimeBudgetSeconds"],
                target_contract["graphicsBackend"],
                target_contract["cpuArchitecture"],
                args.profile == "hardware",
                args.defer_native_to_hardware,
                bounded_package_deadline(
                    deadline,
                    validation_profile["timeBudgetSeconds"],
                ),
                native_lifecycle_lock,
                native_lifecycle_barrier,
                authority,
            )
        except Exception:
            if native_lifecycle_barrier is not None:
                native_lifecycle_barrier.abort()
            raise

    if package_concurrency == 1:
        package_reports = run_serial_packages_collect_all(
            packages, run_selected_package,
            lambda report: revalidate_local_authority_session(
                repository_root, authority, report
            ),
        )
    else:
        # Medium packages own disjoint source, DDC, publication, lease, and
        # report roots, so their CPU/cook stages may overlap. Hosted Metal
        # lifecycle stages first meet at one barrier, then share one lock, so
        # neither native process overlaps the other package's CPU/cook work or
        # contends for the GPU and contaminates throughput/per-process RSS.
        # Hardware remains fully serialized because visible windows and capture
        # devices are shared host resources.
        with concurrent.futures.ThreadPoolExecutor(
            max_workers=package_concurrency,
            thread_name_prefix="production-package",
        ) as executor:
            package_reports = list(executor.map(
                run_selected_package, packages
            ))
    elapsed = time.monotonic() - started
    if authority["executionClass"].startswith("maintainer-local-"):
        revalidate_local_authority_session(repository_root, authority)
    result = {
        "schema": "stoner.production-cook-runtime-summary",
        "schemaVersion": 1,
        "profile": args.profile,
        "executionClass": authority["executionClass"],
        "authoritySource": authority["source"],
        "authorityPreflight": authority["preflight"],
        "measurementDispositions": authority["dispositions"],
        "corpusRevision": manifest["corpusRevision"],
        "corpusDigest": verification["manifestDigest"],
        "targetProfile": target_profile.relative_to(repository_root).as_posix(),
        "targetProfileDigest": sha256_bytes(target_profile.read_bytes()),
        "determinismRuns": max(
            report["cleanRuns"] for report in package_reports
        ),
        "packages": package_reports,
        "acquisitions": acquisition_results,
        "timeBudgetSeconds": validation_profile["timeBudgetSeconds"],
        "profileTimeBudgetSeconds":
            validation_profile["profileTimeBudgetSeconds"],
        "nativeTimeBudgetSeconds":
            validation_profile["nativeTimeBudgetSeconds"],
        "elapsedSeconds": elapsed,
        "passed": elapsed <= validation_profile["profileTimeBudgetSeconds"] and
            aggregate_native_results(
                [report["nativeLifecycle"] for report in package_reports],
                args.defer_native_to_hardware,
            ),
    }
    retain_bounded_validation_evidence(output, package_reports)
    summary_path = output / "summary.json"
    summary_path.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    if elapsed > validation_profile["profileTimeBudgetSeconds"]:
        raise RuntimeError(
            "validation exceeded "
            f"{validation_profile['profileTimeBudgetSeconds']} second profile budget"
        )
    write_artifact_manifest(output)
    return result


def parse_args(values: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=REPOSITORY_ROOT)
    parser.add_argument(
        "--profile", choices=("regular", "medium", "hardware")
    )
    parser.add_argument("--target-profile", type=Path)
    parser.add_argument("--build-root", type=Path)
    parser.add_argument("--output", type=Path)
    parser.add_argument("--verify-only", type=Path)
    parser.add_argument("--acquire-missing", action="store_true")
    parser.add_argument(
        "--defer-native-to-hardware", action="store_true"
    )
    parser.add_argument(
        "--content-root",
        type=Path,
        default=Path("Content/ProductionAcceptance"),
    )
    parser.add_argument("--determinism-runs", type=int, default=20)
    parser.add_argument("--timeout-seconds", type=int, default=5400)
    parser.add_argument("--package-id")
    parser.add_argument(
        "--local-metal-authority", action="store_true",
        help=(
            "run the hardware profile as the maintainer's exclusive native "
            "arm64 macOS Metal authority"
        ),
    )
    parser.add_argument(
        "--local-windows-vulkan-authority", action="store_true",
        help=(
            "run the hardware profile as the maintainer's exclusive native "
            "x86_64 Windows Vulkan authority"
        ),
    )
    args = parser.parse_args(values)
    if args.verify_only is not None:
        if any((args.profile, args.build_root, args.output, args.acquire_missing,
                args.defer_native_to_hardware, args.package_id,
                args.local_metal_authority,
                args.local_windows_vulkan_authority)):
            parser.error(
                "--verify-only cannot be combined with execution options"
            )
        if args.target_profile is None:
            parser.error("--verify-only requires --target-profile")
        return args
    if not all((args.profile, args.target_profile, args.build_root, args.output)):
        parser.error(
            "--profile, --target-profile, --build-root, and --output are required"
        )
    if not 1 <= args.determinism_runs <= 20:
        parser.error("--determinism-runs must be in [1, 20]")
    if not 60 <= args.timeout_seconds <= 7200:
        parser.error("--timeout-seconds must be in [60, 7200]")
    if args.package_id is not None and args.profile != "medium":
        parser.error("--package-id is supported only by --profile medium")
    if args.local_metal_authority and args.profile != "hardware":
        parser.error("--local-metal-authority requires --profile hardware")
    if (
        args.local_windows_vulkan_authority
        and args.profile != "hardware"
    ):
        parser.error(
            "--local-windows-vulkan-authority requires --profile hardware"
        )
    if args.local_metal_authority and args.local_windows_vulkan_authority:
        parser.error("local authority assertions are mutually exclusive")
    return args


def main(values: Sequence[str] | None = None) -> int:
    args = parse_args(values)
    if args.verify_only is not None:
        output = args.verify_only
        if not output.is_absolute():
            output = args.root.resolve() / output
        if args.target_profile is None:
            raise ValueError("--verify-only requires --target-profile")
        target_profile = args.target_profile
        if not target_profile.is_absolute():
            target_profile = args.root.resolve() / target_profile
        result = verify_validation_output(output, target_profile)
    else:
        result = run_profile(args)
    print(json.dumps(result, sort_keys=True))
    return 0 if result.get("passed") is True else 1


if __name__ == "__main__":
    raise SystemExit(main())
