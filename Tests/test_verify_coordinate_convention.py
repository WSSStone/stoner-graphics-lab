from __future__ import annotations

import importlib.util
import shutil
import tempfile
import unittest
from pathlib import Path


_PATH = Path(__file__).with_name("verify_coordinate_convention.py")
_SPEC = importlib.util.spec_from_file_location("verify_coordinate_convention", _PATH)
if _SPEC is None or _SPEC.loader is None:
    raise RuntimeError("unable to load coordinate convention verifier")
_MODULE = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(_MODULE)


class CoordinateConventionVerifierTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        source_root = Path(__file__).resolve().parents[1]
        for relative in (*_MODULE.ACTIVE_FILES, *_MODULE.HISTORICAL_FILES):
            source = source_root / relative
            destination = self.root / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, destination)

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_current_repository_layout_passes(self) -> None:
        self.assertEqual([], _MODULE.verify(self.root))

    def test_missing_historical_amendment_is_reported(self) -> None:
        path = self.root / _MODULE.HISTORICAL_FILES[0]
        path.write_text("historical document", encoding="utf-8")
        self.assertIn(
            f"{_MODULE.HISTORICAL_FILES[0]}: missing Feature 024 historical amendment",
            _MODULE.verify(self.root),
        )

    def test_stale_active_convention_is_reported(self) -> None:
        path = self.root / "Source/Renderer/Private/FForwardViewData.cpp"
        path.write_text(
            path.read_text(encoding="utf-8") + "\n// right-handed\n",
            encoding="utf-8",
        )
        self.assertIn(
            "Source/Renderer/Private/FForwardViewData.cpp: stale active-convention text: right-handed",
            _MODULE.verify(self.root),
        )


if __name__ == "__main__":
    unittest.main()
