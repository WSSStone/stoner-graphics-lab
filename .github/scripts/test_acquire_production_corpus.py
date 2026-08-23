#!/usr/bin/env python3

import hashlib
import importlib.util
import json
from pathlib import Path
import tempfile
import unittest


SCRIPT = Path(__file__).with_name("acquire_production_corpus.py")
SPEC = importlib.util.spec_from_file_location("acquire_production_corpus", SCRIPT)
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def record(path, payload):
    return {
        "path": path,
        "sha256": hashlib.sha256(payload).hexdigest(),
        "sizeBytes": len(payload),
        "role": "model" if path.endswith(".gltf") else "image",
    }


def manifest(payloads, revision="a" * 40):
    package = {
        "packageId": "medium-package",
        "workName": "Medium Work",
        "packageName": "glTF",
        "publisher": "Fixture Publisher",
        "sourceLocation": "https://github.com/example/assets",
        "revision": revision,
        "acquiredOn": "2026-08-22",
        "tier": "medium",
        "packageRoot": "External/Medium",
        "sourcePath": "Models/Medium/glTF",
        "rootAssetId": "StaticModel:medium.gltf#idx.scene.0",
        "rootFile": "medium.gltf",
        "files": [record(path, payloads[path]) for path in sorted(payloads)],
        "coverageClaims": ["medium-indexed"],
    }
    regular = dict(package)
    regular.update({
        "packageId": "regular-package",
        "workName": "Regular Work",
        "tier": "regular",
        "packageRoot": "Regular/Regular",
        "sourcePath": "Models/Regular/glTF",
        "rootAssetId": "StaticModel:regular.gltf#idx.scene.0",
        "rootFile": "regular.gltf",
        "files": [record("regular.gltf", b"regular")],
        "coverageClaims": ["regular-indexed"],
    })
    return {
        "schema": "stoner.production-corpus",
        "schemaVersion": 1,
        "corpusRevision": "test-v1",
        "packages": [package, regular],
        "coverageClaims": [
            {"claimId": "medium-indexed", "packageId": "medium-package", "subject": "root", "evidence": True},
            {"claimId": "regular-indexed", "packageId": "regular-package", "subject": "root", "evidence": True},
        ],
    }


class ProductionCorpusAcquisitionTests(unittest.TestCase):
    def test_pinned_urls_and_exact_inventory_publish_atomically(self):
        payloads = {"medium.gltf": b"model", "textures/base.png": b"image"}
        urls = []

        def fetch(url, destination):
            urls.append(url)
            relative = url.split("/Models/Medium/glTF/", 1)[1]
            destination.write_bytes(payloads[relative])

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            manifest_path = root / "corpus.json"
            manifest_path.write_text(json.dumps(manifest(payloads)), encoding="utf-8")
            destination = root / "External/Medium"
            result = MODULE.acquire_package(
                manifest_path, "medium-package", root, fetcher=fetch)
            self.assertEqual("Acquired", result["status"])
            self.assertEqual(b"model", (destination / "medium.gltf").read_bytes())
            self.assertFalse(list(root.glob(".medium-package.partial-*")))

        self.assertEqual(2, len(urls))
        self.assertTrue(all("/" + "a" * 40 + "/" in url for url in urls))
        self.assertEqual(len(urls), len(set(urls)))

    def test_interrupted_or_unavailable_download_leaves_no_partial_cache(self):
        payloads = {"medium.gltf": b"model", "texture.png": b"image"}

        def interrupted(url, destination):
            destination.write_bytes(b"partial")
            raise OSError("network interrupted")

        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "corpus.json"
            path.write_text(json.dumps(manifest(payloads)), encoding="utf-8")
            with self.assertRaisesRegex(MODULE.AcquisitionError, "source-unavailable"):
                MODULE.acquire_package(path, "medium-package", root, fetcher=interrupted)
            self.assertFalse((root / "External/Medium").exists())
            self.assertFalse(list(root.glob(".medium-package.partial-*")))

    def test_wrong_revision_is_rejected_before_fetch(self):
        payloads = {"medium.gltf": b"model"}
        calls = []
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "corpus.json"
            path.write_text(json.dumps(manifest(payloads, revision="main")), encoding="utf-8")
            with self.assertRaisesRegex(MODULE.AcquisitionError, "revision-not-immutable"):
                MODULE.acquire_package(
                    path, "medium-package", root,
                    fetcher=lambda url, destination: calls.append(url))
        self.assertEqual([], calls)

    def test_wrong_hash_rejects_temporary_download(self):
        payloads = {"medium.gltf": b"model"}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "corpus.json"
            path.write_text(json.dumps(manifest(payloads)), encoding="utf-8")
            with self.assertRaisesRegex(MODULE.AcquisitionError, "digest-mismatch"):
                MODULE.acquire_package(
                    path, "medium-package", root,
                    fetcher=lambda url, destination: destination.write_bytes(b"wrong"))
            self.assertFalse((root / "External/Medium").exists())

    def test_invalid_existing_cache_is_quarantined_before_reacquisition(self):
        payloads = {"medium.gltf": b"model"}
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            path = root / "corpus.json"
            path.write_text(json.dumps(manifest(payloads)), encoding="utf-8")
            destination = root / "External/Medium"
            destination.mkdir(parents=True)
            (destination / "medium.gltf").write_bytes(b"corrupt")
            result = MODULE.acquire_package(
                path, "medium-package", root,
                fetcher=lambda url, output: output.write_bytes(b"model"))
            quarantines = list((root / "External").glob("Medium.quarantine-*"))
            self.assertEqual("Acquired", result["status"])
            self.assertEqual(1, len(quarantines))
            self.assertEqual(b"corrupt", (quarantines[0] / "medium.gltf").read_bytes())


if __name__ == "__main__":
    unittest.main()
