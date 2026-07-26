import sys
import unittest
from pathlib import Path


REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "site_scons"))

import PlatformDetect


class FakeEnvironment:
    def __init__(self):
        self.flags = []

    def Detect(self, compiler):
        return compiler

    def Append(self, **values):
        self.flags.extend(values.get("CXXFLAGS", ()))


class PlatformDetectTests(unittest.TestCase):
    def test_windows_cxx20_toolchain_uses_standard_preprocessor(self):
        env = FakeEnvironment()

        PlatformDetect.ConfigureToolchain(env, "Win64")

        self.assertIn("/std:c++20", env.flags)
        self.assertIn("/Zc:preprocessor", env.flags)


if __name__ == "__main__":
    unittest.main()
