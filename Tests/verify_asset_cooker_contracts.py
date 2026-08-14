#!/usr/bin/env python3
"""Generate and verify Feature 025 cooked-contract golden fixtures."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import struct
import sys
from typing import Any


MAGIC = b"SGCOOK01"
KNOWN_CODECS = {
    "stoner.image",
    "stoner.texture",
    "stoner.ktx2",
    "stoner.shader-source",
    "stoner.shader-payload",
    "stoner.shader-program",
    "stoner.material",
    "stoner.material-instance",
    "stoner.static-mesh",
    "stoner.static-model",
}
REPORT_ACTIONS = {
    "hit", "miss", "invalidate", "quarantine", "cook", "rebuild",
    "fallback", "ineligible", "reuse", "stage", "validate", "publish",
    "fail",
}
EXPECTED_PROFILES = {
    "Linux-Vulkan.json": "linux",
    "Mac-Vulkan.json": "macos",
    "Windows-Vulkan.json": "windows",
}
REQUIRED_IGNORES = {
    "Saved/DerivedDataCache/",
    "Saved/Cooked/",
    "Saved/Feature025*/",
}
RUNTIME_COOKED_HEADERS = {
    "FAssetCookContractCodec.h",
    "FAssetCookManifest.h",
    "FAssetCookedExtensions.h",
    "FAssetCookedPayload.h",
    "FAssetDerivedDataEntry.h",
    "FAssetDerivedKey.h",
    "FAssetTargetProfile.h",
    "FCurrentGenerationPointer.h",
}


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _json_string(value: str) -> str:
    return json.dumps(value, ensure_ascii=False, separators=(",", ":"))


def _string_array(values: list[str]) -> str:
    return "[" + ", ".join(_json_string(value) for value in values) + "]"


def canonical_profile(profile: dict[str, Any], include_display: bool) -> str:
    lines = ["{"]

    def field(name: str, value: str, comma: bool = True) -> None:
        lines.append(f'  "{name}": {value}' + ("," if comma else ""))

    field("schema", _json_string(profile["schema"]))
    field("schemaVersion", str(profile["schemaVersion"]))
    if include_display:
        field("displayName", _json_string(profile["displayName"]))
    for name in ("platform", "cpuArchitecture", "graphicsBackend"):
        field(name, _json_string(profile[name]))
    lines.append('  "shaderPayloadChoices": [')
    choices = profile["shaderPayloadChoices"]
    for index, choice in enumerate(choices):
        lines.extend([
            "    {",
            f'      "backend": {_json_string(choice["backend"])},',
            f'      "profile": {_json_string(choice["profile"])},',
            f'      "format": {_json_string(choice["format"])}',
            "    }" + ("," if index + 1 < len(choices) else ""),
        ])
    lines.append("  ],")
    field("textureCapabilities", _string_array(profile["textureCapabilities"]))
    field("textureFallback", _json_string(profile["textureFallback"]))
    policy = profile["buildPolicy"]
    lines.extend([
        '  "buildPolicy": {',
        f'    "optimization": {_json_string(policy["optimization"])},',
        '    "includeDebugSymbols": '
        + ("true" if policy["includeDebugSymbols"] else "false") + ",",
        f'    "validation": {_json_string(policy["validation"])},',
        '    "producerSettings": [',
    ])
    producers = policy["producerSettings"]
    for producer_index, producer in enumerate(producers):
        lines.extend([
            "      {",
            f'        "producer": {_json_string(producer["producer"])},',
            f'        "schemaVersion": {producer["schemaVersion"]},',
            '        "settings": {',
        ])
        settings = producer["settings"]
        for setting_index, (name, value) in enumerate(settings.items()):
            encoded = json.dumps(value, ensure_ascii=False, separators=(",", ":"))
            comma = "," if setting_index + 1 < len(settings) else ""
            lines.append(f'          "{name}": {encoded}{comma}')
        lines.extend([
            "        }",
            "      }" + ("," if producer_index + 1 < len(producers) else ""),
        ])
    lines.extend(["    ]", "  },", '  "limits": {'])
    limits = profile["limits"]
    limit_names = (
        "maxDiscoveredSources", "maxAssets", "maxDependencyEdges",
        "maxDependencyDepth", "maxSourceBytes", "maxPayloadBytes",
        "maxAggregateBytes", "maxManifestBytes", "maxDiagnostics",
    )
    for index, name in enumerate(limit_names):
        comma = "," if index + 1 < len(limit_names) else ""
        lines.append(f'    "{name}": {limits[name]}{comma}')
    lines.append("  }")
    required = profile.get("requiredExtensions", [])
    optional = profile.get("optionalExtensions", [])
    if required or optional:
        lines[-1] += ","
        field("requiredExtensions", _string_array(required))
        field("optionalExtensions", _string_array(optional), False)
    lines.append("}")
    return "\n".join(lines) + "\n"


def build_envelope(body: bytes = b"\x00\x01\x02\x03\xfe\xff") -> bytes:
    asset_id = b"Image:Cooker/Test"
    asset_type = b"Image"
    codec = b"stoner.image"
    header = bytearray(MAGIC)
    header.extend(struct.pack("<HHI", 1, 0, 0))
    header.extend(struct.pack("<I", len(asset_id)) + asset_id)
    header.extend(struct.pack("<H", len(asset_type)) + asset_type)
    header.extend(struct.pack("<H", len(codec)) + codec)
    header.extend(struct.pack("<IIQ", 1, 1, len(body)))
    header.extend(hashlib.sha256(body).digest())
    struct.pack_into("<H", header, 10, len(header))
    return bytes(header) + body


def parse_envelope(data: bytes) -> dict[str, Any]:
    if len(data) < 72 or data[:8] != MAGIC:
        raise ValueError("invalid magic or truncated fixed header")
    offset = 8
    version, header_bytes, flags = struct.unpack_from("<HHI", data, offset)
    offset += 8

    def read_text(length_format: str) -> str:
        nonlocal offset
        size = struct.unpack_from(length_format, data, offset)[0]
        offset += struct.calcsize(length_format)
        end = offset + size
        if size == 0 or end > len(data) or b"\0" in data[offset:end]:
            raise ValueError("invalid text field")
        try:
            value = data[offset:end].decode("utf-8")
        except UnicodeDecodeError as error:
            raise ValueError("invalid UTF-8") from error
        offset = end
        return value

    asset_id = read_text("<I")
    asset_type = read_text("<H")
    codec = read_text("<H")
    if offset + 48 > len(data):
        raise ValueError("truncated variable header")
    codec_version, schema_version, body_bytes = struct.unpack_from("<IIQ", data, offset)
    offset += 16
    body_digest = data[offset:offset + 32]
    offset += 32
    if version != 1 or flags != 0 or codec_version != 1 or schema_version != 1:
        raise ValueError("unsupported envelope contract")
    if codec not in KNOWN_CODECS or ":" not in asset_id:
        raise ValueError("unknown codec or invalid identity")
    if asset_id.split(":", 1)[0] != asset_type:
        raise ValueError("asset type mismatch")
    if header_bytes < offset or header_bytes > len(data):
        raise ValueError("invalid header boundary")
    if body_bytes == 0 or header_bytes + body_bytes != len(data):
        raise ValueError("invalid body boundary")
    body = data[header_bytes:]
    if hashlib.sha256(body).digest() != body_digest:
        raise ValueError("body digest mismatch")
    return {
        "assetId": asset_id,
        "assetType": asset_type,
        "codec": codec,
        "bodyBytes": body_bytes,
        "envelopeDigest": digest(data),
    }


def _participant(name: str) -> dict[str, str]:
    return {"id": name, "version": "1.0.0"}


def _record(asset_id: str, seed: str) -> dict[str, Any]:
    envelope = digest(f"{seed}.envelope".encode())
    return {
        "assetId": asset_id,
        "assetType": asset_id.split(":", 1)[0],
        "sourceVersion": digest(f"{seed}.source".encode()),
        "sourceManifest": [{
            "assetId": asset_id,
            "version": digest(f"{seed}.source-file".encode()),
            "role": "primary",
        }],
        "importer": _participant("importer.test"),
        "cooker": _participant("cooker.test"),
        "codec": _participant("codec.test"),
        "derivedKey": digest(f"{seed}.key".encode()),
        "payloadSchemaVersion": 1,
        "payloadLocator": f"Payloads/{envelope}.sgasset",
        "payloadBytes": 128,
        "envelopeDigest": envelope,
        "dependencies": [],
    }


def _selection_text(selection: dict[str, Any], depth: int = 1) -> str:
    pad = "  " * depth
    inner = "  " * (depth + 1)
    return "\n".join([
        "{",
        f'{inner}"mode": {_json_string(selection["mode"])},',
        f'{inner}"roots": {_string_array(selection["roots"])},',
        f'{inner}"sourceScopes": {_string_array(selection["sourceScopes"])},',
        f'{inner}"discoveryRulesVersion": {_json_string(selection["discoveryRulesVersion"])}',
        f"{pad}}}",
    ])


def _participant_text(value: dict[str, str], depth: int) -> str:
    pad = "  " * depth
    inner = "  " * (depth + 1)
    return "\n".join([
        "{",
        f'{inner}"id": {_json_string(value["id"])},',
        f'{inner}"version": {_json_string(value["version"])}',
        f"{pad}}}",
    ])


def _records_text(records: list[dict[str, Any]], include_locator: bool) -> str:
    lines = ["["] if records else ["[]"]
    for record_index, record in enumerate(records):
        lines.append("    {")
        fields: list[tuple[str, str]] = [
            ("assetId", _json_string(record["assetId"])),
            ("assetType", _json_string(record["assetType"])),
            ("sourceVersion", _json_string(record["sourceVersion"])),
        ]
        source_lines = ["["]
        for source_index, source in enumerate(record["sourceManifest"]):
            source_lines.extend([
                "        {",
                f'          "assetId": {_json_string(source["assetId"])},',
                f'          "version": {_json_string(source["version"])},',
                f'          "role": {_json_string(source["role"])}',
                "        }" + ("," if source_index + 1 < len(record["sourceManifest"]) else ""),
            ])
        source_lines.append("      ]")
        fields.append(("sourceManifest", "\n".join(source_lines)))
        for name in ("importer", "cooker", "codec"):
            fields.append((name, _participant_text(record[name], 3)))
        fields.extend([
            ("derivedKey", _json_string(record["derivedKey"])),
            ("payloadSchemaVersion", str(record["payloadSchemaVersion"])),
        ])
        if include_locator:
            fields.append(("payloadLocator", _json_string(record["payloadLocator"])))
        fields.extend([
            ("payloadBytes", str(record["payloadBytes"])),
            ("envelopeDigest", _json_string(record["envelopeDigest"])),
        ])
        dependency_lines = ["["] if record["dependencies"] else ["[]"]
        for dependency_index, dependency in enumerate(record["dependencies"]):
            dependency_lines.extend([
                "        {",
                f'          "assetId": {_json_string(dependency["assetId"])},',
                f'          "role": {_json_string(dependency["role"])}',
                "        }" + ("," if dependency_index + 1 < len(record["dependencies"]) else ""),
            ])
        if record["dependencies"]:
            dependency_lines.append("      ]")
        fields.append(("dependencies", "\n".join(dependency_lines)))
        for field_index, (name, value) in enumerate(fields):
            prefix = f'      "{name}": '
            value_lines = value.splitlines()
            lines.append(prefix + value_lines[0])
            lines.extend(value_lines[1:])
            if field_index + 1 < len(fields):
                lines[-1] += ","
        lines.append("    }" + ("," if record_index + 1 < len(records) else ""))
    if records:
        lines.append("  ]")
    return "\n".join(lines)


def semantic_manifest_text(manifest: dict[str, Any]) -> str:
    lines = [
        "{",
        f'  "schema": {_json_string(manifest["schema"])},',
        f'  "schemaVersion": {manifest["schemaVersion"]},',
        f'  "effectiveProfileDigest": {_json_string(manifest["effectiveProfileDigest"])},',
        '  "selection": ' + _selection_text(manifest["selection"]) + ",",
        f'  "snapshotDigest": {_json_string(manifest["snapshotDigest"])},',
        f'  "limitsDigest": {_json_string(manifest["limitsDigest"])},',
        '  "records": ' + _records_text(manifest["records"], False) + ",",
        f'  "requiredExtensions": {_string_array(manifest.get("requiredExtensions", []))}',
        "}",
    ]
    return "\n".join(lines) + "\n"


def canonical_manifest_text(manifest: dict[str, Any]) -> str:
    profile_lines = canonical_profile(manifest["targetProfile"], True).rstrip().splitlines()
    nested_profile = profile_lines[0] + "\n" + "\n".join(
        "  " + line for line in profile_lines[1:]
    )
    lines = [
        "{",
        f'  "schema": {_json_string(manifest["schema"])},',
        f'  "schemaVersion": {manifest["schemaVersion"]},',
        f'  "generationId": {_json_string(manifest["generationId"])},',
        '  "targetProfile": ' + nested_profile + ",",
        f'  "effectiveProfileDigest": {_json_string(manifest["effectiveProfileDigest"])},',
        '  "selection": ' + _selection_text(manifest["selection"]) + ",",
        f'  "snapshotDigest": {_json_string(manifest["snapshotDigest"])},',
        f'  "limitsDigest": {_json_string(manifest["limitsDigest"])},',
        '  "records": ' + _records_text(manifest["records"], True),
    ]
    extensions = manifest.get("requiredExtensions", [])
    if extensions:
        lines[-1] += ","
        lines.append(f'  "requiredExtensions": {_string_array(extensions)}')
    lines.extend(["}", ""])
    return "\n".join(lines)


def build_manifest(profile: dict[str, Any]) -> bytes:
    effective = canonical_profile(profile, False).encode("utf-8")
    image = _record("Image:Cooker/A", "image")
    texture = _record("Texture:Cooker/B", "texture")
    texture["dependencies"] = [{"assetId": image["assetId"], "role": "build"}]
    manifest: dict[str, Any] = {
        "schema": "stoner.asset-cook-manifest",
        "schemaVersion": 1,
        "generationId": "0" * 64,
        "targetProfile": profile,
        "effectiveProfileDigest": digest(effective),
        "selection": {
            "mode": "explicit-roots",
            "roots": [texture["assetId"]],
            "sourceScopes": ["Content"],
            "discoveryRulesVersion": digest(b"discovery.v1"),
        },
        "snapshotDigest": digest(b"snapshot.v1"),
        "limitsDigest": digest(b"limits.v1"),
        "records": [image, texture],
        "requiredExtensions": ["stoner.core"],
    }
    manifest["generationId"] = digest(semantic_manifest_text(manifest).encode("utf-8"))
    return canonical_manifest_text(manifest).encode("utf-8")


def _unique_object(pairs: list[tuple[str, Any]]) -> dict[str, Any]:
    result: dict[str, Any] = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"duplicate JSON key: {key}")
        result[key] = value
    return result


def parse_manifest(data: bytes) -> dict[str, Any]:
    try:
        value = json.loads(data, object_pairs_hook=_unique_object)
    except (json.JSONDecodeError, UnicodeDecodeError) as error:
        raise ValueError("invalid manifest JSON") from error
    if not isinstance(value, dict) or value.get("schema") != "stoner.asset-cook-manifest":
        raise ValueError("invalid manifest schema")
    effective = digest(canonical_profile(value["targetProfile"], False).encode("utf-8"))
    if value.get("effectiveProfileDigest") != effective:
        raise ValueError("effective profile digest mismatch")
    generation = digest(semantic_manifest_text(value).encode("utf-8"))
    if value.get("generationId") != generation:
        raise ValueError("generation digest mismatch")
    if canonical_manifest_text(value).encode("utf-8") != data:
        raise ValueError("manifest is not canonical")
    return value


def validate_schema(path: pathlib.Path) -> list[str]:
    errors: list[str] = []
    try:
        schema = json.loads(path.read_bytes(), object_pairs_hook=_unique_object)
    except (ValueError, json.JSONDecodeError) as error:
        return [f"{path}: invalid JSON schema: {error}"]
    if not isinstance(schema, dict) or schema.get("type") != "object":
        errors.append(f"{path}: root schema must describe an object")
        return errors
    required = schema.get("required")
    properties = schema.get("properties")
    if not isinstance(required, list) or len(required) != len(set(required)):
        errors.append(f"{path}: required fields must be a unique array")
    if not isinstance(properties, dict):
        errors.append(f"{path}: properties must be an object")
    elif isinstance(required, list):
        for name in required:
            if name not in properties:
                errors.append(f"{path}: required field lacks a property: {name}")
    return errors


def parse_report(data: bytes, normalized: bool = True) -> dict[str, Any]:
    value = json.loads(data, object_pairs_hook=_unique_object)
    if not isinstance(value, dict) or value.get("schema") != "stoner.asset-cook-report":
        raise ValueError("invalid report schema")
    required = {
        "schema", "schemaVersion", "command", "result",
        "deterministicDigest", "summary", "decisions", "diagnostics",
    }
    if value.get("schemaVersion") != 1 or not required.issubset(value):
        raise ValueError("incomplete report header")
    if normalized and "telemetry" in value:
        raise ValueError("normalized report contains telemetry")
    if not isinstance(value["summary"], dict) or not isinstance(value["decisions"], list):
        raise ValueError("invalid report aggregate structure")
    if not isinstance(value["diagnostics"], list):
        raise ValueError("invalid report diagnostics")
    for index, decision in enumerate(value["decisions"]):
        if not isinstance(decision, dict) or decision.get("planIndex") != index:
            raise ValueError("non-canonical report decision order")
        if decision.get("action") not in REPORT_ACTIONS or not decision.get("assetId") or not decision.get("reason"):
            raise ValueError("invalid report decision")
    for diagnostic in value["diagnostics"]:
        if not isinstance(diagnostic, dict) or not all(
            diagnostic.get(name) for name in ("category", "stage", "reason")
        ):
            raise ValueError("invalid report diagnostic")
    return value


def write_golden(root: pathlib.Path) -> None:
    profile_path = root / "Tests/Fixtures/AssetCooker/Contracts/Profiles/mac-vulkan.json"
    profile = json.loads(profile_path.read_bytes(), object_pairs_hook=_unique_object)
    payload_root = root / "Tests/Fixtures/AssetCooker/Contracts/Payloads"
    manifest_root = root / "Tests/Fixtures/AssetCooker/Contracts/Manifests"
    payload_root.mkdir(parents=True, exist_ok=True)
    manifest_root.mkdir(parents=True, exist_ok=True)
    envelope = build_envelope()
    (payload_root / "valid-image.sgasset").write_bytes(envelope)
    (payload_root / "invalid-truncated.sgasset").write_bytes(envelope[:-1])
    substituted = bytearray(envelope)
    substituted[-1] ^= 1
    (payload_root / "invalid-body-digest.sgasset").write_bytes(substituted)
    manifest = build_manifest(profile)
    (manifest_root / "valid-manifest.json").write_bytes(manifest)
    (manifest_root / "invalid-noncanonical.json").write_bytes(b" " + manifest)
    invalid_generation = manifest.replace(
        json.loads(manifest)["generationId"].encode("ascii"), b"0" * 64, 1
    )
    (manifest_root / "invalid-generation.json").write_bytes(invalid_generation)


def verify_repository_contracts(root: pathlib.Path) -> list[str]:
    errors: list[str] = []

    profile_root = root / "Config/AssetCooker/Profiles"
    profile_digests: set[str] = set()
    for name, expected_platform in EXPECTED_PROFILES.items():
        path = profile_root / name
        try:
            data = path.read_bytes()
            profile = json.loads(data, object_pairs_hook=_unique_object)
        except (OSError, ValueError, json.JSONDecodeError) as error:
            errors.append(f"invalid target profile {name}: {error}")
            continue
        if profile.get("schema") != "stoner.asset-target-profile" or \
                profile.get("schemaVersion") != 1:
            errors.append(f"invalid target profile contract: {name}")
            continue
        if profile.get("platform") != expected_platform:
            errors.append(
                f"target profile platform mismatch: {name} "
                f"({profile.get('platform')!r})"
            )
        canonical = canonical_profile(profile, True).encode("utf-8")
        if canonical != data:
            errors.append(f"target profile is not canonical: {name}")
        profile_digests.add(digest(canonical_profile(profile, False).encode("utf-8")))
    if len(profile_digests) != len(EXPECTED_PROFILES):
        errors.append("target profiles must have distinct effective digests")

    try:
        ignored = {
            line.strip() for line in (root / ".gitignore").read_text(
                encoding="utf-8"
            ).splitlines()
            if line.strip() and not line.lstrip().startswith("#")
        }
    except OSError as error:
        errors.append(f"unable to read .gitignore: {error}")
    else:
        for required in sorted(REQUIRED_IGNORES - ignored):
            errors.append(f"missing generated-output ignore rule: {required}")

    tool_root = root / "Tools/AssetCooker"
    sconscript = tool_root / "SConscript"
    try:
        build_text = sconscript.read_text(encoding="utf-8").replace("\\", "/")
    except OSError as error:
        errors.append(f"unable to read AssetCooker SConscript: {error}")
    else:
        for source in sorted((tool_root / "Private").glob("*.cpp")):
            relative = source.relative_to(tool_root).as_posix()
            if relative not in build_text:
                errors.append(f"unregistered AssetCooker source: {relative}")

    public_root = root / "Source/Asset/Public/Asset"
    minimal_path = public_root / "AssetMinimal.h"
    try:
        minimal = minimal_path.read_text(encoding="utf-8")
    except OSError as error:
        errors.append(f"unable to read AssetMinimal.h: {error}")
        minimal = ""
    for name in sorted(RUNTIME_COOKED_HEADERS):
        path = public_root / name
        if not path.is_file():
            errors.append(f"missing runtime cooked public contract: {name}")
            continue
        text = path.read_text(encoding="utf-8")
        if "Tools/" in text or "AssetCooker/" in text:
            errors.append(f"runtime cooked contract depends on Tool API: {name}")
        include = f'#include "Asset/{name}"'
        if include not in minimal:
            errors.append(f"AssetMinimal.h does not expose cooked contract: {name}")
    return errors


def verify(root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    contract_root = root / "specs/025-asset-cooker-derived-data/contracts"
    for name in (
        "target-profile.schema.json", "manifest.schema.json",
        "report.schema.json",
    ):
        errors.extend(validate_schema(contract_root / name))
    errors.extend(verify_repository_contracts(root))
    fixture_manifest_path = root / "Validation/025/fixture-manifest.json"
    try:
        fixture_manifest = json.loads(
            fixture_manifest_path.read_bytes(), object_pairs_hook=_unique_object
        )
    except (ValueError, json.JSONDecodeError) as error:
        return errors + [f"{fixture_manifest_path}: invalid fixture manifest: {error}"]
    seen: set[str] = set()
    for fixture in fixture_manifest.get("fixtures", []):
        relative = fixture.get("path")
        if not isinstance(relative, str) or relative in seen or pathlib.PurePosixPath(relative).is_absolute() or ".." in pathlib.PurePosixPath(relative).parts:
            errors.append(f"invalid or duplicate fixture path: {relative!r}")
            continue
        seen.add(relative)
        path = root / relative
        if not path.is_file():
            errors.append(f"missing fixture: {relative}")
            continue
        actual = digest(path.read_bytes())
        if fixture.get("sha256") != actual:
            errors.append(f"fixture checksum mismatch: {relative}")
    payload_root = root / "Tests/Fixtures/AssetCooker/Contracts/Payloads"
    manifest_root = root / "Tests/Fixtures/AssetCooker/Contracts/Manifests"
    try:
        parse_envelope((payload_root / "valid-image.sgasset").read_bytes())
    except (OSError, ValueError) as error:
        errors.append(f"valid payload fixture failed: {error}")
    for name in ("invalid-truncated.sgasset", "invalid-body-digest.sgasset"):
        try:
            parse_envelope((payload_root / name).read_bytes())
        except (OSError, ValueError):
            pass
        else:
            errors.append(f"malformed payload fixture passed: {name}")
    try:
        parse_manifest((manifest_root / "valid-manifest.json").read_bytes())
    except (OSError, ValueError) as error:
        errors.append(f"valid manifest fixture failed: {error}")
    for name in ("invalid-noncanonical.json", "invalid-generation.json"):
        try:
            parse_manifest((manifest_root / name).read_bytes())
        except (OSError, ValueError):
            pass
        else:
            errors.append(f"malformed manifest fixture passed: {name}")
    report_root = root / "Tests/Fixtures/AssetCooker/Reports"
    for name in ("valid-plan.json", "invalid-profile.json"):
        try:
            parse_report((report_root / name).read_bytes())
        except (OSError, ValueError, json.JSONDecodeError) as error:
            errors.append(f"report fixture failed: {name}: {error}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root", type=pathlib.Path,
        default=pathlib.Path(__file__).resolve().parents[1],
    )
    parser.add_argument("--write-golden", action="store_true")
    args = parser.parse_args()
    root = args.root.resolve()
    if args.write_golden:
        write_golden(root)
    errors = verify(root)
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print("Asset cooker schemas and golden fixtures: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
