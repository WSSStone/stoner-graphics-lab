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


class BatchPacketTests(unittest.TestCase):
    def test_core_batch_is_split_into_responsibility_triplets(self):
        core = next(batch for batch in crctl.make_batches() if batch["id"] == "B02")
        self.assertEqual(len(core["steps"]), 24)
        self.assertEqual(
            [step["title"] for step in core["steps"][:3]],
            [
                "Inspect: value identity and containers",
                "Fix: value identity and containers",
                "Verify: value identity and containers",
            ],
        )

    def test_refine_preserves_started_history_and_replaces_pristine_batches(self):
        batches = crctl.make_batches()
        completed = batches[1]
        completed["status"] = "Completed"
        for step in completed["steps"]:
            step["status"] = "Completed"
            step["started_at"] = "start"
            step["completed_at"] = "finish"
        old_core = {
            "id": "B02",
            "title": "Core Features 003-006",
            "status": "Pending",
            "steps": [
                {
                    "id": "B02-S01",
                    "title": "Inspect",
                    "status": "Pending",
                    "started_at": None,
                    "completed_at": None,
                    "commit": None,
                    "evidence": [],
                    "note": "",
                }
            ],
        }
        state = {"batches": [completed, old_core], "active_step": "B02-S01"}

        refined = crctl.refine_pending_batches(state)

        self.assertEqual(refined, ["B02"])
        self.assertIs(state["batches"][0], completed)
        self.assertEqual(len(state["batches"][1]["steps"]), 24)
        self.assertEqual(state["active_step"], "B02-S01")

    def test_refine_does_not_replace_in_progress_batch(self):
        core = next(batch for batch in crctl.make_batches() if batch["id"] == "B02")
        core["status"] = "InProgress"
        core["steps"][0]["status"] = "InProgress"
        core["steps"][0]["started_at"] = "start"
        state = {"batches": [core], "active_step": "B02-S01"}

        self.assertEqual(crctl.refine_pending_batches(state), [])
        self.assertEqual(state["batches"][0]["steps"][0]["status"], "InProgress")


if __name__ == "__main__":
    unittest.main()
