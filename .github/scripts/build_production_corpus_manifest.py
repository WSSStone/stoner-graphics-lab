#!/usr/bin/env python3
"""Build the reviewed v1 corpus inventory from pinned admission inputs."""

import argparse
import hashlib
import json
from pathlib import Path
import sys


SCRIPT_DIR = Path(__file__).parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from production_content_manifest import canonical_bytes, validate_manifest  # noqa: E402


REVISION = "bf2bb4a81c73a7ceb53e80df3dec0105c5a3fdef"
SOURCE_LOCATION = "https://github.com/KhronosGroup/glTF-Sample-Assets"
LANTERN_DIGEST = "a79458c4b02d695187a952f23a63b8bf278e7bc3d316a3c2a314f2d6974181f1"
SPONZA_FILE_COUNT = 71
SPONZA_AGGREGATE_BYTES = 52686624


def file_record(path, role):
    payload = path.read_bytes()
    return {
        "path": path.name,
        "sha256": hashlib.sha256(payload).hexdigest(),
        "sizeBytes": len(payload),
        "role": role,
    }


def tree_records(root):
    records = []
    for path in sorted(item for item in root.rglob("*") if item.is_file()):
        relative = path.relative_to(root).as_posix()
        suffix = path.suffix.lower()
        role = "model" if suffix in {".gltf", ".glb"} else (
            "buffer" if suffix == ".bin" else
            "image" if suffix in {".jpg", ".jpeg", ".png", ".hdr"} else
            "other-source"
        )
        payload = path.read_bytes()
        records.append({
            "path": relative,
            "sha256": hashlib.sha256(payload).hexdigest(),
            "sizeBytes": len(payload),
            "role": role,
        })
    return records


def claims_for(package_id, values):
    return [
        {
            "claimId": claim_id,
            "packageId": package_id,
            "subject": subject,
            "evidence": evidence,
        }
        for claim_id, subject, evidence in values
    ]


def build_document(content_root, sponza_root):
    lantern_root = content_root / "Regular/Lantern"
    lantern_glb = lantern_root / "Lantern.glb"
    if hashlib.sha256(lantern_glb.read_bytes()).hexdigest() != LANTERN_DIGEST:
        raise ValueError("Lantern.glb does not match the pinned revision")
    lantern_files = [
        file_record(lantern_glb, "model"),
        file_record(lantern_root / "README.md", "other-source"),
    ]
    lantern_files.sort(key=lambda item: item["path"])

    sponza_files = tree_records(sponza_root)
    if len(sponza_files) != SPONZA_FILE_COUNT:
        raise ValueError(f"Sponza file count is {len(sponza_files)}, expected {SPONZA_FILE_COUNT}")
    aggregate = sum(item["sizeBytes"] for item in sponza_files)
    if aggregate != SPONZA_AGGREGATE_BYTES:
        raise ValueError(f"Sponza aggregate is {aggregate}, expected {SPONZA_AGGREGATE_BYTES}")

    lantern_claims = claims_for("khronos-lantern-glb", [
        ("lantern-color-texture", "texture:base-color", {"semantic": "color", "size": [2048, 2048]}),
        ("lantern-data-texture", "texture:metallic-roughness", {"semantic": "data", "size": [2048, 2048]}),
        ("lantern-embedded-dependencies", "Lantern.glb", {"images": 4, "storage": "bufferView"}),
        ("lantern-indexed-triangles", "mesh:*", {"indexed": True, "primitiveCount": 3}),
        ("lantern-local-hierarchy", "scene:0", {"nodeCount": 4}),
        ("lantern-multiple-primitives", "mesh:*", {"primitiveCount": 3}),
        ("lantern-normal-texture", "texture:normal", {"semantic": "normal", "size": [2048, 2048]}),
        ("lantern-rgb-source", "images:1-3", {"channels": 3, "count": 3}),
        ("lantern-rgba-source", "image:0", {"channels": 4, "count": 1}),
        ("lantern-texture-2k", "images:*", {"size": [2048, 2048], "count": 4}),
    ])
    sponza_claims = claims_for("khronos-sponza-gltf", [
        ("sponza-color-texture", "materials:*", {"semantic": "color"}),
        ("sponza-data-texture", "materials:*", {"semantic": "data"}),
        ("sponza-external-dependencies", "Sponza.gltf", {"buffers": 1, "images": 69, "storage": "external"}),
        ("sponza-indexed-triangles", "mesh:0", {"indexed": True, "primitiveCount": 103}),
        ("sponza-multiple-materials", "materials:*", {"materialCount": 25}),
        ("sponza-multiple-primitives", "mesh:0", {"primitiveCount": 103}),
        ("sponza-normal-texture", "materials:*", {"semantic": "normal"}),
        ("sponza-rgb-source", "images:*", {"channels": 3, "count": 66}),
        ("sponza-rgba-source", "images:*", {"channels": 4, "count": 3}),
        ("sponza-shared-dependency", "textures:*", {"sharedTextureCount": 2, "maximumReferences": 3}),
        ("sponza-texture-1k", "images:*", {"size": [1024, 1024], "count": 68}),
    ])
    coverage = sorted(lantern_claims + sponza_claims, key=lambda item: item["claimId"])
    packages = [
        {
            "packageId": "khronos-lantern-glb",
            "workName": "Lantern",
            "packageName": "glTF-Binary",
            "publisher": "KhronosGroup glTF Sample Assets",
            "sourceLocation": SOURCE_LOCATION,
            "revision": REVISION,
            "acquiredOn": "2026-08-22",
            "tier": "regular",
            "packageRoot": "Regular/Lantern",
            "sourcePath": "Models/Lantern/glTF-Binary",
            "rootAssetId": "StaticModel:Lantern.glb#idx.scene.0",
            "rootFile": "Lantern.glb",
            "files": lantern_files,
            "coverageClaims": sorted(item["claimId"] for item in lantern_claims),
        },
        {
            "packageId": "khronos-sponza-gltf",
            "workName": "Sponza",
            "packageName": "glTF",
            "publisher": "KhronosGroup glTF Sample Assets",
            "sourceLocation": SOURCE_LOCATION,
            "revision": REVISION,
            "acquiredOn": "2026-08-22",
            "tier": "medium",
            "packageRoot": "External/Sponza",
            "sourcePath": "Models/Sponza/glTF",
            "rootAssetId": "StaticModel:Sponza.gltf#idx.scene.0",
            "rootFile": "Sponza.gltf",
            "files": sponza_files,
            "coverageClaims": sorted(item["claimId"] for item in sponza_claims),
        },
    ]
    document = {
        "schema": "stoner.production-corpus",
        "schemaVersion": 1,
        "corpusRevision": "khronos-2026-08-19-v1",
        "packages": packages,
        "coverageClaims": coverage,
    }
    return validate_manifest(document)


def parse_args(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--content-root", type=Path, default=Path("Content/ProductionAcceptance"))
    parser.add_argument("--sponza-root", type=Path, required=True)
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    document = build_document(args.content_root, args.sponza_root)
    corpus_root = args.content_root / "Corpus"
    corpus_root.mkdir(parents=True, exist_ok=True)
    (corpus_root / "corpus-v1.json").write_bytes(canonical_bytes(document))
    coverage = {
        "schema": "stoner.production-coverage",
        "schemaVersion": 1,
        "corpusRevision": document["corpusRevision"],
        "coverageClaims": document["coverageClaims"],
    }
    (corpus_root / "coverage-v1.json").write_bytes(canonical_bytes(coverage))
    print(json.dumps({
        "corpusRevision": document["corpusRevision"],
        "packageCount": len(document["packages"]),
        "fileCount": sum(len(item["files"]) for item in document["packages"]),
        "coverageClaimCount": len(document["coverageClaims"]),
    }, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
