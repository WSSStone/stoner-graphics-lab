#!/usr/bin/env python3
"""Repeat production Metal cooks and strict native pipeline creation."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import platform
import re
import shutil
import subprocess
import sys
from typing import Sequence


DEFAULT_ROOTS = (
    "ShaderProgram:Engine/Shaders/Triangle",
    "ShaderProgram:Engine/Shaders/Deferred/Surface",
    "ShaderProgram:Engine/Shaders/Deferred/Composition",
    "ShaderProgram:Engine/Shaders/Deferred/DirectionalLight",
    "ShaderProgram:Engine/Shaders/Deferred/PointLight",
    "ShaderProgram:Engine/Shaders/Deferred/SpotLight",
    "ShaderProgram:Engine/Shaders/PostProcess/OutputTransform",
    "ShaderProgram:Engine/Shaders/Validation/NoOp",
)
DEVICE = re.compile(
    r"\[EVIDENCE\] metal-native-device identity=(\S+) "
    r"name-utf8-hex=([0-9a-f]*) capability=([0-9a-f]{64})"
)
PIPELINE = re.compile(
    r"\[EVIDENCE\] metal-production-cooked graphics=(\d+) compute=(\d+)"
    r" libraries=(\d+)"
    r"((?: digest=[0-9a-f]{64})+)"
)


def sha256_bytes(value: bytes) -> str:
    return hashlib.sha256(value).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return sha256_bytes(path.read_bytes())


def run(
    command: Sequence[str], root: pathlib.Path, timeout: int
) -> subprocess.CompletedProcess[str]:
    completed = subprocess.run(
        list(command), cwd=root, capture_output=True, text=True,
        timeout=timeout, check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"command failed ({completed.returncode}): {' '.join(command)}\n"
            f"stdout:\n{completed.stdout}\nstderr:\n{completed.stderr}"
        )
    return completed


def tool_output(command: Sequence[str], root: pathlib.Path, timeout: int) -> str:
    return run(command, root, timeout).stdout.strip()


def parse_native_evidence(output: str) -> dict[str, object]:
    device = DEVICE.search(output)
    pipeline = PIPELINE.search(output)
    if not device or not pipeline:
        raise ValueError("native production pipeline evidence is incomplete")
    graphics, compute, libraries, digest_text = pipeline.groups()
    digests = re.findall(r"digest=([0-9a-f]{64})", digest_text)
    if (
        int(graphics) != 2
        or int(compute) != 1
        or int(libraries) != 14
        or len(digests) != 14
    ):
        raise ValueError("native production pipeline stage count is invalid")
    return {
        "device": {
            "identity": device.group(1),
            "name": bytes.fromhex(device.group(2)).decode("utf-8"),
            "capabilityDigest": device.group(3),
        },
        "graphicsModules": int(graphics),
        "computeModules": int(compute),
        "libraries": int(libraries),
        "libraryDigests": digests,
    }


def parse_args(values: Sequence[str] | None = None) -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=pathlib.Path, default=pathlib.Path("."))
    parser.add_argument("--cooker", type=pathlib.Path, required=True)
    parser.add_argument("--tests", type=pathlib.Path, required=True)
    parser.add_argument("--profile", type=pathlib.Path, required=True)
    parser.add_argument("--source", type=pathlib.Path, default=pathlib.Path("Content"))
    parser.add_argument("--work", type=pathlib.Path, required=True)
    parser.add_argument("--output", type=pathlib.Path, required=True)
    parser.add_argument("--asset-root", action="append", dest="asset_roots")
    parser.add_argument("--repetitions", type=int, default=20)
    parser.add_argument("--timeout-seconds", type=int, default=1800)
    args = parser.parse_args(values)
    if not 2 <= args.repetitions <= 20:
        parser.error("--repetitions must be in [2, 20]")
    if not 1 <= args.timeout_seconds <= 7200:
        parser.error("--timeout-seconds must be in [1, 7200]")
    if args.asset_roots is None:
        args.asset_roots = list(DEFAULT_ROOTS)
    if not args.asset_roots:
        parser.error("at least one --asset-root is required")
    return args


def resolve(root: pathlib.Path, value: pathlib.Path) -> pathlib.Path:
    return value.resolve() if value.is_absolute() else (root / value).resolve()


def validate_work_path(
    work: pathlib.Path, root: pathlib.Path, source: pathlib.Path
) -> None:
    filesystem_root = pathlib.Path(work.anchor)
    if work in {filesystem_root, root, source}:
        raise ValueError("--work must be a dedicated disposable directory")


def current_manifest(publication: pathlib.Path) -> tuple[dict, pathlib.Path]:
    current = json.loads((publication / "Current.json").read_bytes())
    manifest = publication / current["manifestLocator"]
    document = json.loads(manifest.read_bytes())
    if current["generationId"] != document["generationId"]:
        raise ValueError("Current.json and manifest generation differ")
    return document, manifest


def canonical_digest(value: object) -> str:
    return sha256_bytes(json.dumps(
        value, sort_keys=True, separators=(",", ":"), ensure_ascii=True,
    ).encode("utf-8"))


def main(values: Sequence[str] | None = None) -> int:
    args = parse_args(values)
    root = args.root.resolve()
    cooker = resolve(root, args.cooker)
    tests = resolve(root, args.tests)
    profile = resolve(root, args.profile)
    source = resolve(root, args.source)
    work = resolve(root, args.work)
    output = resolve(root, args.output)
    validate_work_path(work, root, source)
    for path in (cooker, tests, profile):
        if not path.is_file():
            raise FileNotFoundError(path)
    if not source.is_dir():
        raise FileNotFoundError(source)
    shutil.rmtree(work, ignore_errors=True)
    work.mkdir(parents=True)

    xcode = tool_output(["xcodebuild", "-version"], root, 60)
    sdk = tool_output(["xcrun", "--sdk", "macosx", "--show-sdk-version"], root, 60)
    metal = tool_output(["xcrun", "metal", "--version"], root, 60)
    tuple_record = {
        "architecture": platform.machine().lower(),
        "deploymentTarget": "12.0",
        "xcode": xcode,
        "sdk": sdk,
        "metalCompiler": metal,
        "profileDigest": sha256_file(profile),
        "assetRoots": sorted(args.asset_roots),
    }

    baselines: dict[str, object] | None = None
    runs: list[dict[str, object]] = []
    for index in range(args.repetitions):
        run_root = work / f"run-{index:02d}"
        publication = run_root / "Cooked"
        ddc = run_root / "DDC"
        lease = run_root / "Lease"
        cook_report = run_root / "cook.json"
        validate_report = run_root / "validate.json"
        command = [
            str(cooker), "cook", "--target-profile", str(profile),
            "--source-root", str(source),
        ]
        for asset_root in args.asset_roots:
            command.extend(["--root", asset_root])
        command.extend([
            "--output", str(publication), "--ddc", str(ddc),
            "--workers", "8", "--clean", "--normalized-report",
            "--report", str(cook_report),
        ])
        run(command, root, args.timeout_seconds)
        run([
            str(cooker), "validate", "--output", str(publication),
            "--strict-files", "--normalized-report", "--report",
            str(validate_report),
        ], root, args.timeout_seconds)
        native = run([
            str(tests), "--suite", "metal-native", "--suite",
            "metal-shader-runtime", "--metal-native",
            "--metal-cooked-root", str(publication),
            "--metal-lease-root", str(lease),
            "--metal-target-profile", str(profile),
        ], root, args.timeout_seconds)
        evidence = parse_native_evidence(native.stdout)
        manifest, manifest_path = current_manifest(publication)
        shader_records = [
            record for record in manifest["records"]
            if record["assetType"] == "ShaderPayload"
        ]
        run_record = {
            "generationId": manifest["generationId"],
            "cookReportDigest": sha256_file(cook_report),
            "validateReportDigest": sha256_file(validate_report),
            "manifestDigest": sha256_file(manifest_path),
            "derivedKeys": [record["derivedKey"] for record in shader_records],
            "envelopeDigests": [
                record["envelopeDigest"] for record in shader_records
            ],
            "libraryDigests": evidence["libraryDigests"],
            "pipelineOutcome": "graphics=passed;compute=passed",
            "device": evidence["device"],
        }
        comparable = {key: value for key, value in run_record.items()}
        if baselines is None:
            baselines = comparable
        elif comparable != baselines:
            raise ValueError(f"native cook run {index} differs from baseline")
        runs.append(run_record)

    assert baselines is not None
    revision = tool_output(["git", "rev-parse", "HEAD"], root, 30)
    evidence_digest = canonical_digest({
        "tuple": tuple_record,
        "baseline": baselines,
        "runs": args.repetitions,
    })
    document = {
        "schemaVersion": 1,
        "revision": revision,
        "tier": "native-offscreen",
        "workload": "metal-native-cook-determinism",
        "backend": "metal",
        "host": {"os": "macos", "architecture": tuple_record["architecture"]},
        "device": baselines["device"],
        "shaderEvidenceDigests": sorted(set(baselines["libraryDigests"])),
        "counts": {
            "frames": 0, "lifecycleCycles": 0,
            "iterations": args.repetitions,
        },
        "probes": [
            {
                "name": "production-cook-generation",
                "result": "passed",
                "evidenceDigest": baselines["generationId"],
            },
            {
                "name": "production-cook-normalized-reports",
                "result": "passed",
                "evidenceDigest": canonical_digest([
                    baselines["cookReportDigest"],
                    baselines["validateReportDigest"],
                ]),
            },
            {
                "name": "production-cook-ddc-keys",
                "result": "passed",
                "evidenceDigest": canonical_digest(baselines["derivedKeys"]),
            },
            {
                "name": "production-cook-envelopes",
                "result": "passed",
                "evidenceDigest": canonical_digest(
                    baselines["envelopeDigests"]
                ),
            },
            {
                "name": "production-cook-strict-load-graphics-compute",
                "result": "passed",
                "evidenceDigest": evidence_digest,
            },
        ],
        "artifacts": [{
            "path": profile.relative_to(root).as_posix(),
            "digest": sha256_file(profile),
        }],
        "result": "passed",
    }
    unsigned = json.dumps(
        document, sort_keys=True, separators=(",", ":"), ensure_ascii=True,
    ).encode("utf-8")
    document["reportDigest"] = sha256_bytes(unsigned)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        json.dumps(document, indent=2, sort_keys=True) + "\n",
        encoding="utf-8", newline="\n",
    )
    detail = work / "native-cook-detail.json"
    detail.write_text(json.dumps({
        "schema": "stoner.metal-native-cook-detail",
        "schemaVersion": 1,
        "tuple": tuple_record,
        "runs": runs,
        "evidenceDigest": evidence_digest,
    }, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    print(json.dumps(document, sort_keys=True))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (FileNotFoundError, RuntimeError, ValueError) as error:
        print(str(error), file=sys.stderr)
        raise SystemExit(1)
