#!/usr/bin/env python3
"""Verify Feature 025 fixture provenance, integrity, and corpus coverage."""

from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import sys
from collections import Counter
from typing import Any

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import verify_asset_cooker_contracts as contracts


SHA256 = re.compile(r"^[0-9a-f]{64}$")
REQUIRED_FIELDS = {
    "path", "sha256", "provenance", "license", "expectedValidity",
    "assetTypes", "targetProfiles", "expectedOutcomes",
}
CORPUS_DIRECTORIES = {
    "representative": "Representative",
    "mutation": "Mutation",
    "corrupt-cache": "CorruptCache",
    "corrupt-published": "CorruptPublished",
    "concurrency": "Concurrency",
    "scale": "Scale",
}


def _digest(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def validate_fixture_record(root: pathlib.Path, fixture: Any) -> list[str]:
    if not isinstance(fixture, dict):
        return ["fixture record is not an object"]
    errors: list[str] = []
    missing = REQUIRED_FIELDS - fixture.keys()
    if missing:
        errors.append(f"fixture missing fields: {sorted(missing)}")
        return errors
    relative = fixture["path"]
    if not isinstance(relative, str):
        return ["fixture path is not text"]
    pure = pathlib.PurePosixPath(relative)
    if pure.is_absolute() or ".." in pure.parts:
        errors.append(f"unsafe fixture path: {relative}")
        return errors
    path = root / relative
    if not path.is_file():
        errors.append(f"missing fixture: {relative}")
    elif not isinstance(fixture["sha256"], str) or not SHA256.fullmatch(fixture["sha256"]):
        errors.append(f"invalid SHA-256: {relative}")
    elif _digest(path) != fixture["sha256"]:
        errors.append(f"fixture checksum mismatch: {relative}")
    for name in ("provenance", "license"):
        if not isinstance(fixture[name], str) or not fixture[name].strip():
            errors.append(f"empty {name}: {relative}")
    if fixture["expectedValidity"] not in {"valid", "invalid"}:
        errors.append(f"invalid expectedValidity: {relative}")
    for name in ("assetTypes", "targetProfiles", "expectedOutcomes"):
        value = fixture[name]
        if not isinstance(value, list) or not value or not all(
            isinstance(item, str) and item for item in value
        ):
            errors.append(f"invalid {name}: {relative}")
    return errors


def validate_corpus_coverage(
    corpora: Any, fixture_paths: list[str],
) -> list[str]:
    errors: list[str] = []
    if not isinstance(corpora, list):
        return ["corpora is not an array"]
    counts: Counter[str] = Counter()
    for relative in fixture_paths:
        parts = pathlib.PurePosixPath(relative).parts
        for corpus, directory in CORPUS_DIRECTORIES.items():
            if directory in parts:
                counts[corpus] += 1
    seen: set[str] = set()
    for corpus in corpora:
        if not isinstance(corpus, dict) or set(corpus) != {"id", "minimumCases"}:
            errors.append("invalid corpus declaration")
            continue
        identifier = corpus["id"]
        minimum = corpus["minimumCases"]
        if identifier in seen or identifier not in CORPUS_DIRECTORIES:
            errors.append(f"unknown or duplicate corpus: {identifier}")
            continue
        seen.add(identifier)
        if not isinstance(minimum, int) or minimum <= 0 or counts[identifier] < minimum:
            errors.append(
                f"corpus coverage shortfall: {identifier} "
                f"{counts[identifier]}/{minimum}"
            )
    if seen != set(CORPUS_DIRECTORIES):
        errors.append("corpus declarations are incomplete")
    return errors


def _tracked_fixture_files(root: pathlib.Path) -> set[str]:
    fixture_root = root / "Tests/Fixtures/AssetCooker"
    ignored_names = {"README.md", "generate_scale_corpus.py"}
    return {
        path.relative_to(root).as_posix()
        for path in fixture_root.rglob("*")
        if path.is_file() and path.name not in ignored_names and
        "__pycache__" not in path.parts and path.suffix != ".pyc"
    }


def verify(root: pathlib.Path) -> list[str]:
    manifest_path = root / "Validation/025/fixture-manifest.json"
    try:
        manifest = json.loads(manifest_path.read_bytes())
    except (OSError, json.JSONDecodeError) as error:
        return [f"invalid fixture manifest: {error}"]
    errors: list[str] = []
    if manifest.get("schema") != "stoner.asset-cooker-fixture-manifest" or \
            manifest.get("schemaVersion") != 1 or \
            manifest.get("feature") != "025-asset-cooker-derived-data":
        errors.append("invalid fixture manifest header")
    if set(manifest.get("requiredFixtureFields", [])) != REQUIRED_FIELDS:
        errors.append("requiredFixtureFields does not match verifier contract")
    fixtures = manifest.get("fixtures")
    if not isinstance(fixtures, list):
        return errors + ["fixtures is not an array"]
    paths: list[str] = []
    seen: set[str] = set()
    for fixture in fixtures:
        errors.extend(validate_fixture_record(root, fixture))
        if isinstance(fixture, dict) and isinstance(fixture.get("path"), str):
            relative = fixture["path"]
            if relative in seen:
                errors.append(f"duplicate fixture path: {relative}")
            seen.add(relative)
            paths.append(relative)
    errors.extend(validate_corpus_coverage(manifest.get("corpora"), paths))
    untracked = _tracked_fixture_files(root) - seen
    if untracked:
        errors.append(f"unregistered fixture files: {sorted(untracked)}")

    scale_path = root / "Tests/Fixtures/AssetCooker/Scale/scale-1000-5000.json"
    try:
        scale = json.loads(scale_path.read_bytes())
        assets = scale["assets"]
        edges = sum(len(asset["dependencies"]) for asset in assets)
        if scale.get("schema") != "stoner.asset-cooker-scale-corpus" or \
                len(assets) != 1_000 or edges != 5_000 or \
                scale.get("maximumDependencyDepth") > 256:
            errors.append("scale corpus shape does not satisfy Feature 025")
    except (OSError, KeyError, TypeError, json.JSONDecodeError) as error:
        errors.append(f"invalid scale corpus: {error}")

    report_root = root / "Tests/Fixtures/AssetCooker/Reports"
    for name, result in (
        ("valid-plan.json", "success"),
        ("invalid-profile.json", "invalid-profile"),
    ):
        try:
            report = contracts.parse_report((report_root / name).read_bytes())
            if report.get("result") != result:
                errors.append(f"unexpected report result: {name}")
        except (OSError, ValueError, json.JSONDecodeError) as error:
            errors.append(f"invalid report fixture {name}: {error}")

    try:
        profile = json.loads((
            root / "Config/AssetCooker/Profiles/Mac-Vulkan.json"
        ).read_bytes())
        effective = contracts.digest(
            contracts.canonical_profile(profile, include_display=False).encode("utf-8")
        )
        plan = json.loads((report_root / "valid-plan.json").read_bytes())
        if plan.get("effectiveProfileDigest") != effective:
            errors.append("plan report profile digest does not match canonical profile")
    except (OSError, KeyError, json.JSONDecodeError) as error:
        errors.append(f"unable to verify report profile evidence: {error}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root", type=pathlib.Path,
        default=pathlib.Path(__file__).resolve().parents[1],
    )
    args = parser.parse_args()
    errors = verify(args.root.resolve())
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print("Asset cooker fixture provenance and corpus coverage: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
