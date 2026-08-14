#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path


SCRIPT = Path(__file__).parent / "Fixtures/StaticModel/generate_invalid_fixtures.py"
SPEC = importlib.util.spec_from_file_location("invalid_fixtures", SCRIPT)
assert SPEC and SPEC.loader
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


class InvalidFixtureGeneratorTests(unittest.TestCase):
    def test_generation_is_deterministic_and_classified(self) -> None:
        with tempfile.TemporaryDirectory() as first, tempfile.TemporaryDirectory() as second:
            first_manifest = MODULE.generate(Path(first))
            second_manifest = MODULE.generate(Path(second))
            self.assertEqual(first_manifest, second_manifest)
            self.assertGreaterEqual(len(first_manifest["mutations"]), 10)
            self.assertEqual(
                sorted(path.name for path in Path(first).iterdir()),
                sorted(path.name for path in Path(second).iterdir()),
            )
            for entry in first_manifest["mutations"]:
                self.assertIn(entry["expected"], {"malformed", "unsupported"})
                self.assertEqual(len(entry["sha256"]), 64)


if __name__ == "__main__":
    unittest.main()
