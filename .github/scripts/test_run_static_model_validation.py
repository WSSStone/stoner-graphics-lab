#!/usr/bin/env python3

import importlib.util
from pathlib import Path
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("run_static_model_validation.py")
SPEC = importlib.util.spec_from_file_location("run_static_model_validation", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class StaticModelValidationTests(unittest.TestCase):
    def test_deterministic_runs_only_renderer_suite(self):
        calls = []

        def record(command, timeout_seconds, env):
            del timeout_seconds, env
            calls.append(command)
            return 0

        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "report.txt"
            original = MODULE.run
            MODULE.run = record
            try:
                result = MODULE.main([
                    "--profile", "deterministic", "--tests", "StonerTest",
                    "--output", str(output),
                ])
            finally:
                MODULE.run = original
            self.assertEqual(0, result)
            self.assertIn("validation-result=pass", output.read_text())
        self.assertEqual(
            [["StonerTest", "--suite", "renderer-static-mesh"]], calls
        )

    def test_native_requires_all_real_execution_suites(self):
        calls = []

        def record(command, timeout_seconds, env):
            del timeout_seconds
            calls.append((command, dict(env)))
            return 0

        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "report.txt"
            original = MODULE.run
            MODULE.run = record
            try:
                result = MODULE.main([
                    "--profile", "native", "--tests", "StonerTest",
                    "--output", str(output),
                ])
            finally:
                MODULE.run = original
            self.assertEqual(0, result)
            report = output.read_text()
            self.assertIn("indexed-clockwise-readback=required", report)
            self.assertIn("non-symmetric-matrix-readback=required", report)
        self.assertEqual(
            ["renderer-static-mesh", "vulkan-native", "deferred-native"],
            [call[0][-1] for call in calls],
        )
        self.assertTrue(all(
            call[1].get("STONER_REQUIRE_STATIC_MESH_NATIVE") == "1"
            and call[1].get("STONER_REQUIRE_DEFERRED_NATIVE") == "1"
            for call in calls
        ))

    def test_failure_stops_later_suites_and_is_reported(self):
        calls = []

        def fail_vulkan(command, timeout_seconds, env):
            del timeout_seconds, env
            calls.append(command[-1])
            return 9 if command[-1] == "vulkan-native" else 0

        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "report.txt"
            original = MODULE.run
            MODULE.run = fail_vulkan
            try:
                result = MODULE.main([
                    "--profile", "native", "--tests", "StonerTest",
                    "--output", str(output),
                ])
            finally:
                MODULE.run = original
            self.assertEqual(9, result)
            self.assertIn("validation-result=fail", output.read_text())
        self.assertEqual(["renderer-static-mesh", "vulkan-native"], calls)

    def test_non_positive_timeout_is_rejected(self):
        with self.assertRaises(SystemExit):
            MODULE.parse_args([
                "--profile", "deterministic", "--tests", "StonerTest",
                "--output", "report.txt", "--timeout-seconds", "0",
            ])


if __name__ == "__main__":
    unittest.main()
