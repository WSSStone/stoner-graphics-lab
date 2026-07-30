#!/usr/bin/env python3

from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile
import unittest
from unittest import mock

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import verify_ktx2_artifacts as validator
import verify_ktx2_provenance as provenance


def custom_key_messages(
    keys: set[str] | frozenset[str],
) -> str:
    import json

    return json.dumps(
        {
            "valid": False,
            "messages": [
                {
                    "id": 7010,
                    "type": "error",
                    "message": "Custom key in Key/Value Data.",
                    "details":
                        f'Custom key "{key}" found in Key/Value Data.',
                }
                for key in sorted(keys)
            ],
        }
    )


class ValidatorTests(unittest.TestCase):
    def test_discovery_is_recursive_unique_and_ordered(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            first = root / "z.ktx2"
            second = root / "nested" / "a.ktx2"
            second.parent.mkdir()
            first.write_bytes(b"a")
            second.write_bytes(b"b")
            self.assertEqual(
                validator.discover([root, first]),
                sorted([first.resolve(), second.resolve()]),
            )

    def test_mixed_inputs_build_stable_relative_labels(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            first_root = root / "generated"
            second_root = root / "golden"
            first = first_root / "nested" / "z.ktx2"
            second = second_root / "a.ktx2"
            first.parent.mkdir(parents=True)
            second.parent.mkdir()
            first.write_bytes(b"a")
            second.write_bytes(b"b")
            artifacts = validator.discover(
                [first_root, second_root, first]
            )
            labels = validator.build_artifact_labels(
                [first_root, second_root, first], artifacts
            )
            self.assertEqual(
                labels[first.resolve()],
                "input-00/nested/z.ktx2",
            )
            self.assertEqual(
                labels[second.resolve()],
                "input-01/a.ktx2",
            )
            self.assertEqual(len(artifacts), 2)

    @mock.patch.object(validator, "run_command")
    def test_version_requires_success(self, run: mock.Mock) -> None:
        run.return_value = subprocess.CompletedProcess(
            ["ktx"], 1, "", "failed"
        )
        with self.assertRaises(RuntimeError):
            validator.tool_version("ktx", 1.0)

    @mock.patch.object(validator, "run_command")
    def test_malformed_json_fails(self, run: mock.Mock) -> None:
        run.return_value = subprocess.CompletedProcess(
            ["ktx"], 0, "not-json", ""
        )
        records, passed = validator.validate(
            "ktx", [pathlib.Path("sample.ktx2")], 1.0
        )
        self.assertFalse(passed)
        self.assertEqual(records[0]["error"], "malformed-json")

    @mock.patch.object(validator, "run_command")
    def test_records_are_ordered_by_stable_label(
        self, run: mock.Mock
    ) -> None:
        run.return_value = subprocess.CompletedProcess(
            ["ktx"], 0, "{}", ""
        )
        artifacts = [
            pathlib.Path("/host/z.ktx2"),
            pathlib.Path("/host/a.ktx2"),
        ]
        labels = {
            artifacts[0]: "input-01/z.ktx2",
            artifacts[1]: "input-00/a.ktx2",
        }
        records, passed = validator.validate(
            "ktx", artifacts, 1.0, labels
        )
        self.assertTrue(passed)
        self.assertEqual(
            [record["artifact"] for record in records],
            ["input-00/a.ktx2", "input-01/z.ktx2"],
        )

    @mock.patch.object(validator, "run_command")
    def test_validator_paths_are_normalized(
        self, run: mock.Mock
    ) -> None:
        artifact = pathlib.Path("/host/build/sample.ktx2")
        run.return_value = subprocess.CompletedProcess(
            ["ktx"],
            0,
            json.dumps(
                {
                    "messages": [
                        f"{artifact.resolve().as_posix()}: valid"
                    ]
                }
            ),
            "",
        )
        records, passed = validator.validate(
            "ktx",
            [artifact],
            1.0,
            {artifact: "input-00/sample.ktx2"},
        )
        self.assertTrue(passed)
        self.assertEqual(
            records[0]["validator"]["messages"],
            ["input-00/sample.ktx2: valid"],
        )

    @mock.patch.object(validator, "run_command")
    def test_timeout_is_normalized(self, run: mock.Mock) -> None:
        run.side_effect = subprocess.TimeoutExpired(["ktx"], 1.0)
        records, passed = validator.validate(
            "ktx", [pathlib.Path("sample.ktx2")], 1.0
        )
        self.assertFalse(passed)
        self.assertEqual(records[0]["error"], "timeout")

    @mock.patch.object(validator, "run_command")
    def test_exact_stoner_custom_key_warning_set_is_accepted(
        self, run: mock.Mock
    ) -> None:
        run.return_value = subprocess.CompletedProcess(
            ["ktx"],
            3,
            custom_key_messages(validator.ALLOWED_CUSTOM_KEYS),
            "",
        )
        records, passed = validator.validate(
            "ktx", [pathlib.Path("sample.ktx2")], 1.0
        )
        self.assertTrue(passed)
        self.assertEqual(
            records[0]["acceptedCustomMetadataWarnings"],
            sorted(validator.ALLOWED_CUSTOM_KEYS),
        )

    @mock.patch.object(validator, "run_command")
    def test_incomplete_custom_key_warning_set_fails(
        self, run: mock.Mock
    ) -> None:
        run.return_value = subprocess.CompletedProcess(
            ["ktx"],
            3,
            custom_key_messages(
                set(validator.ALLOWED_CUSTOM_KEYS)
                - {"stoner.assetId"}
            ),
            "",
        )
        _, passed = validator.validate(
            "ktx", [pathlib.Path("sample.ktx2")], 1.0
        )
        self.assertFalse(passed)

    @mock.patch.object(validator, "run_command")
    def test_warnings_as_errors_and_no_shell_are_fixed(
        self, run: mock.Mock
    ) -> None:
        run.return_value = subprocess.CompletedProcess(
            ["ktx"], 0, "{}", ""
        )
        _, passed = validator.validate(
            "ktx", [pathlib.Path("sample.ktx2")], 1.0
        )
        self.assertTrue(passed)
        command = run.call_args.args[0]
        self.assertEqual(
            command[1:5],
            ["validate", "--format", "json", "--warnings-as-errors"],
        )

    @mock.patch.object(validator, "tool_version")
    def test_missing_local_tool_can_be_reported_as_skip(
        self, version: mock.Mock
    ) -> None:
        version.side_effect = FileNotFoundError()
        with tempfile.TemporaryDirectory() as temporary:
            report = pathlib.Path(temporary) / "report.json"
            with mock.patch(
                "sys.argv",
                [
                    "verify_ktx2_artifacts.py",
                    "--input",
                    temporary,
                    "--report",
                    str(report),
                    "--allow-missing-tool",
                ],
            ):
                self.assertEqual(validator.main(), 0)
            self.assertIn(
                '"status": "tool-unavailable"',
                report.read_text(encoding="utf-8"),
            )

    @mock.patch.object(validator, "tool_version")
    @mock.patch.object(validator, "run_command")
    def test_main_aggregates_mixed_success_and_failure(
        self, run: mock.Mock, version: mock.Mock
    ) -> None:
        version.return_value = "ktx version: 4.4.2"
        run.side_effect = [
            subprocess.CompletedProcess(["ktx"], 0, "{}", ""),
            subprocess.CompletedProcess(
                ["ktx"], 1, '{"errors":["invalid"]}', ""
            ),
        ]
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            generated = root / "generated"
            golden = root / "golden"
            generated.mkdir()
            golden.mkdir()
            (generated / "z.ktx2").write_bytes(b"a")
            (golden / "a.ktx2").write_bytes(b"b")
            report = root / "report.json"
            with mock.patch(
                "sys.argv",
                [
                    "verify_ktx2_artifacts.py",
                    "--input",
                    str(generated),
                    "--input",
                    str(golden),
                    "--report",
                    str(report),
                ],
            ):
                self.assertEqual(validator.main(), 1)
            parsed = __import__("json").loads(
                report.read_text(encoding="utf-8")
            )
            self.assertEqual(parsed["status"], "failed")
            self.assertEqual(parsed["artifactCount"], 2)
            self.assertEqual(
                [item["artifact"] for item in parsed["artifacts"]],
                ["input-00/z.ktx2", "input-01/a.ktx2"],
            )

    def test_provenance_manifest_detects_checksum_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            (root / "payload").write_bytes(b"actual")
            (root / "SHA256SUMS").write_text(
                f"{'0' * 64}  ./payload\n", encoding="ascii"
            )
            self.assertEqual(
                provenance.verify_manifest(root),
                ["payload: checksum mismatch"],
            )


if __name__ == "__main__":
    unittest.main()
