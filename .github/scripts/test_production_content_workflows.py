#!/usr/bin/env python3

from pathlib import Path
import re
import unittest


ROOT = Path(__file__).parents[2]
HOSTED = ROOT / ".github/workflows/feature-028-production-content.yml"
HARDWARE = ROOT / ".github/workflows/feature-028-production-hardware.yml"


class ProductionContentWorkflowContractTests(unittest.TestCase):
    def test_hosted_workflow_has_relevant_paths_regular_matrix_and_medium_cadence(self):
        text = HOSTED.read_text(encoding="utf-8")
        for token in (
            "pull_request:", "push:", "schedule:", "workflow_dispatch:",
            "028-production-content-acceptance", "Content/ProductionAcceptance",
            "Config/Validation/ProductionContent", "run_production_content_validation.py",
            "windows-latest", "ubuntu-latest", "macos-26", "--profile regular",
            "macos-26-intel", "Mac-Metal-X86_64.json",
            "--profile medium", "timeout-minutes: 30",
            "Build strict Debug", "Build strict Release",
            "actions/cache@v4", "External/Sponza",
        ):
            self.assertIn(token, text)
        self.assertRegex(
            text, r"cron:\s*['\"]\d+\s+\d+\s+\*\s+\*\s+[0-6]['\"]"
        )
        self.assertEqual(text.count("slug: macos-intel-metal"), 2)

    def test_hosted_workflow_has_digest_checked_artifact_handoff(self):
        text = HOSTED.read_text(encoding="utf-8")
        self.assertIn("actions/upload-artifact@v6", text)
        self.assertIn("actions/download-artifact@v7", text)
        self.assertIn("--verify-only", text)
        self.assertIn("--target-profile ${{ matrix.target }}", text)
        self.assertIn("artifact-manifest.json", text)

    def test_hosted_workflow_has_feature_028_linux_sanitizer_gates(self):
        text = HOSTED.read_text(encoding="utf-8")
        for token in (
            "sanitizers: address,undefined", "sanitizers: thread",
            "ASAN_OPTIONS", "UBSAN_OPTIONS", "TSAN_OPTIONS",
            "--suite asset-gltf-malformed",
            "--suite asset-manager-cancellation",
            "--suite asset-manager-concurrency",
            "--suite renderer-static-model",
            "--suite production-content",
            "production-sanitizer-${{ matrix.slug }}",
        ):
            self.assertIn(token, text)

    def test_hardware_workflow_uses_explicit_platform_labels_and_dispatch(self):
        text = HARDWARE.read_text(encoding="utf-8")
        for token in (
            "workflow_dispatch:", "self-hosted", "Windows", "Vulkan",
            "macOS", "metal", "arm64", "--profile hardware",
            "reference-image-change", "production-render-path-change",
            "STONER_PRODUCTION_VISIBLE: '1'",
        ):
            self.assertIn(token, text)
        self.assertNotIn("pull_request:", text)
        self.assertNotIn("schedule:", text)

    def test_every_validation_job_uploads_failure_safe_evidence(self):
        for path in (HOSTED, HARDWARE):
            text = path.read_text(encoding="utf-8")
            job_text = text.split("jobs:\n", 1)[1]
            jobs = len(re.findall(r"^  [a-z0-9-]+:\s*$", job_text, re.MULTILINE))
            uploads = job_text.count("if: always()")
            self.assertGreaterEqual(uploads, jobs)


if __name__ == "__main__":
    unittest.main()
