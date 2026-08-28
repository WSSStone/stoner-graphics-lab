from __future__ import annotations

import hashlib
import importlib.util
import json
import tempfile
import unittest
from pathlib import Path


_PATH = Path(__file__).with_name("verify_static_model_fixtures.py")
_SPEC = importlib.util.spec_from_file_location("verify_static_model_fixtures", _PATH)
if _SPEC is None or _SPEC.loader is None:
    raise RuntimeError("unable to load fixture verifier")
_MODULE = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(_MODULE)


class StaticModelFixtureVerifierTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.fixtures = self.root / "Tests/Fixtures/StaticModel"
        self.manifest = self.root / "Tests/Fixtures/StaticModel/fixture-manifest.json"
        self.fixtures.mkdir(parents=True)
        self.manifest.parent.mkdir(parents=True, exist_ok=True)
        entries = []
        for index in range(20):
            scope = ["valid", *(("SC-004",) if index < 12 else ())]
            entries.append(self._entry(f"Valid/{index:02}.gltf", "success", "valid", scope))
        for index in range(40):
            entries.append(self._entry(f"Invalid/Generated/{index:02}.gltf",
                                       "failure", "malformed", ["malformed"]))
        self._write(entries)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def _entry(self, path: str, expected: str, validator: str,
               scope: list[str]) -> dict[str, object]:
        target = self.fixtures / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_text(path, encoding="utf-8")
        digest = hashlib.sha256(target.read_bytes()).hexdigest()
        return {
            "expected_result": expected, "license": "CC0-1.0",
            "path": target.relative_to(self.root).as_posix(), "scope": scope,
            "sha256": f"sha256:{digest}", "source_url": "repository-owned://test",
            "upstream_revision": "test-v1", "validator_result": validator,
        }

    def _write(self, entries: list[dict[str, object]]) -> None:
        self.manifest.write_text(
            json.dumps({"schema": _MODULE.SCHEMA, "fixtures": entries}), encoding="utf-8")

    def _entries(self) -> list[dict[str, object]]:
        return json.loads(self.manifest.read_text(encoding="utf-8"))["fixtures"]

    def test_compliant_corpus_passes(self) -> None:
        self.assertEqual([], _MODULE.verify(self.manifest, self.fixtures))

    def test_rejects_hash_and_unlisted_fixture(self) -> None:
        entries = self._entries()
        entries[0]["sha256"] = "sha256:" + "0" * 64
        self._write(entries)
        (self.fixtures / "Valid/unlisted.glb").write_bytes(b"glb")
        errors = _MODULE.verify(self.manifest, self.fixtures)
        self.assertTrue(any("checksum mismatch" in error for error in errors))
        self.assertTrue(any("unlisted fixture" in error for error in errors))

    def test_enforces_counts_and_golden_scope(self) -> None:
        entries = self._entries()
        self._write(entries[:10] + entries[20:55])
        errors = _MODULE.verify(self.manifest, self.fixtures)
        self.assertIn("valid fixture count 10 is below 20", errors)
        self.assertIn("malformed fixture count 35 is below 40", errors)
        self.assertIn("SC-004 golden primitive count 10 is below 12", errors)

    def test_rejects_missing_metadata_and_result_mismatch(self) -> None:
        entries = self._entries()
        del entries[0]["license"]
        entries[1]["validator_result"] = "unsupported"
        self._write(entries)
        errors = _MODULE.verify(self.manifest, self.fixtures)
        self.assertTrue(any("missing fields: license" in error for error in errors))
        self.assertTrue(any("successful input must be validator-valid" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
