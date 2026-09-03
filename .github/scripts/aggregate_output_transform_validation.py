#!/usr/bin/env python3
"""Aggregate Feature 029 evidence without creating visual authority."""

from __future__ import annotations

import argparse
import importlib.util
import json
from pathlib import Path
from typing import Any


SCRIPT_DIR = Path(__file__).resolve().parent
VERIFY_SPEC = importlib.util.spec_from_file_location(
    "verify_output_transform_evidence",
    SCRIPT_DIR / "verify_output_transform_evidence.py")
assert VERIFY_SPEC is not None and VERIFY_SPEC.loader is not None
VERIFY = importlib.util.module_from_spec(VERIFY_SPEC)
VERIFY_SPEC.loader.exec_module(VERIFY)
PROVENANCE_SPEC = importlib.util.spec_from_file_location(
    "output_transform_provenance", SCRIPT_DIR / "output_transform_provenance.py")
assert PROVENANCE_SPEC is not None and PROVENANCE_SPEC.loader is not None
PROVENANCE = importlib.util.module_from_spec(PROVENANCE_SPEC)
PROVENANCE_SPEC.loader.exec_module(PROVENANCE)


def aggregate(reports: list[dict[str, Any]], registry: dict[str, Any],
              request: dict[str, Any], request_bytes: bytes,
              attestations: list[dict[str, Any]],
              expected_git_revision: str,
              evidence_root: Path = Path(".")) -> tuple[dict[str, Any], list[str]]:
    findings: list[str] = []
    if not isinstance(expected_git_revision, str) or not VERIFY.GIT_RE.fullmatch(expected_git_revision):
        findings.append("target software revision must be exact 40 lowercase hex")
    if not isinstance(reports, list) or len(reports) > VERIFY.MAX_ARTIFACTS:
        findings.append("required machine reports must be a bounded array")
        reports = []
    valid_reports: list[dict] = []
    artifact_index: dict[str, dict] = {}
    for index, report in enumerate(reports):
        errors = VERIFY.validate_output_report(report, evidence_root)
        if not isinstance(report, dict):
            findings.extend(errors)
            continue
        if not isinstance(report.get("native"), dict):
            findings.extend(errors)
            continue
        if report.get("gitRevision") != expected_git_revision:
            errors.append("stale or mixed git revision")
        no_claim = (report.get("hostPlatform") == "windows" and
                    report.get("dynamicRange") == "hdr" and
                    report.get("executionTier") == "deterministic-contract" and
                    report.get("native", {}).get("claimDisposition") == "not-run")
        if report.get("executionResult") != "passed" and not no_claim:
            errors.append("required machine gate is not passed; Unsupported/not-run cannot satisfy it")
        if (report.get("hostPlatform") == "windows" and
                report.get("dynamicRange") == "hdr" and
                report.get("native", {}).get("claimDisposition") != "not-run"):
            errors.append("Windows HDR authority is prohibited")
        if report.get("executionResult") == "unsupported" and report.get("executionTier") != "deterministic-contract":
            errors.append("Unsupported cannot satisfy a native gate")
        findings.extend(f"report[{index}]: {item}" for item in errors)
        if not errors and not no_claim:
            valid_reports.append(report)
        if not errors:
            for entry in report["artifacts"]:
                previous = artifact_index.setdefault(entry["path"], entry)
                if previous != entry:
                    findings.append("artifact path has conflicting digest/size identities")
    findings.extend(VERIFY.validate_artifacts(list(artifact_index.values()), evidence_root))
    findings.extend(VERIFY.validate_sdr_registry(registry))
    records = registry.get("records", []) if isinstance(registry, dict) else []
    records = records if isinstance(records, list) else []
    accepted = [record for record in records if isinstance(record, dict) and
                record.get("state") == "accepted" and not VERIFY.validate_sdr_baseline(record)]
    required_sdr = {
        (workload, backend)
        for workload in ("production-content-lantern-v3",
                         "production-content-sponza-v3")
        for backend in ("metal", "vulkan")
    }
    observed_sdr = {(record.get("workloadRevision"), record.get("backend"))
                    for record in accepted}
    missing = sorted(required_sdr - observed_sdr)
    if missing:
        findings.append(f"SDR authority: missing fresh accepted v3 records {missing}")
    for workload, backend in sorted(required_sdr):
        matches = [record for record in accepted if
                   (record.get("workloadRevision"), record.get("backend")) == (workload, backend)]
        native_matches = [report for report in valid_reports if
                          (report.get("workloadRevision"), report.get("backend"),
                           report.get("executionTier"), report.get("dynamicRange")) ==
                          (workload, backend, "sdr-image-authority", "sdr")]
        if len(matches) != 1 or len(native_matches) != 1:
            findings.append(f"SDR authority: required exactly one Accepted record and physical report for {workload}/{backend}")
        elif not VERIFY.validate_sdr_baseline(matches[0]):
            findings.extend(PROVENANCE.validate_sdr_bundle(
                native_matches[0], matches[0], evidence_root, VERIFY.validate_sdr_baseline))
    if any(str(record.get("workloadRevision", "")).endswith("-v2")
           for record in records if isinstance(record, dict)):
        findings.append("SDR authority: Feature 028 carry-forward reuse is prohibited")
    hdr_findings = []
    current = []
    if not isinstance(request, dict) or request.get("gitRevision") != expected_git_revision:
        hdr_findings.append("HDR request/attestation revision differs from target software revision")
    request_errors = VERIFY.validate_hdr_request(request)
    hdr_findings.extend(request_errors)
    if not request_errors:
        linked_findings, current = VERIFY.validate_hdr_attestations(
            request, request_bytes, attestations, require_all_pass=True)
        hdr_findings.extend(linked_findings)
        if request_bytes != VERIFY.canonical_json_bytes(request):
            hdr_findings.append("HDR request bytes must be canonical and identify the supplied request")
        ordered_reports = []
        ordered_probes = []
        for profile in request["profiles"]:
            matches = [report for report in valid_reports if
                       report.get("dynamicRange") == "hdr" and
                       report.get("outputDeviceProfileId") == profile["profileId"]]
            if len(matches) != 1:
                hdr_findings.append(f"HDR required exactly one native report for {profile['profileId']}")
                continue
            report = matches[0]
            try:
                if (report["backend"] != "metal" or report["hostPlatform"] != "macos" or
                        report["deviceClass"] != request["deviceClass"] or
                        report["workloadRevision"] != request["workloadRevision"] or
                        report["executionTier"] != "native-nonvisual" or
                        report["native"]["claimDisposition"] != "nonvisual-only"):
                    raise ValueError("HDR report does not match request platform/device/workload")
                if (report["transformVersion"] != "Hdr.ACES2.0.0_2025-04-04.Rec2020D65.v1" or
                        report["native"]["format"] != ("bgr10a2-unorm" if profile["profileId"].startswith("Hdr.PQ.") else "rgba16-float")):
                    raise ValueError("HDR transform/native format differs from frozen Metal profile")
                docs = PROVENANCE.documents(report, evidence_root)
                if len(report["artifacts"]) != 1:
                    raise ValueError("HDR report requires exactly one bounded native probe artifact")
                probe = PROVENANCE.linked_probe(report, docs)
                if probe.get("profileKind") != "native-hdr-nonvisual":
                    raise ValueError("HDR profile must reference a native-hdr-nonvisual probe")
                for key in ("modeGeneration", "firstFrameToken", "lastFrameToken",
                            "settledFrameToken", "readbackDigest", "displayAdaptation"):
                    if probe.get(key) != profile[key]:
                        raise ValueError(f"HDR request/probe {key} mismatch")
                ordered_reports.append(report)
                ordered_probes.append(probe)
            except (ValueError, TypeError, KeyError, OSError) as error:
                hdr_findings.append(str(error))
        if len(ordered_reports) == 4:
            if PROVENANCE.digest(ordered_reports) != request["nativeReportDigest"]:
                hdr_findings.append("HDR native report bundle digest mismatch")
            if PROVENANCE.digest(ordered_probes) != request["preflightDigest"]:
                hdr_findings.append("HDR preflight bundle digest mismatch")
    findings.extend(hdr_findings)
    current_decisions: list[dict[str, str]] = []
    if len(current) == 1 and not hdr_findings:
        # Quoting is permitted only after request-SHA and immutable identity
        # linkage passed above. No machine-authored substitute is generated.
        current_decisions = [
            {"profileId": observation["profileId"],
             "humanAttestedDecision": observation["decision"]}
            for observation in current[0].get("observations", [])
            if isinstance(observation, dict) and
            observation.get("decision") in {"pass", "fail"}
        ]
    result = {
        "schema": "stoner.output-transform-validation-aggregate",
        "schemaVersion": 1,
        "gitRevision": expected_git_revision,
        "status": "passed" if not findings else "blocked",
        "findingCount": len(findings),
        "windowsHdrClaim": "none",
        "sdrAcceptedRecordCount": len(accepted),
        "hdrAuthoritySource": "linked-maintainer-attestation" if current_decisions else "missing",
        "hdrDecisions": current_decisions,
        "automaticHdrVisualDecision": False,
    }
    return result, findings


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--report", action="append", default=[])
    parser.add_argument("--sdr-registry", required=True)
    parser.add_argument("--hdr-request", required=True)
    parser.add_argument("--attestation-dir", required=True)
    parser.add_argument("--git-revision", required=True)
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--output", required=True)
    return parser


def main() -> int:
    args = build_parser().parse_args()
    try:
        def bounded(path: str | Path) -> tuple[Any, bytes]:
            value, errors, payload = VERIFY.load_bounded_json(Path(path))
            if errors:
                raise ValueError("; ".join(errors))
            return value, payload

        if len(args.report) > VERIFY.MAX_ARTIFACTS:
            raise ValueError("too many report inputs")
        reports = [bounded(path)[0] for path in args.report]
        registry = bounded(args.sdr_registry)[0]
        request_path = Path(args.hdr_request)
        request, request_bytes = bounded(request_path)
        attestation_paths = sorted(Path(args.attestation_dir).glob("*.json"))
        if len(attestation_paths) > VERIFY.MAX_ARTIFACTS:
            raise ValueError("too many attestation inputs")
        attestations = [bounded(path)[0] for path in attestation_paths]
        result, findings = aggregate(reports, registry, request, request_bytes,
                                     attestations, args.git_revision, args.root)
    except (OSError, ValueError, TypeError, KeyError) as error:
        print(f"finding: {error}")
        return 1
    Path(args.output).write_text(
        json.dumps(result, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    for finding in findings:
        print(f"finding: {finding}")
    return 0 if not findings else 1


if __name__ == "__main__":
    raise SystemExit(main())
