#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
from pathlib import Path
import re
import unittest


ROOT = Path(__file__).resolve().parents[2]
WORKFLOW = ROOT / ".github/workflows/feature-029-hdr-output.yml"
ARCHITECTURE = ROOT / ".github/scripts/verify_output_transform_architecture.py"


class OutputTransformWorkflowTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.text = WORKFLOW.read_text(encoding="utf-8")
        cls.lower = cls.text.lower()

    def job(self, name: str) -> str:
        match = re.search(rf"^  {re.escape(name)}:\n(.*?)(?=^  [\w-]+:|\Z)",
                          self.text, re.MULTILINE | re.DOTALL)
        self.assertIsNotNone(match, f"missing job: {name}")
        return match[1]

    def test_windows_python_checks_run_after_autocrlf_checkout(self) -> None:
        job = self.job("windows-python-validation")
        self.assertIn("runs-on: windows-latest", job)
        self.assertIn("actions/setup-python@v6", job)
        self.assertIn('python-version: "3.12"', job)
        configure = "run: git config --global core.autocrlf true"
        checkout = "uses: actions/checkout@v5"
        self.assertIn(configure, job)
        self.assertIn(checkout, job)
        self.assertLess(job.index(configure), job.index(checkout))
        self.assertIn("shell: bash", job)
        for script in (
            ".github/scripts/test_verify_output_transform_evidence.py",
            ".github/scripts/test_run_output_transform_validation.py",
            ".github/scripts/test_hdr_live_review_contract.py",
            ".github/scripts/test_aggregate_output_transform_validation.py",
            ".github/scripts/test_output_transform_workflows.py",
            ".github/scripts/test_run_production_image_calibration.py",
            ".github/scripts/test_verify_output_transform_vectors.py",
            "Tests/test_verify_repository_shader_assets.py",
        ):
            with self.subTest(script=script):
                self.assertRegex(job, rf"(?m)^          python {re.escape(script)}$")

    def test_aggregate_requires_windows_python_validation(self) -> None:
        needs = re.search(r"^    needs: \[([^\]]+)\]$", self.job("aggregate"),
                          re.MULTILINE)
        self.assertIsNotNone(needs)
        self.assertIn("windows-python-validation", [item.strip() for item in needs[1].split(",")])

    def test_checkout_policy_and_compute_shaders_trigger_validation(self) -> None:
        for event in ("pull_request", "push"):
            with self.subTest(event=event):
                block = re.search(rf"^  {event}:\n(.*?)(?=^  \w|^\w|\Z)",
                                  self.text, re.MULTILINE | re.DOTALL)
                self.assertIsNotNone(block)
                for path in (".gitattributes", "Content/Shaders/**/*.comp"):
                    self.assertIn(f"      - '{path}'", block[1])

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

    def test_metal_presentation_dependency_is_verified_before_build(self) -> None:
        job = self.job("metal-nonvisual")
        build = "scons config=debug strict=1"
        for command in (
            "brew install glfw",
            'GLFW_PREFIX="$(brew --prefix glfw)"',
            'test -f "$GLFW_PREFIX/include/GLFW/glfw3.h"',
            'test -e "$GLFW_PREFIX/lib/libglfw.3.dylib"',
            'echo "GLFW_ROOT=$GLFW_PREFIX" >> "$GITHUB_ENV"',
        ):
            with self.subTest(command=command):
                self.assertIn(command, job)
                self.assertLess(job.index(command), job.index(build))
        self.assertIn("STONER_REQUIRE_METAL_OUTPUT_PRESENTATION: '1'", job)

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
