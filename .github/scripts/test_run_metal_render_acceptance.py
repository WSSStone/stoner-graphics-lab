from __future__ import annotations

import importlib.util
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock


SCRIPT = Path(__file__).with_name("run_metal_render_acceptance.py")
SPEC = importlib.util.spec_from_file_location("run_metal_render_acceptance", SCRIPT)
assert SPEC and SPEC.loader
acceptance = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(acceptance)


class MetalRenderAcceptanceTests(unittest.TestCase):
    def test_metal_only_mode_is_explicit(self) -> None:
        args = acceptance.parse_args([
            "--tests", "tests", "--demo", "demo", "--profile", "profile",
            "--publication", "publication", "--lease", "lease",
            "--output-dir", "output", "--work", "work", "--metal-only",
        ])
        self.assertTrue(args.metal_only)

    def test_metal_only_mode_never_runs_vulkan(self) -> None:
        digest = "a" * 64
        capability = "b" * 64
        shader = "c" * 64
        production = " ".join(f"digest={index:064x}" for index in range(12))
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            for name in ("tests", "demo", "profile"):
                (root / name).write_bytes(b"fixture")
            (root / "publication").mkdir()
            (root / "publication/Current.json").write_text(
                "{}", encoding="utf-8"
            )
            commands: list[list[str]] = []

            def fake_run(command, _root, _timeout, _environment=None):
                values = list(command)
                commands.append(values)
                if "--backend" in values:
                    output = (
                        "[EVIDENCE] metal-native-triangle status=passed "
                        f"readback={digest}\n"
                    )
                elif "deferred-native" in values:
                    output = (
                        f"[EVIDENCE] metal-native-shader evidence={shader}\n"
                        "[EVIDENCE] metal-native-deferred status=passed "
                        f"readback={digest}\n"
                    )
                else:
                    output = (
                        "[EVIDENCE] metal-native-device identity=device "
                        f"name-utf8-hex=4d6574616c capability={capability}\n"
                        "[EVIDENCE] metal-production-cooked graphics=2 compute=1 "
                        f"libraries=12 {production}\n"
                    )
                return subprocess.CompletedProcess(values, 0, output, "")

            with mock.patch.object(acceptance, "run", side_effect=fake_run):
                result = acceptance.main([
                    "--root", str(root), "--tests", "tests", "--demo", "demo",
                    "--profile", "profile", "--publication", "publication",
                    "--lease", "lease", "--output-dir", "output",
                    "--work", "work", "--metal-only",
                ])

            self.assertEqual(0, result)
            flattened = " ".join(" ".join(command) for command in commands)
            self.assertNotIn("--backend vulkan", flattened)
            self.assertNotIn("metal-backend-comparison", flattened)
            self.assertTrue((root / "output/us4-metal-triangle.json").is_file())
            self.assertTrue((root / "output/us4-metal-deferred.json").is_file())

    def test_triangle_patterns_require_native_readback_digest(self) -> None:
        digest = "a" * 64
        self.assertEqual(
            digest,
            acceptance.TRIANGLE["metal"].search(
                f"[EVIDENCE] metal-native-triangle status=passed readback={digest}"
            ).group(1),
        )
        self.assertIsNone(acceptance.TRIANGLE["vulkan"].search(
            "[PASS] triangle"
        ))

    def test_comparison_pattern_requires_frozen_tolerance(self) -> None:
        digest = "b" * 64
        match = acceptance.COMPARISON.search(
            "[EVIDENCE] metal-vulkan-deferred-comparison status=passed "
            f"tolerance=metal-vulkan-tolerance-v1 digest={digest}"
        )
        self.assertIsNotNone(match)
        self.assertEqual("metal-vulkan-tolerance-v1", match.group(1))

    def test_shader_digests_are_sorted_and_unique(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            triangle = root / "Content/Shaders/Triangle"
            deferred = root / "Content/Shaders/Deferred"
            triangle.mkdir(parents=True)
            deferred.mkdir(parents=True)
            (triangle / "a.spv").write_bytes(b"same")
            (deferred / "b.spv").write_bytes(b"same")
            (deferred / "c.spv").write_bytes(b"different")
            values = acceptance.shader_digests(root)
            self.assertEqual(2, len(values))
            self.assertEqual(sorted(values), values)


if __name__ == "__main__":
    unittest.main()
