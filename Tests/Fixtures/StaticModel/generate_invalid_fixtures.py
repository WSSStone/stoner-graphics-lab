#!/usr/bin/env python3
"""Generate deterministic Feature 024 malformed/unsupported glTF mutations."""

from __future__ import annotations

import argparse
import copy
import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
BASE = ROOT / "Tests/Fixtures/StaticModel/Valid/Geometry/01-basis-u16.gltf"
DEFAULT_OUTPUT = ROOT / "Tests/Fixtures/StaticModel/Invalid/Hardening"
FIXTURE_MANIFEST = ROOT / "Tests/Fixtures/StaticModel/fixture-manifest.json"


def mutation_documents() -> list[tuple[str, str, dict[str, object]]]:
    base = json.loads(BASE.read_text(encoding="utf-8"))
    result: list[tuple[str, str, dict[str, object]]] = []

    def add(name: str, category: str, edit) -> None:
        document = copy.deepcopy(base)
        edit(document)
        result.append((name, category, document))

    add("non-triangle", "unsupported", lambda d: d["meshes"][0]["primitives"][0].update(mode=1))
    add("required-extension", "unsupported", lambda d: d.update(extensionsRequired=["KHR_draco_mesh_compression"]))
    add("future-version", "unsupported", lambda d: d["asset"].update(version="3.0"))
    add("future-min-version", "unsupported", lambda d: d["asset"].update(minVersion="3.0"))
    add("missing-position", "malformed", lambda d: d["meshes"][0]["primitives"][0]["attributes"].pop("POSITION"))
    add("invalid-index-accessor", "malformed", lambda d: d["meshes"][0]["primitives"][0].update(indices=999))
    add("unsupported-color", "unsupported", lambda d: d["meshes"][0]["primitives"][0]["attributes"].update(COLOR_0=0))
    add("morph-target", "unsupported", lambda d: d["meshes"][0]["primitives"][0].update(targets=[{"POSITION": 0}]))
    add("skin", "unsupported", lambda d: (d.update(skins=[{"joints": [0]}]), d["nodes"][0].update(skin=0)))
    add("gpu-instancing", "unsupported", lambda d: d["nodes"][0].update(extensions={"EXT_mesh_gpu_instancing": {"attributes": {"TRANSLATION": 0}}}))
    add("meshopt-compression", "unsupported", lambda d: d["bufferViews"][0].update(extensions={"EXT_meshopt_compression": {"buffer": 0, "byteOffset": 0, "byteLength": 8, "byteStride": 4, "count": 2, "mode": "ATTRIBUTES", "filter": "NONE"}}))
    add("draco-primitive", "unsupported", lambda d: d["meshes"][0]["primitives"][0].update(extensions={"KHR_draco_mesh_compression": {"bufferView": 0, "attributes": {"POSITION": 0}}}))
    add("material-variants", "unsupported", lambda d: (
        d.update(extensionsRequired=["KHR_materials_variants"]),
        d["meshes"][0]["primitives"][0].update(
            extensions={"KHR_materials_variants": {"mappings": []}})))
    add("line-loop", "unsupported", lambda d: d["meshes"][0]["primitives"][0].update(mode=2))
    add("line-strip", "unsupported", lambda d: d["meshes"][0]["primitives"][0].update(mode=3))
    add("triangle-strip", "unsupported", lambda d: d["meshes"][0]["primitives"][0].update(mode=5))
    add("triangle-fan", "unsupported", lambda d: d["meshes"][0]["primitives"][0].update(mode=6))
    add("joints-semantic", "unsupported", lambda d: d["meshes"][0]["primitives"][0]["attributes"].update(JOINTS_0=0))
    add("weights-semantic", "unsupported", lambda d: d["meshes"][0]["primitives"][0]["attributes"].update(WEIGHTS_0=0))
    add("texcoord2-semantic", "unsupported", lambda d: d["meshes"][0]["primitives"][0]["attributes"].update(TEXCOORD_2=0))
    add("multiple-morph-targets", "unsupported", lambda d: d["meshes"][0]["primitives"][0].update(targets=[{"POSITION": 0}, {"NORMAL": 1}]))
    add("accessor-buffer-view", "malformed", lambda d: d["accessors"][0].update(bufferView=999))
    add("zero-accessor-count", "malformed", lambda d: d["accessors"][0].update(count=0))
    add("position-scalar", "malformed", lambda d: d["accessors"][0].update(type="SCALAR"))
    add("position-signed-int", "malformed", lambda d: d["accessors"][0].update(componentType=5124))
    add("index-float", "malformed", lambda d: d["accessors"][-1].update(componentType=5126))
    add("index-vec2", "malformed", lambda d: d["accessors"][-1].update(type="VEC2"))
    add("short-buffer", "malformed", lambda d: d["buffers"][0].update(byteLength=999999))
    add("buffer-view-buffer", "malformed", lambda d: d["bufferViews"][0].update(buffer=999))
    add("buffer-view-offset", "malformed", lambda d: d["bufferViews"][0].update(byteOffset=999999))
    add("short-buffer-view", "malformed", lambda d: d["bufferViews"][0].update(byteLength=1))
    add("primitive-material", "malformed", lambda d: d["meshes"][0]["primitives"][0].update(material=999))
    add("node-mesh", "malformed", lambda d: d["nodes"][0].update(mesh=999))
    add("scene-node", "malformed", lambda d: d["scenes"][0].update(nodes=[999]))
    add("default-scene", "malformed", lambda d: d.update(scene=999))
    add("node-cycle", "malformed", lambda d: d["nodes"][0].update(children=[0]))
    add("missing-meshes", "malformed", lambda d: d.pop("meshes"))
    add("missing-nodes", "malformed", lambda d: d.pop("nodes"))
    add("missing-scenes", "malformed", lambda d: d.pop("scenes"))
    add("empty-primitives", "malformed", lambda d: d["meshes"][0].update(primitives=[]))
    add("empty-attributes", "malformed", lambda d: d["meshes"][0]["primitives"][0].update(attributes={}))
    add("invalid-base64-alphabet", "malformed", lambda d: d["buffers"][0].update(uri="data:application/octet-stream;base64,!!!!"))
    add("invalid-base64-padding", "malformed", lambda d: d["buffers"][0].update(uri="data:application/octet-stream;base64,AAA"))
    add("unsupported-buffer-scheme", "malformed", lambda d: d["buffers"][0].update(uri="https://example.invalid/mesh.bin"))
    add("buffer-uri-fragment", "malformed", lambda d: d["buffers"][0].update(uri="mesh.bin#fragment"))
    add("buffer-uri-query", "malformed", lambda d: d["buffers"][0].update(uri="mesh.bin?version=1"))
    add("buffer-uri-traversal", "malformed", lambda d: d["buffers"][0].update(uri="../mesh.bin"))
    add("buffer-uri-backslash", "malformed", lambda d: d["buffers"][0].update(uri="..\\mesh.bin"))
    add("sparse-count-overflow", "malformed", lambda d: d["accessors"][0].update(sparse={"count": 999999, "indices": {"bufferView": 0, "componentType": 5123}, "values": {"bufferView": 0}}))
    return result


def generate(output: Path) -> dict[str, object]:
    output.mkdir(parents=True, exist_ok=True)
    for stale in output.glob("*.gltf"):
        stale.unlink()
    (output / "mutation-manifest.json").unlink(missing_ok=True)
    entries = []
    for index, (name, category, document) in enumerate(mutation_documents(), start=1):
        path = output / f"{index:02d}-{name}.gltf"
        payload = (json.dumps(document, separators=(",", ":"), sort_keys=True) + "\n").encode()
        path.write_bytes(payload)
        entries.append({
            "file": path.name,
            "mutation": name,
            "expected": category,
            "sha256": hashlib.sha256(payload).hexdigest(),
        })
    manifest = {"schema": 1, "base": BASE.relative_to(ROOT).as_posix(), "mutations": entries}
    (output / "mutation-manifest.json").write_text(
        json.dumps(manifest, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    return manifest


def sync_fixture_manifest(mutation_manifest: dict[str, object]) -> None:
    document = json.loads(FIXTURE_MANIFEST.read_text(encoding="utf-8"))
    prefix = DEFAULT_OUTPUT.relative_to(ROOT).as_posix() + "/"
    fixtures = [
        entry for entry in document["fixtures"]
        if not entry["path"].startswith(prefix)
    ]
    for entry in mutation_manifest["mutations"]:
        fixtures.append({
            "expected_result": "failure",
            "license": "CC0-1.0",
            "mutation": entry["mutation"],
            "mutation_base": mutation_manifest["base"],
            "path": prefix + entry["file"],
            "scope": ["US5", "malformed", entry["expected"]],
            "sha256": "sha256:" + entry["sha256"],
            "source_url": "repository-owned://feature-024/hardening-mutation",
            "upstream_revision": "feature-024-generator-v1",
            "validator_result": entry["expected"],
        })
    document["fixtures"] = sorted(fixtures, key=lambda item: item["path"])
    FIXTURE_MANIFEST.write_text(
        json.dumps(document, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    manifest = generate(args.output)
    if args.output.resolve() == DEFAULT_OUTPUT.resolve():
        sync_fixture_manifest(manifest)


if __name__ == "__main__":
    main()
