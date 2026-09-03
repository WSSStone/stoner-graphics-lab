#!/usr/bin/env python3
"""Bounded cross-process production image calibration orchestration."""

from __future__ import annotations

import argparse
import binascii
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import struct
import subprocess
from itertools import combinations
import zlib


CAPTURES_PER_PROCESS = 20
MIN_PROCESSES = 3
MAX_PROCESSES = 6
MAX_MODES = 3
FRAME_TOKEN = re.compile(r"^submission-([1-9][0-9]*)$")
REQUIRED_MUTATIONS = (
    "blank", "stale", "origin", "translation-one-pixel",
    "missing-geometry", "material-swap", "color-space", "opposite-normal",
)
GPU_MUTATIONS = REQUIRED_MUTATIONS


def verify_v3_revision(repository_root: Path, revision: str | None) -> None:
    spec = importlib.util.spec_from_file_location(
        "calibration_output_provenance", Path(__file__).with_name("output_transform_provenance.py"))
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    module.require_frozen_revision(repository_root, revision)


def remove_calibration_tree(path: Path, ignore_errors: bool = False) -> None:
    resolved = path.resolve()
    native_path = Path("\\\\?\\" + str(resolved)) if os.name == "nt" else resolved
    shutil.rmtree(native_path, ignore_errors=ignore_errors)


def _ppm_rgb(path: Path) -> tuple[int, int, bytes, str]:
    payload = path.read_bytes()
    parts = payload.split(b"\n", 3)
    if len(parts) != 4 or parts[0] != b"P6" or parts[2] != b"255":
        raise ValueError("capture is not canonical binary PPM")
    dimensions = parts[1].split()
    if len(dimensions) != 2:
        raise ValueError("capture dimensions are invalid")
    width, height = (int(value) for value in dimensions)
    rgb = parts[3]
    if width != 512 or height != 512 or len(rgb) != width * height * 3:
        raise ValueError("capture is not the formal 512-by-512 RGB extent")
    return width, height, rgb, hashlib.sha256(payload).hexdigest()


def collect_process(
    root: Path, backend: str, ordinal: int, workload_revision: str | None = None,
) -> dict:
    capture_root = root / backend
    frame_tokens = []
    decoded_digests = []
    representative = None
    for index in range(CAPTURES_PER_PROCESS):
        stem = f"capture-{index:02d}"
        image_path = capture_root / f"{stem}.ppm"
        metadata_path = capture_root / f"{stem}.json"
        if not image_path.is_file() or not metadata_path.is_file():
            raise ValueError("independent process capture set is incomplete")
        width, height, rgb, file_digest = _ppm_rgb(image_path)
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        if (
            metadata.get("width") != width
            or metadata.get("height") != height
            or metadata.get("backend") != backend
            or (workload_revision is not None and
                metadata.get("workloadRevision") != workload_revision)
            or metadata.get("captureScope") != "application-window"
            or metadata.get("sha256") != file_digest
            or metadata.get("frameToken") != metadata.get("expectedFrameToken")
        ):
            raise ValueError("capture metadata differs from decoded evidence")
        matched = FRAME_TOKEN.fullmatch(str(metadata.get("frameToken", "")))
        if matched is None:
            raise ValueError("capture frame token is not submission-derived")
        frame_tokens.append(int(matched.group(1)))
        decoded_digests.append(hashlib.sha256(rgb).hexdigest())
        if representative is None:
            representative = rgb
    if frame_tokens != sorted(set(frame_tokens)):
        raise ValueError("capture frame tokens are stale or non-monotonic")
    if len(set(decoded_digests)) != 1:
        raise ValueError("one native process is not internally stable")
    return {
        "ordinal": ordinal,
        "captureRoot": root,
        "decodedPixelSha256": decoded_digests[0],
        "firstFrameToken": frame_tokens[0],
        "lastFrameToken": frame_tokens[-1],
        "staleFrameMutation": {
            "expectedFrameToken": frame_tokens[-1],
            "observedFrameToken": frame_tokens[0],
            "rejected": frame_tokens[0] != frame_tokens[-1],
        },
        "rgb": representative,
    }


def group_modes(processes: list[dict]) -> dict[str, list[dict]]:
    modes: dict[str, list[dict]] = {}
    for process in processes:
        modes.setdefault(process["decodedPixelSha256"], []).append(process)
    if len(modes) > MAX_MODES:
        raise ValueError("cross-process calibration observed more than three modes")
    return modes


def calibration_complete(processes: list[dict]) -> bool:
    modes = group_modes(processes)
    return all(len(members) >= 2 for members in modes.values())


def _png_chunk(kind: bytes, payload: bytes) -> bytes:
    checksum = binascii.crc32(kind)
    checksum = binascii.crc32(payload, checksum) & 0xFFFFFFFF
    return struct.pack(">I", len(payload)) + kind + payload + struct.pack(">I", checksum)


def write_png(path: Path, rgb: bytes) -> str:
    rows = b"".join(
        b"\x00" + rgb[offset:offset + 512 * 3]
        for offset in range(0, len(rgb), 512 * 3)
    )
    payload = (
        b"\x89PNG\r\n\x1a\n"
        + _png_chunk(b"IHDR", struct.pack(">IIBBBBB", 512, 512, 8, 2, 0, 0, 0))
        + _png_chunk(b"IDAT", zlib.compress(rows, level=9))
        + _png_chunk(b"IEND", b"")
    )
    path.write_bytes(payload)
    if decode_written_png(path) != rgb:
        raise ValueError("lossless PNG decoded pixels differ from the candidate")
    return hashlib.sha256(payload).hexdigest()


def decode_written_png(path: Path) -> bytes:
    payload = path.read_bytes()
    if not payload.startswith(b"\x89PNG\r\n\x1a\n"):
        raise ValueError("candidate PNG signature is invalid")
    offset = 8
    compressed = bytearray()
    while offset < len(payload):
        length = struct.unpack(">I", payload[offset:offset + 4])[0]
        kind = payload[offset + 4:offset + 8]
        data = payload[offset + 8:offset + 8 + length]
        offset += 12 + length
        if kind == b"IDAT":
            compressed.extend(data)
        if kind == b"IEND":
            break
    rows = zlib.decompress(bytes(compressed))
    stride = 512 * 3
    if len(rows) != (stride + 1) * 512:
        raise ValueError("candidate PNG payload is invalid")
    rgb = bytearray()
    for row in range(512):
        start = row * (stride + 1)
        if rows[start] != 0:
            raise ValueError("candidate PNG uses an unexpected filter")
        rgb.extend(rows[start + 1:start + 1 + stride])
    return bytes(rgb)


def _run(
    command: list[str], cwd: Path, environment: dict[str, str],
) -> subprocess.CompletedProcess[str]:
    result = subprocess.run(
        command, cwd=cwd, env={**os.environ, **environment},
        check=False, capture_output=True, text=True,
    )
    if result.returncode != 0:
        raise RuntimeError(
            "calibration child failed: "
            + (result.stderr[-2000:] or result.stdout[-2000:])
        )
    return result


def parse_calibration_output(output: str) -> dict:
    prefix = "[CALIBRATION] "
    records = [
        line[len(prefix):] for line in output.splitlines()
        if line.startswith(prefix)
    ]
    if len(records) != 1:
        raise ValueError("calibration FLIP evidence count is invalid")
    value = json.loads(records[0])
    noise = value.get("noise") if isinstance(value, dict) else None
    policy = value.get("policy") if isinstance(value, dict) else None
    metric_fields = {"mean", "p95", "maximum", "badPixelFraction"}
    policy_fields = {
        "meanMax", "p95Max", "maximumMax", "badPixelThreshold",
        "badPixelFractionMax",
    }
    if (
        not isinstance(noise, dict)
        or set(noise) != metric_fields
        or any(not isinstance(metric, (int, float)) or metric < 0.0
               or metric > 1.0 for metric in noise.values())
        or not isinstance(policy, dict)
        or set(policy) != policy_fields
        or any(not isinstance(metric, (int, float)) or metric < 0.0
               or metric > 1.0 for metric in policy.values())
        or policy["badPixelThreshold"] <= 0.0
    ):
        raise ValueError("calibration FLIP evidence is invalid")
    return {"noise": noise, "policy": policy}


def parse_mutation_output(output: str) -> list[str]:
    records = {}
    pattern = re.compile(
        r"^\[MUTATION\] name=([a-z0-9-]+) result=(rejected|accepted)(?: |$)"
    )
    for line in output.splitlines():
        matched = pattern.match(line)
        if matched is None:
            continue
        name, result = matched.groups()
        if name in records:
            raise ValueError("mutation evidence is duplicated")
        records[name] = result
    if set(records) != set(GPU_MUTATIONS):
        raise ValueError("mutation evidence is incomplete")
    if any(result != "rejected" for result in records.values()):
        raise ValueError("candidate reference set accepted a mutation")
    return list(GPU_MUTATIONS)


def measure_pairwise_mode_flip(
    repository_root: Path,
    process_root: Path,
    backend: str,
    mutation_command: list[str],
    modes: dict[str, list[dict]],
) -> list[dict]:
    records = []
    ordered = sorted(modes.items())
    for pair_index, ((first_digest, first_members),
                     (second_digest, second_members)) in enumerate(
                         combinations(ordered, 2), start=1):
        root = process_root / f"pair-{pair_index:02d}"
        capture_root = root / backend
        capture_root.mkdir(parents=True)
        sources = [
            first_members[0]["captureRoot"] / backend / "capture-00.ppm",
            second_members[0]["captureRoot"] / backend / "capture-00.ppm",
        ]
        for index in range(CAPTURES_PER_PROCESS):
            shutil.copy2(
                sources[index % 2],
                capture_root / f"capture-{index:02d}.ppm",
            )
        result = _run(mutation_command, repository_root, {
            "STONER_PRODUCTION_CALIBRATION_ROOT": str(root),
            "STONER_PRODUCTION_CALIBRATION_BACKEND": backend,
            "STONER_PRODUCTION_CALIBRATION_COMPARE_ONLY": "1",
        })
        records.append({
            "firstDecodedPixelSha256": first_digest,
            "secondDecodedPixelSha256": second_digest,
            "flip": parse_calibration_output(result.stdout)["noise"],
        })
    return records


def measure_within_mode_calibration(
    repository_root: Path,
    process_root: Path,
    backend: str,
    mutation_command: list[str],
    modes: dict[str, list[dict]],
) -> dict[str, dict]:
    records = {}
    for mode_index, (digest, members) in enumerate(
        sorted(modes.items()), start=1
    ):
        root = process_root / f"mode-{mode_index:02d}"
        capture_root = root / backend
        capture_root.mkdir(parents=True)
        for index in range(CAPTURES_PER_PROCESS):
            member = members[index % len(members)]
            shutil.copy2(
                member["captureRoot"] / backend / "capture-00.ppm",
                capture_root / f"capture-{index:02d}.ppm",
            )
        result = _run(mutation_command, repository_root, {
            "STONER_PRODUCTION_CALIBRATION_ROOT": str(root),
            "STONER_PRODUCTION_CALIBRATION_BACKEND": backend,
            "STONER_PRODUCTION_CALIBRATION_COMPARE_ONLY": "1",
        })
        records[digest] = parse_calibration_output(result.stdout)
    return records


def calibration_evidence_digest(value: dict) -> str:
    return hashlib.sha256(
        json.dumps(
            value, sort_keys=True, separators=(",", ":"),
        ).encode("utf-8")
    ).hexdigest()


def run_calibration(
    repository_root: Path,
    output: Path,
    backend: str,
    workload_revision: str,
    native_command: list[str],
    mutation_command: list[str],
    git_revision: str | None = None,
) -> dict:
    is_v3 = workload_revision.endswith("-v3")
    if is_v3:
        verify_v3_revision(repository_root, git_revision)
    if output.exists() and any(output.iterdir()):
        raise FileExistsError("calibration output must be empty")
    output.mkdir(parents=True, exist_ok=True)
    process_root = output / "processes"
    process_root.mkdir()
    processes = []
    target_count = MIN_PROCESSES
    while len(processes) < target_count:
        ordinal = len(processes) + 1
        root = process_root / f"process-{ordinal:02d}"
        environment = {
            "STONER_PRODUCTION_CAPTURE_ROOT": str(root),
            "STONER_PRODUCTION_LIFECYCLE_CYCLES": "20",
            "STONER_PRODUCTION_WARMUP_CYCLES": "2",
            "STONER_PRODUCTION_IMAGE_ACCEPTANCE_REQUIRED": "0",
            "STONER_PRODUCTION_WORKLOAD_REVISION": workload_revision,
        }
        _run(native_command, repository_root, environment)
        processes.append(collect_process(
            root, backend, ordinal, workload_revision,
        ))
        if len(processes) >= MIN_PROCESSES and not calibration_complete(processes):
            target_count = min(MAX_PROCESSES, target_count + 1)
    if not calibration_complete(processes):
        raise ValueError("not every candidate mode repeated in two processes")
    modes = group_modes(processes)
    mode_calibrations = measure_within_mode_calibration(
        repository_root, process_root, backend, mutation_command, modes
    )
    pairwise_flip = measure_pairwise_mode_flip(
        repository_root, process_root, backend, mutation_command, modes
    )
    mode_records = []
    reference_roots = ";".join(
        str(members[0]["captureRoot"])
        for _digest, members in sorted(modes.items())
    )
    if not all(
        process["staleFrameMutation"]["rejected"]
        for process in processes
    ):
        raise ValueError("real stale frame mutation was not rejected")
    for index, (digest, members) in enumerate(sorted(modes.items()), start=1):
        mutation_result = _run(mutation_command, repository_root, {
            "STONER_PRODUCTION_CALIBRATION_ROOT": str(members[0]["captureRoot"]),
            "STONER_PRODUCTION_CALIBRATION_BACKEND": backend,
            "STONER_PRODUCTION_CALIBRATION_REFERENCE_ROOTS": reference_roots,
            "STONER_PRODUCTION_WORKLOAD_REVISION": workload_revision,
        })
        rejected = set(parse_mutation_output(mutation_result.stdout))
        png_path = output / f"candidate-mode-{index}.png"
        png_digest = write_png(png_path, members[0]["rgb"])
        calibration_evidence = {
            "capturesPerProcess": CAPTURES_PER_PROCESS,
            "decodedPixelSha256": digest,
            "noise": mode_calibrations[digest]["noise"],
            "processOrdinals": [item["ordinal"] for item in members],
        }
        mode_records.append({
            "modeId": f"mode-{index}",
            "decodedPixelSha256": digest,
            "processOrdinals": [item["ordinal"] for item in members],
            "candidatePng": png_path.name,
            "candidatePngSha256": png_digest,
            "flipPolicy": mode_calibrations[digest]["policy"],
            "calibrationEvidenceSha256": calibration_evidence_digest(
                calibration_evidence
            ),
            "crossProcessNoise": mode_calibrations[digest]["noise"],
            "mutationsRejected": [
                mutation for mutation in REQUIRED_MUTATIONS
                if mutation in rejected
            ],
        })
    summary = {
        "schema": "stoner.production-cross-process-calibration",
        "schemaVersion": 1,
        "backend": backend,
        "workloadRevision": workload_revision,
        "processCount": len(processes),
        "capturesPerProcess": CAPTURES_PER_PROCESS,
        "processes": [{
            key: value for key, value in process.items()
            if key not in ("captureRoot", "rgb")
        } for process in processes],
        "modes": mode_records,
        "pairwiseModeFlip": pairwise_flip,
        "candidateOnly": True,
    }
    if is_v3:
        verify_v3_revision(repository_root, git_revision)
        summary["gitRevision"] = git_revision
    (output / "calibration.json").write_text(
        json.dumps(summary, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    remove_calibration_tree(process_root)
    return summary


def main(values: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--backend", choices=("vulkan", "metal"), required=True)
    parser.add_argument(
        "--workload-revision",
        choices=(
            "production-content-lantern-v2",
            "production-content-sponza-v2",
            "production-content-lantern-v3",
            "production-content-sponza-v3",
        ),
        required=True,
    )
    parser.add_argument("--command-file", type=Path, required=True)
    parser.add_argument("--git-revision", help="Required frozen software commit for v3; v2 is unchanged")
    args = parser.parse_args(values)
    commands = json.loads(args.command_file.read_text(encoding="utf-8"))
    if (
        not isinstance(commands, dict)
        or set(commands) != {"nativeCommand", "mutationCommand"}
        or any(not isinstance(commands[name], list) or not commands[name]
               or any(not isinstance(item, str) or not item
                      for item in commands[name])
               for name in commands)
    ):
        raise ValueError("calibration command file is invalid")
    resolved_output = args.output.resolve()
    if resolved_output.exists() and any(resolved_output.iterdir()):
        raise FileExistsError("calibration output must be empty; existing evidence is preserved")
    try:
        run_calibration(
            args.root.resolve(), resolved_output, args.backend,
            args.workload_revision,
            commands["nativeCommand"], commands["mutationCommand"],
            args.git_revision,
        )
    finally:
        remove_calibration_tree(
            resolved_output / "processes", ignore_errors=True
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
