#!/usr/bin/env python3
"""Generate the deterministic Feature 024 representative GLB benchmark."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import struct


VERTEX_COUNT = 100_000
INDEX_COUNT = 300_000
PRIMITIVE_COUNT = 16
MATERIAL_COUNT = 16
TEXTURE_COUNT = 0


def _align4(data: bytearray, fill: int = 0) -> None:
    data.extend(bytes([fill]) * ((-len(data)) % 4))


def generate() -> bytes:
    binary = bytearray()
    buffer_views: list[dict[str, int]] = []
    accessors: list[dict[str, object]] = []
    primitives: list[dict[str, object]] = []
    vertices_per_primitive = VERTEX_COUNT // PRIMITIVE_COUNT
    indices_per_primitive = INDEX_COUNT // PRIMITIVE_COUNT

    def add_view(payload: bytes, target: int) -> int:
        _align4(binary)
        offset = len(binary)
        binary.extend(payload)
        index = len(buffer_views)
        buffer_views.append({
            "buffer": 0, "byteOffset": offset,
            "byteLength": len(payload), "target": target,
        })
        return index

    for primitive_index in range(PRIMITIVE_COUNT):
        positions = bytearray()
        normals = bytearray()
        texcoords = bytearray()
        for vertex in range(vertices_per_primitive):
            corner = vertex % 3
            x = 1.0 if corner == 1 else 0.0
            y = 1.0 if corner == 2 else 0.0
            z = float(primitive_index)
            positions.extend(struct.pack("<fff", x, y, z))
            normals.extend(struct.pack("<bbb", 0, 0, 127))
            texcoords.extend(struct.pack("<HH", 0, 0))
        usable_vertices = vertices_per_primitive - (vertices_per_primitive % 3)
        indices = bytearray()
        for triangle in range(indices_per_primitive // 3):
            base = (triangle * 3) % usable_vertices
            indices.extend(struct.pack("<III", base, base + 1, base + 2))

        position_view = add_view(positions, 34962)
        normal_view = add_view(normals, 34962)
        texcoord_view = add_view(texcoords, 34962)
        index_view = add_view(indices, 34963)
        position_accessor = len(accessors)
        accessors.append({
            "bufferView": position_view, "componentType": 5126,
            "count": vertices_per_primitive, "type": "VEC3",
            "min": [0.0, 0.0, float(primitive_index)],
            "max": [1.0, 1.0, float(primitive_index)],
        })
        normal_accessor = len(accessors)
        accessors.append({
            "bufferView": normal_view, "componentType": 5120,
            "normalized": True, "count": vertices_per_primitive,
            "type": "VEC3",
        })
        texcoord_accessor = len(accessors)
        accessors.append({
            "bufferView": texcoord_view, "componentType": 5123,
            "normalized": True, "count": vertices_per_primitive,
            "type": "VEC2",
        })
        index_accessor = len(accessors)
        accessors.append({
            "bufferView": index_view, "componentType": 5125,
            "count": indices_per_primitive, "type": "SCALAR",
        })
        primitives.append({
            "attributes": {
                "POSITION": position_accessor,
                "NORMAL": normal_accessor,
                "TEXCOORD_0": texcoord_accessor,
            },
            "indices": index_accessor,
            "material": primitive_index,
            "mode": 4,
        })

    materials = []
    for index in range(MATERIAL_COUNT):
        pbr: dict[str, object] = {
            "baseColorFactor": [1.0, 1.0, 1.0, 1.0],
            "metallicFactor": 0.0,
            "roughnessFactor": 1.0,
        }
        materials.append({"name": f"Material{index:02}",
                          "pbrMetallicRoughness": pbr})

    document = {
        "asset": {"version": "2.0", "generator": "stoner-feature-024-v1"},
        "scene": 0,
        "scenes": [{"nodes": [0]}],
        "nodes": [{"mesh": 0}],
        "meshes": [{"name": "Representative", "primitives": primitives}],
        "materials": materials,
        "buffers": [{"byteLength": len(binary)}],
        "bufferViews": buffer_views,
        "accessors": accessors,
    }
    json_bytes = json.dumps(
        document, sort_keys=True, separators=(",", ":")).encode("utf-8")
    json_bytes += b" " * ((-len(json_bytes)) % 4)
    _align4(binary)
    total_length = 12 + 8 + len(json_bytes) + 8 + len(binary)
    return b"".join((
        struct.pack("<4sII", b"glTF", 2, total_length),
        struct.pack("<I4s", len(json_bytes), b"JSON"), json_bytes,
        struct.pack("<I4s", len(binary), b"BIN\x00"), bytes(binary),
    ))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output", type=Path,
        default=Path(__file__).with_name("Representative.glb"))
    args = parser.parse_args()
    payload = generate()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(payload)
    print(f"generated {args.output} ({len(payload)} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
