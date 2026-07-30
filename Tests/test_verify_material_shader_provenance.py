from __future__ import annotations

import tempfile
import unittest
import importlib.util
from pathlib import Path

_VERIFIER_PATH = Path(__file__).with_name("verify_material_shader_provenance.py")
_SPEC = importlib.util.spec_from_file_location(
    "verify_material_shader_provenance", _VERIFIER_PATH
)
if _SPEC is None or _SPEC.loader is None:
    raise RuntimeError("unable to load material/shader provenance verifier")
_MODULE = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(_MODULE)
EXPECTED = _MODULE.EXPECTED
EXPECTED_VERSION = _MODULE.EXPECTED_VERSION
verify = _MODULE.verify


class ProvenanceVerifierTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        self.vendor = self.root / "ThirdParty" / "yyjson"
        self.vendor.mkdir(parents=True)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def write_valid(self) -> None:
        source = Path(__file__).resolve().parents[1] / "ThirdParty" / "yyjson"
        for name in EXPECTED:
            (self.vendor / name).write_bytes((source / name).read_bytes())
        (self.vendor / "VERSION").write_text(EXPECTED_VERSION + "\n", encoding="ascii")
        (self.vendor / "UPSTREAM.md").write_text(
            "8b4a38dc994a110abaec8a400615567bd996105f\n", encoding="ascii"
        )

    def test_valid_provenance(self) -> None:
        self.write_valid()
        self.assertEqual([], verify(self.root))

    def test_missing_files(self) -> None:
        self.assertTrue(verify(self.root))

    def test_altered_source(self) -> None:
        self.write_valid()
        (self.vendor / "yyjson.c").write_bytes(b"altered")
        self.assertIn("yyjson checksum mismatch: yyjson.c", verify(self.root))

    def test_malformed_version_and_commit(self) -> None:
        self.write_valid()
        (self.vendor / "VERSION").write_text("system\n", encoding="ascii")
        (self.vendor / "UPSTREAM.md").write_text("unknown\n", encoding="ascii")
        errors = verify(self.root)
        self.assertIn("yyjson version mismatch", errors)
        self.assertIn("yyjson upstream commit mismatch", errors)


if __name__ == "__main__":
    unittest.main()
