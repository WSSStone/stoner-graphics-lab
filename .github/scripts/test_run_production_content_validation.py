#!/usr/bin/env python3

import importlib.util
import argparse
import json
from pathlib import Path
import platform
import sys
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).with_name("run_production_content_validation.py")


class ProductionContentRunnerContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.assertTrue(
            SCRIPT.is_file(), "production validation runner is not implemented"
        )
        spec = importlib.util.spec_from_file_location(
            "run_production_content_validation", SCRIPT
        )
        cls.module = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = cls.module
        spec.loader.exec_module(cls.module)

    def test_runner_exposes_profile_driven_entry_point(self):
        self.assertTrue(callable(getattr(self.module, "run_profile", None)))

    def test_cook_command_preserves_source_order_and_explicit_render_roots(self):
        roots = (
            "StaticModel:Lantern.glb#idx.scene.0",
            *self.module.DEFERRED_SHADER_ROOTS,
        )
        command = self.module.build_cook_command(
            Path("cooker"), Path("package"), Path("shader"),
            roots, Path("profile"),
            Path("publication"), Path("ddc"), Path("report"), True, 4,
        )
        self.assertEqual(2, command.count("--source-root"))
        self.assertLess(command.index("package"), command.index("shader"))
        self.assertEqual(len(roots), command.count("--root"))
        self.assertEqual(roots[0], command[command.index("--root") + 1])
        for root in self.module.DEFERRED_SHADER_ROOTS:
            self.assertIn(root, command)
        self.assertIn("--clean", command)

    def test_metal_doctor_preflight_is_normalized_and_serialized(self):
        command = self.module.build_metal_doctor_command(
            Path("cooker"), Path("profile"), Path("doctor.json")
        )
        self.assertEqual([
            "cooker", "doctor",
            "--target-profile", "profile",
            "--normalized-report",
            "--report", "doctor.json",
        ], command)
        with mock.patch.object(self.module.os, "cpu_count", return_value=16):
            self.assertEqual(
                2, self.module.clean_cook_concurrency(
                    20, "metal", "arm64"
                )
            )
            self.assertEqual(
                4, self.module.clean_cook_concurrency(
                    20, "metal", "x86_64"
                )
            )
            self.assertEqual(
                4, self.module.clean_cook_concurrency(
                    20, "vulkan", "x86_64"
                )
            )

    def test_clean_reports_must_be_byte_identical(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            reports = []
            for index in range(20):
                path = root / f"{index:02d}.json"
                path.write_text('{"stable":true}\n', encoding="utf-8")
                reports.append(path)
            digest = self.module.assert_clean_determinism(reports)
            self.assertEqual(64, len(digest))
            reports[-1].write_text('{"stable":false}\n', encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "differ"):
                self.module.assert_clean_determinism(reports)

    def test_acceptance_correctness_is_identical_across_twenty_observational_runs(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            reports = []
            for index in range(20):
                path = root / f"acceptance-{index:02d}.json"
                path.write_text(json.dumps({
                    "schema": "stoner.production-acceptance-report",
                    "schemaVersion": 1,
                    "deterministic": {"generationIdentity": "a" * 64},
                    "observations": {
                        "durationMilliseconds": index,
                        "peakRssBytes": 1000 + index,
                    },
                    "artifacts": [],
                }), encoding="utf-8")
                reports.append(path)
            digest = self.module.assert_acceptance_correctness_determinism(
                reports
            )
            self.assertEqual(64, len(digest))
            changed = json.loads(reports[-1].read_text(encoding="utf-8"))
            changed["deterministic"]["generationIdentity"] = "b" * 64
            reports[-1].write_text(json.dumps(changed), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "correctness"):
                self.module.assert_acceptance_correctness_determinism(reports)

    def test_warm_report_requires_full_reuse_and_no_new_cook(self):
        valid = {"summary": {
            "reachable": 19, "reused": 19, "cooked": 0, "failed": 0,
        }}
        self.module.assert_full_reuse(valid)
        invalid = json.loads(json.dumps(valid))
        invalid["summary"]["reused"] = 18
        with self.assertRaisesRegex(ValueError, "reuse"):
            self.module.assert_full_reuse(invalid)

    def test_source_tree_digest_detects_mutation(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "asset.bin").write_bytes(b"before")
            before = self.module.tree_digest(root)
            (root / "asset.bin").write_bytes(b"after")
            self.assertNotEqual(before, self.module.tree_digest(root))

    def test_default_determinism_count_is_twenty(self):
        parsed = self.module.parse_args([
            "--profile", "regular",
            "--target-profile", "profile.json",
            "--build-root", "Build",
            "--output", "Output",
        ])
        self.assertEqual(20, parsed.determinism_runs)

    def test_native_stage_has_independent_cap_inside_complete_lane(self):
        with mock.patch.object(
            self.module.time, "monotonic", return_value=100.0
        ):
            self.assertEqual(
                3600,
                self.module.native_stage_timeout(4000.0, 3900, 3600),
            )
            self.assertEqual(
                3500,
                self.module.native_stage_timeout(3600.0, 3900, 3600),
            )

    def test_profile_selection_requires_exact_target_and_package_membership(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile_root = root / "Config/Validation/ProductionContent"
            profile_root.mkdir(parents=True)
            target = root / "Config/AssetCooker/Profiles/Test.json"
            target.parent.mkdir(parents=True)
            target.write_text("{}\n", encoding="utf-8")
            (profile_root / "Regular.json").write_text(json.dumps({
                "schema": "stoner.production-validation-profile",
                "schemaVersion": 2,
                "profileId": "regular",
                "corpusRevision": "revision-1",
                "targetProfiles": [
                    "Config/AssetCooker/Profiles/Test.json",
                ],
                "packageIds": ["package-a"],
                "lifecycleCycles": 20,
                "warmupCycles": 2,
                "maxRssGrowthBytes": 16 * 1024 * 1024,
                "timeBudgetSeconds": 600,
                "nativeTimeBudgetSeconds": 600,
                "cadence": ["relevant-pull-request", "relevant-push"],
                "requiredGates": [
                    "corpus", "import", "clean-cook", "warm-cook",
                    "strict-runtime", "semantic-equivalence",
                    "transactional-realization", "platform-applicable-native",
                    "lifecycle", "report",
                ],
                "authorityPolicy": self.module.expected_authority_policy(
                    "regular"
                ),
            }), encoding="utf-8")
            previous = self.module.VALIDATION_PROFILES
            self.module.VALIDATION_PROFILES = Path(
                "Config/Validation/ProductionContent"
            )
            try:
                profile, selected = self.module.validate_profile_selection(
                    root, "regular", target, {
                        "corpusRevision": "revision-1",
                        "packages": [
                            {"packageId": "package-a"},
                            {"packageId": "package-b"},
                        ],
                    },
                )
                self.assertEqual(["package-a"], profile["packageIds"])
                self.assertEqual(["package-a"], [
                    item["packageId"] for item in selected
                ])
                with self.assertRaisesRegex(ValueError, "not declared"):
                    self.module.validate_profile_selection(
                        root, "regular", root / "Other.json", {
                            "corpusRevision": "revision-1",
                            "packages": [{"packageId": "package-a"}],
                        },
                    )
            finally:
                self.module.VALIDATION_PROFILES = previous

    def test_normalized_report_rejects_host_telemetry_and_wrong_command(self):
        with tempfile.TemporaryDirectory() as directory:
            report = Path(directory) / "report.json"
            report.write_text(json.dumps({
                "schema": "stoner.asset-cook-report",
                "result": "success",
                "command": "cook",
                "telemetry": {"seconds": 1.0},
            }), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "invalid normalized"):
                self.module.load_report(report, "cook")
            report.write_text(json.dumps({
                "schema": "stoner.asset-cook-report",
                "result": "success",
                "command": "validate",
            }), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "invalid normalized"):
                self.module.load_report(report, "cook")

    def test_existing_validation_output_is_never_overwritten(self):
        parsed = self.module.parse_args([
            "--profile", "regular",
            "--target-profile", "profile.json",
            "--build-root", "Build",
            "--output", "Output",
        ])
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            parsed.root = root
            parsed.target_profile = Path("profile.json")
            parsed.build_root = Path("Build")
            parsed.output = Path("Output")
            parsed.content_root = Path("Content")
            (root / "profile.json").write_text("{}", encoding="utf-8")
            (root / "Build/Tools/AssetCooker").mkdir(parents=True)
            (root / "Build/Tests").mkdir(parents=True)
            (root / "Build/Tools/AssetCooker/StonerAssetCooker").touch()
            (root / "Build/Tests/StonerTest").touch()
            (root / "Output").mkdir()
            (root / "Output/prior-evidence.json").write_text(
                "{}", encoding="utf-8"
            )
            with self.assertRaisesRegex(FileExistsError, "must be empty"):
                self.module.run_profile(parsed)

    def test_determinism_count_rejects_values_outside_one_to_twenty(self):
        for value in (0, 21):
            with self.assertRaises(SystemExit):
                self.module.parse_args([
                    "--profile", "regular",
                    "--target-profile", "profile.json",
                    "--build-root", "Build",
                    "--output", "Output",
                    "--determinism-runs", str(value),
                ])

    def test_shipping_profiles_have_exact_tier_contracts(self):
        expected = {
            "regular": (20, 2, 600),
            "medium": (1000, 20, 3900),
            "hardware": (1000, 20, 3600),
        }
        for profile_id, (cycles, warmup, budget) in expected.items():
            profile = self.module.load_validation_profile(
                self.module.REPOSITORY_ROOT, profile_id
            )
            self.assertEqual(cycles, profile["lifecycleCycles"])
            self.assertEqual(warmup, profile["warmupCycles"])
            self.assertEqual(16 * 1024 * 1024, profile["maxRssGrowthBytes"])
            self.assertEqual(budget, profile["timeBudgetSeconds"])
            self.assertEqual(
                3600 if profile_id == "medium" else budget,
                profile["nativeTimeBudgetSeconds"],
            )
            self.assertLess(profile["warmupCycles"], profile["lifecycleCycles"])

    def test_only_medium_packages_run_concurrently(self):
        self.assertEqual(
            2, self.module.profile_package_concurrency("medium", 2)
        )
        self.assertEqual(
            1, self.module.profile_package_concurrency("medium", 1)
        )
        self.assertEqual(
            1, self.module.profile_package_concurrency("regular", 2)
        )
        self.assertEqual(
            1, self.module.profile_package_concurrency("hardware", 2)
        )
        with self.assertRaisesRegex(ValueError, "profile"):
            self.module.profile_package_concurrency("unknown", 2)
        with self.assertRaisesRegex(ValueError, "package count"):
            self.module.profile_package_concurrency("medium", 0)

    def test_clean_determinism_is_owned_only_by_regular_profile(self):
        self.assertEqual(
            20,
            self.module.profile_package_clean_runs(
                "regular", "regular", 20
            ),
        )
        self.assertEqual(
            1,
            self.module.profile_package_clean_runs(
                "medium", "regular", 20
            ),
        )
        self.assertEqual(
            1,
            self.module.profile_package_clean_runs(
                "medium", "medium", 20
            ),
        )
        self.assertEqual(
            1,
            self.module.profile_package_clean_runs(
                "hardware", "regular", 20
            ),
        )
        with self.assertRaisesRegex(ValueError, "package tier"):
            self.module.profile_package_clean_runs(
                "medium", "unknown", 20
            )

    def test_medium_package_shards_are_exact_and_fail_closed(self):
        packages = [
            {"packageId": "khronos-lantern-glb"},
            {"packageId": "khronos-sponza-gltf"},
        ]
        self.assertEqual(
            packages,
            self.module.select_profile_packages(
                "medium", packages, None
            ),
        )
        self.assertEqual(
            [packages[1]],
            self.module.select_profile_packages(
                "medium", packages, "khronos-sponza-gltf"
            ),
        )
        with self.assertRaisesRegex(ValueError, "only by medium"):
            self.module.select_profile_packages(
                "regular", packages, "khronos-lantern-glb"
            )
        with self.assertRaisesRegex(ValueError, "not declared"):
            self.module.select_profile_packages(
                "medium", packages, "unknown"
            )

    def test_optional_native_lock_serializes_only_wrapped_action(self):
        events = []

        class RecordingLock:
            def __enter__(self):
                events.append("enter")

            def __exit__(self, exception_type, exception, traceback):
                events.append("exit")

        result = self.module.run_with_optional_lock(
            lambda: events.append("native") or "passed",
            RecordingLock(),
        )
        self.assertEqual("passed", result)
        self.assertEqual(["enter", "native", "exit"], events)

        events.clear()
        result = self.module.run_with_optional_lock(
            lambda: events.append("native") or "passed",
            None,
        )
        self.assertEqual("passed", result)
        self.assertEqual(["native"], events)

    def test_optional_native_barrier_precedes_isolated_execution(self):
        events = []

        class RecordingBarrier:
            def wait(self, timeout):
                events.append(("wait", timeout))

        self.module.wait_for_optional_barrier(RecordingBarrier(), 17)
        self.module.wait_for_optional_barrier(None, 17)
        self.assertEqual([("wait", 17)], events)

        class BrokenBarrier:
            def wait(self, timeout):
                raise self.module.threading.BrokenBarrierError

        broken = BrokenBarrier()
        broken.module = self.module
        with self.assertRaisesRegex(
            self.module.StageFailure, "barrier-broken"
        ):
            self.module.wait_for_optional_barrier(broken, 17)

    def test_profile_parser_rejects_unknown_fields_and_wrong_boundaries(self):
        regular = json.loads((
            self.module.REPOSITORY_ROOT
            / "Config/Validation/ProductionContent/Regular.json"
        ).read_text(encoding="utf-8"))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profiles = root / "Config/Validation/ProductionContent"
            profiles.mkdir(parents=True)
            previous = self.module.VALIDATION_PROFILES
            self.module.VALIDATION_PROFILES = Path(
                "Config/Validation/ProductionContent"
            )
            try:
                for field, value, error in (
                    ("unknown", True, "fields"),
                    ("lifecycleCycles", 19, "cycle"),
                    ("warmupCycles", 3, "warm-up"),
                    ("maxRssGrowthBytes", 1, "RSS"),
                    ("timeBudgetSeconds", 601, "budget"),
                ):
                    candidate = dict(regular)
                    candidate[field] = value
                    (profiles / "Regular.json").write_text(
                        json.dumps(candidate), encoding="utf-8"
                    )
                    with self.assertRaisesRegex(ValueError, error):
                        self.module.load_validation_profile(root, "regular")
            finally:
                self.module.VALIDATION_PROFILES = previous

    def test_profile_parser_requires_exact_cadence_and_required_gates(self):
        profile = self.module.load_validation_profile(
            self.module.REPOSITORY_ROOT, "regular"
        )
        self.assertEqual(
            ["relevant-pull-request", "relevant-push"], profile["cadence"]
        )
        self.assertEqual(
            [
                "corpus", "import", "clean-cook", "warm-cook",
                "strict-runtime", "semantic-equivalence",
                "transactional-realization", "platform-applicable-native",
                "lifecycle", "report",
            ],
            profile["requiredGates"],
        )

    def test_timeout_is_bounded_and_reports_stable_stage(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            with self.assertRaises(self.module.StageFailure) as raised:
                self.module.run_stage(
                    "strict-runtime",
                    [sys.executable, "-c", "import time; time.sleep(2)"],
                    root,
                    1,
                )
            self.assertEqual("timeout", raised.exception.category)
            self.assertEqual("strict-runtime", raised.exception.stage)

    def test_profile_deadline_is_shared_across_all_stages(self):
        with self.assertRaises(self.module.StageFailure) as raised:
            self.module.remaining_stage_timeout(
                self.module.time.monotonic() - 1.0, 600
            )
        self.assertEqual("profile", raised.exception.stage)
        self.assertEqual("timeout", raised.exception.category)

    def test_equivalence_request_timeout_is_bounded_with_exit_slack(self):
        self.assertEqual(
            30, self.module.equivalence_request_timeout_seconds(35)
        )
        self.assertEqual(
            115, self.module.equivalence_request_timeout_seconds(120)
        )
        self.assertEqual(
            300, self.module.equivalence_request_timeout_seconds(1800)
        )

    def test_unsupported_never_aggregates_as_success(self):
        unsupported = self.module.unsupported_result(
            "native", "Metal device unavailable", "macos-metal-hardware"
        )
        self.assertEqual("Unsupported", unsupported["result"])
        self.assertFalse(self.module.aggregate_results([unsupported]))
        self.assertTrue(self.module.aggregate_results([
            {"result": "Passed"}, {"result": "Passed"}
        ]))

    def test_hosted_windows_native_deferral_is_explicit_and_not_a_native_pass(self):
        contract = {
            "platform": "windows",
            "cpuArchitecture": "x86_64",
            "graphicsBackend": "vulkan",
            "targetProfileDigest": "a" * 64,
        }
        deferred = self.module.deferred_native_result(contract)
        self.assertEqual("NotRun", deferred["result"])
        self.assertEqual(
            "windows-vulkan-x86_64-hardware",
            deferred["replacementLane"],
        )
        self.assertFalse(self.module.aggregate_results([deferred]))
        self.assertFalse(
            self.module.aggregate_native_results([deferred], False)
        )
        self.assertTrue(
            self.module.aggregate_native_results([deferred], True)
        )
        unsupported = self.module.unsupported_result(
            "native", "Vulkan device unavailable",
            "windows-vulkan-x86_64-hardware",
        )
        self.assertFalse(
            self.module.aggregate_native_results([unsupported], True)
        )
        self.module.validate_native_deferral("regular", contract, True)
        for profile, changed in (
            ("hardware", contract),
            ("regular", {**contract, "platform": "linux"}),
            ("regular", {**contract, "graphicsBackend": "metal"}),
        ):
            with self.assertRaisesRegex(ValueError, "restricted"):
                self.module.validate_native_deferral(profile, changed, True)

    def test_native_stage_uses_exact_backend_suite_and_lifecycle_environment(self):
        for backend, suite, prefix in (
            ("vulkan", "production-content-vulkan-native", "VULKAN"),
            ("metal", "production-content-metal-native", "METAL"),
        ):
            command, environment = self.module.build_native_lifecycle_stage(
                Path("StonerTest"), backend, Path("publication"),
                Path("lease"), "a" * 64, Path("target.json"),
                "StaticModel:Asset.glb#idx.scene.0",
                "production-content-lantern-v2", 1000, 20,
            )
            self.assertEqual(
                ["StonerTest", "--suite", suite],
                command,
            )
            self.assertEqual("1", environment[f"STONER_REQUIRE_{prefix}_PRODUCTION"])
            self.assertEqual("1000", environment["STONER_PRODUCTION_LIFECYCLE_CYCLES"])
            self.assertEqual("20", environment["STONER_PRODUCTION_WARMUP_CYCLES"])
            self.assertEqual(
                "StaticModel:Asset.glb#idx.scene.0",
                environment["STONER_PRODUCTION_ROOT"],
            )
            self.assertNotIn("STONER_PRODUCTION_VISIBLE", environment)

            _, visible_environment = self.module.build_native_lifecycle_stage(
                Path("StonerTest"), backend, Path("publication"),
                Path("lease"), "a" * 64, Path("target.json"),
                "StaticModel:Asset.glb#idx.scene.0",
                "production-content-lantern-v2", 1000, 20, require_visible=True,
            )
            self.assertEqual("1", visible_environment["STONER_PRODUCTION_VISIBLE"])
            self.assertEqual(
                "1",
                visible_environment[
                    "STONER_REQUIRE_PRODUCTION_IMAGE_ACCEPTANCE"
                ],
            )

    def test_native_lifecycle_preserves_the_normal_production_allocator(self):
        for contract in (
            {"platform": "linux", "graphicsBackend": "vulkan"},
            {
                "platform": "macos", "graphicsBackend": "metal",
                "cpuArchitecture": "x86_64",
            },
        ):
            self.assertEqual(
                {},
                self.module.native_allocator_authority_environment(
                    contract, 1000, 20, False
                ),
            )

    def test_execution_class_is_workflow_owned_and_caller_promotion_fails(self):
        self.assertEqual(
            "local-diagnostic",
            self.module.classify_execution_environment(
                "regular", {}
            )["executionClass"],
        )
        hosted = {
            "GITHUB_ACTIONS": "true",
            "RUNNER_ENVIRONMENT": "github-hosted",
            "GITHUB_WORKFLOW_REF": (
                "owner/repo/.github/workflows/"
                "feature-028-production-content.yml@refs/heads/main"
            ),
        }
        self.assertEqual(
            "github-hosted",
            self.module.classify_execution_environment(
                "medium", hosted
            )["executionClass"],
        )
        with self.assertRaisesRegex(ValueError, "caller.*execution class"):
            self.module.classify_execution_environment(
                "hardware", {
                    "STONER_PRODUCTION_EXECUTION_CLASS":
                        "controlled-physical",
                }
            )
        with self.assertRaises(SystemExit):
            self.module.parse_args([
                "--profile", "hardware", "--target-profile", "profile.json",
                "--build-root", "Build", "--output", "Output",
                "--execution-class", "controlled-physical",
            ])

    def test_controlled_physical_requires_complete_workflow_preflight(self):
        physical = {
            "GITHUB_ACTIONS": "true",
            "RUNNER_ENVIRONMENT": "self-hosted",
            "GITHUB_WORKFLOW_REF": (
                "owner/repo/.github/workflows/"
                "feature-028-production-hardware.yml@refs/heads/main"
            ),
            "GITHUB_SHA": "a" * 40,
            "STONER_PHYSICAL_DEVICE_CLASS": "macos.apple8.metal.rgba8",
            "STONER_PHYSICAL_EXCLUSIVE": "1",
            "STONER_PHYSICAL_FROZEN_REVISION": "a" * 40,
            "STONER_PHYSICAL_ALLOCATOR": "default-production",
            "STONER_PHYSICAL_SAMPLE_PROTOCOL": "warmup20-terminal1000",
            "STONER_PHYSICAL_PRESENTATION": "window-readback",
        }
        authority = self.module.classify_execution_environment(
            "hardware", physical
        )
        self.assertEqual("controlled-physical", authority["executionClass"])
        self.assertEqual("required", authority["dispositions"]["rss"])
        self.assertEqual("required", authority["dispositions"]["image"])
        for field in self.module.PHYSICAL_PREFLIGHT_ENVIRONMENT:
            changed = dict(physical)
            changed.pop(field)
            with self.assertRaises(self.module.StageFailure) as raised:
                self.module.classify_execution_environment(
                    "hardware", changed
                )
            self.assertEqual("unsupported", raised.exception.stage)

    def test_measurement_dispositions_separate_required_operational_and_observed(self):
        regular = self.module.expected_authority_policy("regular")
        self.assertEqual(
            {
                "rss": "observed", "timing": "operational",
                "image": "not-required",
            },
            regular["executionClasses"]["github-hosted"],
        )
        hardware = self.module.expected_authority_policy("hardware")
        self.assertEqual(
            {
                "rss": "required", "timing": "operational",
                "image": "required",
            },
            hardware["executionClasses"]["controlled-physical"],
        )

    def test_each_production_package_has_an_exact_workload_revision(self):
        self.assertEqual(
            "production-content-lantern-v2",
            self.module.package_workload_revision({
                "packageId": "khronos-lantern-glb",
            }),
        )
        self.assertEqual(
            "production-content-sponza-v2",
            self.module.package_workload_revision({
                "packageId": "khronos-sponza-gltf",
            }),
        )
        with self.assertRaisesRegex(ValueError, "not declared"):
            self.module.package_workload_revision({
                "packageId": "undeclared-package",
            })

    def test_lifecycle_evidence_requires_exact_cycles_warmup_and_zero_owners(self):
        valid = (
            "[EVIDENCE] backend=metal cycles=1000 warmup-cycle=20 "
            "warmup-rss=100 terminal-rss=110 peak-rss=120 growth=10 "
            "captures=2000 readbacks=7 counters=0 stale=1\n"
        )
        parsed = self.module.parse_native_lifecycle_evidence(
            valid, 1000, 20, "required"
        )
        self.assertEqual(10, parsed["rssGrowthBytes"])
        self.assertEqual(120, parsed["peakRssBytes"])
        for changed in (
            valid.replace("cycles=1000", "cycles=20"),
            valid.replace("warmup-cycle=20", "warmup-cycle=2"),
            valid.replace("counters=0", "counters=1"),
            valid.replace("stale=1", "stale=0"),
        ):
            with self.assertRaisesRegex(ValueError, "lifecycle"):
                self.module.parse_native_lifecycle_evidence(
                    changed, 1000, 20, "required"
                )
        high_rss = valid.replace(
            "growth=10", f"growth={16 * 1024 * 1024 + 1}"
        )
        with self.assertRaisesRegex(ValueError, "RSS"):
            self.module.parse_native_lifecycle_evidence(
                high_rss, 1000, 20, "required"
            )
        observed = self.module.parse_native_lifecycle_evidence(
            high_rss, 1000, 20, "observed"
        )
        self.assertFalse(observed["rssWithinLimit"])
        self.assertEqual("observed", observed["rssDisposition"])

    def test_native_image_evidence_requires_exact_measured_pass(self):
        valid = (
            "[IMAGE] backend=metal device-class=macos.apple8.metal.rgba8 "
            "baseline=lantern-metal semantic-probes=18 mean=0.00100000 "
            "p95=0.00200000 maximum=0.03000000 bad-fraction=0.00010000 "
            "result=passed\n"
        )
        parsed = self.module.parse_native_image_evidence(valid, "metal")
        self.assertEqual("macos.apple8.metal.rgba8", parsed["deviceClass"])
        self.assertTrue(parsed["flip"]["passed"])
        for changed in (
            valid.replace("backend=metal", "backend=vulkan"),
            valid.replace("result=passed", "result=failed"),
            valid.replace("mean=0.00100000", "mean=nan"),
            valid + valid,
        ):
            with self.assertRaisesRegex(ValueError, "image"):
                self.module.parse_native_image_evidence(changed, "metal")

    def test_target_profile_native_host_contract_is_explicit(self):
        contract = self.module.load_native_target_contract(
            self.module.REPOSITORY_ROOT
            / "Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json"
        )
        self.assertEqual("macos", contract["platform"])
        self.assertEqual("arm64", contract["cpuArchitecture"])
        self.assertEqual("metal", contract["graphicsBackend"])
        self.assertEqual(64, len(contract["targetProfileDigest"]))
        self.assertIn(platform.system().lower(), ("darwin", "linux", "windows"))

    def test_native_host_mismatch_is_unsupported_and_not_success(self):
        contract = {
            "platform": "windows",
            "cpuArchitecture": "x86_64",
            "graphicsBackend": "vulkan",
        }
        unsupported = self.module.native_host_support(
            contract, host_system="darwin", host_machine="arm64"
        )
        self.assertEqual("Unsupported", unsupported["result"])
        self.assertEqual(
            "windows-vulkan-x86_64-hardware",
            unsupported["firstFailure"]["replacementLane"],
        )
        self.assertFalse(self.module.aggregate_results([unsupported]))
        self.assertIsNone(self.module.native_host_support(
            contract, host_system="windows", host_machine="AMD64"
        ))

    def test_metal_cook_requires_matching_macos_architecture(self):
        metal = {
            "platform": "macos",
            "cpuArchitecture": "x86_64",
            "graphicsBackend": "metal",
        }
        unsupported = self.module.cook_host_support(
            metal, host_system="darwin", host_machine="arm64"
        )
        self.assertEqual("Unsupported", unsupported["result"])
        self.assertEqual("cook", unsupported["firstFailure"]["stage"])
        self.assertEqual(
            "macos-metal-x86_64-hardware",
            unsupported["firstFailure"]["replacementLane"],
        )
        self.assertIsNone(self.module.cook_host_support(
            metal, host_system="darwin", host_machine="x86_64"
        ))
        self.assertIsNone(self.module.cook_host_support(
            metal, host_system="darwin", host_machine="x86_64",
            host_translated=True,
        ))
        vulkan = {
            "platform": "windows",
            "cpuArchitecture": "x86_64",
            "graphicsBackend": "vulkan",
        }
        self.assertIsNone(self.module.cook_host_support(
            vulkan, host_system="darwin", host_machine="arm64"
        ))

    def test_rosetta_metal_native_requires_real_intel_hardware(self):
        metal = {
            "platform": "macos",
            "cpuArchitecture": "x86_64",
            "graphicsBackend": "metal",
        }
        unsupported = self.module.native_host_support(
            metal, host_system="darwin", host_machine="x86_64",
            host_translated=True,
        )
        self.assertEqual("Unsupported", unsupported["result"])
        self.assertEqual("native", unsupported["firstFailure"]["stage"])
        self.assertIn(
            "physical macos x86_64 Metal host",
            unsupported["firstFailure"]["missingPrerequisite"],
        )
        self.assertEqual(
            "macos-metal-x86_64-hardware",
            unsupported["firstFailure"]["replacementLane"],
        )
        self.assertIsNone(self.module.native_host_support(
            metal, host_system="darwin", host_machine="x86_64",
            host_translated=False,
        ))
        with mock.patch.dict(
            "os.environ", {"STONER_ROSETTA_TRANSLATED": "1"}, clear=False
        ):
            self.assertTrue(self.module.rosetta_translated("macos"))

    def test_matching_host_runs_only_authoritative_production_lifecycle(self):
        system = {"darwin": "macos"}.get(
            platform.system().lower(), platform.system().lower()
        )
        machine = {
            "amd64": "x86_64", "x86_64": "x86_64",
            "aarch64": "arm64", "arm64": "arm64",
        }[platform.machine().lower()]
        backend = "metal" if system == "macos" else "vulkan"
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            profile = root / "target.json"
            profile.write_text(json.dumps({
                "schema": "stoner.asset-target-profile",
                "schemaVersion": 1,
                "platform": system,
                "cpuArchitecture": machine,
                "graphicsBackend": backend,
            }), encoding="utf-8")
            report = root / "native.txt"
            output = (
                f"[EVIDENCE] backend={backend} cycles=20 warmup-cycle=2 "
                "warmup-rss=100 terminal-rss=100 peak-rss=100 growth=0 "
                "captures=40 readbacks=7 counters=0 stale=1\n"
            )
            with mock.patch.object(
                self.module, "run_stage",
                return_value=self.module.CommandResult(1.0, output, ""),
            ) as run_stage:
                result = self.module.run_native_lifecycle(
                    root, Path("StonerTest"), profile,
                    Path("publication"), Path("lease"), "a" * 64,
                    "StaticModel:Asset.glb#idx.scene.0",
                    "production-content-lantern-v2", 20, 2, report, 60,
                )
            self.assertEqual("Passed", result["result"])
            self.assertIsNone(result["imageAcceptance"])
            self.assertEqual(1, run_stage.call_count)
            self.assertEqual(
                ["StonerTest", "--suite",
                 f"production-content-{backend}-native"],
                run_stage.call_args.args[1],
            )

    def test_artifact_revalidation_rejects_substitution(self):
        with tempfile.TemporaryDirectory() as directory:
            artifact = Path(directory) / "generation.json"
            artifact.write_bytes(b"stable")
            record = self.module.artifact_record(artifact, Path(directory))
            self.module.revalidate_artifact(record, Path(directory))
            artifact.write_bytes(b"change")
            with self.assertRaisesRegex(ValueError, "digest"):
                self.module.revalidate_artifact(record, Path(directory))

    def test_validation_output_revalidates_target_current_and_manifest(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            output = root / "output"
            profile = root / "profile.json"
            output.mkdir()
            profile.write_bytes(b'{"target":"mac-metal"}\n')
            generation = "a" * 64
            publication = output / "package/clean-00/publication"
            manifest = publication / f"Generations/{generation}/Manifest.json"
            manifest.parent.mkdir(parents=True)
            manifest.write_bytes(b'{"generation":"stable"}\n')
            manifest_digest = self.module.sha256_bytes(manifest.read_bytes())
            current = publication / "Current.json"
            current.write_text(json.dumps({
                "schema": "stoner.asset-current-generation",
                "schemaVersion": 1,
                "generationId": generation,
                "manifestLocator": f"Generations/{generation}/Manifest.json",
                "manifestDigest": manifest_digest,
            }, sort_keys=True, separators=(",", ":")), encoding="utf-8")
            (output / "summary.json").write_text(json.dumps({
                "passed": True,
                "targetProfile": "profile.json",
                "targetProfileDigest": self.module.sha256_bytes(profile.read_bytes()),
                "packages": [{
                    "packageId": "package",
                    "generationId": generation,
                    "currentPointerDigest": self.module.sha256_bytes(current.read_bytes()),
                    "generationManifestDigest": manifest_digest,
                }],
            }), encoding="utf-8")
            self.module.write_artifact_manifest(output)
            verified = self.module.verify_validation_output(output, profile)
            self.assertEqual("Passed", verified["result"])
            self.assertTrue(verified["passed"])
            profile.write_bytes(b'{"target":"substituted"}\n')
            with self.assertRaisesRegex(ValueError, "target profile digest"):
                self.module.verify_validation_output(output, profile)

    def test_hardware_profile_is_a_supported_cli_choice(self):
        parsed = self.module.parse_args([
            "--profile", "hardware",
            "--target-profile", "profile.json",
            "--build-root", "Build",
            "--output", "Output",
        ])
        self.assertEqual("hardware", parsed.profile)

    def test_failure_catalog_has_bounded_cross_stage_first_failures(self):
        catalog = self.module.load_failure_catalog(self.module.REPOSITORY_ROOT)
        self.assertGreaterEqual(len(catalog), 30)
        self.assertEqual(len(catalog), len({case["caseId"] for case in catalog}))
        self.assertTrue({
            "corpus", "import", "cook", "publication", "strict-load",
            "realization", "native", "image", "lifecycle", "timeout",
            "unsupported",
        }.issubset({case["stage"] for case in catalog}))
        for case in catalog:
            failure = self.module.failure_from_catalog_case(case)
            self.assertEqual(case["stage"], failure["stage"])
            self.assertEqual(case["expectedCategory"], failure["category"])
            self.assertEqual(case["reproductionProfile"], failure["reproductionProfile"])


if __name__ == "__main__":
    unittest.main()
