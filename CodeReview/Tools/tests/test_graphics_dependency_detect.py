from pathlib import Path
import sys
import tempfile
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

    def test_linux_runtime_loader_without_development_link_is_unavailable(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            (root / "include" / "vulkan").mkdir(parents=True)
            (root / "include" / "vulkan" / "vulkan.h").touch()
            (root / "lib").mkdir()
            (root / "lib" / "libvulkan.so.1").touch()

            with patch.dict(graphics.ARGUMENTS, {"graphics": "auto"}, clear=True):
                with patch.object(graphics, "_sdk_roots", return_value=[directory]):
                    with patch.object(graphics.glob, "glob", return_value=[]):
                        result = graphics.DetectGraphicsDependencies("Linux")

        self.assertFalse(result["vulkan_available"])
        self.assertIsNone(result["vulkan_library"])


if __name__ == "__main__":
    unittest.main()
