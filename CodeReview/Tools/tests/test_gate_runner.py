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

    @patch("gate_runner.platform.system", return_value="Linux")
    def test_sanitizer_profile_is_strict_and_allow_listed(self, _system):
        commands = gate_runner.commands_for("sanitizers")
        self.assertEqual(
            commands[0],
            [
                "scons",
                "config=debug",
                "strict=1",
                "sanitizers=address,undefined",
            ],
        )
        self.assertEqual(commands[1][-1], "Build/Linux/Debug/Tests/StonerTest")
        self.assertIn("STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1", commands[1])

    @patch("gate_runner.platform.system", return_value="Windows")
    def test_sanitizer_profile_rejects_msvc(self, _system):
        with self.assertRaises(ReviewError):
            gate_runner.commands_for("sanitizers")

    @patch("gate_runner.platform.system", return_value="Darwin")
    def test_macos_sanitizer_profile_does_not_enable_unsupported_lsan(self, _system):
        commands = gate_runner.commands_for("sanitizers")
        self.assertEqual(commands[1][1], "ASAN_OPTIONS=halt_on_error=1")


if __name__ == "__main__":
    unittest.main()
