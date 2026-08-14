from __future__ import annotations

import copy
import json
import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import verify_asset_cooker_contracts as contracts


class AssetCookerContractVerifierTests(unittest.TestCase):
    def test_envelope_rejects_truncation_and_substitution(self) -> None:
        envelope = contracts.build_envelope()
        parsed = contracts.parse_envelope(envelope)
        self.assertEqual(parsed["codec"], "stoner.image")
        with self.assertRaises(ValueError):
            contracts.parse_envelope(envelope[:-1])
        substituted = bytearray(envelope)
        substituted[-1] ^= 1
        with self.assertRaises(ValueError):
            contracts.parse_envelope(bytes(substituted))

    def test_manifest_generation_and_canonical_bytes_are_verified(self) -> None:
        root = pathlib.Path(__file__).resolve().parents[1]
        profile = json.loads((
            root / "Tests/Fixtures/AssetCooker/Contracts/Profiles/mac-vulkan.json"
        ).read_bytes())
        manifest = contracts.build_manifest(profile)
        parsed = contracts.parse_manifest(manifest)
        self.assertEqual(len(parsed["records"]), 2)
        with self.assertRaises(ValueError):
            contracts.parse_manifest(b" " + manifest)
        changed = copy.deepcopy(parsed)
        changed["records"][0]["payloadBytes"] += 1
        changed_bytes = contracts.canonical_manifest_text(changed).encode("utf-8")
        with self.assertRaises(ValueError):
            contracts.parse_manifest(changed_bytes)

    def test_schema_requires_declared_properties(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            path = pathlib.Path(temporary) / "schema.json"
            path.write_text(
                json.dumps({"type": "object", "required": ["missing"], "properties": {}}),
                encoding="utf-8",
            )
            self.assertTrue(contracts.validate_schema(path))

    def test_fixture_manifest_detects_checksum_mismatch(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            contract_root = root / "specs/025-asset-cooker-derived-data/contracts"
            contract_root.mkdir(parents=True)
            schema = {"type": "object", "required": [], "properties": {}}
            for name in (
                "target-profile.schema.json", "manifest.schema.json",
                "report.schema.json",
            ):
                (contract_root / name).write_text(json.dumps(schema), encoding="utf-8")
            validation = root / "Validation/025"
            validation.mkdir(parents=True)
            fixture = root / "fixture.bin"
            fixture.write_bytes(b"fixture")
            (validation / "fixture-manifest.json").write_text(json.dumps({
                "fixtures": [{"path": "fixture.bin", "sha256": "0" * 64}],
            }), encoding="utf-8")
            errors = contracts.verify(root)
            self.assertTrue(any("checksum mismatch" in error for error in errors))

    def test_repository_contracts_detect_layout_drift(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            profile_root = root / "Config/AssetCooker/Profiles"
            profile_root.mkdir(parents=True)
            fixture = json.loads((
                pathlib.Path(__file__).resolve().parents[1]
                / "Config/AssetCooker/Profiles/Mac-Vulkan.json"
            ).read_bytes())
            for name, platform in contracts.EXPECTED_PROFILES.items():
                profile = copy.deepcopy(fixture)
                profile["platform"] = platform
                (profile_root / name).write_text(
                    contracts.canonical_profile(profile, True), encoding="utf-8"
                )
            (root / ".gitignore").write_text("Saved/Cooked/\n", encoding="utf-8")
            tool_private = root / "Tools/AssetCooker/Private"
            tool_private.mkdir(parents=True)
            (tool_private / "Registered.cpp").write_text("", encoding="utf-8")
            (tool_private / "Missing.cpp").write_text("", encoding="utf-8")
            (tool_private.parent / "SConscript").write_text(
                "'Private/Registered.cpp'\n", encoding="utf-8"
            )
            public = root / "Source/Asset/Public/Asset"
            public.mkdir(parents=True)
            (public / "AssetMinimal.h").write_text("", encoding="utf-8")
            for name in contracts.RUNTIME_COOKED_HEADERS:
                (public / name).write_text("#pragma once\n", encoding="utf-8")

            errors = contracts.verify_repository_contracts(root)
            self.assertTrue(any("missing generated-output" in error for error in errors))
            self.assertTrue(any("unregistered AssetCooker source" in error for error in errors))
            self.assertTrue(any("does not expose cooked contract" in error for error in errors))

    def test_repository_contracts_accept_current_tree(self) -> None:
        root = pathlib.Path(__file__).resolve().parents[1]
        self.assertEqual([], contracts.verify_repository_contracts(root))


if __name__ == "__main__":
    unittest.main()
