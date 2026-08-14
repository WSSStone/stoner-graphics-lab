#!/usr/bin/env python3
"""Generate the deterministic Feature 025 1,000-asset/5,000-edge DAG."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
from typing import Any


ASSET_COUNT = 1_000
EDGE_COUNT = 5_000
LAYER_WIDTH = 100


def build_corpus() -> dict[str, Any]:
    assets: list[dict[str, Any]] = []
    edges = 0
    for index in range(ASSET_COUNT):
        layer = index // LAYER_WIDTH
        dependencies: list[str] = []
        if layer > 0:
            previous_start = (layer - 1) * LAYER_WIDTH
            count = 6 if index - LAYER_WIDTH < 500 else 5
            offsets: list[int] = []
            candidate = 0
            while len(offsets) < count:
                offset = (index * 17 + candidate * 23) % LAYER_WIDTH
                if offset not in offsets:
                    offsets.append(offset)
                candidate += 1
            dependencies = [
                f"Synthetic:Scale/Asset{previous_start + offset:04d}"
                for offset in sorted(offsets)
            ]
        edges += len(dependencies)
        assets.append({
            "id": f"Synthetic:Scale/Asset{index:04d}",
            "layer": layer,
            "sourceBytes": 256 + (index % 17),
            "dependencies": dependencies,
        })
    assert edges == EDGE_COUNT
    return {
        "schema": "stoner.asset-cooker-scale-corpus",
        "schemaVersion": 1,
        "generator": "layered-dag-v1",
        "assetCount": ASSET_COUNT,
        "dependencyEdgeCount": EDGE_COUNT,
        "layerCount": ASSET_COUNT // LAYER_WIDTH,
        "maximumDependencyDepth": ASSET_COUNT // LAYER_WIDTH - 1,
        "assets": assets,
    }


def canonical_bytes(corpus: dict[str, Any]) -> bytes:
    return (json.dumps(
        corpus, ensure_ascii=True, sort_keys=True, separators=(",", ":"),
    ) + "\n").encode("ascii")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--output",
        type=pathlib.Path,
        default=pathlib.Path(__file__).with_name("Scale") /
            "scale-1000-5000.json",
    )
    args = parser.parse_args()
    payload = canonical_bytes(build_corpus())
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(payload)
    print(f"{hashlib.sha256(payload).hexdigest()}  {args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
