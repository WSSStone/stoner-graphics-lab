from __future__ import annotations

import hashlib
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import verify_asset_cooker_fixtures as fixtures


class AssetCookerFixtureVerifierTests(unittest.TestCase):
    def test_repository_fixture_inventory_is_complete(self) -> None:
        root = pathlib.Path(__file__).resolve().parents[1]
        self.assertEqual(fixtures.verify(root), [])

    def test_record_requires_metadata_and_matching_digest(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            path = root / "fixture.json"
            path.write_bytes(b"{}\n")
            record = {
                "path": "fixture.json",
                "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                "provenance": "test",
                "license": "test",
                "expectedValidity": "valid",
                "assetTypes": ["test"],
                "targetProfiles": ["test"],
                "expectedOutcomes": ["pass"],
            }
            self.assertEqual(fixtures.validate_fixture_record(root, record), [])
            record["sha256"] = "0" * 64
            self.assertTrue(any(
                "checksum mismatch" in error
                for error in fixtures.validate_fixture_record(root, record)
            ))
            del record["license"]
            self.assertTrue(any(
                "missing fields" in error
                for error in fixtures.validate_fixture_record(root, record)
            ))

    def test_declared_corpus_minima_are_enforced(self) -> None:
        corpora = [
            {"id": identifier, "minimumCases": 1}
            for identifier in fixtures.CORPUS_DIRECTORIES
        ]
        paths = [
            f"Tests/Fixtures/AssetCooker/{directory}/case.json"
            for directory in fixtures.CORPUS_DIRECTORIES.values()
        ]
        self.assertEqual(fixtures.validate_corpus_coverage(corpora, paths), [])
        self.assertTrue(any(
            "shortfall" in error
            for error in fixtures.validate_corpus_coverage(corpora, paths[:-1])
        ))


if __name__ == "__main__":
    unittest.main()
