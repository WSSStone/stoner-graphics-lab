from pathlib import Path
import sys
import unittest
from unittest.mock import patch

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

import gate_runner
from reviewlib import ReviewError


class GateRunnerTests(unittest.TestCase):
    def test_rejects_arbitrary_profile(self):
        with self.assertRaises(ReviewError):
            gate_runner.commands_for("debug; rm -rf anything")

    @patch("gate_runner.platform.system", return_value="Darwin")
    def test_test_profile_uses_known_binary(self, _system):
        commands = gate_runner.commands_for("tests")
        self.assertEqual(commands[0], ["scons", "config=debug"])
        self.assertEqual(commands[1], ["Build/Mac/Debug/Tests/StonerTest"])


if __name__ == "__main__":
    unittest.main()
