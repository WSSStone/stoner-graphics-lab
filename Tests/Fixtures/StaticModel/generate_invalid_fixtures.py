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
    return result


def generate(output: Path) -> dict[str, object]:
    output.mkdir(parents=True, exist_ok=True)
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


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    generate(args.output)


if __name__ == "__main__":
    main()
