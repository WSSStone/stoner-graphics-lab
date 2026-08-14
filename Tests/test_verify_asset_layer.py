from __future__ import annotations

import importlib.util
import tempfile
import unittest
from pathlib import Path


_PATH = Path(__file__).with_name("verify_asset_layer.py")
_SPEC = importlib.util.spec_from_file_location("verify_asset_layer", _PATH)
if _SPEC is None or _SPEC.loader is None:
    raise RuntimeError("unable to load Asset architecture verifier")
_MODULE = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(_MODULE)


class AssetArchitectureVerifierTests(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory()
        self.root = Path(self.temporary.name)
        for relative in (
            "Source/Asset/Public/Asset", "Source/Asset/Private",
            "Source/Core/Public/Core", "Source/Core/Private",
            "Source/Backend/Vulkan/Private", "Source/Renderer/Public/Renderer",
            "Source/Renderer/Private", "Source/RHI/Public/RHI",
        ):
            (self.root / relative).mkdir(parents=True)
        (self.root / "Source/Asset/SConscript").write_text(
            "BuildLayer(env, 'Asset', ['Core'])\n"
            "ThirdParty/cgltf/cgltf.c\nThirdParty/mikktspace/mikktspace.c\n"
            "ThirdParty/yyjson/yyjson.c\n", encoding="utf-8")
        (self.root / "Source/Core/SConscript").write_text(
            "ThirdParty/utf8proc/utf8proc.c UTF8PROC_STATIC\n", encoding="utf-8")
        (self.root / "Source/Core/Private/FUnicode.cpp").write_text(
            "// utf8proc", encoding="utf-8")

    def tearDown(self) -> None:
        self.temporary.cleanup()

    def test_minimal_layout_passes(self) -> None:
        self.assertEqual([], _MODULE.verify(self.root))

    def test_detects_public_native_and_third_party_leaks(self) -> None:
        header = self.root / "Source/Asset/Public/Asset/Leak.h"
        header.write_text("#include <cgltf.h>\nVkImage Image;\n", encoding="utf-8")
        errors = _MODULE.verify(self.root)
        self.assertTrue(any("third-party dependency leaks" in error for error in errors))
        self.assertTrue(any("native graphics type leaks" in error for error in errors))

    def test_detects_renderer_source_format_access(self) -> None:
        source = self.root / "Source/Renderer/Private/Leak.cpp"
        source.write_text("#include <filesystem>\n", encoding="utf-8")
        self.assertTrue(any(
            "Renderer must not parse source formats" in error
            for error in _MODULE.verify(self.root)))

    def test_detects_backend_asset_dependency(self) -> None:
        source = self.root / "Source/Backend/Vulkan/Private/Leak.cpp"
        source.write_text('#include "Asset/FStaticMeshAsset.h"\n', encoding="utf-8")
        self.assertTrue(any(
            "Backend must not include Asset" in error
            for error in _MODULE.verify(self.root)))


if __name__ == "__main__":
    unittest.main()
