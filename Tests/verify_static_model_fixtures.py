#!/usr/bin/env python3
"""Verify the complete, offline Feature 024 static-model fixture corpus."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys


SCHEMA = "stoner.static-model.fixture-manifest/v1"
FIXTURE_SUFFIXES = {".bin", ".glb", ".gltf", ".hdr", ".jpeg", ".jpg", ".png"}
REQUIRED_FIELDS = {
    "expected_result", "license", "path", "scope", "sha256",
    "source_url", "upstream_revision", "validator_result",
}
EXPECTED_RESULTS = {"dependency", "failure", "success"}
VALIDATOR_RESULTS = {"invalid", "malformed", "unsupported", "valid"}


def _sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def verify(manifest_path: Path, fixture_root: Path) -> list[str]:
    errors: list[str] = []
    if not manifest_path.is_file():
        return [f"missing manifest: {manifest_path}"]
    if not fixture_root.is_dir():
        return [f"missing fixture directory: {fixture_root}"]
    try:
        document = json.loads(manifest_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        return [f"invalid manifest JSON: {error}"]
    if document.get("schema") != SCHEMA:
        errors.append(f"schema must be {SCHEMA}")
    entries = document.get("fixtures")
    if not isinstance(entries, list):
        return errors + ["fixtures must be an array"]

    repository_root = fixture_root.resolve().parents[2]
    fixture_root = fixture_root.resolve()
    listed: set[str] = set()
    success_count = 0
    failure_count = 0
    golden_paths: set[str] = set()
    for index, entry in enumerate(entries):
        label = f"fixture {index}"
        if not isinstance(entry, dict):
            errors.append(f"{label}: entry must be an object")
            continue
        missing = sorted(REQUIRED_FIELDS - entry.keys())
        if missing:
            errors.append(f"{label}: missing fields: {', '.join(missing)}")
        path_text = entry.get("path")
        if not isinstance(path_text, str) or not path_text:
            errors.append(f"{label}: invalid path")
            continue
        relative = Path(path_text)
        if relative.is_absolute() or ".." in relative.parts:
            errors.append(f"{label}: path escapes repository")
            continue
        target = (repository_root / relative).resolve()
        try:
            target.relative_to(fixture_root)
        except ValueError:
            errors.append(f"{label}: path is outside fixture directory: {path_text}")
            continue
        normalized = target.relative_to(repository_root).as_posix()
        if normalized in listed:
            errors.append(f"{label}: duplicate path: {normalized}")
        listed.add(normalized)
        if not target.is_file():
            errors.append(f"{label}: missing file: {normalized}")

        digest = entry.get("sha256")
        if (not isinstance(digest, str) or not digest.startswith("sha256:")
                or len(digest) != 71
                or any(character not in "0123456789abcdef" for character in digest[7:])):
            errors.append(f"{label}: invalid SHA-256")
        elif target.is_file() and _sha256(target) != digest[7:]:
            errors.append(f"{label}: checksum mismatch: {normalized}")
        for field in ("license", "source_url", "upstream_revision"):
            if not isinstance(entry.get(field), str) or not entry[field].strip():
                errors.append(f"{label}: invalid {field}")
        source_url = entry.get("source_url")
        if isinstance(source_url, str) and "://" not in source_url:
            errors.append(f"{label}: source_url must identify provenance")

        expected = entry.get("expected_result")
        validator = entry.get("validator_result")
        scope = entry.get("scope")
        if expected not in EXPECTED_RESULTS:
            errors.append(f"{label}: invalid expected_result")
        if validator not in VALIDATOR_RESULTS:
            errors.append(f"{label}: invalid validator_result")
        if not isinstance(scope, list) or not scope or not all(
                isinstance(value, str) and value.strip() for value in scope):
            errors.append(f"{label}: invalid scope")
            scope = []
        if expected in {"success", "dependency"} and validator != "valid":
            errors.append(f"{label}: successful input must be validator-valid")
        if expected == "success":
            success_count += 1
        elif expected == "failure":
            failure_count += 1
        if expected == "success" and "SC-004" in scope:
            golden_paths.add(normalized)
        if "/Invalid/Hardening/" in normalized:
            for field in ("mutation", "mutation_base"):
                if not isinstance(entry.get(field), str) or not entry[field].strip():
                    errors.append(f"{label}: hardening mutation missing {field}")
        if "performance" in scope:
            generator_text = entry.get("generator")
            generator_digest = entry.get("generator_sha256")
            shape = entry.get("performance_shape")
            if not isinstance(generator_text, str) or not generator_text:
                errors.append(f"{label}: performance fixture missing generator")
            else:
                generator = (repository_root / generator_text).resolve()
                if not generator.is_file():
                    errors.append(f"{label}: missing generator: {generator_text}")
                elif generator_digest != f"sha256:{_sha256(generator)}":
                    errors.append(f"{label}: generator checksum mismatch")
            minimums = {
                "vertices": 100000, "indices": 300000,
                "primitives": 16, "materials": 16,
            }
            if not isinstance(shape, dict) or any(
                not isinstance(shape.get(field), int) or shape[field] < minimum
                for field, minimum in minimums.items()
            ):
                errors.append(f"{label}: invalid performance shape")

    actual = {
        path.relative_to(repository_root).as_posix()
        for path in fixture_root.rglob("*")
        if path.is_file() and path.suffix.lower() in FIXTURE_SUFFIXES
    }
    for path in sorted(actual - listed):
        errors.append(f"unlisted fixture: {path}")
    for path in sorted(listed - actual):
        errors.append(f"stale manifest path: {path}")
    if success_count < 20:
        errors.append(f"valid fixture count {success_count} is below 20")
    if failure_count < 40:
        errors.append(f"malformed fixture count {failure_count} is below 40")
    if len(golden_paths) < 12:
        errors.append(f"SC-004 golden primitive count {len(golden_paths)} is below 12")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    root = Path(__file__).resolve().parents[1]
    parser.add_argument("--manifest", type=Path,
                        default=root / "Tests/Fixtures/StaticModel/fixture-manifest.json")
    parser.add_argument("--fixtures", type=Path,
                        default=root / "Tests/Fixtures/StaticModel")
    args = parser.parse_args()
    errors = verify(args.manifest.resolve(), args.fixtures.resolve())
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print("Feature 024 static-model fixture corpus: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
