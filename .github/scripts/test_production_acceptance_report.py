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
        authority = {
            "executionClass": "github-hosted",
            "preflight": {
                "state": "not-required",
                "reason": "hosted execution owns no physical authority",
            },
            "measurements": {
                "timing": {
                    "disposition": "operational", "state": "measured",
                    "completed": True,
                },
                "rss": {"disposition": "observed", "state": "measured"},
                "image": {
                    "disposition": "not-required",
                    "state": "not-required",
                    "reason": "hosted profile owns no image authority",
                },
            },
        }
        observations = {
            "durationMilliseconds": 10,
            "rssGrowthBytes": 20,
        }
        if backend != "none":
            observations.update({
                "deviceClass": "macos.apple8.metal.rgba8",
                "flip": {
                    "state": "not-required",
                    "reason": "hosted profile owns no image authority",
                },
            })
        return {
            "schema": "stoner.production-acceptance-report",
            "schemaVersion": 2,
            "deterministic": deterministic,
            "authority": authority,
            "observations": observations,
            "artifacts": [],
        }

    def make_physical(self, report):
        changed = copy.deepcopy(report)
        changed["authority"] = {
            "executionClass": "maintainer-local-metal",
            "preflight": {"state": "passed", "evidenceDigest": SHA},
            "measurements": {
                "timing": {
                    "disposition": "operational", "state": "measured",
                    "completed": True,
                },
                "rss": {
                    "disposition": "required", "state": "measured",
                    "threshold": 16 * 1024 * 1024, "passed": True,
                    "preflightEvidenceDigest": SHA,
                },
                "image": {
                    "disposition": "required", "state": "measured",
                    "passed": True, "preflightEvidenceDigest": SHA,
                },
            },
        }
        if changed["deterministic"]["backend"] != "none":
            changed["observations"]["flip"] = {
                "state": "measured", "baselineId": "lantern-metal",
                "mean": 0.0, "p95": 0.0, "maximum": 0.0,
                "badPixelThreshold": 0.01, "badPixelFraction": 0.0,
                "passed": True,
            }
        return changed

    def make_windows_physical(self, report):
        changed = self.make_physical(report)
        changed["authority"]["executionClass"] = (
            "maintainer-local-windows-vulkan"
        )
        changed["authority"]["measurements"]["rss"] = {
            "disposition": "observed", "state": "measured",
        }
        return changed

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

    def test_native_reports_require_registered_class_and_disposition_owned_flip_state(self):
        report = self.report(backend="metal")
        self.module.validate_report(report)
        del report["observations"]["deviceClass"]
        with self.assertRaisesRegex(ValueError, "device class"):
            self.module.validate_report(report)
        physical = self.make_physical(self.report(backend="vulkan"))
        self.module.validate_report(physical)
        physical["observations"]["flip"] = {
            "state": "not-run", "reason": "comparison unavailable"
        }
        with self.assertRaisesRegex(ValueError, "required image"):
            self.module.validate_report(physical)

    def test_failed_native_report_can_record_not_run_flip(self):
        report = self.report("failed", "vulkan")
        report["deterministic"]["firstFailure"] = self.failure()
        self.module.validate_report(report)

    def test_hosted_observed_rss_cannot_fail_or_promote_the_result(self):
        report = self.report("passed", "metal")
        report["observations"]["rssGrowthBytes"] = 512 * 1024 * 1024
        self.module.validate_report(report)
        report["authority"]["measurements"]["rss"] = {
            "disposition": "required", "state": "measured",
            "threshold": 16 * 1024 * 1024, "passed": False,
            "preflightEvidenceDigest": SHA,
        }
        with self.assertRaisesRegex(ValueError, "promotion|authority"):
            self.module.validate_report(report)

    def test_operational_timeout_fails_only_incomplete_work(self):
        report = self.report("passed")
        report["authority"]["measurements"]["timing"]["completed"] = False
        with self.assertRaisesRegex(ValueError, "operational timeout"):
            self.module.validate_report(report)
        report["deterministic"]["result"] = "failed"
        report["deterministic"]["firstFailure"] = dict(
            self.failure(), stage="timeout", category="operational-timeout"
        )
        self.module.validate_report(report)

    def test_controlled_physical_requires_preflighted_rss_and_image_passes(self):
        report = self.make_physical(self.report("passed", "metal"))
        self.module.validate_report(report)
        for kind in ("rss", "image"):
            changed = copy.deepcopy(report)
            changed["authority"]["measurements"][kind]["passed"] = False
            with self.assertRaisesRegex(ValueError, f"required {kind}"):
                self.module.validate_report(changed)
        changed = copy.deepcopy(report)
        changed["authority"]["preflight"] = {
            "state": "not-required", "reason": "caller override"
        }
        with self.assertRaisesRegex(ValueError, "preflight"):
            self.module.validate_report(changed)

    def test_image_not_required_uses_a_reason_instead_of_fake_flip(self):
        report = self.report("passed", "metal")
        self.module.validate_report(report)
        report["observations"]["flip"] = {
            "state": "measured", "baselineId": "fabricated",
            "mean": 0.0, "p95": 0.0, "maximum": 0.0,
            "badPixelThreshold": 0.01, "badPixelFraction": 0.0,
            "passed": True,
        }
        with self.assertRaisesRegex(ValueError, "not-required image"):
            self.module.validate_report(report)

    def test_windows_vulkan_local_physical_authority_is_supported(self):
        report = self.make_windows_physical(
            self.report("passed", "vulkan")
        )
        report["observations"]["deviceClass"] = (
            "windows.discrete-vulkan.rgba8"
        )
        report["observations"]["rssGrowthBytes"] = 169_361_408
        self.module.validate_report(report)

        promoted = copy.deepcopy(report)
        promoted["authority"]["measurements"]["rss"] = {
            "disposition": "required", "state": "measured",
            "threshold": 16 * 1024 * 1024, "passed": True,
            "preflightEvidenceDigest": SHA,
        }
        with self.assertRaisesRegex(ValueError, "promotion"):
            self.module.validate_report(promoted)

    def test_metal_physical_authority_cannot_demote_required_rss(self):
        report = self.make_physical(self.report("passed", "metal"))
        report["authority"]["measurements"]["rss"] = {
            "disposition": "observed", "state": "measured",
        }
        with self.assertRaisesRegex(ValueError, "required rss"):
            self.module.validate_report(report)

    def test_aggregate_preserves_authority_and_rejects_promotion(self):
        first = self.report("passed", "metal")
        second = copy.deepcopy(first)
        authority = self.module.validate_aggregate_authority([first, second])
        self.assertEqual("observed", authority["measurements"]["rss"]["disposition"])
        second["authority"]["measurements"]["rss"] = {
            "disposition": "required", "state": "measured",
            "threshold": 16 * 1024 * 1024, "passed": True,
            "preflightEvidenceDigest": SHA,
        }
        with self.assertRaisesRegex(ValueError, "aggregate.*authority"):
            self.module.validate_aggregate_authority([first, second])

    def test_unknown_fields_are_rejected_at_every_level(self):
        for path in (("root",), ("deterministic",), ("authority",), ("observations",)):
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
        first["observations"].update({
            "durationMilliseconds": 1, "peakRssBytes": 10,
            "taskVmBytes": 11, "allocatorBytes": 12,
        })
        second["observations"].update({
            "durationMilliseconds": 999, "peakRssBytes": 20,
            "taskVmBytes": 21, "allocatorBytes": 22,
        })
        for report in (first, second):
            report["authority"]["measurements"].update({
                "taskVm": {"disposition": "observed", "state": "measured"},
                "allocator": {"disposition": "observed", "state": "measured"},
                "peakRss": {"disposition": "observed", "state": "measured"},
            })
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
            base = self.report("failed", "vulkan")
            deterministic = dict(base["deterministic"])
            deterministic.pop("evidenceDigest")
            deterministic.pop("firstFailure")
            observations = dict(base["observations"])
            report = self.module.build_report(
                deterministic,
                base["authority"],
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
                authority = copy.deepcopy(self.report()["authority"])
                authority["measurements"]["peakRss"] = {
                    "disposition": "observed", "state": "measured"
                }
                report = self.module.build_report(
                    deterministic,
                    authority,
                    {
                        "durationMilliseconds": run,
                        "rssGrowthBytes": run,
                        "peakRssBytes": 100 + run,
                    },
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
