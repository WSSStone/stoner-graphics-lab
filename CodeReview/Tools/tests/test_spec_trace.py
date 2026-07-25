from pathlib import Path
from tempfile import TemporaryDirectory
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import spec_trace


class SpecTraceTests(unittest.TestCase):
    def test_extracts_feature_requirements_and_success_criteria(self):
        with TemporaryDirectory() as temporary:
            repo = Path(temporary)
            spec = repo / "specs" / "003-example" / "spec.md"
            spec.parent.mkdir(parents=True)
            spec.write_text(
                "- **FR-001**: Required behavior.\n"
                "- **FR-001a**: Clarified required behavior.\n"
                "- **SC-001**: Measurable success.\n"
                "- not a requirement\n",
                encoding="utf-8",
            )

            records = spec_trace.extract(repo)

            self.assertEqual(
                [item["trace_id"] for item in records],
                ["003-FR-001", "003-FR-001a", "003-SC-001"],
            )
            self.assertTrue(all(item["classification"] == "Unclassified" for item in records))


if __name__ == "__main__":
    unittest.main()
