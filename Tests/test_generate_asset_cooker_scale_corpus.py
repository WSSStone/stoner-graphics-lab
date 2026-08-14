from __future__ import annotations

import json
import pathlib
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(
    pathlib.Path(__file__).resolve().parent / "Fixtures/AssetCooker"
))

import generate_scale_corpus as scale


class AssetCookerScaleCorpusTests(unittest.TestCase):
    def test_shape_is_exact_deterministic_and_acyclic(self) -> None:
        first = scale.build_corpus()
        second = scale.build_corpus()
        self.assertEqual(scale.canonical_bytes(first), scale.canonical_bytes(second))
        self.assertEqual(first["assetCount"], 1_000)
        self.assertEqual(first["dependencyEdgeCount"], 5_000)
        self.assertEqual(first["maximumDependencyDepth"], 9)
        seen: set[str] = set()
        edge_count = 0
        for asset in first["assets"]:
            self.assertNotIn(asset["id"], seen)
            self.assertEqual(len(asset["dependencies"]), len(set(asset["dependencies"])))
            self.assertTrue(all(dependency in seen for dependency in asset["dependencies"]))
            seen.add(asset["id"])
            edge_count += len(asset["dependencies"])
        self.assertEqual(edge_count, 5_000)

    def test_cli_reproduces_library_bytes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            output = pathlib.Path(temporary) / "corpus.json"
            script = pathlib.Path(scale.__file__).resolve()
            result = subprocess.run(
                [sys.executable, str(script), "--output", str(output)],
                check=False,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)
            self.assertEqual(output.read_bytes(), scale.canonical_bytes(scale.build_corpus()))
            parsed = json.loads(output.read_bytes())
            self.assertEqual(len(parsed["assets"]), 1_000)


if __name__ == "__main__":
    unittest.main()
