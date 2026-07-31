from __future__ import annotations

import importlib.util
import json
import shutil
import tempfile
import unittest
from pathlib import Path


_PATH = Path(__file__).with_name("verify_static_model_provenance.py")
_SPEC = importlib.util.spec_from_file_location("verify_static_model_provenance", _PATH)
if _SPEC is None or _SPEC.loader is None:
    raise RuntimeError("unable to load static model provenance verifier")
_MODULE = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(_MODULE)


class StaticModelProvenanceTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        source_root = Path(__file__).resolve().parents[1]
        for vendor in _MODULE.VENDORS:
            destination = self.root / "ThirdParty" / vendor
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copytree(source_root / "ThirdParty" / vendor, destination)
        manifest = self.root / "Validation" / "024" / "fixture-manifest.json"
        manifest.parent.mkdir(parents=True, exist_ok=True)
        manifest.write_text(
            json.dumps({"schema": _MODULE.FIXTURE_SCHEMA, "fixtures": []}),
            encoding="utf-8",
        )

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_valid_repository_layout(self) -> None:
        self.assertEqual([], _MODULE.verify(self.root))

    def test_detects_vendor_checksum_change(self) -> None:
        (self.root / "ThirdParty" / "cgltf" / "cgltf.h").write_text("altered", encoding="ascii")
        self.assertIn(
            "cgltf: checksum mismatch: cgltf.h",
            _MODULE.verify(self.root),
        )

    def test_ignores_scons_private_object_output(self) -> None:
        (self.root / "ThirdParty" / "cgltf" / "cgltf.o").write_bytes(b"object")
        self.assertEqual([], _MODULE.verify(self.root))

    def test_detects_fixture_schema_error(self) -> None:
        manifest = self.root / "Validation" / "024" / "fixture-manifest.json"
        manifest.write_text(
            json.dumps({"schema": "wrong", "fixtures": [{}]}), encoding="utf-8"
        )
        errors = _MODULE.verify(self.root)
        self.assertIn("fixture manifest schema mismatch", errors)

    def test_detects_fixture_entry_errors(self) -> None:
        manifest = self.root / "Validation" / "024" / "fixture-manifest.json"
        manifest.write_text(
            json.dumps({"schema": _MODULE.FIXTURE_SCHEMA, "fixtures": [{}]}),
            encoding="utf-8",
        )
        errors = _MODULE.verify(self.root)
        self.assertTrue(any(error.startswith("fixture 0: missing fields:") for error in errors))


if __name__ == "__main__":
    unittest.main()
