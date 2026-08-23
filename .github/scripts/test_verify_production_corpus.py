#!/usr/bin/env python3

import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock


SCRIPT_DIR = Path(__file__).parent
REPOSITORY_ROOT = SCRIPT_DIR.parents[1]
MANIFEST_MODULE = SCRIPT_DIR / "production_content_manifest.py"
VERIFIER_MODULE = SCRIPT_DIR / "verify_production_corpus.py"
SCHEMA = REPOSITORY_ROOT / "specs/028-production-content-acceptance/contracts/production-corpus.schema.json"
FAILURE_CASES = REPOSITORY_ROOT / "Tests/Fixtures/ProductionContent/Failures/corpus-cases.json"


def load_module(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def file_record(path, payload, role="model"):
    return {
        "path": path,
        "sha256": hashlib.sha256(payload).hexdigest(),
        "sizeBytes": len(payload),
        "role": role,
    }


def package(package_id, root, source_path, payload, tier):
    claim = f"{package_id}-indexed"
    return {
        "packageId": package_id,
        "workName": package_id.title(),
        "packageName": "glTF",
        "publisher": "Fixture Publisher",
        "sourceLocation": "https://github.com/example/assets",
        "revision": "1" * 40,
        "acquiredOn": "2026-08-22",
        "tier": tier,
        "packageRoot": root,
        "sourcePath": source_path,
        "rootAssetId": f"StaticModel:{package_id}.gltf#idx.scene.0",
        "rootFile": f"{package_id}.gltf",
        "files": [file_record(f"{package_id}.gltf", payload)],
        "coverageClaims": [claim],
    }


def manifest(first_payload=b"first", second_payload=b"second"):
    packages = [
        package("first", "Regular/First", "Models/First/glTF", first_payload, "regular"),
        package("second", "External/Second", "Models/Second/glTF", second_payload, "medium"),
    ]
    claims = [
        {
            "claimId": f"{item['packageId']}-indexed",
            "packageId": item["packageId"],
            "subject": item["rootAssetId"],
            "evidence": {"indexedTriangles": True},
        }
        for item in packages
    ]
    return {
        "schema": "stoner.production-corpus",
        "schemaVersion": 1,
        "corpusRevision": "test-v1",
        "packages": packages,
        "coverageClaims": claims,
    }


class ProductionCorpusVerifierContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.model = load_module("production_content_manifest", MANIFEST_MODULE)
        cls.verifier = load_module("verify_production_corpus", VERIFIER_MODULE)

    def write_fixture(self, root, document):
        manifest_path = root / "Corpus" / "corpus-v1.json"
        manifest_path.parent.mkdir(parents=True)
        manifest_path.write_text(json.dumps(document), encoding="utf-8")
        for item in document["packages"]:
            package_root = root / item["packageRoot"]
            package_root.mkdir(parents=True)
            for record in item["files"]:
                payload = b"first" if item["packageId"] == "first" else b"second"
                (package_root / record["path"]).write_bytes(payload)
        return manifest_path

    def test_valid_manifest_is_canonical_and_repeatable(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = self.write_fixture(root, manifest())
            outputs = [self.verifier.verify_manifest(path, root) for _ in range(20)]
        self.assertTrue(all(item["result"] == "Passed" for item in outputs))
        self.assertEqual(1, len({item["canonicalJson"] for item in outputs}))
        self.assertEqual(1, len({item["manifestDigest"] for item in outputs}))

    def test_unknown_field_and_noncanonical_order_fail_closed(self):
        document = manifest()
        document["unexpected"] = True
        with self.assertRaisesRegex(self.model.ManifestError, "unknown-field"):
            self.model.validate_manifest(document)

    def test_json_schema_tracks_the_runtime_contract(self):
        schema = json.loads(SCHEMA.read_text(encoding="utf-8"))
        package_schema = schema["$defs"]["package"]
        self.assertFalse(schema["additionalProperties"])
        self.assertFalse(package_schema["additionalProperties"])
        self.assertEqual(
            set(self.model.TOP_LEVEL_FIELDS), set(schema["required"])
        )
        self.assertEqual(
            set(self.model.PACKAGE_REQUIRED_FIELDS), set(package_schema["required"])
        )

        document = manifest()
        document["packages"].reverse()
        with self.assertRaisesRegex(self.model.ManifestError, "canonical-order"):
            self.model.validate_manifest(document)

    def test_packages_must_be_independent_and_coverage_must_close(self):
        document = manifest()
        document["packages"][1]["workName"] = document["packages"][0]["workName"]
        with self.assertRaisesRegex(self.model.ManifestError, "package-independence"):
            self.model.validate_manifest(document)

        document = manifest()
        document["packages"][0]["coverageClaims"].append("missing-claim")
        with self.assertRaisesRegex(self.model.ManifestError, "coverage-closure"):
            self.model.validate_manifest(document)

    def test_missing_extra_size_and_digest_fail_before_acceptance(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = self.write_fixture(root, manifest())
            (root / "Regular/First/first.gltf").unlink()
            result = self.verifier.verify_manifest(path, root)
            self.assertEqual("missing-file", result["firstFailure"]["category"])

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = self.write_fixture(root, manifest())
            (root / "Regular/First/extra.bin").write_bytes(b"extra")
            result = self.verifier.verify_manifest(path, root)
            self.assertEqual("extra-file", result["firstFailure"]["category"])

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = self.write_fixture(root, manifest())
            (root / "Regular/First/first.gltf").write_bytes(b"changed")
            result = self.verifier.verify_manifest(path, root)
            self.assertEqual("size-mismatch", result["firstFailure"]["category"])

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            document = manifest()
            document["packages"][0]["files"][0]["sha256"] = "0" * 64
            path = self.write_fixture(root, document)
            result = self.verifier.verify_manifest(path, root)
            self.assertEqual("digest-mismatch", result["firstFailure"]["category"])

    def test_symlink_escape_fails_before_file_content_is_accepted(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            document = manifest()
            path = self.write_fixture(root, document)
            package_file = root / "Regular/First/first.gltf"
            escaped_file = root / "escaped.gltf"
            escaped_file.write_bytes(b"first")
            package_file.unlink()
            try:
                package_file.symlink_to(escaped_file)
            except OSError as error:
                self.skipTest(f"symlink creation is unavailable: {error}")

            result = self.verifier.verify_manifest(path, root)

        self.assertEqual("path-escape", result["firstFailure"]["category"])

    def test_maintainer_note_is_never_opened_or_hashed(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = self.write_fixture(root, manifest())
            note = root / "MAINTAINER_NOTES.md"
            note.write_text("first note", encoding="utf-8")
            real_read_bytes = Path.read_bytes

            def guarded_read_bytes(candidate):
                if candidate.name == "MAINTAINER_NOTES.md":
                    raise AssertionError("out-of-band note was read")
                return real_read_bytes(candidate)

            with mock.patch.object(Path, "read_bytes", guarded_read_bytes):
                first = self.verifier.verify_manifest(path, root)
                note.write_text("changed note", encoding="utf-8")
                second = self.verifier.verify_manifest(path, root)

        self.assertEqual("Passed", first["result"])
        self.assertEqual(first["canonicalJson"], second["canonicalJson"])
        self.assertEqual(first["manifestDigest"], second["manifestDigest"])

    def test_failure_catalog_rejects_all_provenance_and_path_aliases(self):
        catalog = json.loads(FAILURE_CASES.read_text(encoding="utf-8"))
        self.assertGreaterEqual(len(catalog["cases"]), 10)
        observed = []
        for case in catalog["cases"]:
            document = manifest()
            package = document["packages"][0]
            mutation = case["mutation"]
            if mutation == "file-path":
                package["files"][0]["path"] = case["value"]
                package["rootFile"] = case["value"]
            elif mutation == "case-collision":
                package["files"] = [
                    file_record("A.gltf", b"first"),
                    file_record("a.gltf", b"first"),
                ]
                package["rootFile"] = "A.gltf"
            elif mutation == "nfc-collision":
                package["files"] = [
                    file_record("e\u0301.gltf", b"first"),
                    file_record("\u00e9.gltf", b"first"),
                ]
                package["rootFile"] = "\u00e9.gltf"
            elif mutation == "duplicate-path":
                package["files"].append(dict(package["files"][0]))
            elif mutation == "package-root":
                package["packageRoot"] = case["value"]
            elif mutation == "tier":
                package["tier"] = case["value"]
            elif mutation == "root-file":
                package["rootFile"] = case["value"]
            elif mutation == "coverage":
                package["coverageClaims"].append(case["value"])
            else:
                self.fail(f"unknown catalog mutation: {mutation}")
            try:
                self.model.validate_manifest(document)
            except self.model.ManifestError as error:
                observed.append((case["caseId"], error.category))
            else:
                observed.append((case["caseId"], "accepted"))
        self.assertEqual(
            [(case["caseId"], case["expectedCategory"]) for case in catalog["cases"]],
            observed,
        )


if __name__ == "__main__":
    unittest.main()
