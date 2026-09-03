#!/usr/bin/env python3
"""Exact-revision, bounded physical evidence linkage for Feature 029."""

from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path
import re
import subprocess
from typing import Any


GIT_RE = re.compile(r"^[0-9a-f]{40}$")
PHYSICAL_SDR = {
    "metal": ("macos", "macos.apple8.metal.rgba8"),
    "vulkan": ("windows", "windows.discrete-vulkan.rgba8"),
}
MUTATIONS = {"blank", "stale", "origin", "translation-one-pixel",
             "missing-geometry", "material-swap", "color-space", "opposite-normal"}
SCRIPT_DIR = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location(
    "provenance_png", SCRIPT_DIR / "compare_output_transform_images.py")
assert _spec is not None and _spec.loader is not None
PNG = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(PNG)
_verify_spec = importlib.util.spec_from_file_location(
    "provenance_json", SCRIPT_DIR / "verify_output_transform_evidence.py")
assert _verify_spec is not None and _verify_spec.loader is not None
VERIFY = importlib.util.module_from_spec(_verify_spec)
_verify_spec.loader.exec_module(VERIFY)


def canonical(value: Any) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":"),
                       ensure_ascii=False, allow_nan=False) + "\n").encode("utf-8")


def digest(value: Any) -> str:
    return hashlib.sha256(canonical(value)).hexdigest()


def require_frozen_revision(root: Path, revision: str) -> None:
    """Check the software checkout, not later evidence/documentation commits.

    Capture producers call this before and after execution. Evidence is saved
    afterward, still identifying this frozen software revision. This is not a
    proof of human review or of an untrusted caller's claimed execution.
    """
    if not isinstance(revision, str) or not GIT_RE.fullmatch(revision):
        raise ValueError("software revision must be exact 40 lowercase hex")
    head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=root,
                          check=True, capture_output=True, text=True).stdout.strip()
    if head != revision:
        raise ValueError("software revision must match the capture checkout HEAD")
    paths = ["Source", "Demo", "Tests", "Content", "Config", "Tools", "ThirdParty",
             "SConstruct", "site_scons", "SCons", "BuildScripts", ".github/scripts",
             ".github/workflows", ":(exclude)Tools/Tutorial/**",
             ":(exclude).github/workflows/tutorial-docs.yml",
             ":(exclude)Config/Validation/OutputTransform/SDR/Baselines-v3.json"]
    status = subprocess.run(
        ["git", "status", "--porcelain=v1", "--untracked-files=all", "--", *paths],
        cwd=root, check=True, capture_output=True, text=True).stdout
    if status.strip():
        raise ValueError("software inputs are dirty; commit and rebuild before formal capture")


def artifact(path: Path, root: Path) -> dict[str, Any]:
    resolved = path.resolve(strict=True)
    relative = resolved.relative_to(root.resolve()).as_posix()
    size = resolved.stat().st_size
    limit = 1024 * 1024 if resolved.suffix == ".json" else 64 * 1024 * 1024
    if size > limit or resolved.suffix not in {".json", ".png"}:
        raise ValueError("artifact exceeds bounded PNG/JSON policy")
    return {"path": relative, "sha256": hashlib.sha256(resolved.read_bytes()).hexdigest(),
            "sizeBytes": size}


def documents(report: dict, root: Path) -> dict[str, list[tuple[Path, Any]]]:
    """Called only after the ordinary artifact digest/path/bounds verifier."""
    result: dict[str, list[tuple[Path, Any]]] = {}
    for entry in report["artifacts"]:
        path = (root / entry["path"]).resolve(strict=True)
        path.relative_to(root.resolve())
        if path.suffix == ".json":
            if path.stat().st_size > 1024 * 1024:
                raise ValueError("JSON artifact exceeds 1 MiB")
            value, errors, _ = VERIFY.load_bounded_json(path)
            if errors:
                raise ValueError("; ".join(errors))
            if not isinstance(value, dict):
                raise ValueError("evidence document must be an object")
            result.setdefault(value.get("schema", "unknown"), []).append((path, value))
        else:
            result.setdefault("png", []).append((path, None))
    return result


def one(docs: dict, schema: str) -> tuple[Path, Any]:
    entries = docs.get(schema, [])
    if len(entries) != 1:
        raise ValueError(f"required exactly one {schema} artifact")
    return entries[0]


def linked_probe(report: dict, docs: dict) -> dict:
    _, probe = one(docs, "stoner.output-native-probe")
    for key in ("backend", "capabilityDigest", "deviceClass", "hostPlatform",
                "workloadRevision", "outputDeviceProfileId", "transformVersion",
                "exposureStops", "insertionDigest", "modeGeneration", "frameToken"):
        if probe.get(key) != report.get(key):
            raise ValueError(f"native probe/report {key} mismatch")
    if (probe.get("schemaVersion") != 2 or probe.get("status") != "passed" or
            probe.get("outstandingTerminalOwnerCount") != 0 or
            probe.get("firstFailure") is not None or
            {key: probe.get(key) for key in ("width", "height")} != report["extent"]):
        raise ValueError("native probe lacks completed exact-extent zero-owner provenance")
    for key in ("commandCompleted", "readbackCompleted", "presented"):
        if probe.get(key) is not True or report["native"].get(key) is not True:
            raise ValueError("native terminal operations must all complete")
    for key in ("readbackFrameToken", "presentationFrameToken"):
        if probe.get(key) != report["frameToken"]:
            raise ValueError("native probe frame identity mismatch")
    for key in ("format", "hdrMetadataDigest", "displayAdaptation"):
        if probe.get(key) != report["native"].get(key):
            raise ValueError(f"native probe/report {key} mismatch")
    if probe.get("settledFrameToken") != report["frameToken"]:
        raise ValueError("native settled frame mismatch")
    if not isinstance(probe.get("readbackDigest"), str) or not re.fullmatch(
            r"[0-9a-f]{64}", probe["readbackDigest"]):
        raise ValueError("native readback digest is missing")
    return probe


def validate_sdr_bundle(report: dict, record: dict, root: Path,
                        validate_baseline: Any) -> list[str]:
    try:
        docs = documents(report, root)
        if set(docs) != {"stoner.sdr-image-baseline", "png",
                        "stoner.production-cross-process-calibration",
                        "stoner.output-native-probe"} or len(report["artifacts"]) != 4:
            raise ValueError("required Candidate, PNG, calibration, and native probe artifacts")
        candidate_path, candidate = one(docs, "stoner.sdr-image-baseline")
        png_path, _ = one(docs, "png")
        _, calibration = one(docs, "stoner.production-cross-process-calibration")
        if validate_baseline(candidate) or candidate.get("state") != "candidate":
            raise ValueError("immutable source Candidate is invalid or already promoted")
        for key, value in record.items():
            if key not in {"state", "acceptance", "referencePath"} and candidate.get(key) != value:
                raise ValueError(f"Accepted/Candidate {key} mismatch")
        if (candidate_path.parent / candidate["referencePath"]).resolve() != png_path:
            raise ValueError("Candidate referencePath does not identify its PNG artifact")
        if record.get("state") == "accepted" and (root / record["referencePath"]).resolve() != png_path:
            raise ValueError("Accepted referencePath does not identify the verified PNG")
        for key in ("workloadRevision", "backend", "deviceClass", "capabilityDigest",
                    "outputDeviceProfileId", "transformVersion", "exposureStops"):
            if report.get(key) != record.get(key):
                raise ValueError(f"SDR report/record {key} mismatch")
        if (report["hostPlatform"], report["deviceClass"]) != PHYSICAL_SDR[record["backend"]]:
            raise ValueError("SDR report is not the required physical platform/device")
        if (report["extent"] != {"width": 512, "height": 512} or
                report["rendererStrategy"] != "deferred" or
                report["dynamicRange"] != "sdr" or
                report["executionTier"] != "sdr-image-authority" or
                report["native"]["claimDisposition"] != "sdr-authority"):
            raise ValueError("SDR report does not prove the exact formal authority path")
        if (candidate["outputDeviceProfileId"] != "Sdr.sRGB.v1" or
                candidate["transformVersion"] != "Sdr.KhronosPbrNeutral.v1" or
                candidate["exposureStops"] != 0 or
                candidate["settingsDigest"] != "2e9ec7a45efe6537db4e806e05cca54043d4a841aa013c67c941866e35d20d34" or
                report["insertionDigest"] != hashlib.sha256(b"[]\n").hexdigest()):
            raise ValueError("SDR formal workload settings differ from the frozen v3 profile")
        width, height, rgb = PNG.decode_png(png_path)
        if ((width, height) != (512, 512) or
                hashlib.sha256(png_path.read_bytes()).hexdigest() != record["compressedSha256"] or
                hashlib.sha256(rgb).hexdigest() != record["decodedSha256"]):
            raise ValueError("SDR Candidate PNG dimensions/digests mismatch")
        probe = linked_probe(report, docs)
        if probe.get("profileKind") != "native-sdr":
            raise ValueError("SDR report must reference a native-sdr probe")
        native_format = probe["format"]
        if native_format not in {"rgba8-unorm", "bgra8-unorm"}:
            raise ValueError("SDR formal native readback must be linear-storage RGBA8/BGRA8")
        rgba = bytearray(width * height * 4)
        rgba[3::4] = bytes([255]) * (width * height)
        for destination, source in enumerate((0, 1, 2) if native_format == "rgba8-unorm" else (2, 1, 0)):
            rgba[destination::4] = rgb[source::3]
        if hashlib.sha256(rgba).hexdigest() != probe["readbackDigest"]:
            raise ValueError("SDR PNG does not match the same-frame native readback")
        validate_calibration(calibration, candidate, report["gitRevision"])
    except (ValueError, TypeError, KeyError, OSError, json.JSONDecodeError) as error:
        return [f"SDR provenance: {error}"]
    return []


def validate_calibration(value: dict, candidate: dict, revision: str) -> None:
    if (value.get("schemaVersion") != 1 or value.get("gitRevision") != revision or
            value.get("backend") != candidate["backend"] or
            value.get("workloadRevision") != candidate["workloadRevision"] or
            value.get("candidateOnly") is not True or
            value.get("capturesPerProcess") != 20 or
            not isinstance(value.get("processCount"), int) or
            not 3 <= value["processCount"] <= 6):
        raise ValueError("calibration revision/platform/workload or process contract mismatch")
    processes = value.get("processes", [])
    if not isinstance(processes, list) or len(processes) != value["processCount"] or any(
            not isinstance(process, dict) or
            not isinstance(process.get("staleFrameMutation"), dict) or
            process["staleFrameMutation"].get("rejected") is not True
            for process in processes):
        raise ValueError("calibration lacks independent-process stale-frame rejection")
    if [process.get("ordinal") for process in processes] != list(range(1, value["processCount"] + 1)):
        raise ValueError("calibration process ordinals must be unique and contiguous")
    if not isinstance(value.get("modes"), list):
        raise ValueError("calibration modes must be an array")
    modes = [mode for mode in value["modes"] if isinstance(mode, dict) and
             mode.get("calibrationEvidenceSha256") == candidate["calibrationEvidenceSha256"]]
    if len(modes) != 1:
        raise ValueError("calibration must identify exactly one Candidate mode")
    mode = modes[0]
    if (mode.get("decodedPixelSha256") != candidate["decodedSha256"] or
            mode.get("candidatePngSha256") != candidate["compressedSha256"] or
            mode.get("flipPolicy") != candidate["flipPolicy"] or
            set(mode.get("mutationsRejected", [])) != MUTATIONS or
            len(set(mode.get("processOrdinals", []))) < 2):
        raise ValueError("calibration does not prove Candidate pixels/policy/mutations")
    ordinals = mode["processOrdinals"]
    if (any(type(ordinal) is not int or not 1 <= ordinal <= len(processes) for ordinal in ordinals) or
            len(ordinals) != len(set(ordinals)) or
            any(processes[ordinal - 1].get("decodedPixelSha256") != candidate["decodedSha256"]
                for ordinal in ordinals)):
        raise ValueError("calibration mode does not identify distinct matching processes")
    evidence = {"capturesPerProcess": 20,
                "decodedPixelSha256": candidate["decodedSha256"],
                "noise": mode["crossProcessNoise"],
                "processOrdinals": mode["processOrdinals"]}
    expected = hashlib.sha256(canonical(evidence).rstrip(b"\n")).hexdigest()
    if expected != candidate["calibrationEvidenceSha256"]:
        raise ValueError("calibration evidence digest mismatch")
