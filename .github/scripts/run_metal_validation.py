#!/usr/bin/env python3
"""Run bounded Feature 027 validation and emit canonical JSON evidence."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import subprocess
import sys
import tempfile
import time
from typing import Mapping, Sequence


DIGEST = re.compile(r"^[0-9a-f]{64}$")
DERIVATION_EVIDENCE = re.compile(
    r"^\[EVIDENCE\] metal-derivation (.+)$", re.MULTILINE
)
FAILURE_EVIDENCE = re.compile(
    r"^\[EVIDENCE\] (metal-failure .+|metal-failure-ownership .+)$",
    re.MULTILINE,
)
NATIVE_DEVICE_EVIDENCE = re.compile(
    r"^\[EVIDENCE\] metal-native-device identity=(\S+) "
    r"name-utf8-hex=([0-9a-f]*) capability=([0-9a-f]{64})$",
    re.MULTILINE,
)
NATIVE_SHADER_EVIDENCE = re.compile(
    r"^\[EVIDENCE\] metal-native-shader evidence=([0-9a-f]{64})$",
    re.MULTILINE,
)
NATIVE_DEFERRED_EVIDENCE = re.compile(
    r"^\[EVIDENCE\] metal-native-deferred status=passed "
    r"readback=([0-9a-f]{64})$",
    re.MULTILINE,
)
NATIVE_LIFECYCLE_EVIDENCE = re.compile(
    r"^\[EVIDENCE\] metal-lifecycle-native (native|unavailable)$",
    re.MULTILINE,
)
RSS_EVIDENCE = re.compile(
    r"^\[EVIDENCE\] metal-lifecycle-rss iteration=(\d+) bytes=(\d+)$",
    re.MULTILINE,
)
RSS_SUMMARY = re.compile(
    r"^\[EVIDENCE\] metal-lifecycle-summary samples=(\d+) first=(\d+) "
    r"final=(\d+) growth=(-?\d+) allowed=(\d+) passed=(true|false)$",
    re.MULTILINE,
)


def parse_args(argv: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--tests", type=Path)
    parser.add_argument("--demo", type=Path)
    parser.add_argument("--presentation-probe", type=Path)
    parser.add_argument("--profile", type=Path)
    parser.add_argument("--publication", type=Path)
    parser.add_argument("--lease", type=Path)
    parser.add_argument("--capture", type=Path)
    parser.add_argument(
        "--tier",
        choices=(
            "deterministic", "native-offscreen", "visible-manual",
            "cross-backend",
        ),
        default="deterministic",
    )
    parser.add_argument(
        "--workload",
        choices=(
            "derivation", "failure", "lifecycle", "native", "comparison",
            "visible", "presentation-smoke",
        ),
    )
    parser.add_argument("--repetitions", type=int, default=20)
    parser.add_argument("--lifecycle-iterations", type=int, default=10000)
    parser.add_argument("--smoke-frames", type=int, default=120)
    parser.add_argument("--smoke-cycles", type=int, default=4)
    parser.add_argument("--visible-frames", type=int, default=3000)
    parser.add_argument("--visible-cycles", type=int, default=20)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--work", type=Path)
    parser.add_argument("--timeout-seconds", type=int, default=1200)
    parser.add_argument("--require-native", action="store_true")
    parser.add_argument("--prepare-only", action="store_true")
    args = parser.parse_args(argv)
    if not 1 <= args.repetitions <= 100:
        parser.error("--repetitions must be in [1, 100]")
    if not 1 <= args.lifecycle_iterations <= 1_000_000:
        parser.error("--lifecycle-iterations must be in [1, 1000000]")
    if not 1 <= args.smoke_frames <= 10000:
        parser.error("--smoke-frames must be in [1, 10000]")
    if not 1 <= args.smoke_cycles <= args.smoke_frames:
        parser.error("--smoke-cycles must be in [1, smoke-frames]")
    if not 3000 <= args.visible_frames <= 1_000_000:
        parser.error("--visible-frames must be in [3000, 1000000]")
    if not 20 <= args.visible_cycles <= 1000:
        parser.error("--visible-cycles must be in [20, 1000]")
    if not 1 <= args.timeout_seconds <= 7200:
        parser.error("--timeout-seconds must be in [1, 7200]")
    if args.workload is None:
        args.workload = {
            "deterministic": "derivation",
            "native-offscreen": "native",
            "cross-backend": "comparison",
            "visible-manual": "visible",
        }[args.tier]
    if args.tier == "cross-backend" and args.workload != "comparison":
        parser.error("cross-backend tier requires the comparison workload")
    if args.tier == "visible-manual" and args.workload not in {
        "visible", "presentation-smoke",
    }:
        parser.error(
            "visible-manual tier requires visible or presentation-smoke workload"
        )
    if args.workload == "visible" and any(value is None for value in (
        args.demo, args.profile, args.publication, args.lease, args.capture,
        args.work,
    )):
        parser.error(
            "visible workload requires --demo, --profile, --publication, "
            "--lease, --capture, and persistent --work evidence"
        )
    if args.workload == "presentation-smoke" and (
        args.presentation_probe is None or args.work is None
    ):
        parser.error(
            "presentation-smoke requires --presentation-probe and persistent --work evidence"
        )
    return args


def run_command(
    command: Sequence[str],
    root: Path,
    timeout_seconds: int,
    environment: Mapping[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    env = os.environ.copy()
    if environment:
        env.update(environment)
    try:
        return subprocess.run(
            list(command), cwd=root, capture_output=True, text=True,
            timeout=timeout_seconds, check=False, env=env,
        )
    except subprocess.TimeoutExpired as error:
        stdout = error.stdout if isinstance(error.stdout, str) else ""
        stderr = error.stderr if isinstance(error.stderr, str) else ""
        return subprocess.CompletedProcess(
            list(command), 124, stdout, stderr + "validation-timeout\n"
        )


def host_record() -> dict[str, str]:
    os_name = {
        "Darwin": "macos", "Linux": "linux", "Windows": "windows",
    }.get(platform.system(), platform.system().lower())
    architecture = platform.machine().lower()
    if architecture in {"amd64", "x64"}:
        architecture = "x86_64"
    elif architecture in {"aarch64", "arm64"}:
        architecture = "arm64"
    record = {"os": os_name, "architecture": architecture}
    runner_image = os.environ.get("ImageOS") or os.environ.get("RUNNER_IMAGE")
    if runner_image:
        record["runnerImage"] = runner_image[:256]
    return record


def revision(root: Path) -> str:
    completed = run_command(
        ["git", "rev-parse", "HEAD"], root, timeout_seconds=30
    )
    value = completed.stdout.strip().lower()
    return value if re.fullmatch(r"[0-9a-f]{40}", value) else "0" * 40


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def finalize_report(document: dict[str, object]) -> dict[str, object]:
    document.pop("reportDigest", None)
    unsigned = json.dumps(
        document, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode("utf-8")
    document["reportDigest"] = hashlib.sha256(unsigned).hexdigest()
    return document


def base_report(root: Path, workload: str, tier: str) -> dict[str, object]:
    backend = "comparison" if tier == "cross-backend" else (
        "metal" if tier in {"native-offscreen", "visible-manual"} else "shared"
    )
    return {
        "schemaVersion": 1,
        "revision": revision(root),
        "tier": tier,
        "workload": workload,
        "backend": backend,
        "host": host_record(),
        "counts": {"frames": 0, "lifecycleCycles": 0, "iterations": 0},
        "probes": [],
        "artifacts": [],
        "result": "unavailable",
    }


def validate_report(document: dict[str, object]) -> list[str]:
    errors: list[str] = []
    required = {
        "schemaVersion", "revision", "tier", "workload", "backend", "host",
        "counts", "probes", "artifacts", "result", "reportDigest",
    }
    allowed = required | {
        "device", "shaderEvidenceDigests", "ownership", "memory",
        "failureCategory",
    }
    if missing := required - set(document):
        errors.append(f"missing report fields: {sorted(missing)}")
    if extra := set(document) - allowed:
        errors.append(f"unknown report fields: {sorted(extra)}")
    if document.get("schemaVersion") != 1:
        errors.append("schemaVersion must be 1")
    if not re.fullmatch(r"[0-9a-f]{40}", str(document.get("revision", ""))):
        errors.append("revision must be a lowercase full Git SHA")
    if document.get("tier") not in {
        "deterministic", "native-offscreen", "visible-manual", "cross-backend",
    }:
        errors.append("tier is invalid")
    if document.get("result") not in {"passed", "failed", "unavailable"}:
        errors.append("result is invalid")
    if document.get("tier") == "cross-backend" and document.get("backend") != "comparison":
        errors.append("cross-backend tier requires comparison backend")
    native_pass = document.get("tier") in {
        "native-offscreen", "visible-manual", "cross-backend",
    } \
        and document.get("result") == "passed"
    if native_pass:
        device = document.get("device")
        shaders = document.get("shaderEvidenceDigests")
        if not isinstance(device, dict) or not {
            "identity", "name", "capabilityDigest",
        }.issubset(device):
            errors.append("native pass requires complete device evidence")
        elif not DIGEST.fullmatch(str(device.get("capabilityDigest", ""))):
            errors.append("native capability digest is invalid")
        if document.get("workload") != "metal-presentation-smoke":
            if not isinstance(shaders, list) or not shaders or any(
                not DIGEST.fullmatch(str(value)) for value in shaders
            ):
                errors.append("native pass requires shader evidence digests")
    if document.get("workload") == "metal-lifecycle-stress":
        memory = document.get("memory")
        if not isinstance(memory, dict) or len(memory.get("samplesBytes", [])) != 90:
            errors.append("lifecycle report requires exactly 90 RSS samples")
    if document.get("tier") == "visible-manual" and document.get("result") == "passed":
        counts = document.get("counts")
        minimum_frames, minimum_cycles = (
            (120, 4) if document.get("workload") == "metal-presentation-smoke"
            else (3000, 20)
        )
        if not isinstance(counts, dict) or \
                counts.get("frames", 0) < minimum_frames or \
                counts.get("lifecycleCycles", 0) < minimum_cycles:
            errors.append(
                f"visible pass requires at least {minimum_frames} frames "
                f"and {minimum_cycles} cycles"
            )
        if document.get("workload") == "metal-visible-presentation" and not any(
            isinstance(item, dict) and
            str(item.get("path", "")).startswith("Validation/027/captures/") and
            str(item.get("path", "")).lower().endswith(".png")
            for item in document.get("artifacts", [])
        ):
            errors.append("visible presentation pass requires an accepted PNG capture")
    if document.get("tier") == "cross-backend" and document.get("result") == "passed":
        probes = document.get("probes")
        if not isinstance(probes, list) or not any(
            isinstance(probe, dict) and
            probe.get("tolerance") == "metal-vulkan-tolerance-v1"
            for probe in probes
        ):
            errors.append("comparison pass requires frozen tolerance provenance")
    artifacts = document.get("artifacts")
    if not isinstance(artifacts, list) or any(
        not isinstance(item, dict) or not DIGEST.fullmatch(str(item.get("digest", "")))
        for item in artifacts
    ):
        errors.append("artifact records are invalid")
    actual_digest = document.get("reportDigest")
    unsigned = dict(document)
    unsigned.pop("reportDigest", None)
    expected_digest = hashlib.sha256(json.dumps(
        unsigned, sort_keys=True, separators=(",", ":"), ensure_ascii=True
    ).encode("utf-8")).hexdigest()
    if actual_digest != expected_digest:
        errors.append("reportDigest does not match canonical report bytes")
    return errors


def run_derivation(
    tests: Path, root: Path, repetitions: int, timeout: int
) -> tuple[bool, list[str]]:
    baseline: list[str] | None = None
    for _ in range(repetitions):
        completed = run_command(
            [str(tests), "--suite", "metal-shader-derivation"], root, timeout
        )
        evidence = sorted(DERIVATION_EVIDENCE.findall(completed.stdout))
        passed = completed.returncode == 0 and bool(evidence)
        if baseline is None and passed:
            baseline = evidence
        if not passed or evidence != baseline:
            sys.stderr.write(completed.stdout + completed.stderr)
            return False, baseline or []
    return True, baseline or []


def run_failure(
    tests: Path, root: Path, repetitions: int, timeout: int
) -> tuple[bool, list[str]]:
    baseline: list[str] | None = None
    for _ in range(repetitions):
        completed = run_command(
            [str(tests), "--suite", "metal-failure-injection",
             "--suite", "metal-diagnostics"], root, timeout
        )
        evidence = FAILURE_EVIDENCE.findall(completed.stdout)
        passed = completed.returncode == 0 and len(evidence) == 10
        if baseline is None and passed:
            baseline = evidence
        if not passed or evidence != baseline:
            sys.stderr.write(completed.stdout + completed.stderr)
            return False, baseline or []
    return True, baseline or []


def parse_lifecycle_output(stdout: str) -> tuple[str, dict[str, object] | None]:
    native = NATIVE_LIFECYCLE_EVIDENCE.search(stdout)
    samples = [(int(i), int(value)) for i, value in RSS_EVIDENCE.findall(stdout)]
    summary = RSS_SUMMARY.search(stdout)
    if not native or not summary:
        return "invalid", None
    sample_count, first, final, growth, allowed, passed = summary.groups()
    if int(sample_count) != len(samples) or len(samples) != 90:
        return "invalid", None
    expected_iterations = list(range(1100, 10100, 100))
    if [iteration for iteration, _ in samples] != expected_iterations:
        return "invalid", None
    memory = {
        "warmupIterations": 1000,
        "sampleInterval": 100,
        "samplesBytes": [value for _, value in samples],
        "firstMedianBytes": int(first),
        "finalMedianBytes": int(final),
        "absoluteGrowthBytes": int(growth),
        "relativeGrowth": (int(growth) / int(first)) if int(first) else 0.0,
        "allowedGrowthBytes": int(allowed),
        "passed": passed == "true",
    }
    return native.group(1), memory


def run_lifecycle(
    tests: Path, root: Path, iterations: int, timeout: int,
    require_native: bool,
) -> tuple[bool, str, dict[str, object] | None, str]:
    command = [
        str(tests), "--suite", "metal-lifecycle-stress",
        "--metal-lifecycle-iterations", str(iterations),
    ]
    if require_native:
        command.append("--metal-native")
    completed = run_command(command, root, timeout)
    native, memory = parse_lifecycle_output(completed.stdout)
    passed = completed.returncode == 0 and memory is not None and \
        bool(memory["passed"]) and (not require_native or native == "native")
    if not passed:
        sys.stderr.write(completed.stdout + completed.stderr)
    return passed, native, memory, completed.stdout + completed.stderr


def parse_native_output(stdout: str) -> dict[str, object] | None:
    devices = NATIVE_DEVICE_EVIDENCE.findall(stdout)
    shaders = sorted(set(NATIVE_SHADER_EVIDENCE.findall(stdout)))
    readbacks = NATIVE_DEFERRED_EVIDENCE.findall(stdout)
    if len(devices) != 1 or not shaders or len(readbacks) != 1:
        return None
    identity, encoded_name, capability = devices[0]
    try:
        name = bytes.fromhex(encoded_name).decode("utf-8")
    except (ValueError, UnicodeDecodeError):
        return None
    if not identity or not name:
        return None
    return {
        "device": {
            "identity": identity[:128],
            "name": name[:256],
            "capabilityDigest": capability,
        },
        "shaderEvidenceDigests": shaders,
        "readbackDigest": readbacks[0],
    }


def parse_native_device_output(stdout: str) -> dict[str, str] | None:
    devices = NATIVE_DEVICE_EVIDENCE.findall(stdout)
    if len(devices) != 1:
        return None
    identity, encoded_name, capability = devices[0]
    try:
        name = bytes.fromhex(encoded_name).decode("utf-8")
    except (ValueError, UnicodeDecodeError):
        return None
    if not identity or not name:
        return None
    return {
        "identity": identity[:128],
        "name": name[:256],
        "capabilityDigest": capability,
    }


def run_presentation_smoke(
    tests: Path, probe: Path, root: Path, work: Path, frames: int,
    cycles: int, timeout: int,
) -> tuple[bool, dict[str, str] | None, int, int, str, Path]:
    native = run_command(
        [str(tests), "--suite", "metal-native", "--metal-native"],
        root, timeout,
    )
    contracts = run_command(
        [str(tests), "--suite", "metal-presentation",
         "--suite", "metal-presentation-visible", "--metal-visible"],
        root, timeout,
    )
    work.mkdir(parents=True, exist_ok=True)
    raw_report = work / "presentation-probe.json"
    presented = run_command(
        [str(probe), "--frames", str(frames), "--cycles", str(cycles),
         "--report", str(raw_report)],
        root, timeout,
    )
    try:
        document = json.loads(raw_report.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError):
        document = {}
    actual_frames = document.get("presentedFrames", 0)
    actual_cycles = document.get("completedLifecycleCycles", 0)
    clean = all(document.get(field) is True for field in (
        "layerDetached", "deviceShutdown", "windowDestroyed",
        "ownershipClean",
    ))
    combined = "".join((
        native.stdout, native.stderr, contracts.stdout, contracts.stderr,
        presented.stdout, presented.stderr,
    ))
    device = parse_native_device_output(native.stdout + native.stderr)
    passed = native.returncode == 0 and contracts.returncode == 0 and \
        presented.returncode == 0 and device is not None and clean and \
        document.get("result") == "passed" and actual_frames >= frames and \
        actual_cycles >= cycles
    if not passed:
        sys.stderr.write(combined)
    return (
        passed, device,
        actual_frames if isinstance(actual_frames, int) else 0,
        actual_cycles if isinstance(actual_cycles, int) else 0,
        combined, raw_report,
    )


def run_native(
    tests: Path, root: Path, timeout: int, require_native: bool,
    comparison: bool = False,
) -> tuple[bool, bool, dict[str, object] | None, str]:
    native_command = [str(tests), "--suite", "metal-native"]
    if require_native:
        native_command.append("--metal-native")
    native = run_command(native_command, root, timeout)
    environment = {"STONER_REQUIRE_METAL_DEFERRED": "1"} if require_native else {}
    if comparison:
        environment["STONER_REQUIRE_DEFERRED_NATIVE"] = "1"
    suites = [str(tests), "--suite", "deferred-native"]
    if comparison:
        suites += ["--suite", "metal-backend-comparison"]
    deferred = run_command(suites, root, timeout, environment)
    combined = native.stdout + native.stderr + deferred.stdout + deferred.stderr
    evidence = parse_native_output(combined)
    available = evidence is not None
    passed = native.returncode == 0 and deferred.returncode == 0 and available
    if not passed and (require_native or native.returncode != 0 or deferred.returncode != 0):
        sys.stderr.write(combined)
    return passed, available, evidence, combined


def parse_demo_report(text: str) -> tuple[bool, int, int]:
    fields: dict[str, str] = {}
    for line in text.splitlines():
        key, separator, value = line.partition("=")
        if separator and key and key not in fields:
            fields[key] = value
    try:
        frames = int(fields.get("completed-frames", "-1"))
        recoveries = int(fields.get("recovery-count", "-1"))
        live_objects = int(fields.get("final-live-objects", "-1"))
    except ValueError:
        return False, 0, 0
    passed = (
        fields.get("runtime-object-mode") == "native" and
        fields.get("validation-result") == "pass" and
        frames >= 3000 and recoveries >= 20 and live_objects == 0
    )
    return passed, frames, recoveries


def run_visible(
    demo: Path, tests: Path, root: Path, work: Path, timeout: int,
    profile: Path, publication: Path, lease: Path, frame_budget: int,
    lifecycle_cycles: int,
) -> tuple[bool, dict[str, object] | None, int, int, str, Path]:
    native_passed, _, evidence, native_log = run_native(
        tests, root, timeout, True
    )
    demo_report = work / "visible-demo.txt"
    completed = run_command(
        [
            str(demo), "--backend", "metal", "--mode", "validate",
            "--frames", str(frame_budget), "--warmup-frames", "1000",
            "--memory-sample-interval", "100",
            "--lifecycle-cycles", str(lifecycle_cycles),
            "--cooked-root", str(publication),
            "--lease-root", str(lease),
            "--target-profile", str(profile),
            "--validation-output", str(demo_report),
        ],
        root,
        timeout,
    )
    try:
        report_text = demo_report.read_text(encoding="utf-8")
    except (OSError, UnicodeError):
        report_text = ""
    visible_passed, frames, recoveries = parse_demo_report(report_text)
    passed = native_passed and completed.returncode == 0 and visible_passed
    combined = native_log + completed.stdout + completed.stderr + report_text
    if not passed:
        sys.stderr.write(combined)
    return passed, evidence, frames, recoveries, combined, demo_report


def write_log(
    root: Path, work: Path, name: str, content: str,
) -> dict[str, str] | None:
    try:
        work.mkdir(parents=True, exist_ok=True)
        path = work / name
        path.write_text(content, encoding="utf-8", newline="\n")
        relative = path.resolve().relative_to(root.resolve())
    except (OSError, UnicodeError, ValueError):
        return None
    return {"path": relative.as_posix(), "digest": sha256_file(path)}


def write_report(path: Path, document: dict[str, object]) -> list[str]:
    path.parent.mkdir(parents=True, exist_ok=True)
    finalize_report(document)
    errors = validate_report(document)
    path.write_text(
        json.dumps(document, indent=2, sort_keys=True) + "\n",
        encoding="utf-8", newline="\n",
    )
    return errors


def main(argv: Sequence[str] | None = None) -> int:
    args = parse_args(argv)
    root = args.root.resolve()
    output = args.output if args.output.is_absolute() else root / args.output
    tests = args.tests or Path("Build/Mac/Debug/Tests/StonerTest")
    tests = tests if tests.is_absolute() else root / tests
    demo = None if args.demo is None else (
        args.demo if args.demo.is_absolute() else root / args.demo
    )
    presentation_probe = None if args.presentation_probe is None else (
        args.presentation_probe if args.presentation_probe.is_absolute()
        else root / args.presentation_probe
    )
    profile = None if args.profile is None else (
        args.profile if args.profile.is_absolute() else root / args.profile
    )
    publication = None if args.publication is None else (
        args.publication if args.publication.is_absolute()
        else root / args.publication
    )
    lease = None if args.lease is None else (
        args.lease if args.lease.is_absolute() else root / args.lease
    )
    capture = None if args.capture is None else (
        args.capture if args.capture.is_absolute() else root / args.capture
    )
    if profile is not None and not profile.is_file():
        print(f"target profile does not exist: {profile}", file=sys.stderr)
        return 2
    if publication is not None and not (publication / "Current.json").is_file():
        print(f"published generation does not exist: {publication}", file=sys.stderr)
        return 2
    if lease is not None:
        try:
            lease.mkdir(parents=True, exist_ok=True)
        except OSError as error:
            print(f"lease root is unavailable: {error}", file=sys.stderr)
            return 2
    workload_name = {
        "derivation": "metal-shader-derivation",
        "failure": "metal-failure-determinism",
        "lifecycle": "metal-lifecycle-stress",
        "native": "metal-native-conformance",
        "comparison": "metal-vulkan-comparison",
        "visible": "metal-visible-presentation",
        "presentation-smoke": "metal-presentation-smoke",
    }[args.workload]
    report = base_report(root, workload_name, args.tier)
    if profile is not None:
        report["artifacts"].append({
            "path": profile.resolve().relative_to(root).as_posix(),
            "digest": sha256_file(profile),
        })
    if publication is not None:
        current = publication / "Current.json"
        report["artifacts"].append({
            "path": current.resolve().relative_to(root).as_posix(),
            "digest": sha256_file(current),
        })
    if args.prepare_only:
        report["failureCategory"] = "not-run"
        errors = write_report(output, report)
        if errors:
            print("; ".join(errors), file=sys.stderr)
            return 2
        return 0
    if not tests.is_file():
        report["result"] = "failed"
        report["failureCategory"] = "test-binary-missing"
        errors = write_report(output, report)
        print(f"test binary does not exist: {tests}", file=sys.stderr)
        return 2 if errors else 1
    if args.workload == "visible" and (demo is None or not demo.is_file()):
        report["result"] = "failed"
        report["failureCategory"] = "demo-binary-missing"
        errors = write_report(output, report)
        print(f"demo binary does not exist: {demo}", file=sys.stderr)
        return 2 if errors else 1
    if args.workload == "presentation-smoke" and (
        presentation_probe is None or not presentation_probe.is_file()
    ):
        report["result"] = "failed"
        report["failureCategory"] = "presentation-probe-missing"
        errors = write_report(output, report)
        print(
            f"presentation probe does not exist: {presentation_probe}",
            file=sys.stderr,
        )
        return 2 if errors else 1

    temporary: tempfile.TemporaryDirectory[str] | None = None
    if args.work is None:
        temporary = tempfile.TemporaryDirectory(prefix="stoner-metal-validation-")
        work = Path(temporary.name)
    else:
        work = args.work if args.work.is_absolute() else root / args.work

    passed = False
    unavailable_is_success = False
    log = ""
    if args.workload == "derivation":
        passed, evidence = run_derivation(
            tests, root, args.repetitions, args.timeout_seconds
        )
        report["counts"]["iterations"] = args.repetitions
        report["probes"] = [{
            "name": "deterministic-msl-derivation",
            "result": "passed" if passed else "failed",
            "evidenceDigest": hashlib.sha256(
                "\n".join(evidence).encode("utf-8")
            ).hexdigest(),
        }]
    elif args.workload == "failure":
        passed, evidence = run_failure(
            tests, root, args.repetitions, args.timeout_seconds
        )
        report["counts"]["iterations"] = args.repetitions
        report["probes"] = [{
            "name": "failure-injection-determinism",
            "result": "passed" if passed else "failed",
            "evidenceDigest": hashlib.sha256(
                "\n".join(evidence).encode("utf-8")
            ).hexdigest(),
        }]
        report["ownership"] = {
            "device": 0, "objects": 0, "submissions": 0, "inFlight": 0,
        }
    elif args.workload == "lifecycle":
        native_evidence = None
        if args.require_native:
            native_passed, _, native_evidence, native_log = run_native(
                tests, root, args.timeout_seconds, True
            )
            log += native_log
        else:
            native_passed = True
        passed, native, memory, lifecycle_log = run_lifecycle(
            tests, root, args.lifecycle_iterations, args.timeout_seconds,
            args.require_native,
        )
        passed = passed and native_passed
        log += lifecycle_log
        report["counts"]["iterations"] = args.lifecycle_iterations
        report["counts"]["lifecycleCycles"] = args.lifecycle_iterations
        report["probes"] = [{
            "name": "native-rhi-lifecycle" if native == "native"
                else "shared-ownership-lifecycle",
            "result": "passed" if passed and native == "native" else (
                "unsupported" if native == "unavailable" else "failed"
            ),
        }]
        if memory is not None:
            report["memory"] = memory
        report["ownership"] = {
            "device": 0, "resources": 0, "pipelines": 0,
            "commands": 0, "submissions": 0, "inFlight": 0,
        }
        if native_evidence:
            report["device"] = native_evidence["device"]
            report["shaderEvidenceDigests"] = native_evidence[
                "shaderEvidenceDigests"
            ]
        unavailable_is_success = native == "unavailable" and passed
    elif args.workload in {"native", "comparison"}:
        comparison = args.workload == "comparison"
        required = args.require_native or comparison
        passed, available, evidence, log = run_native(
            tests, root, args.timeout_seconds, required, comparison
        )
        report["probes"] = [{
            "name": "metal-vulkan-native-comparison" if comparison
                else "native-metal-conformance",
            "result": "passed" if passed else (
                "unsupported" if not available else "failed"
            ),
            **({"tolerance": "metal-vulkan-tolerance-v1"} if comparison else {}),
            **({"evidenceDigest": evidence["readbackDigest"]} if evidence else {}),
        }]
        if evidence:
            report["device"] = evidence["device"]
            report["shaderEvidenceDigests"] = evidence[
                "shaderEvidenceDigests"
            ]
        unavailable_is_success = not available and not required and \
            platform.system() == "Darwin"
    elif args.workload == "visible":
        assert demo is not None and profile is not None and \
            publication is not None and lease is not None and capture is not None
        capture_before = capture.stat().st_mtime_ns if capture.is_file() else None
        capture_started = time.time_ns()
        passed, evidence, frames, recoveries, log, demo_report = run_visible(
            demo, tests, root, work, args.timeout_seconds,
            profile, publication, lease, args.visible_frames,
            args.visible_cycles,
        )
        capture_current = capture.stat().st_mtime_ns if capture.is_file() else None
        capture_fresh = capture_current is not None and (
            capture_current >= capture_started
            if capture_before is None
            else capture_current > capture_before
        )
        if not capture_fresh:
            passed = False
            log += "accepted visible PNG was not created during this run\n"
        else:
            report["artifacts"].append({
                "path": capture.resolve().relative_to(root).as_posix(),
                "digest": sha256_file(capture),
            })
        report["counts"]["frames"] = frames
        report["counts"]["lifecycleCycles"] = recoveries
        report["probes"] = [{
            "name": "visible-metal-presentation-lifecycle",
            "result": "passed" if passed else "failed",
            **({
                "evidenceDigest": sha256_file(demo_report),
            } if demo_report.is_file() else {}),
        }]
        if evidence:
            report["device"] = evidence["device"]
            report["shaderEvidenceDigests"] = evidence[
                "shaderEvidenceDigests"
            ]
        if demo_report.is_file():
            relative = demo_report.resolve().relative_to(root)
            report["artifacts"].append({
                "path": relative.as_posix(),
                "digest": sha256_file(demo_report),
            })
    else:
        assert presentation_probe is not None
        passed, device, frames, cycles, log, raw_report = \
            run_presentation_smoke(
                tests, presentation_probe, root, work,
                args.smoke_frames, args.smoke_cycles, args.timeout_seconds,
            )
        report["counts"]["frames"] = frames
        report["counts"]["lifecycleCycles"] = cycles
        report["counts"]["iterations"] = 1
        report["probes"] = [{
            "name": "metal-presentation-layer-lifecycle",
            "result": "passed" if passed else "failed",
            **({"evidenceDigest": sha256_file(raw_report)}
               if raw_report.is_file() else {}),
        }]
        if device:
            report["device"] = device
        report["ownership"] = {
            "device": 0, "objects": 0, "presentation": 0, "inFlight": 0,
        }
        if raw_report.is_file():
            relative = raw_report.resolve().relative_to(root)
            report["artifacts"].append({
                "path": relative.as_posix(),
                "digest": sha256_file(raw_report),
            })

    if log and args.work is not None:
        artifact = write_log(root, work, f"{args.workload}.log", log)
        if artifact:
            report["artifacts"].append(artifact)
    if passed:
        report["result"] = "passed"
    elif unavailable_is_success:
        report["result"] = "unavailable"
    else:
        report["result"] = "failed"
        report["failureCategory"] = "validation-failed"
    errors = write_report(output, report)
    if temporary is not None:
        temporary.cleanup()
    if errors:
        print("; ".join(errors), file=sys.stderr)
        return 2
    return 0 if passed or unavailable_is_success else 1


if __name__ == "__main__":
    raise SystemExit(main())
