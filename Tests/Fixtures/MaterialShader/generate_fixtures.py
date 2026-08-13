#!/usr/bin/env python3
"""Generate deterministic Feature 023 JSON fixture inventory."""

from __future__ import annotations

import hashlib
import json
from pathlib import Path


ROOT = Path(__file__).resolve().parent
VALID = ROOT / "Valid"
INVALID = ROOT / "Invalid"
GOLDEN = ROOT / "Golden"


def write(path: Path, value: object) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(value, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
        newline="\n",
    )


def asset(asset_type: str, path: str, subresource: str | None = None) -> dict:
    result = {"type": asset_type, "path": path}
    if subresource:
        result["subresource"] = subresource
    return result


def shader(index: int) -> dict:
    path = f"Tests/Shaders/Program{index:02d}"
    digest = hashlib.sha256(f"shader-{index}".encode()).hexdigest()
    stage = {
        "stage": "compute" if index == 11 else "vertex",
        "entryPoint": "main",
        "language": "glsl",
        "source": {
            "asset": asset("ShaderSource", path, "source.primary"),
            "locator": f"Program{index:02d}.glsl",
            "digest": f"sha256:{digest}",
        },
    }
    stages = [stage]
    if index != 11:
        fragment = json.loads(json.dumps(stage))
        fragment["stage"] = "fragment"
        fragment["source"]["asset"]["subresource"] = "source.fragment"
        fragment["source"]["locator"] = f"Program{index:02d}.frag"
        stages.append(fragment)
    return {
        "schema": "stoner.shader-program",
        "version": 1,
        "id": asset("ShaderProgram", path),
        "requiredExtensions": [],
        "programKind": "compute" if index == 11 else "graphics",
        "stages": stages,
        "allowedPermutationFlags": ["SKINNED"] if index == 10 else [],
        "requiredParameters": (
            [{"name": "BaseColor", "type": "color"}] if index == 9 else []
        ),
        "interface": {
            "bindings": (
                [{
                    "set": 0,
                    "binding": 0,
                    "kind": "uniformBuffer",
                    "arrayCount": 1,
                    "visibility": ["vertex", "fragment"],
                    "name": "Frame",
                }]
                if index == 8 else []
            ),
            "constantRanges": [],
        },
        "variants": [],
        "extensions": {},
    }


def material(index: int) -> dict:
    domains = [
        ("surface", "opaque"),
        ("surface", "masked"),
        ("surface", "translucent"),
        ("surface", "additive"),
        ("postProcess", "opaque"),
        ("postProcess", "translucent"),
        ("postProcess", "additive"),
        ("ui", "translucent"),
        ("ui", "additive"),
        ("decal", "opaque"),
        ("decal", "masked"),
        ("decal", "translucent"),
    ]
    domain, blend = domains[index]
    parameters = [
        {"name": "Roughness", "type": "scalar", "value": index / 16.0},
        {"name": "Tint", "type": "color", "value": [1, 0.5, 0.25, 1]},
    ]
    if index % 3 == 0:
        parameters.append({
            "name": "Albedo",
            "type": "texture",
            "value": asset("Texture", f"Tests/Textures/T{index:02d}"),
        })
    return {
        "schema": "stoner.material",
        "version": 1,
        "id": asset("Material", f"Tests/Materials/M{index:02d}"),
        "requiredExtensions": [],
        "domain": domain,
        "blendMode": blend,
        "renderState": {
            "depthTest": index % 2 == 0,
            "depthWrite": index % 3 != 0,
            "twoSided": index % 4 == 0,
        },
        "shader": asset("ShaderProgram", f"Tests/Shaders/Program{index:02d}"),
        "permutationFlags": [],
        "parameters": parameters,
        "extensions": {},
    }


def material_v2() -> dict:
    return {
        "schema": "stoner.material",
        "version": 2,
        "id": asset("Material", "Tests/Materials/V2"),
        "requiredExtensions": [],
        "domain": "surface",
        "blendMode": "opaque",
        "renderState": {
            "depthTest": True,
            "depthWrite": True,
            "twoSided": False,
        },
        "shader": asset("ShaderProgram", "Tests/Shaders/Foundation"),
        "permutationFlags": [],
        "parameters": [{
            "name": "BaseColorTexture",
            "type": "textureBinding",
            "value": {
                "texture": "Texture:Tests/Textures/V2#base-color",
                "texCoord": 1,
                "sampler": {
                    "min": "nearest",
                    "mag": "linear",
                    "mip": "none",
                    "addressU": "mirroredRepeat",
                    "addressV": "clampToEdge",
                },
            },
        }],
        "extensions": {},
    }


def instance(index: int) -> dict:
    parent = (
        asset("Material", f"Tests/Materials/M{index % 12:02d}")
        if index < 8
        else asset("MaterialInstance", f"Tests/Instances/I{index - 8:02d}")
    )
    return {
        "schema": "stoner.material-instance",
        "version": 1,
        "id": asset("MaterialInstance", f"Tests/Instances/I{index:02d}"),
        "requiredExtensions": [],
        "parent": parent,
        "overrides": [
            {"name": "Roughness", "type": "scalar", "value": index / 32.0}
        ],
        "extensions": {},
    }


def invalid_cases(base: dict) -> list[tuple[str, object, str]]:
    cases: list[tuple[str, object, str]] = []
    mutations = [
        ("root-array", [], "InvalidDefinition"),
        ("missing-schema", {k: v for k, v in base.items() if k != "schema"}, "InvalidDefinition"),
        ("unknown-schema", {**base, "schema": "stoner.future"}, "UnsupportedSchema"),
        ("version-zero", {**base, "version": 0}, "UnsupportedSchema"),
        ("version-two", {**base, "version": 2}, "InvalidDefinition"),
        ("required-extension", {**base, "requiredExtensions": ["vendor.required"], "extensions": {"vendor.required": {}}}, "UnknownRequiredExtension"),
        ("missing-required-body", {**base, "requiredExtensions": ["vendor.required"]}, "InvalidDefinition"),
        ("extensions-array", {**base, "extensions": []}, "InvalidDefinition"),
        ("unknown-field", {**base, "typo": True}, "InvalidDefinition"),
        ("wrong-id-type", {**base, "id": asset("Texture", "Tests/Bad")}, "InvalidDefinition"),
        ("empty-path", {**base, "id": asset("Material", "")}, "InvalidDefinition"),
        ("unknown-domain", {**base, "domain": "future"}, "InvalidDefinition"),
        ("unknown-blend", {**base, "blendMode": "future"}, "InvalidDefinition"),
        ("state-array", {**base, "renderState": []}, "InvalidDefinition"),
        ("shader-wrong-type", {**base, "shader": asset("Texture", "Tests/Bad")}, "InvalidDefinition"),
        ("flags-object", {**base, "permutationFlags": {}}, "InvalidDefinition"),
        ("parameters-object", {**base, "parameters": {}}, "InvalidDefinition"),
        ("duplicate-parameter", {**base, "parameters": [base["parameters"][0], base["parameters"][0]]}, "InvalidMaterialAsset"),
        ("nan-string", {**base, "parameters": [{"name": "P", "type": "scalar", "value": "NaN"}]}, "InvalidDefinition"),
        ("vector-short", {**base, "parameters": [{"name": "P", "type": "vector", "value": [1, 2, 3]}]}, "InvalidDefinition"),
        ("texture-wrong-type", {**base, "parameters": [{"name": "P", "type": "texture", "value": asset("Material", "Tests/Bad")}]}, "InvalidDefinition"),
        ("parameter-unknown-field", {**base, "parameters": [{"name": "P", "type": "scalar", "value": 1, "x": 2}]}, "InvalidDefinition"),
    ]
    cases.extend(mutations)
    raw = [
        ("empty", "", "DefinitionLimitExceeded"),
        ("truncated-object", "{", "InvalidDefinition"),
        ("trailing-comma", '{"schema":"stoner.material",}', "InvalidDefinition"),
        ("comment", '{"schema":"stoner.material"/*x*/}', "InvalidDefinition"),
        ("trailing-data", '{}{}', "InvalidDefinition"),
        ("root-string", '"material"', "InvalidDefinition"),
        ("leading-zero", '{"version":01}', "InvalidDefinition"),
        ("plus-number", '{"version":+1}', "InvalidDefinition"),
        ("infinity", '{"version":Infinity}', "InvalidDefinition"),
        ("nul-escape-key", '{"schema":"stoner.material","\\u0000":0}', "InvalidDefinition"),
        ("duplicate-key", '{"schema":"stoner.material","schema":"stoner.material"}', "InvalidDefinition"),
        ("escaped-duplicate-key", '{"schema":"stoner.material","\\u0073chema":"stoner.material"}', "InvalidDefinition"),
        ("bom", "\ufeff{}", "InvalidDefinition"),
        ("lone-surrogate", '{"x":"\\ud800"}', "InvalidDefinition"),
        ("unterminated-string", '{"x":"abc}', "InvalidDefinition"),
        ("array-root", "[1,2,3]", "InvalidDefinition"),
        ("boolean-root", "true", "InvalidDefinition"),
        ("number-root", "1", "InvalidDefinition"),
    ]
    cases.extend(raw)
    v2_texcoord = material_v2()
    v2_texcoord["parameters"][0]["value"]["texCoord"] = 2
    cases.append(("v2-texcoord", v2_texcoord, "InvalidDefinition"))
    return cases


def main() -> None:
    for directory in (VALID, INVALID):
        directory.mkdir(parents=True, exist_ok=True)
        for path in directory.iterdir():
            if path.is_file():
                path.unlink()
    GOLDEN.mkdir(parents=True, exist_ok=True)

    inventory = {"valid": [], "invalid": []}
    for index in range(12):
        for prefix, value in (
            ("shader", shader(index)),
            ("material", material(index)),
        ):
            name = f"{prefix}-{index:02d}.json"
            write(VALID / name, value)
            inventory["valid"].append(name)
    for index in range(16):
        name = f"instance-{index:02d}.json"
        write(VALID / name, instance(index))
        inventory["valid"].append(name)
    write(VALID / "material-v2.json", material_v2())
    inventory["valid"].append("material-v2.json")

    for index, (name, value, expected) in enumerate(
        invalid_cases(material(0))
    ):
        filename = f"{index:02d}-{name}.json"
        path = INVALID / filename
        if isinstance(value, str):
            path.write_text(value, encoding="utf-8", newline="\n")
        else:
            write(path, value)
        inventory["invalid"].append(
            {"file": filename, "expected": expected}
        )

    write(GOLDEN / "inventory.json", inventory)


if __name__ == "__main__":
    main()
