#!/usr/bin/env python3

import copy
import hashlib
import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest
import json


SCRIPT = Path(__file__).with_name("production_acceptance_report.py")
SHA = "a" * 64


class ProductionAcceptanceReportContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.assertTrue(SCRIPT.is_file(), "production report module is not implemented")
        spec = importlib.util.spec_from_file_location("production_acceptance_report", SCRIPT)
        cls.module = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = cls.module
        spec.loader.exec_module(cls.module)

    def report(self, result="passed", backend="none"):
        deterministic = {
            "corpusRevision": "corpus-v1",
            "packageId": "package-a",
            "rootAssetId": "StaticModel:Asset.glb#idx.scene.0",
            "sourceSetDigest": SHA,
            "targetProfileDigest": SHA,
            "generationIdentity": SHA,
            "mode": "strict-cooked",
            "dependencyCoverageDigest": SHA,
            "workloadRevision": "production-content-v1",
            "backend": backend,
            "result": result,
            "evidenceDigest": hashlib.sha256(b"[]").hexdigest(),
            "firstFailure": None,
        }
        observations = {}
        if backend != "none":
            observations = {
                "deviceClass": "macos.apple8.metal.rgba8",
                "flip": {
                    "state": "measured", "baselineId": "lantern-metal",
                    "mean": 0.0, "p95": 0.0, "maximum": 0.0,
                    "badPixelThreshold": 0.01, "badPixelFraction": 0.0,
                    "passed": True,
                },
            }
        return {
            "schema": "stoner.production-acceptance-report",
            "schemaVersion": 1,
            "deterministic": deterministic,
            "observations": observations,
            "artifacts": [],
        }

    def failure(self, unsupported=False):
        value = {
            "stage": "strict-load", "category": "payload-missing",
            "subject": "Texture:Asset#idx.texture.0", "expected": "present",
            "observed": "missing", "reproductionProfile": "regular",
        }
        if unsupported:
            value.update({
                "missingPrerequisite": "native Metal device",
                "replacementLane": "macos-metal-hardware",
            })
        return value

    def test_passed_report_requires_real_generation_and_null_failure(self):
        report = self.report()
        self.module.validate_report(report)
        for generation in ("not-created", "bad"):
            changed = copy.deepcopy(report)
            changed["deterministic"]["generationIdentity"] = generation
            with self.assertRaisesRegex(ValueError, "generation"):
                self.module.validate_report(changed)
        report["deterministic"]["firstFailure"] = self.failure()
        with self.assertRaisesRegex(ValueError, "first failure"):
            self.module.validate_report(report)

    def test_failed_and_unsupported_require_exact_failure_contract(self):
        failed = self.report("failed")
        failed["deterministic"]["generationIdentity"] = "not-created"
        failed["deterministic"]["firstFailure"] = self.failure()
        self.module.validate_report(failed)
        unsupported = self.report("unsupported")
        unsupported["deterministic"]["generationIdentity"] = "not-created"
        unsupported["deterministic"]["firstFailure"] = self.failure(True)
        self.module.validate_report(unsupported)
        del unsupported["deterministic"]["firstFailure"]["replacementLane"]
        with self.assertRaisesRegex(ValueError, "replacement"):
            self.module.validate_report(unsupported)

    def test_native_reports_require_registered_class_and_flip_state(self):
        report = self.report(backend="metal")
        self.module.validate_report(report)
        del report["observations"]["deviceClass"]
        with self.assertRaisesRegex(ValueError, "device class"):
            self.module.validate_report(report)
        report = self.report(backend="vulkan")
        report["observations"]["flip"] = {"state": "not-run", "reason": "comparison unavailable"}
        with self.assertRaisesRegex(ValueError, "measured passing FLIP"):
            self.module.validate_report(report)

    def test_failed_native_report_can_record_not_run_flip(self):
        report = self.report("failed", "vulkan")
        report["deterministic"]["firstFailure"] = self.failure()
        report["observations"]["flip"] = {
            "state": "not-run", "reason": "semantic probe failed"
        }
        self.module.validate_report(report)

    def test_unknown_fields_are_rejected_at_every_level(self):
        for path in (("root",), ("deterministic",), ("observations",)):
            report = self.report()
            target = report if path == ("root",) else report[path[0]]
            target["unknown"] = True
            with self.assertRaisesRegex(ValueError, "fields"):
                self.module.validate_report(report)

    def test_artifacts_are_ordered_bounded_and_digest_indexed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "a.bin").write_bytes(b"a")
            (root / "b.bin").write_bytes(b"bb")
            artifacts = [
                self.module.index_artifact(root / "a.bin", root, "readback"),
                self.module.index_artifact(root / "b.bin", root, "window-capture"),
            ]
            report = self.report()
            report["artifacts"] = artifacts
            report["deterministic"]["evidenceDigest"] = self.module.evidence_digest(artifacts)
            self.module.validate_report(report, root)
            report["artifacts"].reverse()
            with self.assertRaisesRegex(ValueError, "order"):
                self.module.validate_report(report, root)

    def test_artifact_missing_substitution_and_escape_are_rejected(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            item = root / "capture.bin"
            item.write_bytes(b"stable")
            artifact = self.module.index_artifact(item, root, "window-capture")
            report = self.report()
            report["artifacts"] = [artifact]
            report["deterministic"]["evidenceDigest"] = self.module.evidence_digest([artifact])
            item.write_bytes(b"change")
            with self.assertRaisesRegex(ValueError, "digest"):
                self.module.validate_report(report, root)
            report["artifacts"][0]["pathToken"] = "../escape"
            with self.assertRaisesRegex(ValueError, "path"):
                self.module.validate_report(report)

    def test_artifact_count_item_and_aggregate_limits_are_enforced(self):
        report = self.report()
        artifact = {
            "kind": "log", "pathToken": "a.log", "sha256": SHA,
            "sizeBytes": 64 * 1024 * 1024,
        }
        report["artifacts"] = [dict(artifact, pathToken=f"{i:02d}.log") for i in range(65)]
        with self.assertRaisesRegex(ValueError, "64 artifacts"):
            self.module.validate_report(report)
        report["artifacts"] = [dict(artifact, sizeBytes=64 * 1024 * 1024 + 1)]
        with self.assertRaisesRegex(ValueError, "64 MiB"):
            self.module.validate_report(report)
        report["artifacts"] = [dict(artifact, pathToken=f"{i}.log") for i in range(5)]
        report["artifacts"].sort(key=lambda item: (item["kind"], item["pathToken"]))
        with self.assertRaisesRegex(ValueError, "256 MiB"):
            self.module.validate_report(report)

    def test_canonical_report_is_limited_to_one_mib(self):
        report = self.report()
        report["observations"]["deviceDescription"] = "x" * (1024 * 1024)
        with self.assertRaisesRegex(ValueError, "1 MiB"):
            self.module.validate_report(report)

    def test_deterministic_identity_excludes_observations(self):
        first = self.report()
        second = copy.deepcopy(first)
        first["observations"] = {"durationMilliseconds": 1, "peakRssBytes": 10}
        second["observations"] = {"durationMilliseconds": 999, "peakRssBytes": 20}
        self.assertEqual(
            self.module.canonical_correctness_bytes(first),
            self.module.canonical_correctness_bytes(second),
        )

    def test_first_failure_uses_stable_stage_precedence(self):
        failures = [
            dict(self.failure(), stage="image", category="flip"),
            dict(self.failure(), stage="cook", category="producer"),
            dict(self.failure(), stage="strict-load", category="missing"),
        ]
        selected = self.module.select_first_failure(failures)
        self.assertEqual("cook", selected["stage"])
        self.assertEqual("producer", selected["category"])

    def test_builder_indexes_artifacts_selects_failure_and_validates_result(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "native.log").write_text("native pass\n", encoding="utf-8")
            deterministic = dict(self.report("failed", "vulkan")["deterministic"])
            deterministic.pop("evidenceDigest")
            deterministic.pop("firstFailure")
            observations = {
                "deviceClass": "macos.apple8.moltenvk.rgba8",
                "flip": {"state": "not-run", "reason": "semantic probe failed"},
            }
            report = self.module.build_report(
                deterministic,
                observations,
                [(root / "native.log", "log")],
                root,
                [
                    dict(self.failure(), stage="image", category="flip"),
                    dict(self.failure(), stage="native", category="submit"),
                ],
            )
            self.assertEqual("native", report["deterministic"]["firstFailure"]["stage"])
            self.assertEqual(1, len(report["artifacts"]))
            self.module.validate_report(report, root)

    def test_builder_writes_canonical_report_and_correctness_is_stable_twenty_times(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            deterministic = dict(self.report()["deterministic"])
            deterministic.pop("evidenceDigest")
            deterministic.pop("firstFailure")
            correctness = []
            for run in range(20):
                report = self.module.build_report(
                    deterministic,
                    {"durationMilliseconds": run, "peakRssBytes": 100 + run},
                    [], root, [],
                )
                correctness.append(
                    self.module.canonical_correctness_bytes(report)
                )
            self.assertEqual(1, len(set(correctness)))
            output = root / "report.json"
            self.module.write_report(output, report, root)
            self.assertEqual(
                self.module.canonical_bytes(report) + b"\n",
                output.read_bytes(),
            )
            self.assertEqual(report, json.loads(output.read_text(encoding="utf-8")))

    def test_schema_contract_stays_in_parity_with_runtime_validator(self):
        schema = (
            SCRIPT.parents[2]
            / "specs/028-production-content-acceptance/contracts/production-acceptance-report.schema.json"
        )
        self.module.validate_schema_contract(schema)


if __name__ == "__main__":
    unittest.main()
