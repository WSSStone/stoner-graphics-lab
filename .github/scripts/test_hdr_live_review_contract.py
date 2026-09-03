#!/usr/bin/env python3

from __future__ import annotations

import importlib.util
import os
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[2]
RUNNER_PATH = ROOT / ".github/scripts/run_output_transform_validation.py"
VERIFY_PATH = ROOT / ".github/scripts/verify_output_transform_evidence.py"


def load(name: str, path: Path):
    spec = importlib.util.spec_from_file_location(name, path)
    assert spec is not None and spec.loader is not None
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


RUNNER = load("run_output_transform_validation_hdr", RUNNER_PATH)
VERIFY = load("verify_output_transform_evidence_hdr", VERIFY_PATH)


def preflights() -> list[dict]:
    result = []
    for index, profile in enumerate(RUNNER.HDR_PROFILES):
        result.append({
            "profileId": profile,
            "executionResult": "passed",
            "commandCompleted": True,
            "readbackCompleted": True,
            "presented": True,
            "modeGeneration": index + 1,
            "firstFrameToken": f"f-{index}-1",
            "lastFrameToken": f"f-{index}-3",
            "settledFrameToken": f"f-{index}-3",
            "readbackDigest": f"{index + 1}" * 64,
            "hdrMetadataDigest": None,
            "displayAdaptation": "system-color-management" if index < 2 else "none",
        })
    return result


def request() -> dict:
    return RUNNER.build_hdr_review_request(
        request_id="hdr-review-1", git_revision="a" * 40,
        workload_revision="output-transform-hdr-v1",
        device_class="macos.apple8.metal.hdr",
        display_class="apple-xdr-edr",
        display_capability_digest="b" * 64,
        native_report_digest="c" * 64,
        review_session_id="session-1", profile_preflights=preflights())


class HdrLiveReviewBoundaryTests(unittest.TestCase):
    def test_request_stops_at_ready_for_live_review(self) -> None:
        value = request()
        self.assertEqual("ready-for-live-review", value["state"])
        self.assertEqual([], VERIFY.validate_hdr_request(value))
        serialized = RUNNER.canonical_bytes(value).decode("utf-8").lower()
        for forbidden in ("decision", "accepted", "visualscore",
                          "perceptualscore", "screenshot"):
            self.assertNotIn(f'"{forbidden}"', serialized)

    def test_no_cli_or_environment_can_author_attestation(self) -> None:
        parser = RUNNER.build_parser()
        help_text = parser.format_help().lower()
        for forbidden in RUNNER.FORBIDDEN_AUTOMATION_ARGUMENTS:
            self.assertNotIn(forbidden, help_text)
        source = RUNNER_PATH.read_text(encoding="utf-8")
        self.assertNotIn("write_attestation", source)
        before = RUNNER.canonical_bytes(request())
        os.environ["STONER_HDR_VISUAL_PASS"] = "1"
        try:
            after = RUNNER.canonical_bytes(request())
        finally:
            os.environ.pop("STONER_HDR_VISUAL_PASS", None)
        self.assertEqual(before, after)

    def test_pq_and_edr_platform_governance_is_frozen(self) -> None:
        profiles = request()["profiles"]
        self.assertEqual(["system-color-management", "system-color-management",
                          "none", "none"],
                         [profile["displayAdaptation"] for profile in profiles])
        self.assertTrue(all(profile["presentationReady"] for profile in profiles))
        invalid = request()
        invalid["profiles"][0]["score"] = 1.0
        self.assertTrue(any("forbidden" in item for item in
                            VERIFY.validate_hdr_request(invalid)))

    def test_incomplete_native_preflight_cannot_become_ready(self) -> None:
        values = preflights()
        values[2]["readbackCompleted"] = False
        with self.assertRaisesRegex(ValueError, "incomplete"):
            RUNNER.build_hdr_review_request(
                request_id="hdr-review-1", git_revision="a" * 40,
                workload_revision="output-transform-hdr-v1",
                device_class="macos.apple8.metal.hdr",
                display_class="apple-xdr-edr",
                display_capability_digest="b" * 64,
                native_report_digest="c" * 64,
                review_session_id="session-1", profile_preflights=values)


if __name__ == "__main__":
    unittest.main()
