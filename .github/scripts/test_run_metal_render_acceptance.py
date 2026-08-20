from __future__ import annotations

import importlib.util
from pathlib import Path
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("run_metal_render_acceptance.py")
SPEC = importlib.util.spec_from_file_location("run_metal_render_acceptance", SCRIPT)
assert SPEC and SPEC.loader
acceptance = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(acceptance)


class MetalRenderAcceptanceTests(unittest.TestCase):
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
