#!/usr/bin/env python3

from __future__ import annotations

import copy
import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]


def load(name: str, relative: str):
    path = ROOT / relative
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


RUNNER = load("aggregate_runner", ".github/scripts/run_output_transform_validation.py")
VERIFY = load("aggregate_verify", ".github/scripts/verify_output_transform_evidence.py")
AGGREGATE = load("aggregate_output", ".github/scripts/aggregate_output_transform_validation.py")
PROBE_FIXTURE = load("aggregate_probe_fixture", ".github/scripts/test_run_output_transform_validation.py")


def preflights() -> list[dict]:
    return [{
        "profileId": profile, "executionResult": "passed",
        "commandCompleted": True, "readbackCompleted": True,
        "presented": True, "modeGeneration": index + 1,
        "firstFrameToken": f"f-{index}-1", "lastFrameToken": f"f-{index}-2",
        "settledFrameToken": f"f-{index}-2", "readbackDigest": str(index + 1) * 64,
        "hdrMetadataDigest": None,
        "displayAdaptation": "system-color-management" if index < 2 else "none",
    } for index, profile in enumerate(RUNNER.HDR_PROFILES)]


def request_and_bytes() -> tuple[dict, bytes]:
    value = RUNNER.build_hdr_review_request(
        request_id="request-1", git_revision="a" * 40,
        workload_revision="output-transform-hdr-v1",
        device_class="macos.apple8.metal.hdr", display_class="apple-xdr-edr",
        display_capability_digest="b" * 64, native_report_digest="c" * 64,
        review_session_id="session-1", profile_preflights=preflights())
    return value, RUNNER.canonical_bytes(value)


def attestation(request: dict, request_bytes: bytes, decision: str = "pass") -> dict:
    return {
        "schema": "stoner.hdr-live-view-attestation", "schemaVersion": 1,
        "attestationId": "attestation-1", "requestId": request["requestId"],
        "requestSha256": VERIFY.sha256_bytes(request_bytes),
        "gitRevision": request["gitRevision"],
        "workloadRevision": request["workloadRevision"],
        "maintainerId": "maintainer", "reviewedAt": "2026-09-02T12:00:00+08:00",
        "deviceClass": request["deviceClass"], "displayClass": request["displayClass"],
        "displayCapabilityDigest": request["displayCapabilityDigest"],
        "reviewSessionId": request["reviewSessionId"],
        "observations": [{"profileId": profile, "viewedLive": True,
                          "decision": decision, "observedDefects": [], "notes": ""}
                         for profile in RUNNER.HDR_PROFILES],
        "acknowledgements": list(VERIFY.ACKNOWLEDGEMENTS),
        "supersedesAttestationId": None,
    }


def accepted_record(workload: str, backend: str) -> dict:
    device = "macos.apple8.metal" if backend == "metal" else "windows.discrete.vulkan"
    return {
        "schema": "stoner.sdr-image-baseline", "schemaVersion": 3,
        "baselineId": f"{workload}.{backend}", "state": "accepted",
        "workloadRevision": workload, "backend": backend, "deviceClass": device,
        "capabilityDigest": "1" * 64, "outputDeviceProfileId": "Sdr.sRGB.v1",
        "transformVersion": "Sdr.KhronosPbrNeutral.v1", "exposureStops": 0,
        "settingsDigest": RUNNER.SETTINGS_DIGEST, "width": 512, "height": 512,
        "sampleCount": 1, "referencePath": f"{workload}-{backend}.png",
        "compressedSha256": "2" * 64, "decodedSha256": "3" * 64,
        "calibrationEvidenceSha256": "4" * 64,
        "flipPolicy": {"meanMax": .0005, "p95Max": .001, "maximumMax": .01,
                       "badPixelThreshold": .05, "badPixelFractionMax": .001},
        "acceptance": {"maintainerId": "maintainer",
                       "reviewedAt": "2026-09-02T12:00:00+08:00",
                       "candidateSha256": "2" * 64, "decision": "accepted"},
    }


def registry() -> dict:
    return {"schema": "stoner.sdr-image-baseline-registry", "schemaVersion": 3,
            "registryId": "output-transform-sdr-baselines-v3",
            "records": [accepted_record(workload, backend)
                        for workload in ("production-content-lantern-v3",
                                         "production-content-sponza-v3")
                        for backend in ("metal", "vulkan")]}


def complete_fixture(root: Path) -> tuple[list[dict], dict, dict, bytes, list[dict]]:
    """Synthetic unit-test evidence only; never persisted as real authority."""
    reports, records, probes, hdr_reports = [], [], [], []
    provenance = AGGREGATE.PROVENANCE
    for workload in ("production-content-lantern-v3", "production-content-sponza-v3"):
        for backend in ("metal", "vulkan"):
            folder = root / f"{workload}-{backend}"
            platform, device = provenance.PHYSICAL_SDR[backend]
            rgb = bytes([37, 91, 143]) * (512 * 512)
            evidence = {"capturesPerProcess": 20,
                        "decodedPixelSha256": RUNNER.sha256_bytes(rgb),
                        "noise": {"mean": 0, "p95": 0, "maximum": 0},
                        "processOrdinals": [1, 2, 3]}
            calibration_digest = RUNNER.sha256_bytes(
                RUNNER.canonical_bytes(evidence).rstrip(b"\n"))
            candidate, png = RUNNER.generate_sdr_candidate(
                {"workloadRevision": workload}, backend, device, "1" * 64,
                calibration_digest, rgb, folder)
            calibration = {
                "schema": "stoner.production-cross-process-calibration", "schemaVersion": 1,
                "gitRevision": "a" * 40, "backend": backend, "workloadRevision": workload,
                "candidateOnly": True, "capturesPerProcess": 20, "processCount": 3,
                "processes": [{"ordinal": ordinal, "decodedPixelSha256": candidate["decodedSha256"],
                               "firstFrameToken": 1, "lastFrameToken": 20,
                               "staleFrameMutation": {"expectedFrameToken": 20,
                                   "observedFrameToken": 1, "rejected": True}}
                              for ordinal in (1, 2, 3)],
                "modes": [{"calibrationEvidenceSha256": calibration_digest,
                           "decodedPixelSha256": candidate["decodedSha256"],
                           "candidatePngSha256": candidate["compressedSha256"],
                           "flipPolicy": candidate["flipPolicy"],
                           "mutationsRejected": sorted(provenance.MUTATIONS),
                           "processOrdinals": [1, 2, 3], "crossProcessNoise": evidence["noise"]}]}
            probe = PROBE_FIXTURE.OutputTransformRunnerTests()._native_probe()
            probe.update(backend=backend, hostPlatform=platform, deviceClass=device,
                         workloadRevision=workload, insertionDigest=RUNNER.EMPTY_INSERTION_DIGEST,
                         format="rgba8-unorm",
                         readbackDigest=RUNNER.sha256_bytes(bytes([37, 91, 143, 255]) * (512 * 512)))
            for name, value in (("candidate", candidate), ("calibration", calibration), ("probe", probe)):
                (folder / f"{name}.json").write_bytes(RUNNER.canonical_bytes(value))
            report = RUNNER.build_native_output_report(probe, "a" * 40, "deferred")
            report["executionTier"] = "sdr-image-authority"
            report["native"]["claimDisposition"] = "sdr-authority"
            report["artifacts"] = [provenance.artifact(path, root) for path in
                                   (folder / "candidate.json", png, folder / "calibration.json", folder / "probe.json")]
            reports.append(report)
            accepted = copy.deepcopy(candidate)
            accepted.update(state="accepted", referencePath=png.relative_to(root).as_posix(),
                            acceptance={"maintainerId": "test-only", "reviewedAt": "2026-09-03",
                                        "candidateSha256": candidate["compressedSha256"], "decision": "accepted"})
            records.append(accepted)
    for index, profile in enumerate(RUNNER.HDR_PROFILES):
        probe = PROBE_FIXTURE.OutputTransformRunnerTests()._native_probe("native-hdr-nonvisual")
        probe.update(outputDeviceProfileId=profile, deviceClass="macos.apple8.metal.hdr",
                     workloadRevision="output-transform-hdr-v1",
                     format="bgr10a2-unorm" if index < 2 else "rgba16-float",
                     displayAdaptation="system-color-management" if index < 2 else "none")
        path = root / f"hdr-{index}.json"
        path.write_bytes(RUNNER.canonical_bytes(probe))
        report = RUNNER.build_native_output_report(probe, "a" * 40, "deferred")
        report["artifacts"] = [provenance.artifact(path, root)]
        hdr_reports.append(report)
        probes.append(probe)
    request = RUNNER.build_hdr_review_request(
        request_id="test-only", git_revision="a" * 40, workload_revision="output-transform-hdr-v1",
        device_class="macos.apple8.metal.hdr", display_class="apple-xdr-edr",
        display_capability_digest="b" * 64, native_report_digest=provenance.digest(hdr_reports),
        review_session_id="test-only", profile_preflights=probes)
    payload = RUNNER.canonical_bytes(request)
    value = registry()
    value["records"] = records
    return reports + hdr_reports, value, request, payload, [attestation(request, payload)]


class OutputTransformAggregateTests(unittest.TestCase):
    def test_empty_machine_reports_cannot_close_out(self) -> None:
        request, payload = request_and_bytes()
        result, findings = AGGREGATE.aggregate(
            [], registry(), request, payload, [attestation(request, payload)], "a" * 40)
        self.assertEqual("blocked", result["status"])
        self.assertTrue(any("required" in item for item in findings))

    def test_self_consistent_hdr_pair_must_match_target_revision(self) -> None:
        request, payload = request_and_bytes()
        result, findings = AGGREGATE.aggregate(
            [], registry(), request, payload, [attestation(request, payload)], "b" * 40)
        self.assertEqual("blocked", result["status"])
        self.assertTrue(any("HDR" in item and "revision" in item for item in findings))
        self.assertEqual([], result["hdrDecisions"])

    def test_invalid_target_revision_fails_closed(self) -> None:
        request, payload = request_and_bytes()
        _, findings = AGGREGATE.aggregate(
            [], registry(), request, payload, [attestation(request, payload)], "not-a-commit")
        self.assertTrue(any("target" in item and "revision" in item for item in findings))

    def test_complete_linked_human_and_sdr_authority_can_pass(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data = complete_fixture(root)
            result, findings = AGGREGATE.aggregate(*data, "a" * 40, root)
            reports, reg, request, payload, attestations = data
            (root / "registry.json").write_bytes(RUNNER.canonical_bytes(reg))
            (root / "request.json").write_bytes(payload)
            (root / "attestations").mkdir()
            (root / "attestations/test-only.json").write_bytes(RUNNER.canonical_bytes(attestations[0]))
            args = ["--sdr-registry", str(root / "registry.json"),
                    "--hdr-request", str(root / "request.json"),
                    "--attestation-dir", str(root / "attestations"),
                    "--git-revision", "a" * 40, "--root", str(root)]
            for index, report in enumerate(reports):
                path = root / f"report-{index}.json"
                path.write_bytes(RUNNER.canonical_bytes(report))
                args.extend(["--report", str(path)])
            for script, extra in (("aggregate_output_transform_validation.py", ["--output", str(root / "aggregate.json")]),
                                  ("verify_output_transform_evidence.py", ["--require-closeout"])):
                completed = subprocess.run([sys.executable, str(ROOT / ".github/scripts" / script), *args, *extra],
                                           capture_output=True, text=True)
                self.assertEqual(0, completed.returncode, completed.stdout + completed.stderr)
        self.assertEqual([], findings)
        self.assertEqual("passed", result["status"])
        self.assertEqual("none", result["windowsHdrClaim"])
        self.assertFalse(result["automaticHdrVisualDecision"])

    def test_each_required_report_and_exact_revision_are_mandatory(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            data = complete_fixture(root)
            for index in range(8):
                for mutation in ("missing", "stale", "failed", "not-run", "incomplete", "duplicate"):
                    with self.subTest(index=index, mutation=mutation):
                        altered = copy.deepcopy(data)
                        reports = altered[0]
                        if mutation == "missing":
                            reports.pop(index)
                        elif mutation == "duplicate":
                            reports.append(copy.deepcopy(reports[index]))
                        elif mutation == "stale":
                            reports[index]["gitRevision"] = "b" * 40
                        elif mutation == "incomplete":
                            reports[index]["native"]["commandCompleted"] = False
                        else:
                            reports[index]["executionResult"] = mutation
                        result, findings = AGGREGATE.aggregate(*altered, "a" * 40, root)
                        self.assertEqual("blocked", result["status"])
                        self.assertTrue(findings)

    def test_sdr_linkage_rejects_rehashed_artifact_mutations(self) -> None:
        for role, field, replacement in (
                ("calibration", "gitRevision", "b" * 40),
                ("calibration", "processes", []),
                ("candidate", "settingsDigest", "f" * 64),
                ("probe", "readbackDigest", "f" * 64),
                ("probe", "presented", False)):
            with self.subTest(role=role, field=field), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                data = complete_fixture(root)
                report = data[0][0]
                entry = next(item for item in report["artifacts"] if item["path"].endswith(f"/{role}.json"))
                path = root / entry["path"]
                value = json.loads(path.read_bytes())
                value[field] = replacement
                path.write_bytes(RUNNER.canonical_bytes(value))
                entry.update(AGGREGATE.PROVENANCE.artifact(path, root))
                result, findings = AGGREGATE.aggregate(*data, "a" * 40, root)
                self.assertEqual("blocked", result["status"])
                self.assertTrue(any("SDR provenance" in item for item in findings))

    def test_hdr_bundle_digest_and_nonready_request_block_quoting(self) -> None:
        for field, replacement in (("nativeReportDigest", "f" * 64),
                                   ("preflightDigest", "f" * 64), ("state", "not-run")):
            with self.subTest(field=field), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                reports, reg, request, _, _ = complete_fixture(root)
                request[field] = replacement
                payload = RUNNER.canonical_bytes(request)
                result, findings = AGGREGATE.aggregate(reports, reg, request, payload,
                    [attestation(request, payload)], "a" * 40, root)
                self.assertEqual("blocked", result["status"])
                self.assertTrue(findings)
                self.assertEqual([], result["hdrDecisions"])

    def test_verifier_closeout_flag_cannot_bypass_full_gate(self) -> None:
        completed = subprocess.run([sys.executable, str(ROOT / ".github/scripts/verify_output_transform_evidence.py"),
                                    "--require-closeout"], capture_output=True, text=True)
        self.assertNotEqual(0, completed.returncode)
        self.assertIn("closeout", completed.stdout)

    def test_human_fail_and_unmatched_attestation_block(self) -> None:
        request, payload = request_and_bytes()
        failed = attestation(request, payload, "fail")
        result, findings = AGGREGATE.aggregate(
            [], registry(), request, payload, [failed], "a" * 40)
        self.assertEqual("blocked", result["status"])
        self.assertTrue(any("must pass" in item for item in findings))
        unmatched = attestation(request, payload)
        unmatched["requestSha256"] = "0" * 64
        _, findings = AGGREGATE.aggregate(
            [], registry(), request, payload, [unmatched], "a" * 40)
        self.assertTrue(any("request SHA" in item for item in findings))

    def test_v2_carry_forward_and_missing_fresh_records_block(self) -> None:
        request, payload = request_and_bytes()
        invalid = registry()
        invalid["records"] = invalid["records"][:1]
        stale = copy.deepcopy(invalid["records"][0])
        stale["baselineId"] = "old-v2"
        stale["workloadRevision"] = "production-content-lantern-v2"
        invalid["records"].append(stale)
        _, findings = AGGREGATE.aggregate(
            [], invalid, request, payload, [attestation(request, payload)], "a" * 40)
        self.assertTrue(any("missing fresh" in item for item in findings))
        self.assertTrue(any("carry-forward" in item for item in findings))

    def test_windows_hdr_claim_stale_revision_and_unsupported_native_block(self) -> None:
        request, payload = request_and_bytes()
        report = RUNNER.build_windows_hdr_no_claim("b" * 40, "contract-v1")
        report["executionTier"] = "native-nonvisual"
        report["native"]["claimDisposition"] = "nonvisual-only"
        _, findings = AGGREGATE.aggregate(
            [report], registry(), request, payload, [attestation(request, payload)], "a" * 40)
        self.assertTrue(any("stale" in item for item in findings))
        self.assertTrue(any("Windows HDR authority" in item for item in findings))
        self.assertTrue(any("Unsupported" in item for item in findings))


if __name__ == "__main__":
    unittest.main()
