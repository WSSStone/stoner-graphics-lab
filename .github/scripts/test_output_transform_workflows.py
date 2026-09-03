#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/feature-029-hdr-output.yml"
ARCHITECTURE = ROOT / ".github/scripts/verify_output_transform_architecture.py"


class OutputTransformWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.text = WORKFLOW.read_text(encoding="utf-8")
        cls.lower = cls.text.lower()

    def test_cross_platform_debug_and_strict_release_matrix(self) -> None:
        for token in ("windows-latest", "ubuntu-latest", "macos-26",
                      "config: debug", "config: release",
                      "scons config=${{ matrix.config }} strict=1"):
            self.assertIn(token, self.text)

    def test_sanitizer_lavapipe_and_metal_nonvisual_lanes(self) -> None:
        for token in ("address,undefined", "flags: 'thread'",
                      "sanitizers=${{ matrix.flags }}",
                      "mesa-vulkan-drivers", "vulkan-output-transform-native",
                      "metal-output-transform-native",
                      "STONER_REQUIRE_METAL_OUTPUT_PRESENTATION"):
            self.assertIn(token, self.text)

    def test_producer_consumer_revalidation_and_aggregate_are_separate(self) -> None:
        for job in ("artifact-producer:", "artifact-consumer:", "aggregate:"):
            self.assertIn(job, self.text)
        self.assertIn("actions/upload-artifact", self.text)
        self.assertIn("actions/download-artifact", self.text)
        self.assertIn("verify_output_transform_evidence.py", self.text)
        self.assertIn("needs: artifact-producer", self.text)

    def test_no_automated_hdr_visual_acceptance_lane(self) -> None:
        for forbidden in ("--require-closeout", "--write-attestation",
                          "--visual-pass", "--auto-accept", "hdr-score"):
            self.assertNotIn(forbidden, self.lower)
        self.assertIn("no automated hdr visual acceptance", self.lower)

    def test_architecture_scan_has_zero_findings(self) -> None:
        spec = importlib.util.spec_from_file_location("architecture", ARCHITECTURE)
        self.assertIsNotNone(spec)
        self.assertIsNotNone(spec.loader)
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        self.assertEqual([], module.scan(ROOT))


if __name__ == "__main__":
    unittest.main()
