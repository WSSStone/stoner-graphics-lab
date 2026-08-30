#!/usr/bin/env python3

import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("run_production_image_calibration.py")
SPEC = importlib.util.spec_from_file_location("production_image_calibration", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(MODULE)


class CrossProcessCalibrationTests(unittest.TestCase):
    def process(self, ordinal, digest):
        return {"ordinal": ordinal, "decodedPixelSha256": digest}

    def test_requires_each_mode_in_two_independent_processes(self):
        self.assertTrue(MODULE.calibration_complete([
            self.process(1, "a"), self.process(2, "a"),
            self.process(3, "a"),
        ]))
        self.assertFalse(MODULE.calibration_complete([
            self.process(1, "a"), self.process(2, "a"),
            self.process(3, "b"),
        ]))
        self.assertTrue(MODULE.calibration_complete([
            self.process(1, "a"), self.process(2, "a"),
            self.process(3, "b"), self.process(4, "b"),
        ]))

    def test_rejects_more_than_three_modes(self):
        with self.assertRaisesRegex(ValueError, "three modes"):
            MODULE.group_modes([
                self.process(1, "a"), self.process(2, "b"),
                self.process(3, "c"), self.process(4, "d"),
            ])

    def test_png_round_trip_is_decoded_pixel_exact(self):
        rgb = bytes((index % 251 for index in range(512 * 512 * 3)))
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "candidate.png"
            digest = MODULE.write_png(path, rgb)
            self.assertEqual(rgb, MODULE.decode_written_png(path))
            self.assertEqual(64, len(digest))

    def test_calibration_output_parses_exact_pairwise_flip_metrics(self):
        noise = {
            "mean": 0.001, "p95": 0.002,
            "maximum": 0.03, "badPixelFraction": 0.0001,
        }
        output = "[CALIBRATION] " + json.dumps({
            "backend": "vulkan", "captures": 20,
            "width": 512, "height": 512, "noise": noise,
            "policy": {
                "meanMax": 0.01, "p95Max": 0.02,
                "maximumMax": 0.1, "badPixelThreshold": 0.05,
                "badPixelFractionMax": 0.01,
            },
        }) + "\n"
        parsed = MODULE.parse_calibration_output(output)
        self.assertEqual(noise, parsed["noise"])
        self.assertEqual(0.01, parsed["policy"]["meanMax"])

    def test_calibration_evidence_digest_is_canonical(self):
        first = {"processOrdinals": [1, 2], "noise": {"mean": 0.0}}
        second = {"noise": {"mean": 0.0}, "processOrdinals": [1, 2]}
        self.assertEqual(
            MODULE.calibration_evidence_digest(first),
            MODULE.calibration_evidence_digest(second),
        )
        self.assertEqual(64, len(MODULE.calibration_evidence_digest(first)))

    def test_mutation_output_requires_every_rejection_once(self):
        output = "\n".join(
            f"[MUTATION] name={name} result=rejected mean=0"
            for name in MODULE.GPU_MUTATIONS
        )
        self.assertEqual(
            list(MODULE.GPU_MUTATIONS),
            MODULE.parse_mutation_output(output),
        )
        with self.assertRaisesRegex(ValueError, "accepted"):
            MODULE.parse_mutation_output(
                output.replace("result=rejected", "result=accepted", 1)
            )
        with self.assertRaisesRegex(ValueError, "incomplete"):
            MODULE.parse_mutation_output("\n".join(output.splitlines()[:-1]))


if __name__ == "__main__":
    unittest.main()
