from __future__ import annotations

import pathlib
import sys
import tempfile
import unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))

import verify_runtime_asset_manager as verifier


class RuntimeAssetManagerVerifierTests(unittest.TestCase):
    def test_missing_contracts_are_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            (root / "Source/Asset/Public/Asset").mkdir(parents=True)
            (root / "Source/Asset/Public/Asset/AssetMinimal.h").write_text(
                "#pragma once\n", encoding="utf-8"
            )
            feature = root / "specs/026-runtime-asset-manager"
            feature.mkdir(parents=True)
            (feature / "spec.md").write_text("", encoding="utf-8")
            (feature / "tasks.md").write_text("", encoding="utf-8")
            errors = verifier.verify(root)
            self.assertTrue(any("missing runtime public contract" in e for e in errors))
            self.assertTrue(any("missing runtime implementation" in e for e in errors))

    def test_forbidden_public_dependency_is_reported(self) -> None:
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            public = root / "Source/Asset/Public/Asset"
            private = root / "Source/Asset/Private"
            public.mkdir(parents=True)
            private.mkdir(parents=True)
            includes = []
            for name in verifier.PUBLIC_HEADERS:
                text = "#pragma once\n"
                if name == verifier.PUBLIC_HEADERS[0]:
                    text += '#include "Renderer/FMaterial.h"\n'
                (public / name).write_text(text, encoding="utf-8")
                includes.append(f'#include "Asset/{name}"')
            (public / "AssetMinimal.h").write_text(
                "\n".join(includes) + "\n", encoding="utf-8"
            )
            for name in verifier.PRIVATE_SOURCES:
                (private / name).write_text("", encoding="utf-8")
            feature = root / "specs/026-runtime-asset-manager"
            feature.mkdir(parents=True)
            requirements = "\n".join(
                f"- **FR-{index:03d}**: requirement"
                for index in range(1, 47)
            ) + "\n" + "\n".join(
                f"- **SC-{index:03d}**: criterion"
                for index in range(1, 14)
            )
            tasks = "\n".join(
                f"- [ ] T{index:03d} task" for index in range(1, 72)
            )
            (feature / "spec.md").write_text(requirements, encoding="utf-8")
            (feature / "tasks.md").write_text(tasks, encoding="utf-8")
            errors = verifier.verify(root)
            self.assertTrue(any("Renderer/" in error for error in errors))

    def test_current_repository_satisfies_runtime_contract(self) -> None:
        root = pathlib.Path(__file__).resolve().parents[1]
        self.assertEqual([], verifier.verify(root))

    def test_requirement_gap_is_reported_even_when_count_is_unchanged(self) -> None:
        text = "\n".join(
            [f"- **FR-{index:03d}**: requirement" for index in range(1, 47)]
            + [f"- **SC-{index:03d}**: criterion" for index in range(1, 14)]
        )
        text = text.replace("FR-023", "FR-022")
        found = {
            f"{kind}-{number}"
            for kind, number in verifier.REQUIREMENT_RE.findall(text)
        }
        self.assertNotEqual(verifier.EXPECTED_REQUIREMENTS, found)
        self.assertIn("FR-023", verifier.EXPECTED_REQUIREMENTS - found)


if __name__ == "__main__":
    unittest.main()
