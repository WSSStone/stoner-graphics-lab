from __future__ import annotations

import json
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import run_asset_cooker_validation as validation


class AssetCookerValidationRunnerTests(unittest.TestCase):
    def test_normalized_report_validation_and_comparison(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            payload = {
                "schema": "stoner.asset-cook-report",
                "result": "success",
                "deterministicDigest": "0" * 64,
            }
            paths = [root / "a.json", root / "b.json"]
            for path in paths:
                path.write_text(json.dumps(payload), encoding="utf-8")
                self.assertEqual(
                    validation.load_normalized_report(path)["result"], "success"
                )
            self.assertEqual(len(validation.compare_reports(paths)), 64)
            paths[1].write_text(json.dumps({**payload, "telemetry": {}}), encoding="utf-8")
            with self.assertRaises(ValueError):
                validation.load_normalized_report(paths[1])
            with self.assertRaises(ValueError):
                validation.compare_reports(paths)

    def test_command_builder_separates_arguments_without_shell(self) -> None:
        command = validation.command_line(
            pathlib.Path("cooker"), "cook", pathlib.Path("source with space"),
            pathlib.Path("output"), pathlib.Path("ddc"), pathlib.Path("profile"),
            pathlib.Path("report"), 8, clean=True,
        )
        self.assertEqual(command[0], "cooker")
        self.assertIn("source with space", command)
        self.assertIn("--clean", command)
        self.assertNotIn("shell=True", command)

    def test_suite_builder_keeps_corruption_gates_separate(self) -> None:
        ddc = validation.suite_command(pathlib.Path("tests"), "asset-cooker-ddc")
        published = validation.suite_command(
            pathlib.Path("tests"), "asset-cooker-published-validation"
        )
        self.assertEqual(ddc, ["tests", "--suite", "asset-cooker-ddc"])
        self.assertNotEqual(ddc, published)

    def test_determinism_run_bounds_are_strict(self) -> None:
        required = [
            "--cooker", "cooker", "--tests", "tests",
            "--work", "work", "--output", "output",
        ]
        self.assertEqual(validation.parse_args(required).determinism_runs, 2)
        with self.assertRaises(SystemExit):
            validation.parse_args(required + ["--determinism-runs", "1"])
        with self.assertRaises(SystemExit):
            validation.parse_args(required + ["--determinism-runs", "21"])


if __name__ == "__main__":
    unittest.main()
