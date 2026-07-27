from pathlib import Path
from tempfile import TemporaryDirectory
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import architecture_scan


class ArchitectureScanTests(unittest.TestCase):
    def test_reports_upward_layer_include(self):
        with TemporaryDirectory() as temporary:
            repo = Path(temporary)
            source = repo / "Source" / "Core" / "Private" / "Bad.cpp"
            source.parent.mkdir(parents=True)
            source.write_text('#include "Renderer/FMaterial.h"\n', encoding="utf-8")

            violations = architecture_scan.scan(repo)

            self.assertEqual(len(violations), 1)
            self.assertEqual(violations[0]["owner"], "Core")
            self.assertEqual(violations[0]["dependency"], "Renderer")

    def test_allows_declared_dependency(self):
        with TemporaryDirectory() as temporary:
            repo = Path(temporary)
            source = repo / "Source" / "Renderer" / "Private" / "Good.cpp"
            source.parent.mkdir(parents=True)
            source.write_text('#include "RHI/IRHIDevice.h"\n', encoding="utf-8")

            self.assertEqual(architecture_scan.scan(repo), [])


if __name__ == "__main__":
    unittest.main()
