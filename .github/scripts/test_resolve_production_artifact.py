#!/usr/bin/env python3

import importlib.util
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).parents[2]
SCRIPT = ROOT / ".github/scripts/resolve_production_artifact.py"
SPEC = importlib.util.spec_from_file_location("resolve_production_artifact", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


def artifact(name: str, *, expired: bool = False) -> dict:
    return {"name": name, "expired": expired}


class ResolveProductionArtifactTests(unittest.TestCase):
    def test_current_attempt_is_preferred(self):
        document = {"artifacts": [
            artifact("production-regular-linux-vulkan-1"),
            artifact("production-regular-linux-vulkan-2"),
        ]}
        self.assertEqual(
            "production-regular-linux-vulkan-2",
            MODULE.resolve_artifact_name(
                document, "production-regular-linux-vulkan-", 2
            ),
        )

    def test_partial_rerun_falls_back_to_latest_available_attempt(self):
        document = [
            {"artifacts": [artifact("production-regular-windows-vulkan-1")]},
            {"artifacts": [artifact("production-regular-macos-intel-metal-2")]},
        ]
        self.assertEqual(
            "production-regular-windows-vulkan-1",
            MODULE.resolve_artifact_name(
                document, "production-regular-windows-vulkan-", 2
            ),
        )

    def test_expired_unrelated_and_future_artifacts_are_not_eligible(self):
        document = {"artifacts": [
            artifact("production-regular-macos-metal-1", expired=True),
            artifact("production-regular-macos-metal-2"),
            artifact("production-regular-macos-metal-4"),
            artifact("production-consumer-macos-metal-3"),
        ]}
        self.assertEqual(
            "production-regular-macos-metal-2",
            MODULE.resolve_artifact_name(
                document, "production-regular-macos-metal-", 3
            ),
        )

    def test_missing_or_ambiguous_artifacts_fail_closed(self):
        with self.assertRaisesRegex(ValueError, "no eligible"):
            MODULE.resolve_artifact_name(
                {"artifacts": []}, "production-regular-linux-vulkan-", 2
            )
        duplicate = {"artifacts": [
            artifact("production-regular-linux-vulkan-1"),
            artifact("production-regular-linux-vulkan-1"),
        ]}
        with self.assertRaisesRegex(ValueError, "ambiguous"):
            MODULE.resolve_artifact_name(
                duplicate, "production-regular-linux-vulkan-", 2
            )

    def test_cli_writes_exact_github_output(self):
        document = {"artifacts": [
            artifact("production-regular-windows-vulkan-1")
        ]}
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "github-output"
            result = subprocess.run(
                [
                    sys.executable,
                    str(SCRIPT),
                    "--prefix",
                    "production-regular-windows-vulkan-",
                    "--max-attempt",
                    "2",
                    "--github-output",
                    str(output),
                ],
                input=json.dumps(document),
                text=True,
                capture_output=True,
                check=False,
            )
            self.assertEqual(0, result.returncode, result.stderr)
            self.assertEqual(
                "name=production-regular-windows-vulkan-1\n",
                output.read_text(encoding="utf-8"),
            )


if __name__ == "__main__":
    unittest.main()
