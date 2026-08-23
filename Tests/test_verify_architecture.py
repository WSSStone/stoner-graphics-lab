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
            "Demo/StonerDemo/Private",
            "Source/Core/Public/Core",
            "Source/Asset/Public/Asset",
            "Source/Asset/Private",
            "Source/RHI/Public/RHI",
            "Source/Renderer/Public/Renderer",
            "Source/Application/Public/Application",
            "Source/Backend/Vulkan/Public/Vulkan",
            "Source/Backend/Metal/Public/MetalRHI",
            "Source/Backend/Metal/Private",
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

    def test_objective_cpp_runtime_source_is_scanned(self) -> None:
        (self.root / "Source/Backend/Metal/Private/Leak.mm").write_text(
            '#include "Tools/AssetCooker/Public.h"\n', encoding="utf-8"
        )
        errors = verify_architecture.verify(self.root)
        self.assertTrue(any("must not include Tools" in error for error in errors))

    def test_objective_cpp_outside_metal_private_fails(self) -> None:
        path = self.root / "Source/Renderer/Private/Leak.mm"
        path.parent.mkdir(parents=True)
        path.write_text("void Leak() {}\n", encoding="utf-8")
        errors = verify_architecture.verify(self.root)
        self.assertTrue(any("Objective-C++" in error for error in errors))

    def test_private_metal_ownership_and_spirv_cross_runtime_fail(self) -> None:
        path = self.root / "Source/Application/Private/Leak.cpp"
        path.parent.mkdir(parents=True)
        path.write_text(
            "MTLDevice* Device;\n#include \"spirv_cross.hpp\"\n",
            encoding="utf-8",
        )
        errors = verify_architecture.verify(self.root)
        self.assertTrue(any("native Metal ownership" in error for error in errors))
        self.assertTrue(any("SPIRV-Cross" in error for error in errors))

    def test_renderer_and_application_cannot_call_native_backends(self) -> None:
        renderer = self.root / "Source/Renderer/Public/Renderer/Leak.h"
        renderer.write_text(
            '#include "VulkanRHI/FVulkanDevice.h"\n', encoding="utf-8"
        )
        application = self.root / "Source/Application/Private/Leak.cpp"
        application.parent.mkdir(parents=True, exist_ok=True)
        application.write_text(
            "void Leak() { Stoner::Backend::Metal::CreateDevice(); }\n",
            encoding="utf-8",
        )
        errors = verify_architecture.verify(self.root)
        self.assertTrue(any("Renderer must not call native backend" in error for error in errors))
        self.assertTrue(any("Application must not call native backend" in error for error in errors))

    def test_asset_namespace_leak_fails_without_an_include(self) -> None:
        path = self.root / "Source/Asset/Private/Leak.cpp"
        path.write_text(
            "Stoner::Renderer::FMaterial* LeakedMaterial;\n",
            encoding="utf-8",
        )
        errors = verify_architecture.verify(self.root)
        self.assertTrue(any("Asset must not reference" in error for error in errors))

    def test_flip_runtime_include_and_link_fail(self) -> None:
        source = self.root / "Source/Renderer/Public/Renderer/Leak.h"
        source.write_text('#include "ThirdParty/flip/FLIP.h"\n', encoding="utf-8")
        sconscript = self.root / "Demo/StonerDemo/SConscript"
        sconscript.parent.mkdir(parents=True, exist_ok=True)
        sconscript.write_text("CPPPATH=['#ThirdParty/flip']\n", encoding="utf-8")
        errors = verify_architecture.verify(self.root)
        self.assertTrue(any("FLIP is validation-only" in error for error in errors))

    def test_demo_composition_root_line_budget_fails(self) -> None:
        path = self.root / "Demo/StonerDemo/Private/FStonerDemoApplication.cpp"
        path.write_text("void Line();\n" * 1601, encoding="utf-8")
        errors = verify_architecture.verify(self.root)
        self.assertTrue(any("Demo composition root exceeds" in error for error in errors))

    def test_feature_scope_exclusions_reject_added_production_paths(self) -> None:
        errors = verify_architecture.verify_feature_diff_paths([
            "Source/Asset/Private/FObjImporter.cpp",
            "Source/Renderer/Private/FMeshletBuilder.cpp",
            "Demo/StonerDemo/Private/FVisualRedesign.cpp",
            "Tests/MeshletFutureContractTests.cpp",
        ])
        self.assertEqual(3, len(errors))
        self.assertTrue(any("new source importer" in error for error in errors))
        self.assertTrue(any("Meshlet or LOD" in error for error in errors))
        self.assertTrue(any("visual-quality redesign" in error for error in errors))

    def test_feature_scope_allows_feature_028_production_helpers(self) -> None:
        self.assertEqual([], verify_architecture.verify_feature_diff_paths([
            "Demo/StonerDemo/Private/FProductionContentSession.cpp",
            "Source/Renderer/Private/FStaticModelRealization.cpp",
            "Tools/AssetCooker/Private/FAssetCookerSelection.cpp",
        ]))


if __name__ == "__main__":
    unittest.main()
