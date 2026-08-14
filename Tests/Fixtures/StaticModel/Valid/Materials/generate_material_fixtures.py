#!/usr/bin/env python3
from __future__ import annotations

import base64
import hashlib
import importlib.util
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[5]
VALID = Path(__file__).resolve().parent
INVALID = ROOT / "Tests/Fixtures/StaticModel/Invalid/Materials"
MANIFEST = ROOT / "Validation/024/fixture-manifest.json"
GEOMETRY = VALID.parent / "Geometry/generate_geometry_fixtures.py"
spec = importlib.util.spec_from_file_location("geometry", GEOMETRY)
assert spec and spec.loader
geometry = importlib.util.module_from_spec(spec)
spec.loader.exec_module(geometry)


def material_document(name: str) -> tuple[dict[str, object], bytes]:
    document, binary = geometry.make_document(name, uv_sets=2)
    png = (ROOT / "Tests/Fixtures/Images/Valid/png-rgba-5x3.png").read_bytes()
    uri = "data:image/png;base64," + base64.b64encode(png).decode("ascii")
    document["images"] = [{"uri": uri, "mimeType": "image/png"}]
    document["samplers"] = [{"magFilter": 9728, "minFilter": 9987,
                              "wrapS": 33648, "wrapT": 33071}]
    document["textures"] = [{"source": 0, "sampler": 0}]
    document["materials"] = [{
        "extras": {"stonerAssetId": "hero-material"},
        "pbrMetallicRoughness": {
            "baseColorFactor": [0.2, 0.4, 0.6, 0.8],
            "metallicFactor": 0.3,
            "roughnessFactor": 0.7,
            "baseColorTexture": {"index": 0, "texCoord": 1},
            "metallicRoughnessTexture": {"index": 0, "texCoord": 0},
        },
        "normalTexture": {"index": 0, "texCoord": 0, "scale": 0.75},
        "occlusionTexture": {"index": 0, "texCoord": 0, "strength": 0.6},
        "emissiveTexture": {"index": 0, "texCoord": 1},
        "emissiveFactor": [0.1, 0.2, 0.3],
        "alphaMode": "MASK", "alphaCutoff": 0.4, "doubleSided": True,
    }]
    document["meshes"][0]["primitives"][0]["material"] = 0
    return document, binary


def main() -> None:
    VALID.mkdir(parents=True, exist_ok=True)
    INVALID.mkdir(parents=True, exist_ok=True)
    document, binary = material_document("pbr-all")
    geometry.write_gltf(VALID / "01-pbr-all-embedded.gltf", document, binary)

    external, binary = geometry.make_document("external-jpeg")
    external["images"] = [{"uri": "albedo.jpg", "mimeType": "image/jpeg"}]
    external["textures"] = [{"source": 0}]
    external["materials"] = [{"pbrMetallicRoughness": {
        "baseColorTexture": {"index": 0}}}]
    external["meshes"][0]["primitives"][0]["material"] = 0
    geometry.write_gltf(VALID / "02-external-jpeg.gltf", external, binary)
    (VALID / "albedo.jpg").write_bytes(
        (ROOT / "Tests/Fixtures/Images/Valid/jpeg-rgb-3x5.jpg").read_bytes())

    missing = json.loads(json.dumps(external))
    missing["images"][0]["uri"] = "missing.jpg"
    geometry.write_gltf(INVALID / "01-missing-image.gltf", missing, binary)

    missing_uv1, binary = material_document("missing-uv1")
    del missing_uv1["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_1"]
    geometry.write_gltf(INVALID / "02-missing-uv1.gltf", missing_uv1, binary)

    current = json.loads(MANIFEST.read_text(encoding="utf-8"))
    retained = [entry for entry in current["fixtures"]
                if "/Materials/" not in entry["path"]]
    entries = []
    for path, expected in [
        (VALID / "01-pbr-all-embedded.gltf", "success"),
        (VALID / "02-external-jpeg.gltf", "success"),
        (VALID / "albedo.jpg", "dependency"),
        (INVALID / "01-missing-image.gltf", "failure"),
        (INVALID / "02-missing-uv1.gltf", "failure"),
    ]:
        entries.append({
            "path": path.relative_to(ROOT).as_posix(),
            "sha256": "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest(),
            "license": "CC0-1.0", "source_url": "repository-owned://feature-024/materials",
            "upstream_revision": "feature-024-v1", "validator_result": "valid",
            "expected_result": expected, "scope": ["US3", "materials"],
        })
    current["fixtures"] = sorted(retained + entries, key=lambda item: item["path"])
    MANIFEST.write_text(json.dumps(current, indent=2, sort_keys=True) + "\n",
                        encoding="utf-8")


if __name__ == "__main__":
    main()
