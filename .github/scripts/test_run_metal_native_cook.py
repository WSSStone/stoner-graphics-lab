from __future__ import annotations

import importlib.util
import pathlib
import unittest


SCRIPT = pathlib.Path(__file__).with_name("run_metal_native_cook.py")
SPEC = importlib.util.spec_from_file_location("run_metal_native_cook", SCRIPT)
assert SPEC and SPEC.loader
validation = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(validation)


class MetalNativeCookRunnerTests(unittest.TestCase):
    def test_default_roots_cover_graphics_deferred_and_compute(self) -> None:
        args = validation.parse_args([
            "--cooker", "cooker", "--tests", "tests",
            "--profile", "profile", "--work", "work", "--output", "out",
        ])
        self.assertEqual(7, len(args.asset_roots))
        self.assertIn(
            "ShaderProgram:Engine/Shaders/Validation/NoOp", args.asset_roots
        )

    def test_repetition_bounds_are_strict(self) -> None:
        base = [
            "--cooker", "cooker", "--tests", "tests",
            "--profile", "profile", "--work", "work", "--output", "out",
        ]
        with self.assertRaises(SystemExit):
            validation.parse_args(base + ["--repetitions", "1"])
        with self.assertRaises(SystemExit):
            validation.parse_args(base + ["--repetitions", "21"])

    def test_native_evidence_requires_exact_pipeline_stage_counts(self) -> None:
        output = (
            "[EVIDENCE] metal-native-device identity=registry-1 "
            "name-utf8-hex=4d34 capability=" + "1" * 64 + "\n" +
            "[EVIDENCE] metal-production-cooked graphics=2 compute=1 "
            "libraries=12 " +
            " ".join(
                f"digest={format(value, 'x') * 64}" for value in range(12)
            )
        )
        parsed = validation.parse_native_evidence(output)
        self.assertEqual("M4", parsed["device"]["name"])
        self.assertEqual(12, len(parsed["libraryDigests"]))
        with self.assertRaises(ValueError):
            validation.parse_native_evidence(output.replace("compute=1", "compute=0"))

    def test_work_path_rejects_non_disposable_roots(self) -> None:
        root = pathlib.Path("/repo")
        source = root / "Content"
        for unsafe in (pathlib.Path("/"), root, source):
            with self.assertRaises(ValueError):
                validation.validate_work_path(unsafe, root, source)
        validation.validate_work_path(root / "Build/Feature027", root, source)


if __name__ == "__main__":
    unittest.main()
