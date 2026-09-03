#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / ".github/scripts/verify_output_transform_evidence.py"
SPEC = importlib.util.spec_from_file_location("verify_output_transform_evidence", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
VERIFY = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFY)


def valid_report() -> dict:
    return {
        "schema": "stoner.output-validation-report", "schemaVersion": 1,
        "gitRevision": "a" * 40, "workloadRevision": "contract-v1",
        "hostPlatform": "macos", "rendererStrategy": "deferred",
        "backend": "metal", "executionTier": "native-nonvisual",
        "deviceClass": "macos.apple8.metal", "capabilityDigest": "b" * 64,
        "sceneColor": {"format": "rgba16-float", "primaries": "rec709",
                       "whitePoint": "d65", "transfer": "linear",
                       "sampleCount": 1, "alphaMode": "opaque-one"},
        "extent": {"width": 512, "height": 512}, "dynamicRange": "hdr",
        "outputDeviceProfileId": "Hdr.PQ.Rec2020.1000.v1",
        "transformVersion": "Hdr.ACES2.0.0_2025-04-04.Rec2020D65.v1",
        "exposureStops": 0, "insertionDigest": "c" * 64,
        "transferOwner": "renderer-output-device-stage", "modeGeneration": 2,
        "frameToken": "frame-7", "executionResult": "passed",
        "firstFailure": None,
        "native": {"claimDisposition": "nonvisual-only",
                   "commandCompleted": True, "readbackCompleted": True,
                   "presented": True, "readbackFrameToken": "frame-7",
                   "presentationFrameToken": "frame-7", "format": "bgr10a2-unorm",
                   "colorSpace": "itu-r-2100-pq", "hdrMetadataDigest": None,
                   "displayAdaptation": "system-color-management"},
        "visualAuthority": {"disposition": "manual-review-required",
                            "reason": "Maintainer must inspect the live display."},
        "artifacts": [],
    }


class OutputTransformEvidenceTests(unittest.TestCase):
    def test_duplicate_keys_and_nonfinite_json_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "report.json"
            for payload in ('{"status":"failed","status":"passed"}', '{"value":NaN}', '{"value":Infinity}'):
                path.write_text(payload, encoding="utf-8")
                value, errors, _ = VERIFY.load_bounded_json(path)
                self.assertIsNone(value)
                self.assertTrue(errors)

    def test_duplicate_artifacts_and_nonboolean_completion_are_rejected(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "probe.json"
            path.write_bytes(b"{}")
            entry = {"path": path.name, "sha256": VERIFY.sha256_bytes(b"{}"), "sizeBytes": 2}
            self.assertTrue(any("duplicate" in item for item in VERIFY.validate_artifacts([entry, entry], root)))
        report = valid_report()
        report["native"]["commandCompleted"] = 1
        self.assertTrue(VERIFY.validate_output_report(report, ROOT))

    def test_valid_report_and_canonicalization_are_stable(self) -> None:
        report = valid_report()
        self.assertEqual([], VERIFY.validate_output_report(report, ROOT))
        first = VERIFY.canonical_json_bytes(report)
        second = VERIFY.canonical_json_bytes(json.loads(first))
        self.assertEqual(first, second)

    def test_metal_hdr_metadata_and_same_frame_are_fail_closed(self) -> None:
        report = valid_report()
        report["native"]["hdrMetadataDigest"] = "d" * 64
        report["native"]["presentationFrameToken"] = "frame-8"
        findings = VERIFY.validate_output_report(report, ROOT)
        self.assertTrue(any("EDRMetadata=nil" in item for item in findings))
        self.assertTrue(any("one frame" in item for item in findings))

    def test_json_and_artifact_bounds_and_privacy(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            huge = root / "huge.json"
            huge.write_bytes(b"{\"x\":\"" + b"x" * VERIFY.MAX_JSON_BYTES + b"\"}")
            _, findings, _ = VERIFY.load_bounded_json(huge)
            self.assertTrue(any("exceeds" in item for item in findings))
        artifacts = [{"path": "/Users/alice/private.png", "sha256": "a" * 64,
                      "sizeBytes": 1},
                     {"path": "capture.raw", "sha256": "b" * 64,
                      "sizeBytes": VERIFY.MAX_ARTIFACT_BYTES + 1}]
        findings = VERIFY.validate_artifacts(artifacts, ROOT)
        self.assertTrue(any("repository-relative" in item for item in findings))
        self.assertTrue(any("only bounded" in item for item in findings))
        self.assertTrue(any("per-artifact" in item for item in findings))

    def test_v3_registry_key_and_explicit_acceptance(self) -> None:
        registry = json.loads((ROOT / "Config/Validation/OutputTransform/SDR/Baselines-v3.json").read_text())
        self.assertEqual([], VERIFY.validate_sdr_registry(registry))
        record = {
            "schema": "stoner.sdr-image-baseline", "schemaVersion": 3,
            "baselineId": "candidate.one", "state": "accepted",
            "workloadRevision": "production-content-lantern-v3",
            "backend": "metal", "deviceClass": "macos.apple8.metal",
            "capabilityDigest": "1" * 64, "outputDeviceProfileId": "Sdr.sRGB.v1",
            "transformVersion": "Sdr.KhronosPbrNeutral.v1", "exposureStops": 0,
            "settingsDigest": "2" * 64, "width": 512, "height": 512,
            "sampleCount": 1, "referencePath": "candidate.png",
            "compressedSha256": "3" * 64, "decodedSha256": "4" * 64,
            "calibrationEvidenceSha256": "5" * 64,
            "flipPolicy": {"meanMax": .0005, "p95Max": .001,
                           "maximumMax": .01, "badPixelThreshold": .05,
                           "badPixelFractionMax": .001}, "acceptance": None}
        self.assertTrue(any("maintainer" in item for item in
                            VERIFY.validate_sdr_baseline(record)))


if __name__ == "__main__":
    unittest.main()
