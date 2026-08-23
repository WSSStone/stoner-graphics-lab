#!/usr/bin/env python3

import importlib.util
from pathlib import Path
import sys
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("verify_production_evidence.py")


class ProductionEvidencePrivacyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.assertTrue(SCRIPT.is_file(), "production evidence verifier is not implemented")
        spec = importlib.util.spec_from_file_location("verify_production_evidence", SCRIPT)
        cls.module = importlib.util.module_from_spec(spec)
        sys.modules[spec.name] = cls.module
        spec.loader.exec_module(cls.module)

    def test_rejects_absolute_paths_usernames_credentials_and_environment_secrets(self):
        cases = (
            "/Users/alice/project/capture.png",
            "C:\\Users\\alice\\capture.png",
            "user=alice",
            "https://alice:secret@example.test/evidence",
            "Authorization: Bearer secret-token",
            "AWS_SECRET_ACCESS_KEY=secret",
            "HOME=/Users/alice",
        )
        for text in cases:
            with self.subTest(text=text):
                with self.assertRaisesRegex(ValueError, "private"):
                    self.module.validate_text(text)

    def test_rejects_pid_native_pointer_and_marketing_device_identity(self):
        for text in (
            "pid=48291", "process-id: 100", "native=0x7ffee12a4100",
            "deviceName=Apple M4 Pro", "adapter=GeForce RTX 5090",
        ):
            with self.subTest(text=text):
                with self.assertRaisesRegex(ValueError, "private"):
                    self.module.validate_text(text)

    def test_bounded_relative_diagnostic_text_is_accepted(self):
        self.module.validate_text(
            "stage=native result=Passed deviceClass=macos.apple8.metal.rgba8"
        )

    def test_rejects_unbounded_logs(self):
        with self.assertRaisesRegex(ValueError, "bounded"):
            self.module.validate_text("x" * (1024 * 1024 + 1))

    def test_capture_requires_window_scope_current_frame_and_fresh_file(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            capture = root / "capture.ppm"
            capture.write_bytes(b"P6\n1 1\n255\n\x00\x00\x00")
            digest = self.module.sha256_file(capture)
            metadata = {
                "captureScope": "application-window",
                "width": 1, "height": 1,
                "workloadRevision": "production-content-v1",
                "backend": "metal", "frameToken": 20,
                "expectedFrameToken": 20,
                "sha256": digest,
                "captureStartedNs": 0,
            }
            self.module.validate_window_capture(capture, metadata)
            for field, value, error in (
                ("captureScope", "desktop", "window"),
                ("frameToken", 19, "stale"),
                ("sha256", "0" * 64, "digest"),
                ("captureStartedNs", capture.stat().st_mtime_ns + 1, "stale"),
            ):
                changed = dict(metadata)
                changed[field] = value
                with self.assertRaisesRegex(ValueError, error):
                    self.module.validate_window_capture(capture, changed)

    def test_evidence_tree_scans_text_and_validates_declared_window_captures(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            capture = root / "capture.ppm"
            capture.write_bytes(b"P6\n1 1\n255\n\x01\x02\x03")
            metadata = {
                "captureScope": "application-window",
                "width": 1, "height": 1,
                "workloadRevision": "production-content-v1",
                "backend": "vulkan", "frameToken": 20,
                "expectedFrameToken": 20,
                "sha256": self.module.sha256_file(capture),
                "captureStartedNs": 0,
            }
            (root / "capture.json").write_text(
                __import__("json").dumps(metadata), encoding="utf-8"
            )
            (root / "result.txt").write_text(
                "stage=image result=Passed\n", encoding="utf-8"
            )
            result = self.module.validate_evidence_tree(root)
            self.assertEqual(1, result["windowCaptureCount"])
            (root / "private.log").write_text(
                "HOME=/Users/alice", encoding="utf-8"
            )
            with self.assertRaisesRegex(ValueError, "private"):
                self.module.validate_evidence_tree(root)


if __name__ == "__main__":
    unittest.main()
