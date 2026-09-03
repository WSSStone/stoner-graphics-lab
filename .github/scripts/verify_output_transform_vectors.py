#!/usr/bin/env python3
"""Verify Feature 029 immutable color vectors without regenerating expectations."""

from __future__ import annotations

import argparse
from collections import Counter
import hashlib
import json
import math
from pathlib import Path
from typing import Any, Callable

from output_transform_common import (
    decoded_rgb_nits_tolerance,
    matrix_propagated_xyz_tolerance,
    matrix_vector,
)


CPU_ABSOLUTE_FLOOR = 1e-10
CPU_RELATIVE_FRACTION = 1e-10


def _load_json(path: Path) -> Any:
    def reject_duplicates(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
        result: dict[str, Any] = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"duplicate JSON key {key!r}")
            result[key] = value
        return result

    return json.loads(path.read_text(encoding="utf-8"),
                      object_pairs_hook=reject_duplicates)


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _find_root(path: Path) -> Path:
    current = path.resolve().parent
    for candidate in (current, *current.parents):
        if (candidate / "SConstruct").is_file() and (candidate / "Source").is_dir():
            return candidate
    raise ValueError(f"cannot locate repository root from {path}")


def _finite_tree(value: Any) -> bool:
    if isinstance(value, float):
        return math.isfinite(value)
    if isinstance(value, list):
        return all(_finite_tree(item) for item in value)
    if isinstance(value, dict):
        return all(_finite_tree(item) for item in value.values())
    return True


def _tolerance(expected: float) -> float:
    return max(CPU_ABSOLUTE_FLOOR,
               CPU_RELATIVE_FRACTION * abs(expected))


def _near(left: float, right: float) -> bool:
    return abs(left - right) <= _tolerance(right)


def _near_vector(left: list[float], right: list[float]) -> bool:
    return len(left) == len(right) and all(
        _near(a, b) for a, b in zip(left, right))


def _matrix_vector(matrix: list[list[float]], value: list[float]) -> list[float]:
    return matrix_vector(matrix, value)


def _khronos(value: list[float]) -> list[float]:
    minimum = min(value)
    offset = minimum - 6.25 * minimum * minimum if minimum < 0.08 else 0.04
    color = [component - offset for component in value]
    peak = max(color)
    if peak < 0.76:
        return color
    distance = 0.24
    new_peak = 1.0 - distance * distance / (peak + distance - 0.76)
    color = [component * new_peak / peak for component in color]
    desaturation = 1.0 - 1.0 / (0.15 * (peak - new_peak) + 1.0)
    return [component * (1.0 - desaturation) + new_peak * desaturation
            for component in color]


def _narkowicz(value: list[float]) -> list[float]:
    def curve(component: float) -> float:
        result = component * (2.51 * component + 0.03) / (
            component * (2.43 * component + 0.59) + 0.14)
        return min(1.0, max(0.0, result))
    return [curve(component) for component in value]


def _reinhard(value: list[float]) -> list[float]:
    luminance = 0.2126 * value[0] + 0.7152 * value[1] + 0.0722 * value[2]
    if luminance == 0.0:
        return [0.0, 0.0, 0.0]
    mapped = luminance * (1.0 + luminance / 16.0) / (1.0 + luminance)
    return [min(1.0, max(0.0, component * mapped / luminance))
            for component in value]


SDR_CURVES: dict[str, Callable[[list[float]], list[float]]] = {
    "Sdr.KhronosPbrNeutral.v1": _khronos,
    "Sdr.NarkowiczAcesFit.v1": _narkowicz,
    "Sdr.ExtendedReinhardRec709.v1": _reinhard,
}


def _pq_encode(nits: float) -> float:
    m1, m2 = 0.1593017578125, 78.84375
    c1, c2, c3 = 0.8359375, 18.8515625, 18.6875
    normalized = min(1.0, max(0.0, nits / 10000.0))
    power = normalized ** m1
    return ((c1 + c2 * power) / (1.0 + c3 * power)) ** m2


def _pq_decode(encoded: float) -> float:
    m1, m2 = 0.1593017578125, 78.84375
    c1, c2, c3 = 0.8359375, 18.8515625, 18.6875
    power = min(1.0, max(0.0, encoded)) ** (1.0 / m2)
    numerator = max(power - c1, 0.0)
    denominator = max(c2 - c3 * power, float.fromhex("0x0.0000000000001p-1022"))
    return 10000.0 * (numerator / denominator) ** (1.0 / m1)


def _encode_transfer(value: float, transfer: str) -> float:
    linear = max(0.0, value)
    if transfer == "srgb":
        return 12.92 * linear if linear <= 0.0031308 else (
            1.055 * linear ** (1.0 / 2.4) - 0.055)
    if transfer == "bt709-oetf":
        return 4.5 * linear if linear < 0.018 else (
            1.099 * linear ** 0.45 - 0.099)
    if transfer == "gamma22":
        return linear ** (1.0 / 2.2)
    raise ValueError(f"unknown SDR transfer {transfer}")


def _decode_transfer(value: float, transfer: str) -> float:
    encoded = max(0.0, value)
    if transfer == "srgb":
        return encoded / 12.92 if encoded <= 0.04045 else (
            (encoded + 0.055) / 1.055) ** 2.4
    if transfer == "bt709-oetf":
        return encoded / 4.5 if encoded < 0.081 else (
            (encoded + 0.099) / 1.099) ** (1.0 / 0.45)
    if transfer == "gamma22":
        return encoded ** 2.2
    raise ValueError(f"unknown SDR transfer {transfer}")


def _hdr_tolerance(value: list[float], encoding: str,
                   reference_white: float) -> list[float]:
    return decoded_rgb_nits_tolerance(value, encoding, reference_white)


def _xyz_tolerance(matrix: list[list[float]], rgb: list[float]) -> list[float]:
    return matrix_propagated_xyz_tolerance(matrix, rgb)


def _check_vector(label: str, actual: list[float], expected: list[float],
                  errors: list[str]) -> None:
    if not _near_vector(actual, expected):
        errors.append(f"VALUE_MISMATCH: {label}")


def _verify_sdr(case: dict[str, Any], expectation: dict[str, Any],
                profile: dict[str, Any], matrix: list[list[float]],
                errors: list[str]) -> None:
    boundary = [max(0.0, value) for value in case["preTonemapSceneLinear"]]
    transform = expectation["transformVersionId"]
    if transform not in SDR_CURVES:
        errors.append(f"UNKNOWN_SDR_TRANSFORM: {case['caseId']}:{transform}")
        return
    display = SDR_CURVES[transform](boundary)
    label = f"{case['caseId']}:{profile['profileId']}"
    _check_vector(f"{label}:display", expectation["displayLinearRgb"],
                  display, errors)
    native = expectation.get("nativeEncodings", [])
    if len(native) != 1 or native[0].get("encodingId") != "sdr-explicit":
        errors.append(f"SDR_ENCODING_CARDINALITY: {label}")
        return
    encoded = [_encode_transfer(value, profile["transfer"]) for value in display]
    decoded = [_decode_transfer(value, profile["transfer"]) for value in encoded]
    _check_vector(f"{label}:encoded", native[0]["encodedRgb"], encoded, errors)
    _check_vector(f"{label}:decoded", native[0]["decodedLinearRgb"], decoded, errors)
    _check_vector(f"{label}:xyz", expectation["expectedXyz"],
                  _matrix_vector(matrix, display), errors)


def _verify_hdr(case: dict[str, Any], expectation: dict[str, Any],
                profile: dict[str, Any], matrix: list[list[float]],
                errors: list[str]) -> None:
    label = f"{case['caseId']}:{profile['profileId']}"
    if expectation.get("transformVersionId") != (
            "Hdr.ACES2.0.0_2025-04-04.Rec2020D65.v1"):
        errors.append(f"HDR_TRANSFORM_ID: {label}")
    display = expectation["displayLinearRgb"]
    peak = float(profile["targetPeakNits"])
    if any(value < 0.0 or value > peak for value in display):
        errors.append(f"HDR_DISPLAY_RANGE: {label}")
    _check_vector(f"{label}:xyz", expectation["expectedXyz"],
                  _matrix_vector(matrix, display), errors)
    expected_encodings = (set(profile["nativeEncodingOptions"]))
    native = expectation.get("nativeEncodings", [])
    if {item.get("encodingId") for item in native} != expected_encodings:
        errors.append(f"HDR_ENCODING_SET: {label}")
        return
    for item in native:
        encoding = item["encodingId"]
        reference_white = float(item["referenceWhiteNits"])
        if encoding == "pq-rec2020":
            encoded = [_pq_encode(value) for value in display]
            decoded = [_pq_decode(value) for value in encoded]
        else:
            scale = 80.0 if encoding == "scrgb80" else reference_white
            encoded = [value / scale for value in display]
            decoded = [value * scale for value in encoded]
        tolerance = _hdr_tolerance(display, encoding, reference_white)
        xyz_tolerance = _xyz_tolerance(matrix, tolerance)
        _check_vector(f"{label}:{encoding}:encoded", item["encodedRgb"],
                      encoded, errors)
        _check_vector(f"{label}:{encoding}:decoded", item["decodedLinearRgb"],
                      decoded, errors)
        _check_vector(f"{label}:{encoding}:rgb-tolerance",
                      item["decodedRgbTolerance"], tolerance, errors)
        _check_vector(f"{label}:{encoding}:xyz-tolerance",
                      item["decodedXyzTolerance"], xyz_tolerance, errors)


def _verify_loaded(profiles: dict[str, Any], manifest: dict[str, Any],
                   vectors: dict[str, Any], root: Path,
                   vectors_path: Path) -> tuple[list[str], dict[str, Any]]:
    errors: list[str] = []
    if manifest.get("vectorSetId") != vectors.get("vectorSetId"):
        errors.append("VECTOR_SET_ID_MISMATCH")
    vector_info = manifest.get("vectorDocument", {})
    if _sha256(vectors_path) != vector_info.get("sha256"):
        errors.append("VECTOR_DOCUMENT_DIGEST_MISMATCH")
    for item in manifest.get("repositoryInputDigests", []):
        path = root / item["path"]
        if not path.is_file() or _sha256(path) != item["sha256"]:
            errors.append(f"REPOSITORY_INPUT_DIGEST_MISMATCH: {item['path']}")
    if not _finite_tree(vectors):
        errors.append("NON_FINITE_VECTOR_VALUE")

    profile_list = profiles.get("profiles", [])
    profile_by_id = {item["profileId"]: item for item in profile_list}
    required_profiles = manifest["coverage"]["requiredProfiles"]
    if list(profile_by_id) != required_profiles:
        errors.append("PROFILE_ORDER_OR_ID_MISMATCH")
    cases = vectors.get("cases", [])
    case_by_id = {item.get("caseId"): item for item in cases}
    if len(cases) != 32 or len(case_by_id) != len(cases):
        errors.append("CASE_COUNT_OR_UNIQUENESS")
    exposure_samples = sorted({case.get("exposureStops") for case in cases})
    if exposure_samples != manifest["coverage"]["requiredExposureSamples"]:
        errors.append("EXPOSURE_SAMPLE_COVERAGE")
    categories = {case.get("category") for case in cases}
    if not set(manifest["coverage"]["requiredCategories"]).issubset(categories):
        errors.append("CATEGORY_COVERAGE")

    matrices = profiles["matrices"]
    rec709_xyz = matrices["rec709ToXyzD65"]
    rec2020_xyz = matrices["rec2020ToXyzD65"]
    counts: Counter[str] = Counter()
    sdr_strategies: set[str] = set()
    hdr_cases: set[str] = set()
    expectation_index: dict[tuple[str, str], dict[str, Any]] = {}
    for case in cases:
        case_id = case["caseId"]
        scale = 2.0 ** float(case["exposureStops"])
        expected_pre = [value * scale for value in case["sceneLinearRec709"]]
        _check_vector(f"{case_id}:pre-tonemap",
                      case["preTonemapSceneLinear"], expected_pre, errors)
        expectations = case.get("expectations", [])
        ids = [item.get("profileId") for item in expectations]
        if ids != required_profiles:
            errors.append(f"EXPECTATION_ORDER_OR_CARDINALITY: {case_id}")
            continue
        for expectation in expectations:
            profile_id = expectation["profileId"]
            profile = profile_by_id[profile_id]
            counts[profile_id] += 1
            expectation_index[(case_id, profile_id)] = expectation
            if profile["dynamicRange"] == "sdr":
                sdr_strategies.add(expectation["transformVersionId"])
                _verify_sdr(case, expectation, profile, rec709_xyz, errors)
            else:
                hdr_cases.add(case_id)
                matrix = rec2020_xyz if profile["targetPrimaries"] == "rec2020-d65" else rec709_xyz
                _verify_hdr(case, expectation, profile, matrix, errors)

    if any(counts[profile_id] < 16 for profile_id in required_profiles):
        errors.append("PER_PROFILE_VECTOR_COVERAGE")
    if len(hdr_cases) < 32:
        errors.append("HDR_VECTOR_COVERAGE")
    if sdr_strategies != set(manifest["coverage"]["requiredSdrStrategies"]):
        errors.append("SDR_STRATEGY_COVERAGE")
    for lower_id, upper_id in manifest["coverage"]["oneStopPairs"]:
        lower, upper = case_by_id[lower_id], case_by_id[upper_id]
        doubled = [value * 2.0 for value in lower["preTonemapSceneLinear"]]
        _check_vector(f"one-stop:{lower_id}:{upper_id}",
                      upper["preTonemapSceneLinear"], doubled, errors)
        for profile_id in required_profiles:
            low = expectation_index[(lower_id, profile_id)]["displayLinearRgb"]
            high = expectation_index[(upper_id, profile_id)]["displayLinearRgb"]
            if any(high[index] + _tolerance(low[index]) < low[index]
                   for index in range(3)):
                errors.append(f"NON_MONOTONIC_PAIR: {profile_id}:{lower_id}:{upper_id}")

    normalized = json.dumps(vectors, sort_keys=True,
                            separators=(",", ":"), ensure_ascii=True).encode()
    repeated = [hashlib.sha256(normalized).hexdigest()
                for _ in range(manifest["coverage"]["determinismRepeatCount"])]
    if len(set(repeated)) != 1:
        errors.append("NORMALIZED_REPEAT_NONDETERMINISM")
    metrics = {
        "caseCount": len(cases),
        "expectationCount": sum(counts.values()),
        "hdrCaseCount": len(hdr_cases),
        "perProfileCounts": dict(counts),
        "sdrStrategies": sorted(sdr_strategies),
        "determinismRepeatCount": len(repeated),
        "normalizedSha256": repeated[0] if repeated else None,
    }
    return errors, metrics


def verify_documents(profiles: Path, manifest: Path, vectors: Path) -> list[str]:
    try:
        loaded_profiles = _load_json(profiles)
        loaded_manifest = _load_json(manifest)
        loaded_vectors = _load_json(vectors)
        root = _find_root(profiles)
        errors, _ = _verify_loaded(
            loaded_profiles, loaded_manifest, loaded_vectors, root, vectors)
        return errors
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        return [f"DOCUMENT_ERROR: {error}"]


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--profiles", type=Path, required=True)
    parser.add_argument("--manifest", type=Path,
                        default=Path("Tests/Fixtures/OutputTransform/manifest-v1.json"))
    parser.add_argument("--vectors", type=Path, required=True)
    parser.add_argument("--output", type=Path)
    return parser


def main() -> int:
    arguments = build_parser().parse_args()
    try:
        profiles = _load_json(arguments.profiles)
        manifest = _load_json(arguments.manifest)
        vectors = _load_json(arguments.vectors)
        root = _find_root(arguments.profiles)
        errors, metrics = _verify_loaded(
            profiles, manifest, vectors, root, arguments.vectors)
    except (OSError, ValueError, KeyError, TypeError, json.JSONDecodeError) as error:
        errors = [f"DOCUMENT_ERROR: {error}"]
        metrics = {}
    report = {
        "schema": "stoner.output-transform-vector-verification",
        "schemaVersion": 1,
        "status": "failed" if errors else "passed",
        "findings": errors,
        "metrics": metrics,
    }
    if arguments.output is not None:
        arguments.output.parent.mkdir(parents=True, exist_ok=True)
        arguments.output.write_text(
            json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(report, sort_keys=True))
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
