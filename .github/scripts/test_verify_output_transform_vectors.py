#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import hashlib
import json
from pathlib import Path
import sys
import unittest


ROOT = Path(__file__).resolve().parents[2]
VERIFIER_PATH = ROOT / ".github/scripts/verify_output_transform_vectors.py"
sys.path.insert(0, str(VERIFIER_PATH.parent))
SPEC = importlib.util.spec_from_file_location(
    "verify_output_transform_vectors", VERIFIER_PATH)
assert SPEC is not None and SPEC.loader is not None
VERIFIER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(VERIFIER)

COMMON_PATH = ROOT / ".github/scripts/output_transform_common.py"
COMMON_SPEC = importlib.util.spec_from_file_location(
    "output_transform_common", COMMON_PATH)
assert COMMON_SPEC is not None and COMMON_SPEC.loader is not None
COMMON = importlib.util.module_from_spec(COMMON_SPEC)
COMMON_SPEC.loader.exec_module(COMMON)


class OutputTransformVectorTests(unittest.TestCase):
    def test_profiles_freeze_the_complete_m0_domain(self) -> None:
        profiles = json.loads(
            (ROOT / "Config/Validation/OutputTransform/Profiles.json").read_text(
                encoding="utf-8"))
        self.assertEqual(7, len(profiles["profiles"]))
        self.assertEqual([-16, -8, -1, 0, 1, 8, 15, 16],
                         profiles["manualExposure"]["samples"])
        self.assertEqual("Sdr.KhronosPbrNeutral.v1",
                         profiles["toneMapStrategies"][0]["versionId"])
        self.assertEqual([1000, 2000],
                         profiles["hdrViewingTransform"]["peakPresetsNits"])

    def test_frozen_documents_verify_without_regeneration(self) -> None:
        errors = VERIFIER.verify_documents(
            ROOT / "Config/Validation/OutputTransform/Profiles.json",
            ROOT / "Tests/Fixtures/OutputTransform/manifest-v1.json",
            ROOT / "Tests/Fixtures/OutputTransform/vectors-v1.json")
        self.assertEqual([], errors)

    def test_verifier_has_no_expected_value_generation_option(self) -> None:
        parser = VERIFIER.build_parser()
        _, unknown = parser.parse_known_args([
            "--profiles", "profiles.json",
            "--manifest", "manifest.json",
            "--vectors", "vectors.json",
            "--generate-expected-values",
        ])
        self.assertEqual(["--generate-expected-values"], unknown)

    def test_frozen_authority_and_checked_in_shader_digests(self) -> None:
        expected = {
            "Config/Validation/OutputTransform/Profiles.json":
                "6e3373ab31b36aff9e91d1b6b854d75d1171226e57bc3c236f8a1bbce1e1f7d9",
            "Tests/Fixtures/OutputTransform/manifest-v1.json":
                "77aabc634d565862079cc6aa55625bccfc934632d7d4068d360654952aaa121d",
            "Tests/Fixtures/OutputTransform/vectors-v1.json":
                "16189846ba601040696fbb1db6120eed36253feffeae02362a691e250c24aa76",
            "Content/Shaders/PostProcess/Fullscreen.vert.spv":
                "347c12d94126663d233b9baa37d4a1e9a2bbcc015f79d8e587cf3992cfacce36",
            "Content/Shaders/PostProcess/OutputTransform.frag.spv":
                "3d4faf6d5ba0fdfb1f92e557ef3e1a107816bf0f2bbd8cea463f2fb2216df2fa",
        }
        for relative, digest in expected.items():
            self.assertEqual(digest, hashlib.sha256(
                (ROOT / relative).read_bytes()).hexdigest())

    def test_normalized_verification_is_stable_across_twenty_runs(self) -> None:
        reports = []
        for _ in range(20):
            findings = VERIFIER.verify_documents(
                ROOT / "Config/Validation/OutputTransform/Profiles.json",
                ROOT / "Tests/Fixtures/OutputTransform/manifest-v1.json",
                ROOT / "Tests/Fixtures/OutputTransform/vectors-v1.json")
            reports.append(json.dumps(findings, separators=(",", ":")))
        self.assertEqual(1, len(set(reports)))
        self.assertEqual("[]", reports[0])

    def test_common_helpers_compare_only_decoded_nits_and_xyz(self) -> None:
        matrix = [
            [0.6369580483, 0.1446169036, 0.1688809752],
            [0.2627002120, 0.6779980715, 0.0593017165],
            [0.0, 0.0280726930, 1.0609850577],
        ]
        result = COMMON.compare_decoded_hdr(
            [100.01, 199.99, 50.0], [100.0, 200.0, 50.0],
            "pq-rec2020", 100.0, matrix)
        self.assertEqual("absolute-nits-xyz", result["comparisonDomain"])
        self.assertFalse(result["rawCodeComparison"])
        self.assertTrue(result["rgbPassed"])
        self.assertTrue(result["xyzPassed"])
        self.assertEqual(3, len(result["rgbTolerance"]))
        self.assertEqual(3, len(result["xyzTolerance"]))

    def test_common_helpers_reject_raw_or_unknown_hdr_encodings(self) -> None:
        with self.assertRaises(ValueError):
            COMMON.decoded_rgb_nits_tolerance(
                [1.0, 1.0, 1.0], "raw-native-code", 100.0)


if __name__ == "__main__":
    unittest.main()
