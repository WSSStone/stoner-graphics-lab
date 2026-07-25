from pathlib import Path
import sys
import unittest

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import crctl
from reviewlib import ReviewError


class FindingStateTests(unittest.TestCase):
    def test_accepted_finding_reaches_verified_in_order(self):
        finding = {"status": "Open", "history": []}
        crctl.transition(finding, "Triaged", "reviewed")
        crctl.transition(finding, "Accepted", "confirmed")
        crctl.transition(finding, "Fixed", "patched")
        crctl.transition(finding, "Verified", "tested")
        self.assertEqual(finding["status"], "Verified")
        self.assertEqual(len(finding["history"]), 4)

    def test_cannot_verify_an_open_finding(self):
        with self.assertRaises(ReviewError):
            crctl.transition({"status": "Open"}, "Verified", "skipped protocol")


if __name__ == "__main__":
    unittest.main()
