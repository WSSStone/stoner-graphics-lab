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

    def write_native_report(self, report, extra_probes_per_convention=12, omit_local=None):
        omit_local = set(omit_local or ())
        probes = []
        for convention in MODULE.REQUIRED_CONVENTIONS:
            probes.extend(
                f"probe convention={convention} name=p{index} semantic=Surface passed=true"
                for index in range(extra_probes_per_convention)
            )
            probes.extend(
                f"probe convention={convention} name={name} "
                "semantic=LocalLightCase passed=true"
                for name in MODULE.REQUIRED_LOCAL_LIGHT_PROBES
                if name not in omit_local
            )
        report.write_text(
            "runtime=RealRuntime\nreference_path=NativeDeferredReadback\n"
            "software_device=true\nnative_submission=true\n"
            "final_live_objects=0\n"
            + "\n".join(probes)
            + "\nresult=PASS\n",
            encoding="utf-8",
        )

    def test_twelve_probe_native_report_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "report.txt"
            self.write_native_report(report, extra_probes_per_convention=6)
            self.assertFalse(MODULE.validate_readback_report(report))

    def test_missing_local_light_probe_is_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "report.txt"
            self.write_native_report(report, omit_local={"spot-near-plane"})
            self.assertFalse(MODULE.validate_readback_report(report))

    def test_complete_native_report_is_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "report.txt"
            self.write_native_report(report)
            self.assertTrue(MODULE.validate_readback_report(report))

    def test_executed_comparison_report_is_required(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "comparison.txt"
            lines = [
                "measurement-source=RendererComparisonTests",
                "forward-median=1.0",
                "forward-p95=1.1",
                "deferred-median=0.9",
                "deferred-p95=1.0",
                "crossover=DeferredAt64",
            ]
            for tier in MODULE.REQUIRED_TIERS:
                lines.extend(
                    (
                        f"tier={tier}",
                        "measured-frames=100",
                        "fingerprint-match=true",
                    )
                )
            lines.append("comparison-result=pass")
            report.write_text("\n".join(lines) + "\n", encoding="utf-8")
            self.assertTrue(MODULE.validate_comparison_report(report))
            report.write_text(
                report.read_text(encoding="utf-8").replace(
                    "measured-frames=100", "measured-frames=99", 1
                ),
                encoding="utf-8",
            )
            self.assertFalse(MODULE.validate_comparison_report(report))


if __name__ == "__main__":
    unittest.main()
