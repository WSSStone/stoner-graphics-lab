from __future__ import annotations

import pathlib
import json
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / "site_scons"))

import verify_metal_backend as verifier
from LayerBuilder import _FindCppAndObjectiveCppFiles


class MetalBackendVerifierTests(unittest.TestCase):
    def _write_feature(self, root: pathlib.Path) -> None:
        feature = root / "specs/027-metal-backend"
        contracts = feature / "contracts"
        contracts.mkdir(parents=True)
        requirements = "\n".join(
            [f"- **FR-{index:03d}**: requirement" for index in range(1, 46)]
            + [f"- **SC-{index:03d}**: criterion" for index in range(1, 11)]
        )
        tasks = "\n".join(
            f"- [ ] T{index:03d} task" for index in range(1, 129)
        )
        (feature / "spec.md").write_text(requirements + "\n", encoding="utf-8")
        (feature / "tasks.md").write_text(tasks + "\n", encoding="utf-8")

    def test_matching_overload_inventory_passes(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            self._write_feature(root)
            public = root / "Source/RHI/Public/RHI"
            public.mkdir(parents=True)
            (public / "IRHISample.h").write_text(
                "class IRHISample { public:\n"
                "virtual ~IRHISample() = default;\n"
                "virtual int Get() const = 0;\n"
                "virtual int Set(int) = 0;\n"
                "virtual int Set(float) { return 0; }\n};\n",
                encoding="utf-8",
            )
            contract = root / "specs/027-metal-backend/contracts/rhi-operation-matrix.md"
            contract.write_text(
                "| Interface | Frozen public operations |\n|---|---|\n"
                "| `IRHISample` | `Get`, both `Set` overloads |\n",
                encoding="utf-8",
            )
            self.assertEqual([], verifier.verify_rhi_matrix(root))

    def test_overload_drift_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            self._write_feature(root)
            public = root / "Source/RHI/Public/RHI"
            public.mkdir(parents=True)
            (public / "IRHISample.h").write_text(
                "class IRHISample { public: virtual int Get() = 0; };\n",
                encoding="utf-8",
            )
            contract = root / "specs/027-metal-backend/contracts/rhi-operation-matrix.md"
            contract.write_text(
                "| Interface | Frozen public operations |\n|---|---|\n"
                "| `IRHISample` | `Get`, `Set` |\n",
                encoding="utf-8",
            )
            errors = verifier.verify_rhi_matrix(root)
            self.assertTrue(any("operation mismatch" in error for error in errors))

    def test_public_apple_api_leak_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            public = root / "Source/Backend/Metal/Public/MetalRHI"
            public.mkdir(parents=True)
            (public / "Leak.h").write_text(
                "#include <Metal/Metal.h>\n", encoding="utf-8"
            )
            errors = verifier.verify_architecture(root)
            self.assertTrue(any("Apple API token" in error for error in errors))

    def test_objective_cpp_outside_backend_private_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            renderer = root / "Source/Renderer/Private"
            renderer.mkdir(parents=True)
            (renderer / "Leak.mm").write_text("", encoding="utf-8")
            errors = verifier.verify_architecture(root)
            self.assertTrue(any("Objective-C++" in error for error in errors))

    def test_private_apple_api_outside_metal_backend_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            renderer = root / "Source/Renderer/Private"
            renderer.mkdir(parents=True)
            (renderer / "Leak.cpp").write_text(
                "void f(CAMetalLayer* layer);\n", encoding="utf-8"
            )
            errors = verifier.verify_architecture(root)
            self.assertTrue(any("Apple API token" in error for error in errors))

    def test_spirv_cross_runtime_use_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            asset = root / "Source/Asset/Private"
            asset.mkdir(parents=True)
            (asset / "Leak.cpp").write_text(
                '#include "spirv_cross.hpp"\n', encoding="utf-8"
            )
            errors = verifier.verify_architecture(root)
            self.assertTrue(any("SPIRV-Cross" in error for error in errors))

    def test_current_rhi_matches_frozen_contract(self) -> None:
        root = pathlib.Path(__file__).resolve().parents[1]
        self.assertEqual([], verifier.verify_rhi_matrix(root))

    def test_requirement_trace_covers_every_fr_and_sc(self) -> None:
        root = pathlib.Path(__file__).resolve().parents[1]
        self.assertEqual([], verifier.verify_requirement_trace(root))

    def test_native_pass_requires_device_shader_and_gpu_proof(self) -> None:
        document = {
            "tier": "native-offscreen",
            "backend": "metal",
            "result": "passed",
            "probes": [],
        }
        errors = verifier.validate_validation_evidence(document, "native.json")
        self.assertTrue(any("device proof" in error for error in errors))
        self.assertTrue(any("shader payload proof" in error for error in errors))
        self.assertTrue(any("GPU evidence digest" in error for error in errors))

    def test_semantic_oracle_cannot_be_native_evidence(self) -> None:
        document = {
            "tier": "native-offscreen",
            "backend": "metal",
            "result": "passed",
            "device": {
                "identity": "gpu", "name": "GPU", "capabilityDigest": "1" * 64,
            },
            "shaderEvidenceDigests": ["2" * 64],
            "probes": [{
                "name": "semantic-oracle", "result": "passed",
                "evidenceDigest": "3" * 64,
            }],
        }
        errors = verifier.validate_validation_evidence(document, "native.json")
        self.assertTrue(any("semantic oracle" in error for error in errors))

    def test_visible_pass_requires_frozen_duration(self) -> None:
        document = {
            "tier": "visible-manual",
            "backend": "metal",
            "result": "passed",
            "device": {
                "identity": "gpu", "name": "GPU", "capabilityDigest": "1" * 64,
            },
            "shaderEvidenceDigests": ["2" * 64],
            "counts": {"frames": 2999, "lifecycleCycles": 20},
            "probes": [{
                "name": "visible", "result": "passed",
                "evidenceDigest": "3" * 64,
            }],
        }
        errors = verifier.validate_validation_evidence(document, "visible.json")
        self.assertTrue(any("3000 frames" in error for error in errors))

    def test_forbidden_ios_api_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            private = root / "Source/Backend/Metal/Private"
            private.mkdir(parents=True)
            (private / "Leak.mm").write_text(
                "void Use(UIApplication* App);\n", encoding="utf-8"
            )
            errors = verifier.verify_forbidden_scope(root)
            self.assertTrue(any("iOS application lifecycle" in error for error in errors))

    def test_layer_builder_discovers_cpp_and_objective_cpp_only(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            for name in ("A.cpp", "B.mm", "C.c", "D.h"):
                (root / name).write_text("", encoding="ascii")
            self.assertEqual(
                ["A.cpp", "B.mm"],
                _FindCppAndObjectiveCppFiles(str(root)),
            )

    def test_missing_metal_build_settings_are_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            sconscript = root / "Source/Backend/Metal/SConscript"
            sconscript.parent.mkdir(parents=True)
            sconscript.write_text("lib = None\n", encoding="utf-8")
            errors = verifier.verify_metal_build_contract(root)
            self.assertTrue(any("Objective-C ARC" in error for error in errors))
            self.assertTrue(any("deployment" in error for error in errors))
            self.assertTrue(any("Metal framework" in error for error in errors))
            self.assertTrue(any("unsupported Metal factory" in error for error in errors))

    def test_current_spirv_cross_vendor_is_complete_and_hashed(self) -> None:
        root = pathlib.Path(__file__).resolve().parents[1]
        self.assertEqual([], verifier.verify_spirv_cross_vendor(root))

    def test_spirv_cross_vendor_tamper_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            source = pathlib.Path(__file__).resolve().parents[1]
            vendor = root / "ThirdParty/spirv-cross"
            vendor.mkdir(parents=True)
            for name in {
                "LICENSE", "UPSTREAM.md", "SHA256SUMS",
                *verifier.SPIRV_CROSS_SOURCES,
                *verifier.SPIRV_CROSS_HEADERS,
            }:
                (vendor / name).write_bytes(
                    (source / "ThirdParty/spirv-cross" / name).read_bytes()
                )
            (vendor / "spirv_msl.cpp").write_text("tampered\n", encoding="utf-8")
            tool = root / "Tools/AssetCooker"
            tool.mkdir(parents=True)
            (tool / "SConscript").write_text(
                "\n".join(
                    f"ThirdParty/spirv-cross/{name}"
                    for name in verifier.SPIRV_CROSS_SOURCES
                ),
                encoding="utf-8",
            )
            asset = root / "Source/Asset"
            asset.mkdir(parents=True)
            (asset / "SConscript").write_text("", encoding="utf-8")
            errors = verifier.verify_spirv_cross_vendor(root)
            self.assertTrue(any("digest mismatch" in error for error in errors))

    def test_shader_evidence_schema_oracle_accepts_derivation(self) -> None:
        root = pathlib.Path(__file__).resolve().parents[1]
        schema_path = (
            root
            / "specs/027-metal-backend/contracts/metal-shader-evidence.schema.json"
        )
        schema = json.loads(schema_path.read_text(encoding="utf-8"))
        document = {field: "placeholder" for field in schema["required"]}
        document.update(
            {
                "schemaVersion": 1,
                "kind": "derivation",
                "shaderAssetVersion": "0" * 64,
                "spirvDigest": "1" * 64,
                "interfaceDigest": "2" * 64,
                "normalizedMslDigest": "3" * 64,
                "evidenceDigest": "4" * 64,
            }
        )
        self.assertEqual(
            [], verifier.validate_metal_shader_evidence(schema_path, document)
        )
        document["nativeLibrary"] = {}
        self.assertTrue(
            verifier.validate_metal_shader_evidence(schema_path, document)
        )


if __name__ == "__main__":
    unittest.main()
