from __future__ import annotations

import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import verify_architecture


class ArchitectureVerifierTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.temporary.name)
        for relative in (
            "Source/Core/Public/Core",
            "Source/Asset/Public/Asset",
            "Source/Asset/Private",
            "Source/RHI/Public/RHI",
            "Source/Renderer/Public/Renderer",
            "Source/Application/Public/Application",
            "Source/Backend/Vulkan/Public/Vulkan",
            "Tools/AssetCooker/Public/AssetCooker",
            "Tools/AssetCooker/Private",
        ):
            (self.root / relative).mkdir(parents=True)
        (self.root / "Source/Asset/Private/FPrivateCodec.h").write_text(
            "#pragma once\n", encoding="utf-8"
        )

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_clean_layout_passes(self) -> None:
        self.assertEqual([], verify_architecture.verify(self.root))

    def test_asset_graphics_and_runtime_tool_dependencies_fail(self) -> None:
        (self.root / "Source/Asset/Private/Leak.cpp").write_text(
            '#include "RHI/IRHIDevice.h"\n', encoding="utf-8"
        )
        (self.root / "Source/Renderer/Public/Renderer/Leak.h").write_text(
            '#include "Tools/AssetCooker/Public.h"\n', encoding="utf-8"
        )
        errors = verify_architecture.verify(self.root)
        self.assertTrue(any("Asset must not include" in error for error in errors))
        self.assertTrue(any("must not include Tools" in error for error in errors))

    def test_tool_asset_private_include_fails_by_name_and_path(self) -> None:
        (self.root / "Tools/AssetCooker/Private/Leak.cpp").write_text(
            '#include "FPrivateCodec.h"\n', encoding="utf-8"
        )
        errors = verify_architecture.verify(self.root)
        self.assertTrue(any("Asset public APIs" in error for error in errors))

    def test_native_and_private_json_public_types_fail(self) -> None:
        (self.root / "Source/Asset/Public/Asset/Leak.h").write_text(
            "VkImage Image;\nyyjson_doc* Document;\n", encoding="utf-8"
        )
        errors = verify_architecture.verify(self.root)
        self.assertTrue(any("native or private parser" in error for error in errors))

    def test_runtime_build_cannot_link_offline_tool(self) -> None:
        renderer = self.root / "Source/Renderer/SConscript"
        renderer.write_text("LIBS=['AssetCooker']\n", encoding="utf-8")
        asset = self.root / "Source/Asset/SConscript"
        asset.write_text("BuildLayer(env, 'Asset', ['Core', 'RHI'])\n", encoding="utf-8")
        errors = verify_architecture.verify(self.root)
        self.assertTrue(any("must not link AssetCooker" in error for error in errors))
        self.assertTrue(any("depend on Core only" in error for error in errors))


if __name__ == "__main__":
    unittest.main()
