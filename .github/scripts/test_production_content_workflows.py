#!/usr/bin/env python3

from pathlib import Path
import json
import re
import unittest


ROOT = Path(__file__).parents[2]
HOSTED = ROOT / ".github/workflows/feature-028-production-content.yml"
HARDWARE = ROOT / ".github/workflows/feature-028-production-hardware.yml"
DEVICE_CLASS_SCHEMA = (
    ROOT / "specs/028-production-content-acceptance/contracts/"
    "device-class-registry.schema.json"
)
IMAGE_BASELINE_SCHEMA = (
    ROOT / "specs/028-production-content-acceptance/contracts/"
    "image-baseline.schema.json"
)


class ProductionContentWorkflowContractTests(unittest.TestCase):
    def test_capability_schemas_accept_authoritative_intel_shader_profile(self):
        for path in (DEVICE_CLASS_SCHEMA, IMAGE_BASELINE_SCHEMA):
            schema = json.loads(path.read_text(encoding="utf-8"))
            definitions = schema.get("$defs", {})
            signature = definitions.get("capabilitySignature")
            if signature is None:
                signature = schema["properties"]["capabilitySignature"]
            pattern = signature["properties"]["shaderProfile"]["pattern"]
            self.assertIsNotNone(
                re.fullmatch(pattern, "metal-macos-12-x86_64"), path.name
            )

    def test_hosted_workflow_has_relevant_paths_regular_matrix_and_medium_cadence(self):
        text = HOSTED.read_text(encoding="utf-8")
        for token in (
            "pull_request:", "push:", "schedule:", "workflow_dispatch:",
            "028-production-content-acceptance", "Content/ProductionAcceptance",
            "Config/Validation/ProductionContent", "run_production_content_validation.py",
            "windows-latest", "ubuntu-latest", "macos-26", "--profile regular",
            "macos-26-intel", "Mac-Metal-X86_64.json",
            "--profile medium", "timeout-minutes: 90", "--timeout-seconds 3900",
            "production-medium-macos-metal", "Mac-Metal-Arm64.json",
            "khronos-lantern-glb", "khronos-sponza-gltf",
            "aggregate_production_medium.py", "--package-id",
            "production-medium-aggregate",
            "--defer-native-to-hardware",
            "Build strict Debug", "Build strict Release",
            "actions/cache@v4", "External/Sponza",
        ):
            self.assertIn(token, text)
        self.assertRegex(
            text, r"cron:\s*['\"]\d+\s+\d+\s+\*\s+\*\s+[0-6]['\"]"
        )
        self.assertEqual(text.count("slug: macos-intel-metal"), 2)
        self.assertNotIn("slug: macos-vulkan", text)
        self.assertEqual(text.count("native: --defer-native-to-hardware"), 1)
        self.assertIn("Mac-Vulkan.json", HARDWARE.read_text(encoding="utf-8"))

    def test_hosted_workflow_has_digest_checked_artifact_handoff(self):
        text = HOSTED.read_text(encoding="utf-8")
        self.assertIn("actions/upload-artifact@v6", text)
        self.assertIn("actions/download-artifact@v7", text)
        self.assertIn("actions/download-artifact@v6", text)
        self.assertIn("--verify-only", text)
        self.assertIn("--target-profile ${{ matrix.target }}", text)
        self.assertIn("artifact-manifest.json", text)
        self.assertEqual(2, text.count("include-hidden-files: true"))
        self.assertEqual(
            1,
            HARDWARE.read_text(encoding="utf-8").count(
                "include-hidden-files: true"
            ),
        )

    def test_medium_uses_two_isolated_exact_package_lanes_and_aggregate(self):
        text = HOSTED.read_text(encoding="utf-8")
        medium = text.split("  medium:\n", 1)[1].split(
            "  medium-aggregate:\n", 1
        )[0]
        aggregate = text.split("  medium-aggregate:\n", 1)[1]
        self.assertIn("name: Medium Metal (${{ matrix.slug }})", text)
        self.assertIn("fail-fast: false", text)
        self.assertEqual(1, text.count("package: khronos-lantern-glb"))
        self.assertEqual(1, text.count("package: khronos-sponza-gltf"))
        self.assertIn("needs: medium", text)
        self.assertIn("production-medium-authority-*", text)
        self.assertIn("Validate exact medium package authority", text)
        self.assertIn("runs-on: macos-26-intel", medium)
        self.assertIn("Mac-Metal-X86_64.json", medium)
        self.assertNotIn("Mac-Metal-Arm64.json", medium)
        self.assertIn("Validate Metal readback modes", medium)
        self.assertIn("--suite metal-resource", medium)
        self.assertIn("--timeout-seconds 3900", medium)
        self.assertIn("timeout-minutes: 90", medium)
        self.assertNotIn("MallocMediumZone", medium)
        self.assertNotIn("MALLOC_ARENA_MAX", medium)
        self.assertIn("Mac-Metal-X86_64.json", aggregate)
        self.assertNotIn("Mac-Metal-Arm64.json", aggregate)

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
            "STONER_PHYSICAL_DEVICE_CLASS", "STONER_PHYSICAL_EXCLUSIVE",
            "STONER_PHYSICAL_FROZEN_REVISION", "STONER_PHYSICAL_ALLOCATOR",
            "STONER_PHYSICAL_SAMPLE_PROTOCOL",
            "STONER_PHYSICAL_PRESENTATION",
        ):
            self.assertIn(token, text)
        self.assertNotIn("pull_request:", text)
        self.assertNotIn("schedule:", text)

    def test_workflows_own_environment_classification_and_operational_caps(self):
        hosted = HOSTED.read_text(encoding="utf-8")
        hardware = HARDWARE.read_text(encoding="utf-8")
        self.assertNotIn("--execution-class", hosted)
        self.assertNotIn("--execution-class", hardware)
        self.assertIn("--timeout-seconds 3900", hosted)
        self.assertIn("nativeTimeBudgetSeconds\": 3600", (
            ROOT / "Config/Validation/ProductionContent/Medium.json"
        ).read_text(encoding="utf-8"))
        self.assertIn("timeout-minutes: 90", hosted)

    def test_every_validation_job_uploads_failure_safe_evidence(self):
        for path in (HOSTED, HARDWARE):
            text = path.read_text(encoding="utf-8")
            job_text = text.split("jobs:\n", 1)[1]
            jobs = len(re.findall(r"^  [a-z0-9-]+:\s*$", job_text, re.MULTILINE))
            uploads = job_text.count("if: always()")
            self.assertGreaterEqual(uploads, jobs)


if __name__ == "__main__":
    unittest.main()
