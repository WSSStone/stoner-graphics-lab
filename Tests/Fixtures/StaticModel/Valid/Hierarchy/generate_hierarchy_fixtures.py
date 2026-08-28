#!/usr/bin/env python3
"""Regenerate valid and invalid Feature 024 hierarchy fixtures."""

from __future__ import annotations

import hashlib
import importlib.util
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parents[5]
VALID = Path(__file__).resolve().parent
INVALID = ROOT / "Tests/Fixtures/StaticModel/Invalid/Hierarchy"
MANIFEST = ROOT / "Tests/Fixtures/StaticModel/fixture-manifest.json"
GEOMETRY_SCRIPT = VALID.parent / "Geometry/generate_geometry_fixtures.py"

spec = importlib.util.spec_from_file_location("geometry_fixtures", GEOMETRY_SCRIPT)
if spec is None or spec.loader is None:
    raise RuntimeError("cannot load geometry fixture generator")
geometry = importlib.util.module_from_spec(spec)
spec.loader.exec_module(geometry)


def base(name: str) -> tuple[dict[str, object], bytes]:
    document, binary = geometry.make_document(name)
    return document, binary


def explicit(value: str) -> dict[str, object]:
    return {"stonerAssetId": value}


def valid_cases() -> list[tuple[str, dict[str, object], bytes]]:
    cases: list[tuple[str, dict[str, object], bytes]] = []

    document, binary = base("multi-scene-shared")
    document["nodes"] = [
        {"mesh": 0, "name": "SharedA"},
        {"mesh": 0, "name": "SharedB", "translation": [0.0, 0.0, 2.0]},
    ]
    document["scenes"] = [
        {"name": "First", "nodes": [0]},
        {"name": "Second", "nodes": [1]},
    ]
    document["scene"] = 1
    cases.append(("01-multi-scene-shared.gltf", document, binary))

    document, binary = base("nested-trs")
    document["nodes"] = [
        {"name": "Root", "translation": [1.0, 2.0, 3.0], "children": [1]},
        {"name": "Child", "mesh": 0, "scale": [-1.0, 2.0, 0.5]},
    ]
    document["scenes"] = [{"nodes": [0]}]
    cases.append(("02-nested-trs-negative.gltf", document, binary))

    document, binary = base("matrix")
    document["nodes"] = [
        {
            "name": "MatrixNode",
            "mesh": 0,
            "matrix": [
                1.0, 0.0, 0.0, 0.0,
                0.0, 1.0, 0.0, 0.0,
                0.0, 0.0, 1.0, 0.0,
                1.0, 2.0, 3.0, 1.0,
            ],
        }
    ]
    cases.append(("03-matrix-transform.gltf", document, binary))

    document, binary = base("explicit-a")
    document["meshes"] = [
        {**document["meshes"][0], "extras": explicit("hero")},
        {**document["meshes"][0], "extras": explicit("prop")},
    ]
    document["nodes"] = [
        {"name": "Duplicate", "mesh": 0, "extras": explicit("root")},
        {"name": "Duplicate", "mesh": 1, "extras": explicit("child")},
    ]
    document["scenes"] = [
        {"nodes": [0, 1], "extras": explicit("showroom")}
    ]
    cases.append(("04-explicit-keys-a.gltf", document, binary))

    document, binary = base("explicit-b")
    document["meshes"] = [
        {**document["meshes"][0], "extras": explicit("prop")},
        {**document["meshes"][0], "extras": explicit("hero")},
    ]
    document["nodes"] = [
        {"name": "Renamed", "mesh": 0, "extras": explicit("child")},
        {"name": "Renamed", "mesh": 1, "extras": explicit("root")},
    ]
    document["scenes"] = [
        {"nodes": [1, 0], "extras": explicit("showroom")}
    ]
    cases.append(("05-explicit-keys-reordered.gltf", document, binary))

    document, binary = base("fallback-unreferenced")
    document["meshes"] = [document["meshes"][0], document["meshes"][0]]
    document["nodes"] = [{"mesh": 0, "name": "OnlyReferenced"}]
    cases.append(("06-fallback-unreferenced.gltf", document, binary))
    return cases


def invalid_cases() -> list[tuple[str, dict[str, object], bytes]]:
    cases: list[tuple[str, dict[str, object], bytes]] = []
    document, binary = base("cycle")
    document["nodes"] = [
        {"children": [1]},
        {"mesh": 0, "children": [0]},
    ]
    document["scenes"] = [{"nodes": [0]}]
    cases.append(("01-cycle.gltf", document, binary))

    document, binary = base("multiple-parent")
    document["nodes"] = [
        {"children": [2]},
        {"children": [2]},
        {"mesh": 0},
    ]
    document["scenes"] = [{"nodes": [0, 1]}]
    cases.append(("02-multiple-parent.gltf", document, binary))

    document, binary = base("duplicate-key")
    document["nodes"] = [
        {"mesh": 0, "extras": explicit("same")},
        {"mesh": 0, "extras": explicit("same")},
    ]
    document["scenes"] = [{"nodes": [0, 1]}]
    cases.append(("03-duplicate-node-key.gltf", document, binary))

    document, binary = base("invalid-key")
    document["meshes"][0]["extras"] = explicit("bad/key")
    cases.append(("04-invalid-mesh-key.gltf", document, binary))

    document, binary = base("empty-scene")
    document["scenes"] = [{"nodes": []}]
    cases.append(("05-empty-scene.gltf", document, binary))
    return cases


def main() -> None:
    VALID.mkdir(parents=True, exist_ok=True)
    INVALID.mkdir(parents=True, exist_ok=True)
    entries: list[dict[str, object]] = []
    for expected, directory, cases in (
        ("success", VALID, valid_cases()),
        ("failure", INVALID, invalid_cases()),
    ):
        for filename, document, binary in cases:
            path = directory / filename
            geometry.write_gltf(path, document, binary)
            entries.append(
                {
                    "path": path.relative_to(ROOT).as_posix(),
                    "source_url": "repository-owned://feature-024/hierarchy",
                    "upstream_revision": "feature-024-v1",
                    "sha256": "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest(),
                    "license": "CC0-1.0",
                    "validator_result": "valid" if expected == "success" else "invalid",
                    "expected_result": expected,
                    "scope": ["US2", "hierarchy", filename.removesuffix(".gltf")],
                }
            )
    current = json.loads(MANIFEST.read_text(encoding="utf-8"))
    retained = [
        item
        for item in current.get("fixtures", [])
        if "/Hierarchy/" not in item.get("path", "")
    ]
    fixtures = sorted(retained + entries, key=lambda item: item["path"])
    MANIFEST.write_text(
        json.dumps({"schema": current["schema"], "fixtures": fixtures}, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
