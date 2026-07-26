from pathlib import Path
import sys
import unittest
from unittest.mock import patch

REPO = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(REPO / "site_scons"))

import GraphicsDependencyDetect as graphics


class GraphicsDependencyDetectTests(unittest.TestCase):
    def test_disabled_mode_skips_installed_runtime_dependencies(self):
        with patch.dict(graphics.ARGUMENTS, {"graphics": "disabled"}, clear=True):
            with patch.object(graphics, "_sdk_roots") as sdk_roots:
                result = graphics.DetectGraphicsDependencies("Mac")

        sdk_roots.assert_not_called()
        self.assertFalse(result["vulkan_available"])
        self.assertFalse(result["glfw_available"])
        self.assertIsNone(result["vulkan_include_dir"])
        self.assertIsNone(result["vulkan_library"])
        self.assertIsNone(result["glfw_include_dir"])
        self.assertIsNone(result["glfw_library"])


if __name__ == "__main__":
    unittest.main()
