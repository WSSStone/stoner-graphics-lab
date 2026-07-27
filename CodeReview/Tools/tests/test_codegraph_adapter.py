import json
from pathlib import Path
from tempfile import TemporaryDirectory
import sys
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import codegraph_adapter


class CodeGraphAdapterTests(unittest.TestCase):
    @patch("codegraph_adapter.run")
    @patch("codegraph_adapter.status")
    def test_first_rebuild_initializes_index(self, mock_status, mock_run):
        mock_status.side_effect = [
            {"available": True, "raw": {"initialized": False}},
            {"available": True, "raw": {"initialized": True}},
        ]
        mock_run.return_value.returncode = 0

        result = codegraph_adapter.rebuild(Path("/repo"))

        self.assertTrue(result["raw"]["initialized"])
        self.assertEqual(mock_run.call_args.args[0][:2], ["codegraph", "init"])

    @patch("codegraph_adapter.indexed_files")
    def test_cpp_coverage_reports_missing_demo_file(self, mock_indexed):
        with TemporaryDirectory() as temporary:
            repo = Path(temporary)
            source = repo / "Source" / "Core" / "A.cpp"
            demo = repo / "Demo" / "App" / "Main.cpp"
            source.parent.mkdir(parents=True)
            demo.parent.mkdir(parents=True)
            source.write_text("", encoding="utf-8")
            demo.write_text("", encoding="utf-8")
            mock_indexed.return_value = [
                {"path": "Source/Core/A.cpp", "language": "cpp"}
            ]

            coverage = codegraph_adapter.cpp_coverage(repo)

            self.assertEqual(coverage["expected_cpp_files"], 2)
            self.assertEqual(coverage["indexed_cpp_files"], 1)
            self.assertEqual(coverage["missing"], ["Demo/App/Main.cpp"])


if __name__ == "__main__":
    unittest.main()
