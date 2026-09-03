#!/usr/bin/env python3
"""Fail-closed Feature 029 evidence and human-attestation verifier.

This module intentionally has no write path for HDR attestations.  It validates
machine-authored preflight requests and separately authored maintainer records.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path, PurePosixPath
import re
from typing import Any, Iterable


MAX_JSON_BYTES = 1 * 1024 * 1024
MAX_ARTIFACTS = 64
MAX_ARTIFACT_BYTES = 64 * 1024 * 1024
MAX_AGGREGATE_BYTES = 256 * 1024 * 1024
SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
GIT_RE = re.compile(r"^[0-9a-f]{40}$")
TOKEN_RE = re.compile(r"^[A-Za-z0-9][A-Za-z0-9._-]{0,127}$")
HDR_PROFILES = (
    "Hdr.PQ.Rec2020.1000.v1",
    "Hdr.PQ.Rec2020.2000.v1",
    "Hdr.Linear.1000.v1",
    "Hdr.Linear.2000.v1",
)
ACKNOWLEDGEMENTS = (
    "live-metal-output-viewed",
    "no-sdr-capture-substitution",
    "no-automated-visual-decision",
)
FORBIDDEN_HDR_KEYS = {
    "score", "visualscore", "perceptualscore", "threshold", "screenshot",
    "image", "png", "video", "candidate", "reference",
    "automaticdecision", "inferreddecision", "autoaccept", "measuredpeak",
    "measuredpeaknits", "photometricmeasurement",
}


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def canonical_json_bytes(value: Any) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":"),
                       ensure_ascii=False, allow_nan=False) + "\n").encode("utf-8")


def load_bounded_json(path: Path) -> tuple[Any | None, list[str], bytes]:
    errors: list[str] = []
    try:
        if path.stat().st_size > MAX_JSON_BYTES:
            return None, [f"{path}: JSON exceeds {MAX_JSON_BYTES} bytes"], b""
        payload = path.read_bytes()
    except OSError as error:
        return None, [f"{path}: cannot read JSON: {error}"], b""
    if len(payload) > MAX_JSON_BYTES:
        errors.append(f"{path}: JSON exceeds {MAX_JSON_BYTES} bytes")
    try:
        def unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
            result = {}
            for key, item in pairs:
                if key in result:
                    raise ValueError(f"duplicate JSON key: {key}")
                result[key] = item
            return result

        def nonfinite(token: str) -> Any:
            raise ValueError(f"non-finite JSON number: {token}")

        value = json.loads(payload, object_pairs_hook=unique_object,
                           parse_constant=nonfinite)
    except (UnicodeDecodeError, ValueError) as error:
        return None, errors + [f"{path}: invalid UTF-8 JSON: {error}"], payload
    return value, errors, payload


def _keys(value: dict[str, Any], required: Iterable[str], allowed: Iterable[str],
          label: str) -> list[str]:
    required_set, allowed_set = set(required), set(allowed)
    errors = [f"{label}: missing field {key}" for key in
              sorted(required_set - value.keys())]
    errors.extend(f"{label}: unexpected field {key}" for key in
                  sorted(value.keys() - allowed_set))
    return errors


def _token(value: Any) -> bool:
    return isinstance(value, str) and TOKEN_RE.fullmatch(value) is not None


def _sha(value: Any) -> bool:
    return isinstance(value, str) and SHA256_RE.fullmatch(value) is not None


def _walk_keys(value: Any) -> Iterable[str]:
    if isinstance(value, dict):
        for key, child in value.items():
            yield key.lower()
            yield from _walk_keys(child)
    elif isinstance(value, list):
        for child in value:
            yield from _walk_keys(child)


def validate_artifacts(artifacts: Any, root: Path,
                       hdr_authority: bool = False) -> list[str]:
    if not isinstance(artifacts, list) or len(artifacts) > MAX_ARTIFACTS:
        return ["artifacts: must be an array with at most 64 entries"]
    errors: list[str] = []
    aggregate = 0
    paths = set()
    for index, artifact in enumerate(artifacts):
        label = f"artifacts[{index}]"
        if not isinstance(artifact, dict):
            errors.append(f"{label}: must be an object")
            continue
        errors.extend(_keys(artifact, ("path", "sha256", "sizeBytes"),
                            ("path", "sha256", "sizeBytes"), label))
        path_value = artifact.get("path")
        size = artifact.get("sizeBytes")
        if (not isinstance(path_value, str) or not path_value or
                "\\" in path_value or Path(path_value).is_absolute() or
                ":" in path_value or len(path_value) > 240 or
                ".." in PurePosixPath(path_value).parts):
            errors.append(f"{label}: path must be safe repository-relative POSIX")
            continue
        if path_value in paths:
            errors.append(f"{label}: duplicate artifact path")
        paths.add(path_value)
        suffix = PurePosixPath(path_value).suffix.lower()
        allowed = {".json"} if hdr_authority else {".json", ".png"}
        if suffix not in allowed:
            errors.append(f"{label}: only bounded {sorted(allowed)} evidence is allowed")
        if not _sha(artifact.get("sha256")):
            errors.append(f"{label}: sha256 must be lowercase SHA-256")
        if not isinstance(size, int) or isinstance(size, bool) or not (
                0 <= size <= MAX_ARTIFACT_BYTES):
            errors.append(f"{label}: sizeBytes exceeds per-artifact bound")
            continue
        aggregate += size
        if suffix == ".json" and size > MAX_JSON_BYTES:
            errors.append(f"{label}: JSON artifact exceeds 1 MiB")
            continue
        candidate = root / path_value
        try:
            resolved = candidate.resolve(strict=True)
            resolved.relative_to(root.resolve())
            if resolved.stat().st_size != size:
                errors.append(f"{label}: size does not match file")
                continue
            payload = resolved.read_bytes()
        except (OSError, ValueError):
            errors.append(f"{label}: artifact is missing or escapes evidence root")
            continue
        if len(payload) != size or sha256_bytes(payload) != artifact.get("sha256"):
            errors.append(f"{label}: size or digest does not match file")
    if aggregate > MAX_AGGREGATE_BYTES:
        errors.append("artifacts: aggregate size exceeds 256 MiB")
    return errors


def validate_output_report(value: Any, root: Path) -> list[str]:
    if not isinstance(value, dict):
        return ["output report: root must be an object"]
    required = (
        "schema", "schemaVersion", "gitRevision", "workloadRevision",
        "hostPlatform", "rendererStrategy", "backend", "executionTier",
        "deviceClass", "capabilityDigest", "sceneColor", "extent",
        "dynamicRange", "outputDeviceProfileId", "transformVersion",
        "exposureStops", "insertionDigest", "transferOwner", "modeGeneration",
        "frameToken", "executionResult", "firstFailure", "native",
        "visualAuthority", "artifacts",
    )
    errors = _keys(value, required, required, "output report")
    if value.get("schema") != "stoner.output-validation-report" or value.get("schemaVersion") != 1:
        errors.append("output report: schema identity must be version 1")
    if not isinstance(value.get("gitRevision"), str) or not GIT_RE.fullmatch(value["gitRevision"]):
        errors.append("output report: gitRevision must be 40 lowercase hex")
    if not _token(value.get("workloadRevision")) or not _sha(value.get("capabilityDigest")):
        errors.append("output report: workload/capability identity is invalid")
    if value.get("hostPlatform") not in {"windows", "macos", "linux"}:
        errors.append("output report: hostPlatform is invalid")
    if value.get("rendererStrategy") not in {"deferred", "forward"}:
        errors.append("output report: rendererStrategy is invalid")
    if value.get("backend") not in {"vulkan", "metal"}:
        errors.append("output report: backend is invalid")
    if value.get("dynamicRange") not in {"sdr", "hdr"}:
        errors.append("output report: dynamicRange is invalid")
    if value.get("executionTier") not in {"deterministic-contract", "native-nonvisual", "sdr-image-authority"}:
        errors.append("output report: executionTier is invalid")
    if value.get("executionResult") not in {"passed", "failed", "unsupported", "paused"}:
        errors.append("output report: executionResult is invalid")
    if value.get("executionResult") == "failed":
        if not isinstance(value.get("firstFailure"), dict):
            errors.append("output report: failed result requires firstFailure")
    elif value.get("firstFailure") is not None:
        errors.append("output report: non-failed result requires null firstFailure")
    if value.get("transferOwner") != "renderer-output-device-stage":
        errors.append("output report: Renderer must own the only output transfer")
    scene = value.get("sceneColor")
    if scene != {"format": "rgba16-float", "primaries": "rec709",
                 "whitePoint": "d65", "transfer": "linear",
                 "sampleCount": 1, "alphaMode": "opaque-one"}:
        errors.append("output report: canonical SceneColor contract mismatch")
    extent = value.get("extent")
    if (not isinstance(extent, dict) or set(extent) != {"width", "height"} or
            not all(isinstance(extent.get(key), int) and
                    1 <= extent[key] <= 16384 for key in ("width", "height"))):
        errors.append("output report: extent is invalid")
    native = value.get("native")
    native_required = {"claimDisposition", "commandCompleted",
                       "readbackCompleted", "presented",
                       "readbackFrameToken", "presentationFrameToken",
                       "format", "colorSpace", "hdrMetadataDigest",
                       "displayAdaptation"}
    if not isinstance(native, dict) or set(native) != native_required:
        errors.append("output report: native record shape is invalid")
    else:
        if any(type(native.get(key)) is not bool for key in
               ("commandCompleted", "readbackCompleted", "presented")):
            errors.append("output report: native completion fields must be booleans")
        if value.get("executionTier") in {"native-nonvisual", "sdr-image-authority"} and value.get("executionResult") == "passed":
            expected_claim = ("sdr-authority" if value["executionTier"] == "sdr-image-authority"
                              else "nonvisual-only")
            if (native.get("claimDisposition") != expected_claim or
                    any(native.get(key) is not True for key in
                        ("commandCompleted", "readbackCompleted", "presented")) or
                    not _token(value.get("frameToken")) or
                    type(value.get("modeGeneration")) is not int or value["modeGeneration"] < 1 or
                    native.get("readbackFrameToken") != value["frameToken"] or
                    native.get("presentationFrameToken") != value["frameToken"]):
                errors.append("output report: passed native gate lacks same-frame terminal completion")
        if value.get("hostPlatform") == "windows" and value.get("dynamicRange") == "hdr":
            expected = (value.get("executionTier") == "deterministic-contract" and
                        native.get("claimDisposition") == "not-run" and
                        not native.get("commandCompleted") and
                        not native.get("readbackCompleted") and
                        not native.get("presented") and
                        native.get("readbackFrameToken") is None and
                        native.get("presentationFrameToken") is None and
                        native.get("hdrMetadataDigest") is None and
                        native.get("displayAdaptation") == "none")
            if not expected:
                errors.append("output report: Windows HDR must make no native/visual claim")
        if (value.get("hostPlatform") == "macos" and value.get("backend") == "metal" and
                value.get("dynamicRange") == "hdr" and
                native.get("hdrMetadataDigest") is not None):
            errors.append("output report: Metal PQ/EDR requires EDRMetadata=nil")
        if native.get("readbackCompleted") and native.get("presented") and (
                native.get("readbackFrameToken") != native.get("presentationFrameToken") or
                native.get("readbackFrameToken") != value.get("frameToken")):
            errors.append("output report: readback and presentation must identify one frame")
    visual = value.get("visualAuthority")
    if not isinstance(visual, dict) or set(visual) != {"disposition", "reason"}:
        errors.append("output report: visualAuthority shape is invalid")
    elif value.get("dynamicRange") == "hdr" and value.get("hostPlatform") == "macos" and value.get("backend") == "metal":
        if visual.get("disposition") not in {"manual-review-required", "attestation-recorded"}:
            errors.append("output report: Metal HDR can only request or quote human authority")
    errors.extend(validate_artifacts(value.get("artifacts"), root,
                                     hdr_authority=value.get("dynamicRange") == "hdr"))
    return errors


def validate_sdr_baseline(record: Any) -> list[str]:
    if not isinstance(record, dict):
        return ["SDR baseline: record must be an object"]
    required = ("schema", "schemaVersion", "baselineId", "state",
                "workloadRevision", "backend", "deviceClass",
                "capabilityDigest", "outputDeviceProfileId",
                "transformVersion", "exposureStops", "settingsDigest",
                "width", "height", "sampleCount", "referencePath",
                "compressedSha256", "decodedSha256",
                "calibrationEvidenceSha256", "flipPolicy", "acceptance")
    errors = _keys(record, required, required, "SDR baseline")
    if record.get("schema") != "stoner.sdr-image-baseline" or record.get("schemaVersion") != 3:
        errors.append("SDR baseline: schema identity must be v3")
    if not _token(record.get("baselineId")) or not _token(record.get("deviceClass")):
        errors.append("SDR baseline: baseline/device identity is invalid")
    if not re.fullmatch(r"production-content-(lantern|sponza)-v3(?:[.-][a-z0-9.-]+)?",
                        str(record.get("workloadRevision", ""))):
        errors.append("SDR baseline: only a fresh v3 workload is admissible")
    if (record.get("width"), record.get("height"), record.get("sampleCount")) != (512, 512, 1):
        errors.append("SDR baseline: exact 512x512 sampleCount=1 required")
    if record.get("backend") not in {"vulkan", "metal"}:
        errors.append("SDR baseline: backend is invalid")
    if record.get("outputDeviceProfileId") not in {
            "Sdr.sRGB.v1", "Sdr.BT709.v1", "Sdr.ExplicitGamma22.v1"}:
        errors.append("SDR baseline: output profile is invalid")
    if record.get("transformVersion") not in {
            "Sdr.KhronosPbrNeutral.v1", "Sdr.NarkowiczAcesFit.v1",
            "Sdr.ExtendedReinhardRec709.v1"}:
        errors.append("SDR baseline: transform version is invalid")
    exposure = record.get("exposureStops")
    if (not isinstance(exposure, (int, float)) or isinstance(exposure, bool) or
            not -16 <= exposure <= 16):
        errors.append("SDR baseline: exposureStops is invalid")
    reference_path = record.get("referencePath")
    if (not isinstance(reference_path, str) or not reference_path or
            len(reference_path) > 240 or "\\" in reference_path or
            Path(reference_path).is_absolute() or
            ".." in PurePosixPath(reference_path).parts or
            PurePosixPath(reference_path).suffix.lower() != ".png"):
        errors.append("SDR baseline: referencePath must be safe lossless PNG")
    for key in ("capabilityDigest", "settingsDigest", "compressedSha256",
                "decodedSha256", "calibrationEvidenceSha256"):
        if not _sha(record.get(key)):
            errors.append(f"SDR baseline: {key} must be SHA-256")
    policy = record.get("flipPolicy")
    policy_keys = {"meanMax", "p95Max", "maximumMax", "badPixelThreshold",
                   "badPixelFractionMax"}
    if (not isinstance(policy, dict) or set(policy) != policy_keys or
            any(not isinstance(policy.get(key), (int, float)) or
                isinstance(policy.get(key), bool) or
                not 0 <= policy[key] <= 1 for key in policy_keys) or
            (isinstance(policy, dict) and policy.get("badPixelThreshold", 0) <= 0)):
        errors.append("SDR baseline: flipPolicy is invalid")
    state, acceptance = record.get("state"), record.get("acceptance")
    if state not in {"candidate", "calibrated", "reviewed", "accepted", "superseded"}:
        errors.append("SDR baseline: state is invalid")
    if state == "accepted":
        if (not isinstance(acceptance, dict) or
                set(acceptance) != {"maintainerId", "reviewedAt",
                                    "candidateSha256", "decision"} or
                acceptance.get("decision") != "accepted" or
                not _token(acceptance.get("maintainerId")) or
                not isinstance(acceptance.get("reviewedAt"), str) or
                len(acceptance.get("reviewedAt", "")) > 64 or
                not _sha(acceptance.get("candidateSha256")) or
                acceptance.get("candidateSha256") !=
                    record.get("compressedSha256")):
            errors.append("SDR baseline: accepted state requires explicit maintainer acceptance")
    elif state in {"candidate", "calibrated", "reviewed"} and acceptance is not None:
        errors.append("SDR baseline: non-accepted states cannot contain acceptance")
    return errors


def validate_sdr_registry(value: Any) -> list[str]:
    if not isinstance(value, dict) or set(value) != {"schema", "schemaVersion", "registryId", "records"}:
        return ["SDR registry: invalid shape"]
    errors: list[str] = []
    if value.get("schema") != "stoner.sdr-image-baseline-registry" or value.get("schemaVersion") != 3:
        errors.append("SDR registry: schema identity must be v3")
    if value.get("registryId") != "output-transform-sdr-baselines-v3":
        errors.append("SDR registry: registryId mismatch")
    records = value.get("records")
    if not isinstance(records, list) or len(records) > MAX_ARTIFACTS:
        return errors + ["SDR registry: records must be bounded array"]
    keys: set[tuple[Any, ...]] = set()
    ids: set[str] = set()
    for index, record in enumerate(records):
        record_errors = validate_sdr_baseline(record)
        errors.extend(f"records[{index}]: {error}" for error in record_errors)
        if record_errors:
            continue
        key = tuple(record.get(field) for field in (
            "workloadRevision", "backend", "deviceClass",
            "outputDeviceProfileId", "transformVersion", "exposureStops",
            "settingsDigest"))
        if key in keys:
            errors.append(f"records[{index}]: duplicate complete v3 authority key")
        keys.add(key)
        baseline_id = record.get("baselineId")
        if baseline_id in ids:
            errors.append(f"records[{index}]: duplicate baselineId")
        if isinstance(baseline_id, str):
            ids.add(baseline_id)
    return errors


def validate_hdr_request(value: Any) -> list[str]:
    if not isinstance(value, dict):
        return ["HDR request: root must be object"]
    if any(key in FORBIDDEN_HDR_KEYS for key in _walk_keys(value)):
        return ["HDR request: visual scoring/image/automatic decision fields are forbidden"]
    required = ("schema", "schemaVersion", "requestId", "gitRevision",
                "workloadRevision", "backend", "hostClass", "deviceClass",
                "displayClass", "displayCapabilityDigest", "preflightDigest",
                "nativeReportDigest", "reviewSessionId", "profiles", "state",
                "firstFailure")
    errors = _keys(value, required, required, "HDR request")
    if value.get("schema") != "stoner.hdr-live-review-request" or value.get("schemaVersion") != 1:
        errors.append("HDR request: schema identity must be v1")
    for key in ("requestId", "workloadRevision", "deviceClass", "displayClass",
                "reviewSessionId"):
        if not _token(value.get(key)):
            errors.append(f"HDR request: {key} is invalid")
    if not isinstance(value.get("gitRevision"), str) or not GIT_RE.fullmatch(value["gitRevision"]):
        errors.append("HDR request: gitRevision is invalid")
    for key in ("displayCapabilityDigest", "preflightDigest", "nativeReportDigest"):
        if not _sha(value.get(key)):
            errors.append(f"HDR request: {key} is invalid")
    if value.get("backend") != "metal" or value.get("hostClass") != "maintainer-local-macos-metal":
        errors.append("HDR request: only maintainer-local macOS Metal is eligible")
    profiles = value.get("profiles")
    if not isinstance(profiles, list) or [item.get("profileId") if isinstance(item, dict) else None for item in profiles] != list(HDR_PROFILES):
        errors.append("HDR request: exact ordered four-profile set required")
    else:
        for index, profile in enumerate(profiles):
            expected_keys = {"profileId", "targetPeakNits", "transfer", "gamut",
                             "displayAdaptation", "modeGeneration",
                             "firstFrameToken", "lastFrameToken",
                             "settledFrameToken", "readbackDigest",
                             "presentationReady"}
            pq = index < 2
            peak = 1000 if index in (0, 2) else 2000
            expected_transfer = "st2084" if pq else "metal-edr-linear"
            expected_gamut = "rec2020-d65" if pq else "rec709-d65"
            if (set(profile) != expected_keys or
                    profile.get("targetPeakNits") != peak or
                    profile.get("transfer") != expected_transfer or
                    profile.get("gamut") != expected_gamut or
                    not isinstance(profile.get("modeGeneration"), int) or
                    isinstance(profile.get("modeGeneration"), bool) or
                    profile.get("modeGeneration", 0) < 1 or
                    not all(_token(profile.get(key)) for key in
                            ("firstFrameToken", "lastFrameToken", "settledFrameToken")) or
                    not _sha(profile.get("readbackDigest")) or
                    profile.get("presentationReady") is not True):
                errors.append(f"HDR request: profiles[{index}] is incomplete")
            expected_adaptation = "system-color-management" if index < 2 else "none"
            if profile.get("displayAdaptation") != expected_adaptation:
                errors.append(f"HDR request: profiles[{index}] display adaptation mismatch")
    state = value.get("state")
    if state not in {"not-run", "unsupported", "failed", "ready-for-live-review"}:
        errors.append("HDR request: invalid state")
    if state == "failed" and not isinstance(value.get("firstFailure"), dict):
        errors.append("HDR request: failed state requires firstFailure")
    elif state == "failed" and (set(value["firstFailure"]) != {"code", "message"} or
                                not _token(value["firstFailure"].get("code")) or
                                not isinstance(value["firstFailure"].get("message"), str) or
                                not 1 <= len(value["firstFailure"].get("message", "")) <= 1024):
        errors.append("HDR request: firstFailure shape is invalid")
    if state != "failed" and value.get("firstFailure") is not None:
        errors.append("HDR request: non-failed state requires null firstFailure")
    return errors


def validate_hdr_attestations(request: Any, request_bytes: bytes,
                              attestations: list[Any],
                              require_all_pass: bool = False) -> tuple[list[str], list[dict[str, Any]]]:
    errors = validate_hdr_request(request)
    if not isinstance(request, dict):
        return errors, []
    if not isinstance(attestations, list) or len(attestations) > MAX_ARTIFACTS:
        return errors + ["HDR attestations: bounded array required"], []
    request_sha = sha256_bytes(request_bytes)
    by_id: dict[str, dict[str, Any]] = {}
    superseded: set[str] = set()
    for index, value in enumerate(attestations):
        label = f"attestation[{index}]"
        if not isinstance(value, dict):
            errors.append(f"{label}: root must be object")
            continue
        if any(key in FORBIDDEN_HDR_KEYS for key in _walk_keys(value)):
            errors.append(f"{label}: automated visual/image fields are forbidden")
        required = ("schema", "schemaVersion", "attestationId", "requestId",
                    "requestSha256", "gitRevision", "workloadRevision",
                    "maintainerId", "reviewedAt", "deviceClass", "displayClass",
                    "displayCapabilityDigest", "reviewSessionId", "observations",
                    "acknowledgements", "supersedesAttestationId")
        errors.extend(_keys(value, required, required, label))
        identity_matches = (
            value.get("schema") == "stoner.hdr-live-view-attestation" and
            value.get("schemaVersion") == 1 and
            value.get("requestId") == request.get("requestId") and
            value.get("requestSha256") == request_sha and
            all(value.get(key) == request.get(key) for key in
                ("gitRevision", "workloadRevision", "deviceClass",
                 "displayClass", "displayCapabilityDigest", "reviewSessionId")))
        if not identity_matches:
            errors.append(f"{label}: request SHA or immutable review identity mismatch")
        if value.get("acknowledgements") != list(ACKNOWLEDGEMENTS):
            errors.append(f"{label}: exact human-boundary acknowledgements required")
        if (not _token(value.get("attestationId")) or
                not _token(value.get("maintainerId")) or
                not isinstance(value.get("reviewedAt"), str) or
                not 1 <= len(value.get("reviewedAt", "")) <= 64):
            errors.append(f"{label}: attestation/maintainer/review-time identity is invalid")
        observations = value.get("observations")
        if (not isinstance(observations, list) or
                [item.get("profileId") if isinstance(item, dict) else None
                 for item in observations] != list(HDR_PROFILES)):
            errors.append(f"{label}: exact ordered four observations required")
        else:
            for observation in observations:
                if (set(observation) != {"profileId", "viewedLive", "decision",
                                         "observedDefects", "notes"} or
                        observation.get("viewedLive") is not True or
                        observation.get("decision") not in {"pass", "fail"} or
                        not isinstance(observation.get("observedDefects"), list) or
                        len(observation["observedDefects"]) > 16 or
                        any(not isinstance(defect, str) or
                            not 1 <= len(defect) <= 256
                            for defect in observation.get("observedDefects", [])) or
                        not isinstance(observation.get("notes"), str) or
                        len(observation.get("notes", "")) > 1024):
                    errors.append(f"{label}: observation is invalid")
                    break
        attestation_id = value.get("attestationId")
        if not _token(attestation_id) or attestation_id in by_id:
            errors.append(f"{label}: attestationId must be unique token")
        else:
            by_id[attestation_id] = value

    for attestation_id, value in by_id.items():
        parent = value.get("supersedesAttestationId")
        if parent is not None:
            if not _token(parent) or parent == attestation_id or parent not in by_id or parent in superseded:
                errors.append(f"attestation {attestation_id}: invalid supersession link")
            else:
                superseded.add(parent)
    current = [value for key, value in by_id.items() if key not in superseded]
    if len(current) > 1:
        errors.append("HDR attestations: multiple current non-superseded records")
    if require_all_pass:
        if request.get("state") != "ready-for-live-review":
            errors.append("HDR closeout: request is not ready-for-live-review")
        if len(current) != 1:
            errors.append("HDR closeout: exactly one current human attestation required")
        elif (not isinstance(current[0].get("observations"), list) or
              len(current[0]["observations"]) != 4 or
              any(not isinstance(item, dict) or item.get("decision") != "pass"
                  for item in current[0]["observations"])):
            errors.append("HDR closeout: every current human observation must pass")
    return errors, current


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--report", action="append", default=[])
    parser.add_argument("--sdr-registry")
    parser.add_argument("--hdr-request")
    parser.add_argument("--attestation-dir")
    parser.add_argument("--root", default=".")
    parser.add_argument("--require-closeout", action="store_true")
    parser.add_argument("--git-revision", help="Required exact software revision for closeout")
    return parser


def main() -> int:
    args = build_parser().parse_args()
    root = Path(args.root).resolve()
    errors: list[str] = []
    if args.require_closeout and not all((args.sdr_registry, args.hdr_request,
                                         args.attestation_dir, args.git_revision, args.report)):
        print("finding: closeout requires reports, SDR registry, HDR request, attestations, and exact git revision")
        return 1
    if len(args.report) > MAX_ARTIFACTS:
        print("finding: too many report inputs")
        return 1
    reports, registry, request, request_bytes, attestations = [], None, None, b"", []
    for report_name in args.report:
        value, load_errors, _ = load_bounded_json(Path(report_name))
        errors.extend(load_errors)
        if value is not None:
            reports.append(value)
            errors.extend(validate_output_report(value, root))
    if args.sdr_registry:
        value, load_errors, _ = load_bounded_json(Path(args.sdr_registry))
        errors.extend(load_errors)
        if value is not None:
            registry = value
            errors.extend(validate_sdr_registry(value))
    if args.hdr_request:
        request, load_errors, request_bytes = load_bounded_json(Path(args.hdr_request))
        errors.extend(load_errors)
        if args.attestation_dir:
            paths = sorted(Path(args.attestation_dir).glob("*.json"))
            if len(paths) > MAX_ARTIFACTS:
                print("finding: too many attestation inputs")
                return 1
            for path in paths:
                value, attestation_errors, _ = load_bounded_json(path)
                errors.extend(attestation_errors)
                if value is not None:
                    attestations.append(value)
        if request is not None:
            linked_errors, _ = validate_hdr_attestations(
                request, request_bytes, attestations, args.require_closeout)
            errors.extend(linked_errors)
    if args.require_closeout and not errors:
        spec = importlib.util.spec_from_file_location(
            "verify_closeout_aggregate", Path(__file__).with_name("aggregate_output_transform_validation.py"))
        assert spec is not None and spec.loader is not None
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        _, closeout_errors = module.aggregate(reports, registry, request, request_bytes,
                                              attestations, args.git_revision, root)
        errors.extend(closeout_errors)
    for error in errors:
        print(f"finding: {error}")
    if errors:
        print(f"output-transform evidence: FAILED ({len(errors)} findings)")
        return 1
    print("output-transform evidence: PASS (0 findings)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
