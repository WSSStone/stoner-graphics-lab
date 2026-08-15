from __future__ import annotations

import json
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import run_runtime_asset_manager_validation as validation


class RuntimeAssetManagerValidationRunnerTests(unittest.TestCase):
    def test_suite_builder_preserves_argument_boundaries(self) -> None:
        command = validation.suite_command(
            pathlib.Path("tests with space"), "asset-manager", "asset-manager-cooked"
        )
        self.assertEqual(command[0], "tests with space")
        self.assertEqual(command.count("--suite"), 2)

    def test_repetition_and_timeout_bounds_are_strict(self) -> None:
        required = ["--tests", "tests", "--work", "work", "--output", "out"]
        self.assertEqual(validation.parse_args(required).repetitions, 20)
        with self.assertRaises(SystemExit):
            validation.parse_args(required + ["--repetitions", "19"])
        with self.assertRaises(SystemExit):
            validation.parse_args(required + ["--timeout-seconds", "0"])

    def test_prepare_only_creates_uploadable_normalized_report(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            output = root / "report.json"
            code = validation.main([
                "--root", str(root), "--work", "work",
                "--output", str(output), "--prepare-only",
            ])
            payload = json.loads(output.read_text(encoding="utf-8"))
            self.assertEqual(code, 0)
            self.assertTrue(payload["prepared"])
            self.assertFalse(payload["passed"])

    def test_timeout_is_normalized_to_exit_124(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            result = validation.run_command(
                "timeout",
                [sys.executable, "-c", "import time; time.sleep(1)"],
                pathlib.Path(temporary), 0.01,
            )
            self.assertEqual(result.returncode, 124)


if __name__ == "__main__":
    unittest.main()
