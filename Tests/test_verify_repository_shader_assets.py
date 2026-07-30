import json
import shutil
import sys
import tempfile
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import verify_repository_shader_assets as verifier


ROOT = Path(__file__).resolve().parents[1]


class RepositoryShaderVerifierTests(unittest.TestCase):
    def make_repo(self) -> tuple[tempfile.TemporaryDirectory, Path]:
        temporary = tempfile.TemporaryDirectory()
        root = Path(temporary.name)
        shutil.copytree(ROOT / "Content", root / "Content")
        shutil.copytree(ROOT / "Source/Backend", root / "Source/Backend")
        (root / "Demo/StonerDemo/Shaders").mkdir(parents=True)
        (root / "Source/Renderer/Shaders/Deferred").mkdir(parents=True)
        return temporary, root

    def test_valid_repository(self):
        temporary, root = self.make_repo()
        self.addCleanup(temporary.cleanup)
        self.assertEqual([], verifier.verify(root))

    def test_normalized_report_is_deterministic(self):
        temporary, root = self.make_repo()
        self.addCleanup(temporary.cleanup)
        report = root / "Validation/023/repository.txt"
        verifier.write_report(report, verifier.verify(root))
        expected = (
            "feature=023\n"
            "programs=6\n"
            "sources=11\n"
            "payloads=11\n"
            "result=pass\n"
        )
        self.assertEqual(expected, report.read_text(encoding="utf-8"))
        verifier.write_report(report, ["z-error", "a-error"])
        self.assertEqual(
            expected.replace(
                "result=pass\n",
                "result=fail\n"
                "error.000=z-error\n"
                "error.001=a-error\n",
            ),
            report.read_text(encoding="utf-8"),
        )

    def test_missing_payload_is_reported(self):
        temporary, root = self.make_repo()
        self.addCleanup(temporary.cleanup)
        (root / "Content/Shaders/Triangle/Triangle.vert.spv").unlink()
        self.assertTrue(
            any("dependency-missing" in error for error in verifier.verify(root))
        )

    def test_stale_digest_is_reported(self):
        temporary, root = self.make_repo()
        self.addCleanup(temporary.cleanup)
        path = root / "Content/Shaders/Triangle/Triangle.shader.json"
        value = json.loads(path.read_text())
        value["stages"][0]["source"]["digest"] = "sha256:" + "0" * 64
        path.write_text(json.dumps(value), encoding="utf-8")
        self.assertTrue(
            any("dependency-digest" in error for error in verifier.verify(root))
        )

    def test_extra_program_is_reported(self):
        temporary, root = self.make_repo()
        self.addCleanup(temporary.cleanup)
        shutil.copy(
            root / "Content/Shaders/Triangle/Triangle.shader.json",
            root / "Content/Shaders/Triangle/Extra.shader.json",
        )
        self.assertIn("program-inventory", verifier.verify(root))

    def mutate_triangle(self, callback):
        temporary, root = self.make_repo()
        self.addCleanup(temporary.cleanup)
        path = root / "Content/Shaders/Triangle/Triangle.shader.json"
        value = json.loads(path.read_text(encoding="utf-8"))
        callback(value)
        path.write_text(json.dumps(value), encoding="utf-8")
        return verifier.verify(root)

    def test_program_identity_is_reported(self):
        errors = self.mutate_triangle(
            lambda value: value["id"].update(path="Wrong/Program")
        )
        self.assertTrue(any(error.startswith("program-id:") for error in errors))

    def test_dependency_type_and_subresource_are_reported(self):
        def mutate(value):
            value["stages"][0]["source"]["asset"]["type"] = "Texture"
            value["stages"][1]["source"]["asset"]["subresource"] = "wrong"

        errors = self.mutate_triangle(mutate)
        self.assertTrue(any("dependency-type:" in error for error in errors))
        self.assertTrue(any("dependency-subresource:" in error for error in errors))

    def test_stage_entry_and_target_are_reported(self):
        def mutate(value):
            value["stages"][0]["entryPoint"] = "other"
            value["variants"][0]["payloads"][0]["profile"] = "portable"

        errors = self.mutate_triangle(mutate)
        self.assertTrue(
            any("dependency-stage-entry:" in error for error in errors)
        )
        self.assertTrue(any("dependency-target:" in error for error in errors))

    def test_duplicate_destination_with_different_identity_is_reported(self):
        def mutate(value):
            payloads = value["variants"][0]["payloads"]
            payloads[1]["locator"] = payloads[0]["locator"]
            payloads[1]["digest"] = payloads[0]["digest"]

        errors = self.mutate_triangle(mutate)
        self.assertTrue(
            any("dependency-destination-conflict:" in error for error in errors)
        )

    def test_unowned_dependency_and_backend_path_read_are_reported(self):
        temporary, root = self.make_repo()
        self.addCleanup(temporary.cleanup)
        (root / "Content/Shaders/Triangle/Unowned.vert").write_text(
            "void main() {}\n", encoding="utf-8"
        )
        backend = root / "Source/Backend/Vulkan/Private/Bad.cpp"
        backend.write_text('const char* path = "bad.spv";\n', encoding="utf-8")
        errors = verifier.verify(root)
        self.assertIn("dependency-inventory", errors)
        self.assertTrue(
            any(error.startswith("backend-direct-shader-path:") for error in errors)
        )

    def test_mixed_separator_and_deterministic_errors(self):
        errors = self.mutate_triangle(
            lambda value: value["stages"][0]["source"].update(
                locator="nested\\Triangle.vert"
            )
        )
        self.assertTrue(any("dependency-locator:" in error for error in errors))
        self.assertEqual(errors, sorted(set(errors)))


if __name__ == "__main__":
    unittest.main()
