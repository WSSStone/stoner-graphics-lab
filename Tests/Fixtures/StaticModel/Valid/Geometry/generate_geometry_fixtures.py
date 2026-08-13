#!/usr/bin/env python3
"""Regenerate the repository-owned Feature 024 geometry corpus."""

from __future__ import annotations

import base64
import hashlib
import json
import struct
from pathlib import Path


ROOT = Path(__file__).resolve().parents[5]
OUTPUT = Path(__file__).resolve().parent
MANIFEST = ROOT / "Validation/024/fixture-manifest.json"

POSITIONS = [(0.0, 0.0, 0.0), (2.0, 0.0, 0.0), (0.0, 3.0, 0.0)]
NORMALS = [(0.0, 0.0, 1.0)] * 3
TANGENTS = [(1.0, 0.0, 0.0, 1.0)] * 3
UVS = [(0.0, 0.0), (1.0, 0.0), (0.0, 1.0)]


class Builder:
    def __init__(self) -> None:
        self.binary = bytearray()
        self.views: list[dict[str, int]] = []
        self.accessors: list[dict[str, object]] = []

    def view(self, data: bytes, *, target: int | None = None, stride: int | None = None) -> int:
        while len(self.binary) % 4:
            self.binary.append(0)
        result = len(self.views)
        view: dict[str, int] = {
            "buffer": 0,
            "byteOffset": len(self.binary),
            "byteLength": len(data),
        }
        if target is not None:
            view["target"] = target
        if stride is not None:
            view["byteStride"] = stride
        self.views.append(view)
        self.binary.extend(data)
        return result

    def accessor(
        self,
        view: int | None,
        component: int,
        count: int,
        kind: str,
        *,
        offset: int = 0,
        normalized: bool = False,
        sparse: dict[str, object] | None = None,
    ) -> int:
        result = len(self.accessors)
        value: dict[str, object] = {
            "componentType": component,
            "count": count,
            "type": kind,
        }
        if view is not None:
            value["bufferView"] = view
        if offset:
            value["byteOffset"] = offset
        if normalized:
            value["normalized"] = True
        if sparse is not None:
            value["sparse"] = sparse
        self.accessors.append(value)
        return result


def floats(values: list[tuple[float, ...]]) -> bytes:
    return b"".join(struct.pack("<" + "f" * len(value), *value) for value in values)


def expected() -> dict[str, object]:
    return {
        "coordinateConvention": "UnrealLH_ZUp_XForward_YRight_Meters_CW",
        "positions": [[0.0, -0.0, 0.0], [0.0, -2.0, 0.0], [0.0, -0.0, 3.0]],
        "indices": [0, 1, 2],
        "bounds": {"min": [0.0, -2.0, 0.0], "max": [0.0, -0.0, 3.0]},
        "frontFace": "clockwise",
    }


def make_document(
    name: str,
    *,
    index_component: int = 5123,
    indexed: bool = True,
    normals: bool = True,
    tangents: bool = True,
    uv_sets: int = 1,
    interleaved: bool = False,
    normalized_normals: bool = False,
    sparse_positions: bool = False,
    primitive_count: int = 1,
    translated_node: bool = False,
) -> tuple[dict[str, object], bytes]:
    builder = Builder()
    attributes: dict[str, int] = {}
    if sparse_positions:
        sparse_indices = builder.view(bytes([0, 1, 2]))
        sparse_values = builder.view(floats(POSITIONS))
        attributes["POSITION"] = builder.accessor(
            None,
            5126,
            3,
            "VEC3",
            sparse={
                "count": 3,
                "indices": {"bufferView": sparse_indices, "componentType": 5121},
                "values": {"bufferView": sparse_values},
            },
        )
    elif interleaved:
        records = [POSITIONS[index] + NORMALS[index] for index in range(3)]
        view = builder.view(floats(records), target=34962, stride=24)
        attributes["POSITION"] = builder.accessor(view, 5126, 3, "VEC3")
        attributes["NORMAL"] = builder.accessor(view, 5126, 3, "VEC3", offset=12)
    else:
        position_view = builder.view(floats(POSITIONS), target=34962)
        attributes["POSITION"] = builder.accessor(position_view, 5126, 3, "VEC3")

    if normals and "NORMAL" not in attributes:
        if normalized_normals:
            normal_view = builder.view(bytes([0, 0, 127] * 3), target=34962)
            attributes["NORMAL"] = builder.accessor(
                normal_view, 5120, 3, "VEC3", normalized=True
            )
        else:
            normal_view = builder.view(floats(NORMALS), target=34962)
            attributes["NORMAL"] = builder.accessor(normal_view, 5126, 3, "VEC3")
    if tangents:
        tangent_view = builder.view(floats(TANGENTS), target=34962)
        attributes["TANGENT"] = builder.accessor(tangent_view, 5126, 3, "VEC4")
    for uv_set in range(uv_sets):
        uv_view = builder.view(floats(UVS), target=34962)
        attributes[f"TEXCOORD_{uv_set}"] = builder.accessor(uv_view, 5126, 3, "VEC2")

    primitive: dict[str, object] = {
        "attributes": attributes,
        "mode": 4,
        "extras": {"stonerExpected": expected()},
    }
    if indexed:
        format_by_component = {5121: "B", 5123: "H", 5125: "I"}
        index_data = struct.pack("<" + format_by_component[index_component] * 3, 0, 1, 2)
        index_view = builder.view(index_data, target=34963)
        primitive["indices"] = builder.accessor(index_view, index_component, 3, "SCALAR")

    node: dict[str, object] = {"mesh": 0, "name": name}
    if translated_node:
        node["translation"] = [1.0, 2.0, 3.0]
    document: dict[str, object] = {
        "asset": {"version": "2.0", "generator": "stoner-feature-024"},
        "buffers": [{"byteLength": len(builder.binary)}],
        "bufferViews": builder.views,
        "accessors": builder.accessors,
        "meshes": [{"name": name, "primitives": [primitive] * primitive_count}],
        "nodes": [node],
        "scenes": [{"nodes": [0]}],
        "scene": 0,
    }
    return document, bytes(builder.binary)


CASES = [
    ("01-basis-u16.gltf", {}),
    ("02-index-u8.gltf", {"index_component": 5121}),
    ("03-index-u32.gltf", {"index_component": 5125}),
    ("04-non-indexed.gltf", {"indexed": False}),
    ("05-interleaved.gltf", {"interleaved": True}),
    ("06-sparse-position.gltf", {"sparse_positions": True}),
    ("07-normalized-normal.gltf", {"normalized_normals": True}),
    ("08-missing-normal.gltf", {"normals": False}),
    ("09-missing-tangent.gltf", {"tangents": False}),
    ("10-uv1.gltf", {"uv_sets": 2}),
    ("11-node-transform.gltf", {"translated_node": True}),
    ("12-two-primitives.glb", {"primitive_count": 2}),
]


def write_gltf(path: Path, document: dict[str, object], binary: bytes) -> None:
    document["buffers"][0]["uri"] = (
        "data:application/octet-stream;base64," + base64.b64encode(binary).decode("ascii")
    )
    path.write_text(json.dumps(document, sort_keys=True, separators=(",", ":")) + "\n", encoding="utf-8")


def write_glb(path: Path, document: dict[str, object], binary: bytes) -> None:
    json_bytes = json.dumps(document, sort_keys=True, separators=(",", ":")).encode("utf-8")
    json_bytes += b" " * ((-len(json_bytes)) % 4)
    binary += b"\0" * ((-len(binary)) % 4)
    total = 12 + 8 + len(json_bytes) + 8 + len(binary)
    path.write_bytes(
        b"glTF"
        + struct.pack("<II", 2, total)
        + struct.pack("<II", len(json_bytes), 0x4E4F534A)
        + json_bytes
        + struct.pack("<II", len(binary), 0x004E4942)
        + binary
    )


def main() -> None:
    OUTPUT.mkdir(parents=True, exist_ok=True)
    entries: list[dict[str, object]] = []
    for filename, options in CASES:
        document, binary = make_document(filename, **options)
        path = OUTPUT / filename
        if path.suffix == ".glb":
            write_glb(path, document, binary)
        else:
            write_gltf(path, document, binary)
        relative = path.relative_to(ROOT).as_posix()
        entries.append(
            {
                "path": relative,
                "source_url": "repository-owned://feature-024/geometry",
                "upstream_revision": "feature-024-v1",
                "sha256": "sha256:" + hashlib.sha256(path.read_bytes()).hexdigest(),
                "license": "CC0-1.0",
                "validator_result": "valid",
                "expected_result": "success",
                "scope": ["US1", "SC-004", filename.removesuffix(path.suffix)],
            }
        )
    MANIFEST.write_text(
        json.dumps(
            {"schema": "stoner.static-model.fixture-manifest/v1", "fixtures": entries},
            indent=2,
            sort_keys=True,
        )
        + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
