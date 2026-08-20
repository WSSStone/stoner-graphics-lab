#!/usr/bin/env python3
"""Run native Feature 027 triangle/deferred acceptance and write four reports."""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import re
from pathlib import Path
import subprocess
import sys
from typing import Sequence


SCRIPT_DIR = Path(__file__).resolve().parent
VALIDATION_SPEC = importlib.util.spec_from_file_location(
    "run_metal_validation", SCRIPT_DIR / "run_metal_validation.py"
)
assert VALIDATION_SPEC and VALIDATION_SPEC.loader
validation = importlib.util.module_from_spec(VALIDATION_SPEC)
VALIDATION_SPEC.loader.exec_module(validation)

TRIANGLE = {
    "metal": re.compile(
        r"^\[EVIDENCE\] metal-native-triangle status=passed "
        r"readback=([0-9a-f]{64})$", re.MULTILINE
    ),
    "vulkan": re.compile(
        r"^\[EVIDENCE\] vulkan-native-triangle status=passed "
        r"readback=([0-9a-f]{64})$", re.MULTILINE
    ),
}
DEFERRED = re.compile(
    r"^\[EVIDENCE\] metal-native-deferred status=passed "
    r"readback=([0-9a-f]{64})$", re.MULTILINE
)
COMPARISON = re.compile(
    r"^\[EVIDENCE\] metal-vulkan-deferred-comparison status=passed "
    r"tolerance=(metal-vulkan-tolerance-v1) digest=([0-9a-f]{64})$",
    re.MULTILINE,
)
PRODUCTION = re.compile(
    r"^\[EVIDENCE\] metal-production-cooked graphics=2 compute=1 "
    r"libraries=12((?: digest=[0-9a-f]{64}){12})$", re.MULTILINE
)


def parse_args(values: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--tests", type=Path, required=True)
    parser.add_argument("--demo", type=Path, required=True)
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--publication", type=Path, required=True)
    parser.add_argument("--lease", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--work", type=Path, required=True)
    parser.add_argument("--timeout-seconds", type=int, default=1800)
    args = parser.parse_args(values)
    if not 1 <= args.timeout_seconds <= 7200:
        parser.error("--timeout-seconds must be in [1, 7200]")
    return args


def resolve(root: Path, value: Path) -> Path:
    return value.resolve() if value.is_absolute() else (root / value).resolve()


def run(
    command: Sequence[str], root: Path, timeout: int,
    environment: dict[str, str] | None = None,
) -> subprocess.CompletedProcess[str]:
    completed = validation.run_command(command, root, timeout, environment)
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )
    return completed


def digest_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def shader_digests(root: Path) -> list[str]:
    paths = sorted((root / "Content/Shaders/Triangle").glob("*.spv"))
    paths += sorted((root / "Content/Shaders/Deferred").glob("*.spv"))
    return sorted({validation.sha256_file(path) for path in paths})


def write_report(
    root: Path, output: Path, workload: str, tier: str, backend: str,
    device: dict[str, str], shaders: list[str],
    probes: list[dict[str, str]], frames: int = 0,
) -> None:
    report = validation.base_report(root, workload, tier)
    report["backend"] = backend
    report["device"] = device
    report["shaderEvidenceDigests"] = sorted(set(shaders))
    report["counts"]["frames"] = frames
    report["counts"]["iterations"] = 1
    report["probes"] = probes
    report["result"] = "passed"
    errors = validation.write_report(output, report)
    if errors:
        raise ValueError(f"{output.name}: {'; '.join(errors)}")


def parse_device(output: str) -> dict[str, str]:
    matches = validation.NATIVE_DEVICE_EVIDENCE.findall(output)
    if len(matches) != 1:
        raise ValueError("exactly one native Metal device record is required")
    identity, encoded_name, capability = matches[0]
    return {
        "identity": identity[:128],
        "name": bytes.fromhex(encoded_name).decode("utf-8")[:256],
        "capabilityDigest": capability,
    }


def main(values: Sequence[str] | None = None) -> int:
    args = parse_args(values)
    root = args.root.resolve()
    tests = resolve(root, args.tests)
    demo = resolve(root, args.demo)
    profile = resolve(root, args.profile)
    publication = resolve(root, args.publication)
    lease = resolve(root, args.lease)
    output_dir = resolve(root, args.output_dir)
    work = resolve(root, args.work)
    for path in (tests, demo, profile, publication / "Current.json"):
        if not path.is_file():
            raise FileNotFoundError(path)
    output_dir.mkdir(parents=True, exist_ok=True)
    work.mkdir(parents=True, exist_ok=True)
    lease.mkdir(parents=True, exist_ok=True)

    strict = run([
        str(tests), "--suite", "metal-native", "--suite",
        "metal-shader-runtime", "--metal-native",
        "--metal-cooked-root", str(publication),
        "--metal-lease-root", str(lease),
        "--metal-target-profile", str(profile),
    ], root, args.timeout_seconds)
    device = parse_device(strict.stdout)
    production = PRODUCTION.search(strict.stdout)
    if not production:
        raise ValueError("strict production Metal library evidence is incomplete")
    production_digests = re.findall(
        r"digest=([0-9a-f]{64})", production.group(1)
    )

    def demo_command(backend: str, report: Path) -> list[str]:
        command = [
            str(demo), "--backend", backend, "--mode", "headless-vulkan",
            "--frames", "120", "--warmup-frames", "20",
            "--memory-sample-interval", "10",
            "--max-memory-growth-mib", "64",
            "--max-memory-growth-percent", "10",
            "--validation-output", str(report),
        ]
        if backend == "metal":
            command += [
                "--cooked-root", str(publication),
                "--lease-root", str(lease),
                "--target-profile", str(profile),
            ]
        return command

    metal_demo = run(
        demo_command("metal", work / "metal-triangle.txt"),
        root, args.timeout_seconds,
    )
    vulkan_demo = run(
        demo_command("vulkan", work / "vulkan-triangle.txt"),
        root, args.timeout_seconds,
    )
    metal_triangle = TRIANGLE["metal"].search(metal_demo.stdout)
    vulkan_triangle = TRIANGLE["vulkan"].search(vulkan_demo.stdout)
    if not metal_triangle or not vulkan_triangle:
        raise ValueError("independent native triangle readback evidence is incomplete")

    vulkan_raw = work / "vulkan-deferred.txt"
    deferred = run([
        str(tests), "--suite", "deferred-native", "--suite",
        "metal-backend-comparison",
    ], root, args.timeout_seconds, {
        "STONER_REQUIRE_METAL_DEFERRED": "1",
        "STONER_REQUIRE_DEFERRED_NATIVE": "1",
        "STONER_DEFERRED_NATIVE_SHADER_DIR": "Content/Shaders/Deferred",
        "STONER_DEFERRED_READBACK_REPORT": str(vulkan_raw),
    })
    metal_deferred = DEFERRED.search(deferred.stdout)
    comparison = COMPARISON.search(deferred.stdout)
    if not metal_deferred or not comparison or not vulkan_raw.is_file():
        raise ValueError("native deferred or comparison evidence is incomplete")
    vulkan_text = vulkan_raw.read_text(encoding="utf-8")
    if "native_submission=true" not in vulkan_text or "result=PASS" not in vulkan_text:
        raise ValueError("Vulkan deferred report did not pass native submission")
    adapter = next(
        (line.partition("=")[2] for line in vulkan_text.splitlines()
         if line.startswith("adapter=")), "",
    )
    if not adapter:
        raise ValueError("Vulkan adapter evidence is missing")
    vulkan_device = {
        "identity": f"vulkan:{digest_bytes(adapter.encode())[:24]}",
        "name": adapter[:256],
        "capabilityDigest": digest_bytes(vulkan_text.encode()),
    }
    metal_shader_digests = sorted(set(
        validation.NATIVE_SHADER_EVIDENCE.findall(deferred.stdout)
    ))
    if not metal_shader_digests:
        raise ValueError("Metal deferred shader evidence is missing")

    write_report(
        root, output_dir / "us4-metal-triangle.json",
        "metal-triangle", "native-offscreen", "metal", device,
        production_digests,
        [{"name": "strict-cooked-metal-triangle-readback", "result": "passed",
          "evidenceDigest": metal_triangle.group(1)}], frames=1,
    )
    write_report(
        root, output_dir / "us4-metal-deferred.json",
        "metal-deferred", "native-offscreen", "metal", device,
        metal_shader_digests,
        [{"name": "metal-deferred-attachment-readback", "result": "passed",
          "evidenceDigest": metal_deferred.group(1)}], frames=1,
    )
    vulkan_report_digest = digest_bytes(vulkan_text.encode())
    write_report(
        root, output_dir / "us4-vulkan-regression.json",
        "vulkan-triangle-deferred-regression", "native-offscreen", "vulkan",
        vulkan_device, shader_digests(root), [
            {"name": "vulkan-triangle-readback", "result": "passed",
             "evidenceDigest": vulkan_triangle.group(1)},
            {"name": "vulkan-deferred-attachment-readback", "result": "passed",
             "evidenceDigest": vulkan_report_digest},
        ], frames=1,
    )
    write_report(
        root, output_dir / "us4-metal-vulkan-comparison.json",
        "metal-vulkan-deferred-comparison", "cross-backend", "comparison",
        device, sorted(set(production_digests + metal_shader_digests)),
        [{"name": "native-deferred-gbuffer-comparison", "result": "passed",
          "tolerance": comparison.group(1),
          "evidenceDigest": comparison.group(2)}], frames=2,
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, ValueError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)
