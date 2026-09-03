#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import json
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest
from unittest import mock
import zlib


ROOT = Path(__file__).resolve().parents[2]
SCRIPT = ROOT / ".github/scripts/run_output_transform_validation.py"
SPEC = importlib.util.spec_from_file_location("run_output_transform_validation", SCRIPT)
assert SPEC is not None and SPEC.loader is not None
RUNNER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(RUNNER)
COMPARE = RUNNER.COMPARE


def write_generic_rgb_png(path: Path, width: int, height: int, rgb: bytes) -> None:
    rows = b"".join(b"\0" + rgb[y * width * 3:(y + 1) * width * 3]
                    for y in range(height))
    payload = (COMPARE.PNG_SIGNATURE +
               COMPARE._chunk(b"IHDR", struct.pack(">IIBBBBB", width, height,
                                                     8, 2, 0, 0, 0)) +
               COMPARE._chunk(b"IDAT", zlib.compress(rows)) +
               COMPARE._chunk(b"IEND", b""))
    path.write_bytes(payload)


class OutputTransformRunnerTests(unittest.TestCase):
    def test_native_capture_cannot_relabel_existing_probe(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            probe = root / "probe.json"
            probe.write_text("{}", encoding="utf-8")
            with mock.patch.object(RUNNER.PROVENANCE, "require_frozen_revision"):
                with self.assertRaisesRegex(ValueError, "fresh"):
                    RUNNER.capture_native_output(root / "command.json", probe, "a" * 40,
                                                 "native-sdr", "deferred", root)

    def test_native_capture_runs_command_between_revision_guards(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "probe.json"
            command = root / "command.json"
            command.write_text('["StonerDemo", "--output-native-probe", "probe.json"]', encoding="utf-8")
            events = []

            def execute(*args, **kwargs):
                events.append("capture")
                path.write_bytes(RUNNER.canonical_bytes(self._native_probe()))

            with mock.patch.object(RUNNER.PROVENANCE, "require_frozen_revision",
                                   side_effect=lambda *_: events.append("guard")), \
                    mock.patch.object(RUNNER.PROVENANCE.subprocess, "run", side_effect=execute):
                report = RUNNER.capture_native_output(command, path, "a" * 40,
                                                     "native-sdr", "deferred", root)
            self.assertEqual(["guard", "capture", "guard"], events)
            self.assertEqual("a" * 40, report["gitRevision"])
            self.assertEqual(1, len(report["artifacts"]))
            self.assertEqual([], RUNNER.VERIFY.validate_output_report(report, root))

    def test_frozen_revision_rejects_mismatch_and_dirty_software(self) -> None:
        guard = RUNNER.PROVENANCE.require_frozen_revision
        with self.assertRaisesRegex(ValueError, "exact"):
            guard(ROOT, "HEAD")
        with mock.patch.object(RUNNER.PROVENANCE.subprocess, "run",
                               return_value=mock.Mock(stdout="b" * 40)):
            with self.assertRaisesRegex(ValueError, "HEAD"):
                guard(ROOT, "a" * 40)
        with mock.patch.object(RUNNER.PROVENANCE.subprocess, "run",
                               side_effect=[mock.Mock(stdout="a" * 40),
                                            mock.Mock(stdout=" M Source/Renderer.cpp")]):
            with self.assertRaisesRegex(ValueError, "dirty"):
                guard(ROOT, "a" * 40)

    def _native_probe(self, profile: str = "native-sdr") -> dict[str, object]:
        hdr = profile == "native-hdr-nonvisual"
        return {
            "schema": "stoner.output-native-probe", "schemaVersion": 2,
            "backend": "metal", "capabilityDigest": "1" * 64,
            "commandCompleted": True,
            "deviceClass": "macos.apple8.metal.output-v1",
            "displayAdaptation": "system-color-management" if hdr else "none",
            "exposureStops": 0, "firstFailure": None,
            "format": "bgr10a2-unorm" if hdr else "bgra8-unorm",
            "firstFrameToken": "1", "frameToken": "17",
            "lastFrameToken": "17", "settledFrameToken": "17",
            "hdrMetadataDigest": None,
            "height": 512, "hostPlatform": "macos",
            "insertionDigest": "2" * 64, "modeGeneration": 4,
            "outputDeviceProfileId": ("Hdr.PQ.Rec2020.1000.v1" if hdr
                                      else "Sdr.sRGB.v1"),
            "outstandingTerminalOwnerCount": 0, "presented": True,
            "presentationFrameToken": "17", "profileKind": profile,
            "readbackCompleted": True, "readbackDigest": "3" * 64,
            "readbackFrameToken": "17", "status": "passed",
            "transformVersion": ("Hdr.ACES2.0.0_2025-04-04.Rec2020D65.v1"
                                 if hdr else "Sdr.KhronosPbrNeutral.v1"),
            "width": 512,
            "workloadRevision": "production-content-lantern-v3",
        }

    def test_native_modes_require_same_frame_and_zero_terminal_owners(self) -> None:
        for profile in ("native-sdr", "native-hdr-nonvisual", "lifecycle",
                        "failure-injection"):
            probe = self._native_probe(profile)
            with tempfile.TemporaryDirectory() as directory:
                path = Path(directory) / "probe.json"
                path.write_text(json.dumps(probe), encoding="utf-8")
                loaded = RUNNER.load_native_probe(path)
                report = RUNNER.build_native_output_report(
                    loaded, "a" * 40, "deferred")
                self.assertEqual("passed", report["executionResult"])
                self.assertEqual(report["frameToken"],
                                 report["native"]["readbackFrameToken"])
                self.assertEqual(report["frameToken"],
                                 report["native"]["presentationFrameToken"])
        leaked = self._native_probe()
        leaked["outstandingTerminalOwnerCount"] = 1
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "leaked.json"
            path.write_text(json.dumps(leaked), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "terminal completion"):
                RUNNER.load_native_probe(path)

    def test_metal_hdr_probe_is_nonvisual_and_metadata_nil(self) -> None:
        probe = self._native_probe("native-hdr-nonvisual")
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "pq.json"
            path.write_text(json.dumps(probe), encoding="utf-8")
            report = RUNNER.build_native_output_report(
                RUNNER.load_native_probe(path), "b" * 40, "deferred")
            self.assertEqual("manual-review-required",
                             report["visualAuthority"]["disposition"])
            self.assertIsNone(report["native"]["hdrMetadataDigest"])
            self.assertEqual("system-color-management",
                             report["native"]["displayAdaptation"])
        probe["hdrMetadataDigest"] = "4" * 64
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "forbidden.json"
            path.write_text(json.dumps(probe), encoding="utf-8")
            with self.assertRaisesRegex(ValueError, "EDRMetadata=nil"):
                RUNNER.load_native_probe(path)

    def test_hdr_request_consumes_four_real_native_probe_shapes(self) -> None:
        probes = []
        for profile_id in RUNNER.HDR_PROFILES:
            probe = self._native_probe("native-hdr-nonvisual")
            probe["outputDeviceProfileId"] = profile_id
            probe["format"] = ("bgr10a2-unorm" if profile_id.startswith(
                "Hdr.PQ.") else "rgba16-float")
            probe["displayAdaptation"] = ("system-color-management" if
                profile_id.startswith("Hdr.PQ.") else "none")
            probes.append(probe)
        request = RUNNER.build_hdr_review_request(
            request_id="request-1", git_revision="a" * 40,
            workload_revision="production-content-lantern-v3",
            device_class="macos.apple8.metal.hdr",
            display_class="apple-xdr-edr",
            display_capability_digest="b" * 64,
            native_report_digest="c" * 64,
            review_session_id="session-1", profile_preflights=probes)
        self.assertEqual("ready-for-live-review", request["state"])
        self.assertEqual(list(RUNNER.HDR_PROFILES),
                         [item["profileId"] for item in request["profiles"]])

    def test_frozen_workloads_are_exact_v3(self) -> None:
        for name in ("Lantern-v3.json", "Sponza-v3.json"):
            workload = RUNNER.load_workload(
                ROOT / "Config/Validation/OutputTransform/Workloads" / name)
            self.assertEqual((512, 512, 1),
                             (workload["width"], workload["height"],
                              workload["sampleCount"]))
            self.assertEqual(RUNNER.SETTINGS_DIGEST, workload["settingsDigest"])

    def test_candidate_is_lossless_exact_and_never_accepted(self) -> None:
        workload = RUNNER.load_workload(
            ROOT / "Config/Validation/OutputTransform/Workloads/Lantern-v3.json")
        rgb = bytes((index * 17) & 0xff for index in range(512 * 512 * 3))
        with tempfile.TemporaryDirectory() as directory:
            record, path = RUNNER.generate_sdr_candidate(
                workload, "metal", "macos.apple8.metal.sdr-v3", "1" * 64,
                "2" * 64, rgb, Path(directory))
            width, height, decoded = COMPARE.decode_png(path)
            self.assertEqual((512, 512, rgb), (width, height, decoded))
            self.assertEqual("candidate", record["state"])
            self.assertIsNone(record["acceptance"])
            self.assertEqual(COMPARE.sha256_bytes(rgb), record["decodedSha256"])

    def test_exact_native_p6_capture_is_accepted_without_conversion(self) -> None:
        rgb = bytes((index * 29) & 0xff for index in range(512 * 512 * 3))
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            exact = root / "capture.ppm"
            exact.write_bytes(b"P6\n512 512\n255\n" + rgb)
            self.assertEqual(rgb, RUNNER.load_exact_p6_ppm(exact))
            for invalid in (
                    b"P6\n511 512\n255\n" + bytes(511 * 512 * 3),
                    b"P6\n# comment\n512 512\n255\n" + rgb,
                    b"P6\n512 512\n255\n" + rgb + b"\0"):
                exact.write_bytes(invalid)
                with self.assertRaisesRegex(ValueError, "exact 512x512"):
                    RUNNER.load_exact_p6_ppm(exact)

    def test_dimension_mismatch_is_rejected_before_flip(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            candidate, reference = root / "candidate.png", root / "reference.png"
            write_generic_rgb_png(candidate, 512, 512, bytes(512 * 512 * 3))
            write_generic_rgb_png(reference, 511, 512, bytes(511 * 512 * 3))
            marker = root / "flip-ran"
            with self.assertRaisesRegex(ValueError, "before FLIP"):
                COMPARE.compare_exact(
                    candidate, reference,
                    [sys.executable, "-c", f"open({str(marker)!r},'w').close()"])
            self.assertFalse(marker.exists())

    def test_one_pixel_mutation_is_observable(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            baseline = bytearray(512 * 512 * 3)
            mutated = bytearray(baseline)
            mutated[(241 * 512 + 317) * 3 + 1] = 1
            COMPARE.write_lossless_rgb_png(root / "a.png", 512, 512, baseline)
            COMPARE.write_lossless_rgb_png(root / "b.png", 512, 512, mutated)
            result = COMPARE.compare_exact(root / "a.png", root / "b.png")
            self.assertFalse(result["pixelExact"])
            self.assertEqual(1, result["differingChannelCount"])

    def test_png_cannot_expand_beyond_declared_dimensions(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / "overflow.png"
            path.write_bytes(COMPARE.PNG_SIGNATURE +
                COMPARE._chunk(b"IHDR", struct.pack(">IIBBBBB", 512, 512, 8, 2, 0, 0, 0)) +
                COMPARE._chunk(b"IDAT", zlib.compress(bytes(2 * 1024 * 1024))) +
                COMPARE._chunk(b"IEND", b""))
            with self.assertRaisesRegex(ValueError, "decoded byte count"):
                COMPARE.decode_png(path)

    def test_spatial_normalization_flags_do_not_exist(self) -> None:
        completed = subprocess.run(
            [sys.executable, str(SCRIPT), "candidate", "--align", "true"],
            capture_output=True, text=True, check=False)
        self.assertNotEqual(0, completed.returncode)
        self.assertIn("spatial-normalization", completed.stdout)
        parser_text = SCRIPT.read_text(encoding="utf-8")
        for operation in ("--align", "--crop", "--scale", "--warp",
                          "--resample", "--resize"):
            self.assertNotIn(f'add_argument("{operation}"', parser_text)


if __name__ == "__main__":
    unittest.main()
