#!/usr/bin/env python3
"""Canonical Feature 028 acceptance report validation and evidence indexing."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path, PurePosixPath
import re
from typing import Iterable, Sequence


SHA256 = re.compile(r"^[0-9a-f]{64}$")
TOKEN = re.compile(r"^[a-z0-9][a-z0-9.-]{0,127}$")
ROOT_FIELDS = {
    "schema", "schemaVersion", "deterministic", "authority",
    "observations", "artifacts",
}
DETERMINISTIC_FIELDS = {
    "corpusRevision", "packageId", "rootAssetId", "sourceSetDigest",
    "targetProfileDigest", "generationIdentity", "mode",
    "dependencyCoverageDigest", "workloadRevision", "backend", "result",
    "evidenceDigest", "firstFailure",
}
OBSERVATION_FIELDS = {
    "hostClass", "deviceClass", "deviceDescription", "durationMilliseconds",
    "peakRssBytes", "rssGrowthBytes", "taskVmBytes", "allocatorBytes",
    "lifecycleCycles", "flip",
}
AUTHORITY_FIELDS = {"executionClass", "preflight", "measurements"}
MEASUREMENT_KINDS = {"timing", "rss", "image", "taskVm", "allocator", "peakRss"}
ARTIFACT_FIELDS = {"kind", "pathToken", "sha256", "sizeBytes"}
FAILURE_FIELDS = {
    "stage", "category", "subject", "expected", "observed",
    "reproductionProfile",
}
UNSUPPORTED_FAILURE_FIELDS = FAILURE_FIELDS | {
    "missingPrerequisite", "replacementLane",
}
ARTIFACT_KINDS = {"report", "readback", "window-capture", "comparison", "log"}
STAGE_ORDER = {
    stage: index for index, stage in enumerate((
        "corpus", "import", "cook", "publication", "strict-load",
        "realization", "native", "image", "lifecycle", "timeout",
        "unsupported",
    ))
}
MAX_REPORT_BYTES = 1024 * 1024
MAX_ARTIFACTS = 64
MAX_ARTIFACT_BYTES = 64 * 1024 * 1024
MAX_AGGREGATE_BYTES = 256 * 1024 * 1024


def canonical_bytes(value: object) -> bytes:
    return json.dumps(
        value, ensure_ascii=False, sort_keys=True, separators=(",", ":")
    ).encode("utf-8")


def canonical_correctness_bytes(report: dict) -> bytes:
    return canonical_bytes({
        "schema": report.get("schema"),
        "schemaVersion": report.get("schemaVersion"),
        "deterministic": report.get("deterministic"),
    })


def evidence_digest(artifacts: Iterable[dict]) -> str:
    return hashlib.sha256(canonical_bytes(list(artifacts))).hexdigest()


def _safe_path_token(token: object) -> bool:
    if not isinstance(token, str) or not token or len(token) > 512:
        return False
    if token.startswith("/") or "\\" in token:
        return False
    path = PurePosixPath(token)
    return not path.is_absolute() and all(part not in ("", ".", "..") for part in path.parts)


def index_artifact(path: Path, root: Path, kind: str) -> dict:
    if kind not in ARTIFACT_KINDS:
        raise ValueError("artifact kind is invalid")
    resolved = path.resolve()
    try:
        token = resolved.relative_to(root.resolve()).as_posix()
    except ValueError as error:
        raise ValueError("artifact path is outside evidence root") from error
    if not _safe_path_token(token) or not resolved.is_file():
        raise ValueError("artifact path is invalid")
    payload = resolved.read_bytes()
    if len(payload) > MAX_ARTIFACT_BYTES:
        raise ValueError("artifact exceeds 64 MiB")
    return {
        "kind": kind,
        "pathToken": token,
        "sha256": hashlib.sha256(payload).hexdigest(),
        "sizeBytes": len(payload),
    }


def _require_exact_fields(value: object, fields: set[str], subject: str) -> dict:
    if not isinstance(value, dict) or set(value) != fields:
        raise ValueError(f"{subject} fields are invalid")
    return value


def _require_bounded_string(value: object, subject: str, maximum: int) -> str:
    if not isinstance(value, str) or not value or len(value) > maximum:
        raise ValueError(f"{subject} is invalid")
    return value


def _validate_failure(value: object, unsupported: bool) -> None:
    if unsupported and isinstance(value, dict):
        if "missingPrerequisite" not in value:
            raise ValueError("unsupported first failure missing prerequisite")
        if "replacementLane" not in value:
            raise ValueError("unsupported first failure replacement lane is missing")
    fields = UNSUPPORTED_FAILURE_FIELDS if unsupported else FAILURE_FIELDS
    failure = _require_exact_fields(value, fields, "first failure")
    for field in FAILURE_FIELDS:
        _require_bounded_string(
            failure.get(field), f"first failure {field}",
            64 if field == "stage" else 512,
        )
    if unsupported:
        _require_bounded_string(
            failure.get("missingPrerequisite"), "missing prerequisite", 256
        )
        _require_bounded_string(
            failure.get("replacementLane"), "replacement lane", 128
        )


def _validate_flip(value: object) -> None:
    if not isinstance(value, dict):
        raise ValueError("FLIP observation is invalid")
    state = value.get("state")
    if state in ("not-run", "not-required"):
        _require_exact_fields(value, {"state", "reason"}, f"FLIP {state}")
        _require_bounded_string(value.get("reason"), f"FLIP {state} reason", 256)
        return
    fields = {
        "state", "baselineId", "mean", "p95", "maximum",
        "badPixelThreshold", "badPixelFraction", "passed",
    }
    measured = _require_exact_fields(value, fields, "measured FLIP")
    if state != "measured":
        raise ValueError("FLIP state is invalid")
    _require_bounded_string(measured.get("baselineId"), "FLIP baseline", 128)
    for field in ("mean", "p95", "maximum", "badPixelThreshold", "badPixelFraction"):
        metric = measured.get(field)
        if not isinstance(metric, (int, float)) or isinstance(metric, bool) or not 0 <= metric <= 1:
            raise ValueError(f"FLIP {field} is invalid")
    if not isinstance(measured.get("passed"), bool):
        raise ValueError("FLIP result is invalid")


def _validate_observations(value: object, backend: str) -> None:
    if not isinstance(value, dict) or not set(value).issubset(OBSERVATION_FIELDS):
        raise ValueError("observations fields are invalid")
    for field in ("hostClass", "deviceDescription"):
        if field in value:
            _require_bounded_string(value[field], field, 128 if field == "hostClass" else 256)
    for field in (
        "durationMilliseconds", "peakRssBytes", "taskVmBytes",
        "allocatorBytes", "lifecycleCycles",
    ):
        if field in value and (not isinstance(value[field], int) or isinstance(value[field], bool) or value[field] < 0):
            raise ValueError(f"observation {field} is invalid")
    if "rssGrowthBytes" in value and (not isinstance(value["rssGrowthBytes"], int) or isinstance(value["rssGrowthBytes"], bool)):
        raise ValueError("observation rssGrowthBytes is invalid")
    native = backend in ("vulkan", "metal")
    if native:
        device_class = value.get("deviceClass")
        if not isinstance(device_class, str) or not TOKEN.fullmatch(device_class):
            raise ValueError("native report device class is invalid")
        if "flip" not in value:
            raise ValueError("native report FLIP observation is missing")
        _validate_flip(value["flip"])
    elif "deviceClass" in value or "flip" in value or "deviceDescription" in value:
        raise ValueError("non-native observations contain native fields")


def _validate_preflight(value: object, execution_class: str) -> dict:
    if not isinstance(value, dict):
        raise ValueError("authority preflight is invalid")
    state = value.get("state")
    if execution_class in ("github-hosted", "local-diagnostic"):
        preflight = _require_exact_fields(
            value, {"state", "reason"}, "authority preflight"
        )
        if state != "not-required":
            raise ValueError("authority preflight promotion is forbidden")
        _require_bounded_string(preflight.get("reason"), "preflight reason", 256)
        return preflight
    if state == "passed":
        preflight = _require_exact_fields(
            value, {"state", "evidenceDigest"}, "authority preflight"
        )
        if not isinstance(preflight.get("evidenceDigest"), str) or not SHA256.fullmatch(
            preflight["evidenceDigest"]
        ):
            raise ValueError("authority preflight evidence digest is invalid")
        return preflight
    if state == "failed":
        preflight = _require_exact_fields(
            value, {"state", "reason", "replacementLane"},
            "authority preflight",
        )
        _require_bounded_string(preflight.get("reason"), "preflight reason", 256)
        _require_bounded_string(
            preflight.get("replacementLane"), "preflight replacement lane", 128
        )
        return preflight
    raise ValueError("controlled physical preflight is invalid")


def _validate_observed_measurement(value: object, kind: str) -> None:
    record = _require_exact_fields(
        value, {"disposition", "state"}, f"{kind} measurement"
    )
    if record != {"disposition": "observed", "state": "measured"}:
        raise ValueError(f"{kind} observed measurement is invalid")


def _validate_not_required_measurement(value: object, kind: str) -> dict:
    record = _require_exact_fields(
        value, {"disposition", "state", "reason"}, f"{kind} measurement"
    )
    if record.get("disposition") != "not-required" or record.get("state") != "not-required":
        raise ValueError(f"{kind} not-required measurement is invalid")
    _require_bounded_string(record.get("reason"), f"{kind} not-required reason", 256)
    return record


def _validate_required_measurement(
    value: object, kind: str, preflight: dict
) -> dict:
    if preflight.get("state") == "failed":
        record = _require_exact_fields(
            value,
            {"disposition", "state", "reason", "replacementLane"},
            f"required {kind} measurement",
        )
        if (
            record.get("disposition") != "required"
            or record.get("state") != "not-run"
            or record.get("replacementLane") != preflight.get("replacementLane")
        ):
            raise ValueError(f"required {kind} not-run measurement is invalid")
        _require_bounded_string(record.get("reason"), f"required {kind} reason", 256)
        return record
    fields = {
        "disposition", "state", "passed", "preflightEvidenceDigest",
    }
    if kind == "rss":
        fields.add("threshold")
    record = _require_exact_fields(value, fields, f"required {kind} measurement")
    if (
        record.get("disposition") != "required"
        or record.get("state") != "measured"
        or not isinstance(record.get("passed"), bool)
        or record.get("preflightEvidenceDigest") != preflight.get("evidenceDigest")
    ):
        raise ValueError(f"required {kind} authority is invalid")
    if kind == "rss" and (
        not isinstance(record.get("threshold"), int)
        or isinstance(record.get("threshold"), bool)
        or record["threshold"] < 0
    ):
        raise ValueError("required rss threshold is invalid")
    return record


def _validate_authority(
    value: object, observations: dict, backend: str, result: str,
    first_failure: object,
) -> dict:
    authority = _require_exact_fields(value, AUTHORITY_FIELDS, "authority")
    execution_class = authority.get("executionClass")
    if execution_class not in (
        "github-hosted", "controlled-physical", "local-diagnostic"
    ):
        raise ValueError("execution class is invalid")
    preflight = _validate_preflight(authority.get("preflight"), execution_class)
    measurements = authority.get("measurements")
    if (
        not isinstance(measurements, dict)
        or not {"timing", "rss", "image"}.issubset(measurements)
        or not set(measurements).issubset(MEASUREMENT_KINDS)
    ):
        raise ValueError("authority measurement fields are invalid")
    timing = _require_exact_fields(
        measurements["timing"],
        {"disposition", "state", "completed"}, "timing measurement",
    )
    if (
        timing.get("disposition") != "operational"
        or timing.get("state") != "measured"
        or not isinstance(timing.get("completed"), bool)
    ):
        raise ValueError("operational timing measurement is invalid")
    if timing["completed"] is False:
        if (
            result != "failed" or not isinstance(first_failure, dict)
            or first_failure.get("stage") != "timeout"
        ):
            raise ValueError("operational timeout requires incomplete failed work")
    elif result == "passed" and timing["completed"] is not True:
        raise ValueError("Passed report requires completed operational work")

    for kind, observation_field in (
        ("taskVm", "taskVmBytes"),
        ("allocator", "allocatorBytes"),
        ("peakRss", "peakRssBytes"),
    ):
        present = kind in measurements
        if present != (observation_field in observations):
            raise ValueError(f"{kind} observation and authority differ")
        if present:
            _validate_observed_measurement(measurements[kind], kind)

    if execution_class in ("github-hosted", "local-diagnostic"):
        try:
            _validate_observed_measurement(measurements["rss"], "rss")
            image = _validate_not_required_measurement(
                measurements["image"], "image"
            )
        except ValueError as error:
            raise ValueError("authority promotion is forbidden") from error
        if "rssGrowthBytes" not in observations:
            raise ValueError("observed RSS value is missing")
        if backend in ("vulkan", "metal"):
            flip = observations.get("flip")
            if (
                not isinstance(flip, dict)
                or flip.get("state") != "not-required"
                or flip.get("reason") != image.get("reason")
            ):
                raise ValueError("not-required image must use its stable reason")
        return authority

    rss = _validate_required_measurement(measurements["rss"], "rss", preflight)
    image = _validate_required_measurement(
        measurements["image"], "image", preflight
    )
    if preflight.get("state") == "failed":
        if result != "unsupported":
            raise ValueError("failed physical preflight must be Unsupported")
        return authority
    if "rssGrowthBytes" not in observations:
        raise ValueError("required RSS value is missing")
    expected_rss = observations["rssGrowthBytes"] <= rss["threshold"]
    if rss["passed"] != expected_rss:
        raise ValueError("required rss result differs from threshold")
    if backend not in ("vulkan", "metal"):
        raise ValueError("required image authority needs a native backend")
    flip = observations.get("flip")
    if not isinstance(flip, dict) or flip.get("state") != "measured":
        raise ValueError("required image must be measured")
    if image["passed"] != flip.get("passed"):
        raise ValueError("required image result differs from FLIP")
    if result == "passed" and rss["passed"] is not True:
        raise ValueError("required rss must pass for a Passed report")
    if result == "passed" and image["passed"] is not True:
        raise ValueError("required image must pass for a Passed report")
    return authority


def _validate_artifacts(artifacts: object, evidence_root: Path | None) -> list[dict]:
    if not isinstance(artifacts, list):
        raise ValueError("artifacts must be an array")
    if len(artifacts) > MAX_ARTIFACTS:
        raise ValueError("report exceeds 64 artifacts")
    keys = []
    aggregate = 0
    for item in artifacts:
        artifact = _require_exact_fields(item, ARTIFACT_FIELDS, "artifact")
        if artifact.get("kind") not in ARTIFACT_KINDS:
            raise ValueError("artifact kind is invalid")
        if not _safe_path_token(artifact.get("pathToken")):
            raise ValueError("artifact path is invalid")
        if not isinstance(artifact.get("sha256"), str) or not SHA256.fullmatch(artifact["sha256"]):
            raise ValueError("artifact digest is invalid")
        size = artifact.get("sizeBytes")
        if not isinstance(size, int) or isinstance(size, bool) or size < 0:
            raise ValueError("artifact size is invalid")
        if size > MAX_ARTIFACT_BYTES:
            raise ValueError("artifact exceeds 64 MiB")
        aggregate += size
        keys.append((artifact["kind"], artifact["pathToken"]))
        if evidence_root is not None:
            path = (evidence_root.resolve() / artifact["pathToken"]).resolve()
            try:
                path.relative_to(evidence_root.resolve())
            except ValueError as error:
                raise ValueError("artifact path escapes evidence root") from error
            if not path.is_file():
                raise ValueError("artifact is missing")
            payload = path.read_bytes()
            if len(payload) != size:
                raise ValueError("artifact size differs")
            if hashlib.sha256(payload).hexdigest() != artifact["sha256"]:
                raise ValueError("artifact digest differs")
    if aggregate > MAX_AGGREGATE_BYTES:
        raise ValueError("artifact aggregate exceeds 256 MiB")
    if keys != sorted(keys) or len(keys) != len(set(keys)):
        raise ValueError("artifact order or uniqueness is invalid")
    return artifacts


def validate_report(report: object, evidence_root: Path | None = None) -> None:
    if len(canonical_bytes(report)) > MAX_REPORT_BYTES:
        raise ValueError("canonical report exceeds 1 MiB")
    root = _require_exact_fields(report, ROOT_FIELDS, "report")
    if root.get("schema") != "stoner.production-acceptance-report" or root.get("schemaVersion") != 2:
        raise ValueError("report schema is invalid")
    deterministic = _require_exact_fields(
        root.get("deterministic"), DETERMINISTIC_FIELDS, "deterministic"
    )
    for field in (
        "sourceSetDigest", "targetProfileDigest", "dependencyCoverageDigest",
        "evidenceDigest",
    ):
        if not isinstance(deterministic.get(field), str) or not SHA256.fullmatch(deterministic[field]):
            raise ValueError(f"deterministic {field} is invalid")
    for field in ("corpusRevision", "packageId", "rootAssetId", "workloadRevision"):
        _require_bounded_string(deterministic.get(field), field, 512)
    if deterministic.get("mode") not in ("development", "strict-cooked"):
        raise ValueError("mode is invalid")
    backend = deterministic.get("backend")
    result = deterministic.get("result")
    if backend not in ("none", "vulkan", "metal"):
        raise ValueError("backend is invalid")
    if result not in ("passed", "failed", "unsupported"):
        raise ValueError("result is invalid")
    generation = deterministic.get("generationIdentity")
    if result == "passed":
        if not isinstance(generation, str) or not SHA256.fullmatch(generation):
            raise ValueError("Passed report generation is invalid")
        if deterministic.get("firstFailure") is not None:
            raise ValueError("Passed report first failure must be null")
    else:
        if generation != "not-created" and (
            not isinstance(generation, str) or not SHA256.fullmatch(generation)
        ):
            raise ValueError("failure report generation is invalid")
        _validate_failure(deterministic.get("firstFailure"), result == "unsupported")
    observations = root.get("observations")
    _validate_observations(observations, backend)
    _validate_authority(
        root.get("authority"), observations, backend, result,
        deterministic.get("firstFailure"),
    )
    artifacts = _validate_artifacts(root.get("artifacts"), evidence_root)
    if deterministic["evidenceDigest"] != evidence_digest(artifacts):
        raise ValueError("evidence digest differs from artifact index")


def select_first_failure(failures: Iterable[dict]) -> dict | None:
    candidates = list(failures)
    if not candidates:
        return None
    for failure in candidates:
        unsupported = "missingPrerequisite" in failure or "replacementLane" in failure
        _validate_failure(failure, unsupported)
    return min(
        candidates,
        key=lambda item: (
            STAGE_ORDER.get(item["stage"], len(STAGE_ORDER)),
            item["stage"], item["category"], item["subject"],
        ),
    )


def validate_aggregate_authority(reports: Sequence[dict]) -> dict:
    if not reports:
        raise ValueError("aggregate authority requires at least one report")
    expected = None
    for report in reports:
        try:
            validate_report(report)
        except ValueError as error:
            raise ValueError("aggregate authority is invalid or promoted") from error
        authority = report["authority"]
        encoded = canonical_bytes(authority)
        if expected is None:
            expected = encoded
        elif encoded != expected:
            raise ValueError("aggregate authority differs or was promoted")
    return dict(reports[0]["authority"])


def build_report(
    deterministic: dict,
    authority: dict,
    observations: dict,
    artifact_inputs: Sequence[tuple[Path, str]],
    evidence_root: Path,
    failures: Iterable[dict],
) -> dict:
    expected_input_fields = DETERMINISTIC_FIELDS - {
        "evidenceDigest", "firstFailure",
    }
    if not isinstance(deterministic, dict) or set(deterministic) != expected_input_fields:
        raise ValueError("deterministic builder fields are invalid")
    artifacts = sorted(
        (
            index_artifact(path, evidence_root, kind)
            for path, kind in artifact_inputs
        ),
        key=lambda item: (item["kind"], item["pathToken"]),
    )
    selected_failure = select_first_failure(failures)
    result = deterministic.get("result")
    if result == "passed" and selected_failure is not None:
        raise ValueError("Passed report cannot contain a failure")
    if result in ("failed", "unsupported") and selected_failure is None:
        raise ValueError("non-Passed report requires a first failure")
    completed_deterministic = dict(deterministic)
    completed_deterministic["evidenceDigest"] = evidence_digest(artifacts)
    completed_deterministic["firstFailure"] = selected_failure
    report = {
        "schema": "stoner.production-acceptance-report",
        "schemaVersion": 2,
        "deterministic": completed_deterministic,
        "authority": dict(authority),
        "observations": dict(observations),
        "artifacts": artifacts,
    }
    validate_report(report, evidence_root)
    return report


def write_report(path: Path, report: dict, evidence_root: Path) -> None:
    validate_report(report, evidence_root)
    payload = canonical_bytes(report) + b"\n"
    if len(payload) > MAX_REPORT_BYTES:
        raise ValueError("canonical report exceeds 1 MiB")
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)


def validate_schema_contract(path: Path) -> None:
    schema = json.loads(path.read_text(encoding="utf-8"))
    if (
        not isinstance(schema, dict)
        or set(schema.get("required", [])) != ROOT_FIELDS
        or schema.get("properties", {}).get("schema", {}).get("const") !=
            "stoner.production-acceptance-report"
        or schema.get("properties", {}).get("schemaVersion", {}).get("const") != 2
    ):
        raise ValueError("acceptance report root schema differs from runtime")
    deterministic = schema["properties"]["deterministic"]
    observations = schema["properties"]["observations"]
    authority = schema["properties"]["authority"]
    artifacts = schema["properties"]["artifacts"]
    artifact_item = artifacts["items"]
    if (
        set(deterministic.get("required", [])) != DETERMINISTIC_FIELDS
        or set(authority.get("required", [])) != AUTHORITY_FIELDS
        or set(observations.get("properties", {})) != OBSERVATION_FIELDS
        or set(artifact_item.get("required", [])) != ARTIFACT_FIELDS
        or set(artifact_item["properties"]["kind"].get("enum", [])) !=
            ARTIFACT_KINDS
        or artifacts.get("maxItems") != MAX_ARTIFACTS
        or artifact_item["properties"]["sizeBytes"].get("maximum") !=
            MAX_ARTIFACT_BYTES
    ):
        raise ValueError("acceptance report schema fields differ from runtime")
