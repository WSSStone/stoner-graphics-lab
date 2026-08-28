#!/usr/bin/env python3
"""Verify Feature 024 pinned parser, tangent source, and fixture metadata."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path


VENDORS = {
    "cgltf": ("1.15", "8211a9f12a729e7c2998bcc68f43c2ed2e5462d9"),
    "mikktspace": ("1.0", "3e895b49d05ea07e4c2133156cfa94369e19e409"),
}
FIXTURE_SCHEMA = "stoner.static-model.fixture-manifest/v1"
FIXTURE_FIELDS = {
    "path",
    "source_url",
    "upstream_revision",
    "sha256",
    "license",
    "validator_result",
    "expected_result",
    "scope",
}
GENERATED_VENDOR_SUFFIXES = {".o", ".obj"}


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify_vendor(directory: Path, version: str, revision: str) -> list[str]:
    errors: list[str] = []
    if not directory.is_dir():
        return [f"missing vendor directory: {directory}"]
    version_path = directory / "VERSION"
    if not version_path.is_file() or version_path.read_text(encoding="ascii").strip() != version:
        errors.append(f"{directory.name}: version mismatch")
    upstream = directory / "UPSTREAM.md"
    if not upstream.is_file() or revision not in upstream.read_text(encoding="utf-8"):
        errors.append(f"{directory.name}: upstream revision mismatch")
    manifest = directory / "SHA256SUMS"
    if not manifest.is_file():
        return errors + [f"{directory.name}: missing SHA256SUMS"]
    listed: set[Path] = set()
    for line_number, line in enumerate(manifest.read_text(encoding="ascii").splitlines(), 1):
        try:
            expected, relative_text = line.split("  ", 1)
        except ValueError:
            errors.append(f"{directory.name}: malformed hash entry {line_number}")
            continue
        relative = Path(relative_text)
        listed.add(relative)
        target = directory / relative
        if len(expected) != 64 or any(char not in "0123456789abcdef" for char in expected):
            errors.append(f"{directory.name}: invalid SHA-256 for {relative.as_posix()}")
        elif not target.is_file():
            errors.append(f"{directory.name}: missing {relative.as_posix()}")
        elif sha256(target) != expected:
            errors.append(f"{directory.name}: checksum mismatch: {relative.as_posix()}")
    actual = {
        path.relative_to(directory)
        for path in directory.iterdir()
        if path.is_file() and path.name != "SHA256SUMS" and
        path.suffix not in GENERATED_VENDOR_SUFFIXES
    }
    for relative in sorted(actual - listed):
        errors.append(f"{directory.name}: unlisted {relative.as_posix()}")
    for relative in sorted(listed - actual):
        errors.append(f"{directory.name}: stale {relative.as_posix()}")
    return errors


def verify_fixture_manifest(path: Path) -> list[str]:
    if not path.is_file():
        return [f"missing fixture manifest: {path}"]
    try:
        document = json.loads(path.read_text(encoding="utf-8"))
    except json.JSONDecodeError as error:
        return [f"fixture manifest is invalid JSON: {error.msg}"]
    if document.get("schema") != FIXTURE_SCHEMA:
        return ["fixture manifest schema mismatch"]
    fixtures = document.get("fixtures")
    if not isinstance(fixtures, list):
        return ["fixture manifest fixtures must be an array"]
    errors: list[str] = []
    root = path.parents[3]
    listed_paths: set[str] = set()
    for index, fixture in enumerate(fixtures):
        if not isinstance(fixture, dict):
            errors.append(f"fixture {index}: entry must be an object")
            continue
        missing = sorted(FIXTURE_FIELDS - fixture.keys())
        if missing:
            errors.append(f"fixture {index}: missing fields: {', '.join(missing)}")
        digest = fixture.get("sha256")
        if not isinstance(digest, str) or len(digest.removeprefix("sha256:")) != 64:
            errors.append(f"fixture {index}: invalid SHA-256")
        relative_text = fixture.get("path")
        if not isinstance(relative_text, str) or not relative_text:
            errors.append(f"fixture {index}: invalid path")
            continue
        relative = Path(relative_text)
        if relative.is_absolute() or ".." in relative.parts:
            errors.append(f"fixture {index}: path escapes repository")
            continue
        normalized = relative.as_posix()
        if normalized in listed_paths:
            errors.append(f"fixture {index}: duplicate path: {normalized}")
            continue
        listed_paths.add(normalized)
        target = root / relative
        if not target.is_file():
            errors.append(f"fixture {index}: missing file: {normalized}")
        elif isinstance(digest, str) and len(digest.removeprefix("sha256:")) == 64:
            if sha256(target) != digest.removeprefix("sha256:"):
                errors.append(f"fixture {index}: checksum mismatch: {normalized}")
        scope = fixture.get("scope")
        if not isinstance(scope, list) or not scope or not all(
            isinstance(value, str) and value for value in scope
        ):
            errors.append(f"fixture {index}: invalid scope")
    return errors


def verify(root: Path) -> list[str]:
    errors: list[str] = []
    for name, (version, revision) in VENDORS.items():
        errors.extend(verify_vendor(root / "ThirdParty" / name, version, revision))
    errors.extend(verify_fixture_manifest(root / "Tests" / "Fixtures" / "StaticModel" / "fixture-manifest.json"))
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    errors = verify(args.root.resolve())
    if errors:
        print("\n".join(f"ERROR: {error}" for error in errors))
        return 1
    print("Feature 024 static-model provenance: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
