#!/usr/bin/env python3
"""Feature 029 bounded validation orchestrator.

Automation may create SDR Candidates and a Metal HDR ready-for-live-review
request.  It cannot author, infer, or default a human visual decision.
"""

from __future__ import annotations

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys
from typing import Any


SCRIPT_DIR = Path(__file__).resolve().parent
COMPARE_SPEC = importlib.util.spec_from_file_location(
    "compare_output_transform_images",
    SCRIPT_DIR / "compare_output_transform_images.py")
assert COMPARE_SPEC is not None and COMPARE_SPEC.loader is not None
COMPARE = importlib.util.module_from_spec(COMPARE_SPEC)
COMPARE_SPEC.loader.exec_module(COMPARE)
PROVENANCE_SPEC = importlib.util.spec_from_file_location(
    "runner_output_provenance", SCRIPT_DIR / "output_transform_provenance.py")
assert PROVENANCE_SPEC is not None and PROVENANCE_SPEC.loader is not None
PROVENANCE = importlib.util.module_from_spec(PROVENANCE_SPEC)
PROVENANCE_SPEC.loader.exec_module(PROVENANCE)
VERIFY_SPEC = importlib.util.spec_from_file_location(
    "runner_output_verify", SCRIPT_DIR / "verify_output_transform_evidence.py")
assert VERIFY_SPEC is not None and VERIFY_SPEC.loader is not None
VERIFY = importlib.util.module_from_spec(VERIFY_SPEC)
VERIFY_SPEC.loader.exec_module(VERIFY)

SETTINGS_DIGEST = "2e9ec7a45efe6537db4e806e05cca54043d4a841aa013c67c941866e35d20d34"
EMPTY_INSERTION_DIGEST = hashlib.sha256(b"[]\n").hexdigest()
HDR_PROFILES = (
    "Hdr.PQ.Rec2020.1000.v1",
    "Hdr.PQ.Rec2020.2000.v1",
    "Hdr.Linear.1000.v1",
    "Hdr.Linear.2000.v1",
)
FORBIDDEN_AUTOMATION_ARGUMENTS = {
    "--accept", "--approve", "--visual-pass", "--attest",
    "--write-attestation", "--auto-accept", "--hdr-score",
}
NATIVE_PROFILES = {
    "native-sdr", "native-hdr-nonvisual", "lifecycle", "failure-injection",
}


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def canonical_bytes(value: Any) -> bytes:
    return (json.dumps(value, sort_keys=True, separators=(",", ":"),
                       ensure_ascii=False, allow_nan=False) + "\n").encode("utf-8")


def load_exact_p6_ppm(path: Path, expected_width: int = 512,
                      expected_height: int = 512) -> bytes:
    """Read the deliberately narrow native-capture P6 dialect.

    Comments, alternate maximum values, extra bytes, and any dimension change
    are rejected so Candidate generation cannot become an image-normalization
    stage.
    """
    payload = path.read_bytes()
    prefix = f"P6\n{expected_width} {expected_height}\n255\n".encode("ascii")
    expected_size = len(prefix) + expected_width * expected_height * 3
    if len(payload) != expected_size or not payload.startswith(prefix):
        raise ValueError(
            "PPM must be exact 512x512 P6/max255 with no comments or trailing bytes")
    return payload[len(prefix):]


def load_native_probe(path: Path) -> dict[str, Any]:
    value, errors, _ = VERIFY.load_bounded_json(path)
    if errors:
        raise ValueError("; ".join(errors))
    required = {
        "schema", "schemaVersion", "backend", "capabilityDigest",
        "commandCompleted", "deviceClass", "displayAdaptation",
        "exposureStops", "firstFailure", "format", "frameToken",
        "firstFrameToken", "lastFrameToken", "settledFrameToken",
        "hdrMetadataDigest", "height", "hostPlatform", "insertionDigest",
        "modeGeneration", "outputDeviceProfileId",
        "outstandingTerminalOwnerCount", "presented",
        "presentationFrameToken", "profileKind", "readbackCompleted",
        "readbackDigest", "readbackFrameToken", "status",
        "transformVersion", "width", "workloadRevision",
    }
    if not isinstance(value, dict) or set(value) != required:
        raise ValueError("native probe shape is invalid")
    if value.get("schema") != "stoner.output-native-probe" or value.get("schemaVersion") != 2:
        raise ValueError("native probe schema identity is invalid")
    if value.get("profileKind") not in NATIVE_PROFILES:
        raise ValueError("native probe profile is invalid")
    for key in ("backend", "deviceClass", "frameToken", "firstFrameToken",
                "lastFrameToken", "settledFrameToken", "hostPlatform",
                "outputDeviceProfileId", "transformVersion",
                "workloadRevision"):
        if not isinstance(value.get(key), str) or not re.fullmatch(
                r"[A-Za-z0-9][A-Za-z0-9._-]{0,127}", value[key]):
            raise ValueError(f"native probe {key} is invalid")
    for key in ("capabilityDigest", "insertionDigest"):
        if not isinstance(value.get(key), str) or not re.fullmatch(
                r"[0-9a-f]{64}", value[key]):
            raise ValueError(f"native probe {key} is invalid")
    if value.get("status") not in {"passed", "failed", "unsupported", "paused"}:
        raise ValueError("native probe status is invalid")
    passed = value.get("status") == "passed"
    if passed and (not value.get("commandCompleted") or
                   not value.get("readbackCompleted") or
                   not value.get("presented") or
                   value.get("outstandingTerminalOwnerCount") != 0 or
                   value.get("readbackFrameToken") != value.get("frameToken") or
                   value.get("presentationFrameToken") != value.get("frameToken") or
                   not isinstance(value.get("readbackDigest"), str) or
                   not re.fullmatch(r"[0-9a-f]{64}", value["readbackDigest"])):
        raise ValueError("passed native probe lacks same-frame terminal completion")
    if passed and value.get("firstFailure") is not None:
        raise ValueError("passed native probe cannot contain firstFailure")
    if not passed and not isinstance(value.get("firstFailure"), dict):
        raise ValueError("non-passing native probe requires firstFailure")
    hdr = value.get("profileKind") == "native-hdr-nonvisual"
    if hdr and (value.get("backend") != "metal" or
                value.get("hostPlatform") != "macos" or
                value.get("hdrMetadataDigest") is not None):
        raise ValueError("Metal is the only HDR native probe and requires EDRMetadata=nil")
    if hdr:
        pq = str(value.get("outputDeviceProfileId", "")).startswith("Hdr.PQ.")
        if value.get("displayAdaptation") != (
                "system-color-management" if pq else "none"):
            raise ValueError("Metal HDR display adaptation does not match the profile")
    return value


def capture_native_output(command_file: Path, probe_path: Path, git_revision: str,
                          profile: str, renderer_strategy: str, root: Path) -> dict[str, Any]:
    """Generate fresh native evidence; never relabel a pre-existing probe."""
    PROVENANCE.require_frozen_revision(Path.cwd(), git_revision)
    if probe_path.exists():
        raise ValueError("native capture requires a fresh probe path; existing evidence is immutable")
    command, errors, _ = VERIFY.load_bounded_json(command_file)
    if (errors or not isinstance(command, list) or not command or
            any(not isinstance(item, str) or not item for item in command)):
        raise ValueError("native command file must be a bounded JSON argv array")
    PROVENANCE.subprocess.run(command, cwd=Path.cwd(), check=True)
    PROVENANCE.require_frozen_revision(Path.cwd(), git_revision)
    probe = load_native_probe(probe_path)
    if probe["profileKind"] != profile:
        raise ValueError("requested profile does not match native probe")
    report = build_native_output_report(probe, git_revision, renderer_strategy)
    report["artifacts"] = [PROVENANCE.artifact(probe_path, root)]
    errors = VERIFY.validate_output_report(report, root)
    if errors:
        raise ValueError("; ".join(errors))
    return report


def build_native_output_report(probe: dict[str, Any], git_revision: str,
                               renderer_strategy: str) -> dict[str, Any]:
    if not re.fullmatch(r"[0-9a-f]{40}", git_revision):
        raise ValueError("git revision must be exact 40 lowercase hex")
    if renderer_strategy not in {"deferred", "forward"}:
        raise ValueError("renderer strategy is invalid")
    profile = probe["profileKind"]
    hdr = profile == "native-hdr-nonvisual"
    status = probe["status"]
    return {
        "schema": "stoner.output-validation-report",
        "schemaVersion": 1,
        "gitRevision": git_revision,
        "workloadRevision": probe["workloadRevision"],
        "hostPlatform": probe["hostPlatform"],
        "rendererStrategy": renderer_strategy,
        "backend": probe["backend"],
        "executionTier": "native-nonvisual",
        "deviceClass": probe["deviceClass"],
        "capabilityDigest": probe["capabilityDigest"],
        "sceneColor": {"format": "rgba16-float", "primaries": "rec709",
                       "whitePoint": "d65", "transfer": "linear",
                       "sampleCount": 1, "alphaMode": "opaque-one"},
        "extent": {"width": probe["width"], "height": probe["height"]},
        "dynamicRange": "hdr" if hdr else "sdr",
        "outputDeviceProfileId": probe["outputDeviceProfileId"],
        "transformVersion": probe["transformVersion"],
        "exposureStops": probe["exposureStops"],
        "insertionDigest": probe["insertionDigest"],
        "transferOwner": "renderer-output-device-stage",
        "modeGeneration": probe["modeGeneration"],
        "frameToken": probe["frameToken"],
        "executionResult": status,
        "firstFailure": probe["firstFailure"],
        "native": {
            "claimDisposition": "nonvisual-only",
            "commandCompleted": probe["commandCompleted"],
            "readbackCompleted": probe["readbackCompleted"],
            "presented": probe["presented"],
            "readbackFrameToken": probe["readbackFrameToken"],
            "presentationFrameToken": probe["presentationFrameToken"],
            "format": probe["format"],
            "colorSpace": "hdr10-st2084" if
                probe["outputDeviceProfileId"].startswith("Hdr.PQ.") else
                ("extended-srgb-linear" if hdr else "srgb-nonlinear"),
            "hdrMetadataDigest": probe["hdrMetadataDigest"],
            "displayAdaptation": probe["displayAdaptation"],
        },
        "visualAuthority": {
            "disposition": "manual-review-required" if hdr else "not-required",
            "reason": ("Native HDR state is non-visual evidence; live maintainer inspection is required."
                       if hdr else
                       "SDR native validation requires no HDR visual authority."),
        },
        "artifacts": [],
    }


def load_workload(path: Path) -> dict[str, Any]:
    value = json.loads(path.read_text(encoding="utf-8"))
    expected = {
        "dynamicRange": "sdr",
        "exposureStops": 0,
        "outputDeviceProfileId": "Sdr.sRGB.v1",
        "toneMapVersion": "Sdr.KhronosPbrNeutral.v1",
        "preTonemapOperations": [],
        "postTonemapOperations": [],
    }
    if (value.get("schema") != "stoner.output-transform-workload" or
            value.get("schemaVersion") != 3 or
            value.get("rendererStrategy") != "deferred" or
            (value.get("width"), value.get("height"), value.get("sampleCount")) != (512, 512, 1) or
            value.get("settings") != expected or
            value.get("settingsDigest") != SETTINGS_DIGEST or
            sha256_bytes(json.dumps(expected, sort_keys=True,
                                    separators=(",", ":"),
                                    ensure_ascii=False).encode("utf-8")) != SETTINGS_DIGEST or
            value.get("authorityPolicy") != "fresh-v3-candidate-explicit-maintainer-acceptance"):
        raise ValueError("workload is not the frozen exact v3 SDR contract")
    if not re.fullmatch(r"production-content-(lantern|sponza)-v3",
                        value.get("workloadRevision", "")):
        raise ValueError("workloadRevision must be a fresh v3 identity")
    return value


def generate_sdr_candidate(workload: dict[str, Any], backend: str,
                           device_class: str, capability_digest: str,
                           calibration_digest: str, rgb: bytes,
                           output_dir: Path) -> tuple[dict[str, Any], Path]:
    if backend not in {"vulkan", "metal"}:
        raise ValueError("Candidate backend must be Vulkan or Metal")
    if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9._-]{0,127}", device_class):
        raise ValueError("device class is invalid")
    for name, value in (("capability", capability_digest),
                        ("calibration", calibration_digest)):
        if not re.fullmatch(r"[0-9a-f]{64}", value):
            raise ValueError(f"{name} digest must be lowercase SHA-256")
    output_dir.mkdir(parents=True, exist_ok=True)
    name = f"{workload['workloadRevision']}-{backend}-{device_class}-candidate.png"
    path = output_dir / name
    compressed, decoded = COMPARE.write_lossless_rgb_png(
        path, 512, 512, rgb)
    record = {
        "schema": "stoner.sdr-image-baseline",
        "schemaVersion": 3,
        "baselineId": f"{workload['workloadRevision']}.{backend}.{device_class}",
        "state": "candidate",
        "workloadRevision": workload["workloadRevision"],
        "backend": backend,
        "deviceClass": device_class,
        "capabilityDigest": capability_digest,
        "outputDeviceProfileId": "Sdr.sRGB.v1",
        "transformVersion": "Sdr.KhronosPbrNeutral.v1",
        "exposureStops": 0,
        "settingsDigest": SETTINGS_DIGEST,
        "width": 512,
        "height": 512,
        "sampleCount": 1,
        "referencePath": path.name,
        "compressedSha256": compressed,
        "decodedSha256": decoded,
        "calibrationEvidenceSha256": calibration_digest,
        "flipPolicy": {
            "meanMax": 0.0005,
            "p95Max": 0.001,
            "maximumMax": 0.01,
            "badPixelThreshold": 0.05,
            "badPixelFractionMax": 0.001,
        },
        "acceptance": None,
    }
    return record, path


def build_windows_hdr_no_claim(git_revision: str,
                               workload_revision: str) -> dict[str, Any]:
    if not re.fullmatch(r"[0-9a-f]{40}", git_revision):
        raise ValueError("git revision must be exact 40 lowercase hex")
    return {
        "schema": "stoner.output-validation-report",
        "schemaVersion": 1,
        "gitRevision": git_revision,
        "workloadRevision": workload_revision,
        "hostPlatform": "windows",
        "rendererStrategy": "deferred",
        "backend": "vulkan",
        "executionTier": "deterministic-contract",
        "deviceClass": "windows-hdr-no-claim",
        "capabilityDigest": "0" * 64,
        "sceneColor": {"format": "rgba16-float", "primaries": "rec709",
                       "whitePoint": "d65", "transfer": "linear",
                       "sampleCount": 1, "alphaMode": "opaque-one"},
        "extent": {"width": 512, "height": 512},
        "dynamicRange": "hdr",
        "outputDeviceProfileId": "Hdr.PQ.Rec2020.1000.v1",
        "transformVersion": "Hdr.ACES2.0.0_2025-04-04.Rec2020D65.v1",
        "exposureStops": 0,
        "insertionDigest": EMPTY_INSERTION_DIGEST,
        "transferOwner": "renderer-output-device-stage",
        "modeGeneration": 0,
        "frameToken": "not-run",
        "executionResult": "unsupported",
        "firstFailure": None,
        "native": {"claimDisposition": "not-run",
                   "commandCompleted": False, "readbackCompleted": False,
                   "presented": False, "readbackFrameToken": None,
                   "presentationFrameToken": None, "format": "not-run",
                   "colorSpace": "not-run", "hdrMetadataDigest": None,
                   "displayAdaptation": "none"},
        "visualAuthority": {"disposition": "not-required",
                            "reason": "Feature 029 makes no Windows HDR validation claim."},
        "artifacts": [],
    }


def _profile_request_record(profile: dict[str, Any]) -> dict[str, Any]:
    if profile.get("schema") == "stoner.output-native-probe":
        profile = {
            "profileId": profile.get("outputDeviceProfileId"),
            "executionResult": profile.get("status"),
            "commandCompleted": profile.get("commandCompleted"),
            "readbackCompleted": profile.get("readbackCompleted"),
            "presented": profile.get("presented"),
            "modeGeneration": profile.get("modeGeneration"),
            "firstFrameToken": profile.get("firstFrameToken"),
            "lastFrameToken": profile.get("lastFrameToken"),
            "settledFrameToken": profile.get("settledFrameToken"),
            "readbackDigest": profile.get("readbackDigest"),
            "hdrMetadataDigest": profile.get("hdrMetadataDigest"),
            "displayAdaptation": profile.get("displayAdaptation"),
        }
    profile_id = profile.get("profileId")
    if profile_id not in HDR_PROFILES:
        raise ValueError("unexpected HDR profile preflight")
    pq = profile_id.startswith("Hdr.PQ.")
    peak = 1000 if ".1000." in profile_id else 2000
    required = ("modeGeneration", "firstFrameToken", "lastFrameToken",
                "settledFrameToken", "readbackDigest")
    if (profile.get("executionResult") != "passed" or
            not profile.get("commandCompleted") or
            not profile.get("readbackCompleted") or
            not profile.get("presented") or
            not all(profile.get(field) for field in required) or
            not re.fullmatch(r"[0-9a-f]{64}", profile["readbackDigest"])):
        raise ValueError(f"{profile_id}: non-visual preflight is incomplete")
    if profile.get("hdrMetadataDigest") is not None:
        raise ValueError(f"{profile_id}: Metal requires EDRMetadata=nil")
    expected_adaptation = "system-color-management" if pq else "none"
    if profile.get("displayAdaptation") != expected_adaptation:
        raise ValueError(f"{profile_id}: display adaptation mismatch")
    return {
        "profileId": profile_id,
        "targetPeakNits": peak,
        "transfer": "st2084" if pq else "metal-edr-linear",
        "gamut": "rec2020-d65" if pq else "rec709-d65",
        "displayAdaptation": expected_adaptation,
        "modeGeneration": profile["modeGeneration"],
        "firstFrameToken": str(profile["firstFrameToken"]),
        "lastFrameToken": str(profile["lastFrameToken"]),
        "settledFrameToken": str(profile["settledFrameToken"]),
        "readbackDigest": profile["readbackDigest"],
        "presentationReady": True,
    }


def build_hdr_review_request(*, request_id: str, git_revision: str,
                             workload_revision: str, device_class: str,
                             display_class: str,
                             display_capability_digest: str,
                             native_report_digest: str,
                             review_session_id: str,
                             profile_preflights: list[dict[str, Any]]) -> dict[str, Any]:
    if not isinstance(git_revision, str) or not re.fullmatch(r"[0-9a-f]{40}", git_revision):
        raise ValueError("HDR software revision must be exact 40 lowercase hex")
    if len(profile_preflights) != 4:
        raise ValueError("exactly four Metal HDR preflight records are required")
    records = [_profile_request_record(profile) for profile in profile_preflights]
    if [record["profileId"] for record in records] != list(HDR_PROFILES):
        raise ValueError("HDR preflights must use the frozen profile order")
    for digest in (display_capability_digest, native_report_digest):
        if not re.fullmatch(r"[0-9a-f]{64}", digest):
            raise ValueError("HDR request digests must be lowercase SHA-256")
    preflight_digest = sha256_bytes(canonical_bytes(profile_preflights))
    return {
        "schema": "stoner.hdr-live-review-request",
        "schemaVersion": 1,
        "requestId": request_id,
        "gitRevision": git_revision,
        "workloadRevision": workload_revision,
        "backend": "metal",
        "hostClass": "maintainer-local-macos-metal",
        "deviceClass": device_class,
        "displayClass": display_class,
        "displayCapabilityDigest": display_capability_digest,
        "preflightDigest": preflight_digest,
        "nativeReportDigest": native_report_digest,
        "reviewSessionId": review_session_id,
        "profiles": records,
        "state": "ready-for-live-review",
        "firstFailure": None,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="mode", required=True)
    candidate = sub.add_parser("candidate")
    candidate.add_argument("--workload", required=True)
    candidate.add_argument("--backend", choices=("vulkan", "metal"), required=True)
    candidate.add_argument("--device-class", required=True)
    candidate.add_argument("--capability-digest", required=True)
    candidate.add_argument("--calibration-digest", required=True)
    candidate_input = candidate.add_mutually_exclusive_group(required=True)
    candidate_input.add_argument("--rgb-input")
    candidate_input.add_argument("--ppm-input")
    candidate.add_argument("--output-dir", required=True)
    no_claim = sub.add_parser("windows-hdr-no-claim")
    no_claim.add_argument("--git-revision", required=True)
    no_claim.add_argument("--workload-revision", required=True)
    no_claim.add_argument("--output", required=True)
    native = sub.add_parser("native-capture")
    native.add_argument("--command-file", type=Path, required=True)
    native.add_argument("--profile", choices=sorted(NATIVE_PROFILES), required=True)
    native.add_argument("--probe", required=True)
    native.add_argument("--git-revision", required=True)
    native.add_argument("--renderer-strategy", choices=("deferred", "forward"),
                        default="deferred")
    native.add_argument("--output", required=True)
    native.add_argument("--root", type=Path, default=Path("."))
    sdr = sub.add_parser("sdr-report")
    sdr.add_argument("--probe", type=Path, required=True)
    sdr.add_argument("--native-report", type=Path, required=True)
    sdr.add_argument("--candidate", type=Path, required=True)
    sdr.add_argument("--calibration", type=Path, required=True)
    sdr.add_argument("--git-revision", required=True)
    sdr.add_argument("--root", type=Path, default=Path("."))
    sdr.add_argument("--output", type=Path, required=True)
    request = sub.add_parser("hdr-request")
    request.add_argument("--identity", required=True,
                         help="JSON containing request/display immutable identity")
    request.add_argument("--profile-preflight", action="append", required=True)
    request.add_argument("--native-report", action="append", required=True)
    request.add_argument("--root", type=Path, default=Path("."))
    request.add_argument("--output", required=True)
    return parser


def main() -> int:
    if any(argument.split("=", 1)[0] in FORBIDDEN_AUTOMATION_ARGUMENTS
           for argument in sys.argv[1:]):
        print("finding: HDR automated acceptance/attestation option is forbidden")
        return 2
    if any(operation in " ".join(sys.argv[1:]).lower() for operation in
           ("--align", "--crop", "--scale", "--warp", "--resample", "--resize")):
        print("finding: spatial-normalization option is forbidden")
        return 2
    args, unknown = build_parser().parse_known_args()
    if unknown or any(operation in " ".join(unknown).lower() for operation in
                      COMPARE.FORBIDDEN_SPATIAL_OPERATIONS):
        print("finding: unknown or spatial-normalization option is forbidden")
        return 2
    try:
        if args.mode == "candidate":
            workload = load_workload(Path(args.workload))
            rgb = (Path(args.rgb_input).read_bytes() if args.rgb_input else
                   load_exact_p6_ppm(Path(args.ppm_input)))
            record, _ = generate_sdr_candidate(
                workload, args.backend, args.device_class,
                args.capability_digest, args.calibration_digest, rgb,
                Path(args.output_dir))
            destination = Path(args.output_dir) / "candidate.json"
            destination.write_bytes(canonical_bytes(record))
        elif args.mode == "windows-hdr-no-claim":
            report = build_windows_hdr_no_claim(
                args.git_revision, args.workload_revision)
            Path(args.output).write_bytes(canonical_bytes(report))
        elif args.mode == "native-capture":
            if Path(args.output).exists():
                raise ValueError("native report output must be fresh")
            report = capture_native_output(args.command_file, Path(args.probe), args.git_revision,
                                           args.profile, args.renderer_strategy, args.root)
            Path(args.output).write_bytes(canonical_bytes(report))
        elif args.mode == "sdr-report":
            PROVENANCE.require_frozen_revision(Path.cwd(), args.git_revision)
            if args.output.exists():
                raise ValueError("SDR report output must be fresh")
            probe = load_native_probe(args.probe)
            candidate, errors, _ = VERIFY.load_bounded_json(args.candidate)
            if errors or not isinstance(candidate, dict):
                raise ValueError("invalid bounded Candidate JSON")
            report = build_native_output_report(probe, args.git_revision, "deferred")
            native_report, native_errors, _ = VERIFY.load_bounded_json(args.native_report)
            report["artifacts"] = [PROVENANCE.artifact(args.probe, args.root)]
            if native_errors or native_report != report:
                raise ValueError("SDR input must match the exact-revision native-capture report")
            report["executionTier"] = "sdr-image-authority"
            report["native"]["claimDisposition"] = "sdr-authority"
            report["artifacts"] = [PROVENANCE.artifact(path, args.root) for path in
                                   (args.candidate, args.candidate.parent / candidate["referencePath"],
                                    args.calibration, args.probe)]
            errors = VERIFY.validate_output_report(report, args.root)
            if not errors:
                errors = PROVENANCE.validate_sdr_bundle(
                    report, candidate, args.root, VERIFY.validate_sdr_baseline)
            if errors:
                raise ValueError("; ".join(errors))
            args.output.write_bytes(canonical_bytes(report))
        else:
            if Path(args.output).exists():
                raise ValueError("HDR review request output must be fresh")
            identity, errors, _ = VERIFY.load_bounded_json(Path(args.identity))
            if errors or not isinstance(identity, dict):
                raise ValueError("invalid bounded HDR identity JSON")
            PROVENANCE.require_frozen_revision(Path.cwd(), identity["git_revision"])
            preflights = [load_native_probe(Path(path))
                          for path in args.profile_preflight]
            native_reports = []
            for path in args.native_report:
                report, errors, _ = VERIFY.load_bounded_json(Path(path))
                if errors:
                    raise ValueError("; ".join(errors))
                errors = VERIFY.validate_output_report(report, args.root)
                if errors or report.get("gitRevision") != identity["git_revision"]:
                    raise ValueError("HDR native report revision/validation mismatch")
                native_reports.append(report)
            if [report["outputDeviceProfileId"] for report in native_reports] != list(HDR_PROFILES):
                raise ValueError("HDR native reports require the ordered four-profile set")
            for probe, report in zip(preflights, native_reports):
                linked = PROVENANCE.linked_probe(report, PROVENANCE.documents(report, args.root))
                if linked != probe or probe["workloadRevision"] != identity["workload_revision"] or probe["deviceClass"] != identity["device_class"]:
                    raise ValueError("HDR request/probe/report identity mismatch")
            native_digest = PROVENANCE.digest(native_reports)
            if identity.get("native_report_digest", native_digest) != native_digest:
                raise ValueError("HDR native report bundle digest mismatch")
            identity["native_report_digest"] = native_digest
            request = build_hdr_review_request(
                profile_preflights=preflights, **identity)
            Path(args.output).write_bytes(canonical_bytes(request))
    except (OSError, ValueError, KeyError, TypeError, PROVENANCE.subprocess.SubprocessError) as error:
        print(f"finding: {error}")
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
