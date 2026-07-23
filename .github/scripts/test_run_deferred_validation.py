#!/usr/bin/env python3

import importlib.util
from pathlib import Path
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("run_deferred_validation.py")
SPEC = importlib.util.spec_from_file_location("run_deferred_validation", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class DeferredValidationTests(unittest.TestCase):
    def test_deterministic_profile_requires_output(self):
        with self.assertRaises(SystemExit):
            MODULE.parse_args(
                ["--profile", "deterministic", "--tests", "StonerTest"]
            )

    def test_native_profile_requires_all_artifacts(self):
        with self.assertRaises(SystemExit):
            MODULE.parse_args(
                ["--profile", "native-lavapipe", "--tests", "StonerTest"]
            )

    def test_non_positive_timeout_is_rejected(self):
        with self.assertRaises(SystemExit):
            MODULE.parse_args(
                [
                    "--profile",
                    "deterministic",
                    "--tests",
                    "StonerTest",
                    "--output",
                    "report.txt",
                    "--timeout-seconds",
                    "0",
                ]
            )

    def test_semantic_oracle_is_not_accepted_as_native_readback(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "report.txt"
            report.write_text(
                "runtime=RealRuntime\n"
                "reference_path=NativeVulkanSubmission+DeterministicSemanticOracle\n"
                "software_device=true\nnative_submission=true\n"
                "final_live_objects=0\nresult=PASS\n",
                encoding="utf-8",
            )
            self.assertFalse(MODULE.validate_readback_report(report))

    def test_complete_native_report_is_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "report.txt"
            probes = []
            for convention in MODULE.REQUIRED_CONVENTIONS:
                probes.extend(
                    f"probe convention={convention} name=p{index} passed=true"
                    for index in range(12)
                )
            report.write_text(
                "runtime=RealRuntime\nreference_path=NativeDeferredReadback\n"
                "software_device=true\nnative_submission=true\n"
                "final_live_objects=0\n"
                + "\n".join(probes)
                + "\nresult=PASS\n",
                encoding="utf-8",
            )
            self.assertTrue(MODULE.validate_readback_report(report))


if __name__ == "__main__":
    unittest.main()
