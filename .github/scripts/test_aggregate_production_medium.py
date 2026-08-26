#!/usr/bin/env python3

import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("aggregate_production_medium.py")


class ProductionMediumAggregateTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        spec = importlib.util.spec_from_file_location(
            "aggregate_production_medium", SCRIPT
        )
        cls.module = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = cls.module
        spec.loader.exec_module(cls.module)

    def write_fixture(self, root: Path):
        package_ids = ["lantern", "sponza"]
        profile = root / "Medium.json"
        profile.write_text(json.dumps({
            "profileId": "medium",
            "packageIds": package_ids,
            "lifecycleCycles": 1000,
            "warmupCycles": 20,
            "timeBudgetSeconds": 2100,
        }), encoding="utf-8")
        target = root / "Target.json"
        target.write_text("{}\n", encoding="utf-8")
        target_digest = self.module.sha256_file(target)
        for index, package_id in enumerate(package_ids):
            shard = root / f"shard-{index}"
            shard.mkdir()
            summary = {
                "schema": "stoner.production-cook-runtime-summary",
                "schemaVersion": 1,
                "profile": "medium",
                "corpusRevision": "corpus-v1",
                "corpusDigest": "a" * 64,
                "targetProfile": "Target.json",
                "targetProfileDigest": target_digest,
                "determinismRuns": 1,
                "timeBudgetSeconds": 2100,
                "elapsedSeconds": 1000 + index,
                "passed": True,
                "packages": [{
                    "packageId": package_id,
                    "workloadRevision": f"{package_id}-v2",
                    "generationId": str(index) * 64,
                    "cleanRuns": 1,
                    "reachableAssets": 10 + index,
                    "reusedAssets": 10 + index,
                    "nativeLifecycle": {
                        "result": "Passed",
                        "lifecycleCycles": 1000,
                        "warmupCycles": 20,
                        "ownersAtTerminal": 0,
                        "staleHandleRejected": True,
                        "captureCount": 2000,
                        "readbackCount": 7,
                        "rssGrowthBytes": index,
                        "seconds": 900 + index,
                    },
                }],
            }
            (shard / "summary.json").write_text(
                json.dumps(summary), encoding="utf-8"
            )
            (shard / "artifact-manifest.json").write_text(
                "{}\n", encoding="utf-8"
            )
        return profile, target

    def test_exact_two_shard_aggregate_passes(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile, target = self.write_fixture(root)
            result = self.module.aggregate_medium_shards(
                root, profile, target
            )
            self.assertTrue(result["passed"])
            self.assertEqual(["lantern", "sponza"], result["packageIds"])
            self.assertEqual(1001, result["maximumLaneSeconds"])

    def test_missing_or_duplicate_package_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile, target = self.write_fixture(root)
            (root / "shard-1/summary.json").unlink()
            with self.assertRaisesRegex(ValueError, "count is incomplete"):
                self.module.aggregate_medium_shards(root, profile, target)

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile, target = self.write_fixture(root)
            changed = json.loads(
                (root / "shard-1/summary.json").read_text(encoding="utf-8")
            )
            changed["packages"][0]["packageId"] = "lantern"
            (root / "shard-1/summary.json").write_text(
                json.dumps(changed), encoding="utf-8"
            )
            with self.assertRaisesRegex(ValueError, "package evidence"):
                self.module.aggregate_medium_shards(root, profile, target)

    def test_failed_native_or_authority_mismatch_fails_closed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile, target = self.write_fixture(root)
            path = root / "shard-1/summary.json"
            changed = json.loads(path.read_text(encoding="utf-8"))
            changed["packages"][0]["nativeLifecycle"]["result"] = "Failed"
            path.write_text(json.dumps(changed), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "package evidence"):
                self.module.aggregate_medium_shards(root, profile, target)

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile, target = self.write_fixture(root)
            path = root / "shard-1/summary.json"
            changed = json.loads(path.read_text(encoding="utf-8"))
            changed["corpusDigest"] = "b" * 64
            path.write_text(json.dumps(changed), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "authority differs"):
                self.module.aggregate_medium_shards(root, profile, target)


if __name__ == "__main__":
    unittest.main()
