#!/usr/bin/env python3
"""Verify Feature 023 repository shader ownership without invoking a shell."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


PROGRAM_FILES = {
    "Content/Shaders/Triangle/Triangle.shader.json",
    "Content/Shaders/Deferred/Surface.shader.json",
    "Content/Shaders/Deferred/Composition.shader.json",
    "Content/Shaders/Deferred/DirectionalLight.shader.json",
    "Content/Shaders/Deferred/PointLight.shader.json",
    "Content/Shaders/Deferred/SpotLight.shader.json",
}

PROGRAM_IDENTITIES = {
    "Content/Shaders/Triangle/Triangle.shader.json":
        ("ShaderProgram", "Engine/Shaders/Triangle", ""),
    "Content/Shaders/Deferred/Surface.shader.json":
        ("ShaderProgram", "Engine/Shaders/Deferred/Surface", ""),
    "Content/Shaders/Deferred/Composition.shader.json":
        ("ShaderProgram", "Engine/Shaders/Deferred/Composition", ""),
    "Content/Shaders/Deferred/DirectionalLight.shader.json":
        ("ShaderProgram", "Engine/Shaders/Deferred/DirectionalLight", ""),
    "Content/Shaders/Deferred/PointLight.shader.json":
        ("ShaderProgram", "Engine/Shaders/Deferred/PointLight", ""),
    "Content/Shaders/Deferred/SpotLight.shader.json":
        ("ShaderProgram", "Engine/Shaders/Deferred/SpotLight", ""),
}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def asset_key(value: dict) -> tuple[str, str, str]:
    return (
        value.get("type", ""),
        value.get("path", ""),
        value.get("subresource", ""),
    )


def verify(root: Path) -> list[str]:
    errors: list[str] = []
    found = {
        path.relative_to(root).as_posix()
        for path in (root / "Content/Shaders").rglob("*.shader.json")
    }
    if found != PROGRAM_FILES:
        errors.append("program-inventory")

    program_ids: set[tuple[str, str, str]] = set()
    dependency_ids: dict[tuple[str, str, str], tuple[str, str]] = {}
    destination_ids: dict[Path, tuple[str, str, str]] = {}
    owned_files: set[Path] = set()
    for relative in sorted(found):
        path = root / relative
        try:
            document = json.loads(path.read_text(encoding="utf-8"))
        except (OSError, UnicodeError, json.JSONDecodeError):
            errors.append(f"definition-malformed:{relative}")
            continue
        program_id = asset_key(document.get("id", {}))
        if program_id != PROGRAM_IDENTITIES.get(relative):
            errors.append(f"program-id:{relative}")
        if program_id in program_ids:
            errors.append(f"program-id-duplicate:{relative}")
        program_ids.add(program_id)
        if document.get("schema") != "stoner.shader-program":
            errors.append(f"schema:{relative}")
        if (
            document.get("version") != 1
            or document.get("programKind") != "graphics"
            or document.get("allowedPermutationFlags") != []
            or document.get("requiredExtensions") != []
        ):
            errors.append(f"program-contract:{relative}")
        parent = path.parent
        stage_names = {
            stage.get("stage")
            for stage in document.get("stages", [])
        }
        if stage_names != {"vertex", "fragment"}:
            errors.append(f"stage-set:{relative}")
        for stage in document.get("stages", []):
            source = stage.get("source", {})
            _verify_dependency(
                relative,
                parent,
                source,
                "ShaderSource",
                stage.get("stage"),
                stage.get("entryPoint"),
                dependency_ids,
                destination_ids,
                owned_files,
                errors,
            )
        for variant in document.get("variants", []):
            if variant.get("name") != "default" or variant.get("flags") != []:
                errors.append(f"variant-contract:{relative}")
            for payload in variant.get("payloads", []):
                _verify_dependency(
                    relative,
                    parent,
                    payload,
                    "ShaderPayload",
                    payload.get("stage"),
                    payload.get("entryPoint"),
                    dependency_ids,
                    destination_ids,
                    owned_files,
                    errors,
                )
        if len(document.get("variants", [])) != 1:
            errors.append(f"variant-count:{relative}")

    actual_files = {
        path
        for path in (root / "Content/Shaders").rglob("*")
        if path.is_file()
        and not path.name.endswith(".shader.json")
    }
    if owned_files != actual_files:
        errors.append("dependency-inventory")
    if len([p for p in owned_files if p.suffix == ".spv"]) != 11:
        errors.append("spirv-count")
    if len([p for p in owned_files if p.suffix in {".vert", ".frag"}]) != 11:
        errors.append("source-count")

    point = (
        "ShaderPayload",
        "Engine/Shaders/Deferred/PointLight",
        "payload.vulkan.vertex",
    )
    spot = (
        "ShaderPayload",
        "Engine/Shaders/Deferred/SpotLight",
        "payload.vulkan.vertex",
    )
    if (
        point not in dependency_ids
        or spot not in dependency_ids
        or dependency_ids.get(point, ("", ""))[1]
        != dependency_ids.get(spot, ("!", "!"))[1]
    ):
        errors.append("point-spot-identity")

    for obsolete in (
        root / "Demo/StonerDemo/Shaders",
        root / "Source/Renderer/Shaders/Deferred",
    ):
        if obsolete.exists() and any(obsolete.iterdir()):
            errors.append("obsolete-shader-directory")

    backend = root / "Source/Backend"
    if backend.exists():
        for path in sorted(backend.rglob("*")):
            if path.suffix not in {".h", ".cpp"}:
                continue
            text = path.read_text(encoding="utf-8")
            if (
                ".spv" in text
                or "ShaderDirectory" in text
                or "ShaderPath" in text
                or "ReadSpirv" in text
            ):
                errors.append(
                    "backend-direct-shader-path:"
                    + path.relative_to(root).as_posix()
                )
    return sorted(set(errors))


def write_report(path: Path, errors: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "feature=023",
        "programs=6",
        "sources=11",
        "payloads=11",
        "result=" + ("pass" if not errors else "fail"),
    ]
    lines.extend(
        f"error.{index:03d}={error}"
        for index, error in enumerate(errors)
    )
    path.write_text("\n".join(lines) + "\n", encoding="utf-8", newline="\n")


def _verify_dependency(
    definition: str,
    parent: Path,
    record: dict,
    expected_type: str,
    stage: str | None,
    entry: str | None,
    identities: dict[tuple[str, str, str], tuple[str, str]],
    destinations: dict[Path, tuple[str, str, str]],
    owned_files: set[Path],
    errors: list[str],
) -> None:
    identity = asset_key(record.get("asset", {}))
    locator = record.get("locator", "")
    digest = record.get("digest", "")
    if identity[0] != expected_type:
        errors.append(f"dependency-type:{definition}:{locator}")
    expected_subresource = (
        f"source.{stage}"
        if expected_type == "ShaderSource"
        else f"payload.vulkan.{stage}"
    )
    if identity[2] != expected_subresource:
        errors.append(f"dependency-subresource:{definition}:{locator}")
    if stage not in {"vertex", "fragment"} or entry != "main":
        errors.append(f"dependency-stage-entry:{definition}:{locator}")
    if expected_type == "ShaderSource":
        if record.get("language") not in (None, "glsl"):
            errors.append(f"dependency-language:{definition}:{locator}")
    elif (
        record.get("backend") != "vulkan"
        or record.get("profile") != "vulkan-1.3"
        or record.get("format") != "spirv"
        or record.get("producer") != "Stoner.CheckedInSpirv"
        or record.get("producerVersion") != "023-v1"
    ):
        errors.append(f"dependency-target:{definition}:{locator}")
    target = parent / locator
    if (
        not locator
        or "\\" in locator
        or Path(locator).is_absolute()
        or ".." in Path(locator).parts
    ):
        errors.append(f"dependency-locator:{definition}")
        return
    if not target.is_file():
        errors.append(f"dependency-missing:{definition}:{locator}")
        return
    owned_files.add(target)
    previous_destination = destinations.get(target)
    if previous_destination is not None and previous_destination != identity:
        errors.append(f"dependency-destination-conflict:{definition}:{locator}")
    destinations[target] = identity
    actual = "sha256:" + sha256(target)
    if digest != actual:
        errors.append(f"dependency-digest:{definition}:{locator}")
    fact = (locator, digest)
    previous = identities.get(identity)
    if previous is not None and previous != fact:
        errors.append(f"dependency-identity-conflict:{definition}:{locator}")
    identities[identity] = fact


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path("."))
    parser.add_argument("--report", type=Path)
    args = parser.parse_args()
    errors = verify(args.root.resolve())
    if args.report is not None:
        write_report(args.report, errors)
    if errors:
        for error in errors:
            print("ERROR " + error)
        return 1
    print("Repository shader assets passed: programs=6 sources=11 payloads=11")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
