from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).with_name("run_metal_validation.py")
SPEC = importlib.util.spec_from_file_location("run_metal_validation", SCRIPT)
assert SPEC and SPEC.loader
validation = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validation)


class MetalValidationRunnerTests(unittest.TestCase):
    def test_timeout_is_normalized(self) -> None:
        with mock.patch.object(
            validation.subprocess, "run",
            side_effect=subprocess.TimeoutExpired(["probe"], 1),
        ):
            result = validation.run_command(["probe"], Path("."), 1)
        self.assertEqual(124, result.returncode)
        self.assertIn("validation-timeout", result.stderr)

    def test_lifecycle_parser_requires_all_samples(self) -> None:
        lines = ["[EVIDENCE] metal-lifecycle-native native"]
        for iteration in range(1100, 10100, 100):
            lines.append(
                f"[EVIDENCE] metal-lifecycle-rss iteration={iteration} bytes=100"
            )
        lines.append(
            "[EVIDENCE] metal-lifecycle-summary samples=90 first=100 "
            "final=100 growth=0 allowed=16777216 passed=true"
        )
        native, memory = validation.parse_lifecycle_output("\n".join(lines))
        self.assertEqual("native", native)
        self.assertIsNotNone(memory)
        self.assertEqual(90, len(memory["samplesBytes"]))
        self.assertTrue(memory["passed"])

    def test_report_digest_is_stable_and_not_self_referential(self) -> None:
        first = {"value": 7}
        second = {"value": 7}
        validation.finalize_report(first)
        validation.finalize_report(second)
        self.assertEqual(first["reportDigest"], second["reportDigest"])

    def test_prepare_only_writes_failure_safe_report(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            output = root / "report.json"
            with mock.patch.object(validation, "revision", return_value="0" * 40):
                result = validation.main([
                    "--root", str(root), "--output", str(output),
                    "--prepare-only",
                ])
            document = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(0, result)
            self.assertEqual("unavailable", document["result"])
            self.assertEqual("not-run", document["failureCategory"])

    def test_argument_bounds_are_enforced(self) -> None:
        with self.assertRaises(SystemExit):
            validation.parse_args(["--output", "x", "--repetitions", "0"])
        with self.assertRaises(SystemExit):
            validation.parse_args(["--output", "x", "--timeout-seconds", "0"])
        with self.assertRaises(SystemExit):
            validation.parse_args(["--output", "x", "--visible-frames", "2999"])
        with self.assertRaises(SystemExit):
            validation.parse_args(["--output", "x", "--visible-cycles", "19"])

    def test_tier_selects_native_and_comparison_workloads(self) -> None:
        native = validation.parse_args([
            "--output", "x", "--tier", "native-offscreen",
        ])
        comparison = validation.parse_args([
            "--output", "x", "--tier", "cross-backend",
        ])
        self.assertEqual("native", native.workload)
        self.assertEqual("comparison", comparison.workload)

    def test_visible_tier_requires_strict_cooked_inputs(self) -> None:
        with self.assertRaises(SystemExit):
            validation.parse_args([
                "--output", "x", "--tier", "visible-manual",
            ])
        visible = validation.parse_args([
            "--output", "x", "--tier", "visible-manual",
            "--demo", "demo", "--profile", "profile",
            "--publication", "publication", "--lease", "lease",
            "--capture", "capture.png",
            "--work", "work",
        ])
        self.assertEqual("visible", visible.workload)

    def test_visible_runner_forwards_strict_cooked_inputs(self) -> None:
        completed = subprocess.CompletedProcess([], 0, "", "")
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            report = root / "work" / "visible-demo.txt"
            report.parent.mkdir(parents=True)
            report.write_text("", encoding="utf-8")
            with mock.patch.object(
                validation, "run_native", return_value=(True, True, {}, "")
            ), mock.patch.object(
                validation, "run_command", return_value=completed
            ) as command:
                validation.run_visible(
                    root / "demo", root / "tests", root, root / "work", 30,
                    root / "profile.json", root / "Published", root / "Lease",
                    30000, 20,
                )
        invocation = command.call_args.args[0]
        self.assertEqual(
            ["--cooked-root", str(root / "Published")],
            invocation[invocation.index("--cooked-root"):invocation.index("--cooked-root") + 2],
        )
        self.assertIn(str(root / "profile.json"), invocation)
        self.assertIn(str(root / "Lease"), invocation)
        self.assertIn("30000", invocation)
        self.assertIn("20", invocation)

    def test_presentation_smoke_requires_probe_and_persistent_work(self) -> None:
        with self.assertRaises(SystemExit):
            validation.parse_args([
                "--output", "x", "--tier", "visible-manual",
                "--workload", "presentation-smoke",
            ])
        smoke = validation.parse_args([
            "--output", "x", "--tier", "visible-manual",
            "--workload", "presentation-smoke",
            "--presentation-probe", "probe", "--work", "work",
        ])
        self.assertEqual("presentation-smoke", smoke.workload)

    def test_persistent_work_is_available_to_lifecycle_logging(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            tests = root / "StonerTest"
            tests.write_bytes(b"test")
            output = root / "report.json"
            with mock.patch.object(validation, "revision", return_value="0" * 40), \
                    mock.patch.object(
                        validation,
                        "run_lifecycle",
                        return_value=(True, "unavailable", {
                            "warmupIterations": 1000,
                            "sampleInterval": 100,
                            "samplesBytes": [100] * 90,
                            "firstMedianBytes": 100,
                            "finalMedianBytes": 100,
                            "absoluteGrowthBytes": 0,
                            "relativeGrowth": 0.0,
                            "allowedGrowthBytes": 16777216,
                            "passed": True,
                        }, "lifecycle-log\n"),
                    ):
                result = validation.main([
                    "--root", str(root),
                    "--tests", str(tests),
                    "--workload", "lifecycle",
                    "--work", "work",
                    "--output", str(output),
                ])
            self.assertEqual(0, result)
            self.assertEqual(
                "lifecycle-log\n",
                (root / "work" / "lifecycle.log").read_text(encoding="utf-8"),
            )

    def test_native_parser_requires_device_shader_and_readback(self) -> None:
        device = (
            "[EVIDENCE] metal-native-device identity=registry-42 "
            "name-utf8-hex=4d342050726f capability=" + "1" * 64
        )
        shader = "[EVIDENCE] metal-native-shader evidence=" + "2" * 64
        readback = (
            "[EVIDENCE] metal-native-deferred status=passed readback=" +
            "3" * 64
        )
        self.assertIsNone(validation.parse_native_output(device + "\n" + shader))
        evidence = validation.parse_native_output(
            "\n".join((device, shader, readback))
        )
        self.assertIsNotNone(evidence)
        self.assertEqual("M4 Pro", evidence["device"]["name"])
        self.assertEqual(["2" * 64], evidence["shaderEvidenceDigests"])

    def test_unavailable_native_report_is_schema_valid(self) -> None:
        with mock.patch.object(validation, "revision", return_value="0" * 40):
            document = validation.base_report(
                Path("."), "metal-native-conformance", "native-offscreen"
            )
        validation.finalize_report(document)
        self.assertEqual([], validation.validate_report(document))

    def test_native_pass_requires_evidence(self) -> None:
        with mock.patch.object(validation, "revision", return_value="0" * 40):
            document = validation.base_report(
                Path("."), "metal-native-conformance", "native-offscreen"
            )
        document["result"] = "passed"
        validation.finalize_report(document)
        errors = validation.validate_report(document)
        self.assertTrue(any("device evidence" in error for error in errors))
        self.assertTrue(any("shader evidence" in error for error in errors))

    def test_presentation_smoke_pass_does_not_require_shader_payload(self) -> None:
        with mock.patch.object(validation, "revision", return_value="0" * 40):
            document = validation.base_report(
                Path("."), "metal-presentation-smoke", "visible-manual"
            )
        document.update({
            "result": "passed",
            "device": {
                "identity": "registry-42",
                "name": "M4 Pro",
                "capabilityDigest": "1" * 64,
            },
            "counts": {"frames": 120, "lifecycleCycles": 4, "iterations": 1},
            "probes": [{
                "name": "presentation", "result": "passed",
                "evidenceDigest": "2" * 64,
            }],
        })
        validation.finalize_report(document)
        self.assertEqual([], validation.validate_report(document))

    def test_visible_presentation_pass_requires_accepted_png(self) -> None:
        with mock.patch.object(validation, "revision", return_value="0" * 40):
            document = validation.base_report(
                Path("."), "metal-visible-presentation", "visible-manual"
            )
        document.update({
            "result": "passed",
            "device": {
                "identity": "registry-42", "name": "M4 Pro",
                "capabilityDigest": "1" * 64,
            },
            "shaderEvidenceDigests": ["2" * 64],
            "counts": {"frames": 3000, "lifecycleCycles": 20, "iterations": 1},
        })
        validation.finalize_report(document)
        self.assertTrue(any(
            "accepted PNG" in error
            for error in validation.validate_report(document)
        ))

    def test_visible_report_parser_requires_native_long_run_and_cleanup(self) -> None:
        accepted = "\n".join((
            "runtime-object-mode=native",
            "completed-frames=3000",
            "recovery-count=20",
            "final-live-objects=0",
            "validation-result=pass",
        ))
        rejected = accepted.replace("completed-frames=3000", "completed-frames=2999")
        self.assertEqual((True, 3000, 20), validation.parse_demo_report(accepted))
        self.assertEqual((False, 2999, 20), validation.parse_demo_report(rejected))

    def test_comparison_pass_requires_native_and_tolerance_proof(self) -> None:
        with mock.patch.object(validation, "revision", return_value="0" * 40):
            document = validation.base_report(
                Path("."), "metal-vulkan-comparison", "cross-backend"
            )
        document["result"] = "passed"
        document["probes"] = [{"name": "comparison", "result": "passed"}]
        validation.finalize_report(document)
        errors = validation.validate_report(document)
        self.assertTrue(any("device evidence" in error for error in errors))
        self.assertTrue(any("tolerance provenance" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
