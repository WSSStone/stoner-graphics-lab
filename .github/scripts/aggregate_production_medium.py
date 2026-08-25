#!/usr/bin/env python3

import argparse
import hashlib
import json
from pathlib import Path


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def load_json(path: Path) -> dict:
    value = json.loads(path.read_text(encoding="utf-8"))
    if not isinstance(value, dict):
        raise ValueError(f"JSON root is not an object: {path}")
    return value


def aggregate_medium_shards(
    root: Path, profile_path: Path, target_profile: Path
) -> dict:
    profile = load_json(profile_path)
    expected_packages = profile.get("packageIds")
    if (
        profile.get("profileId") != "medium"
        or profile.get("lifecycleCycles") != 1000
        or profile.get("warmupCycles") != 20
        or profile.get("timeBudgetSeconds") != 1800
        or not isinstance(expected_packages, list)
        or len(expected_packages) < 2
        or len(set(expected_packages)) != len(expected_packages)
    ):
        raise ValueError("medium profile contract is invalid")

    summary_paths = sorted(root.rglob("summary.json"))
    if len(summary_paths) != len(expected_packages):
        raise ValueError("medium shard summary count is incomplete")
    target_digest = sha256_file(target_profile)
    common = None
    records = []
    observed_packages = set()
    for summary_path in summary_paths:
        summary = load_json(summary_path)
        packages = summary.get("packages")
        if (
            summary.get("schema") !=
                "stoner.production-cook-runtime-summary"
            or summary.get("schemaVersion") != 1
            or summary.get("profile") != "medium"
            or summary.get("passed") is not True
            or summary.get("determinismRuns") != 1
            or summary.get("timeBudgetSeconds") != 1800
            or not isinstance(summary.get("elapsedSeconds"), (int, float))
            or summary["elapsedSeconds"] > 1800
            or summary.get("targetProfileDigest") != target_digest
            or not isinstance(packages, list)
            or len(packages) != 1
        ):
            raise ValueError("medium shard summary contract is invalid")
        package = packages[0]
        native = package.get("nativeLifecycle")
        package_id = package.get("packageId")
        if (
            package_id not in expected_packages
            or package_id in observed_packages
            or package.get("cleanRuns") != 1
            or package.get("reachableAssets") != package.get("reusedAssets")
            or not isinstance(native, dict)
            or native.get("result") != "Passed"
            or native.get("lifecycleCycles") != 1000
            or native.get("warmupCycles") != 20
            or native.get("ownersAtTerminal") != 0
            or native.get("staleHandleRejected") is not True
            or native.get("captureCount") != 2000
            or native.get("readbackCount") != 7
            or native.get("rssGrowthBytes", 16 * 1024 * 1024 + 1) >
                16 * 1024 * 1024
        ):
            raise ValueError("medium shard package evidence is invalid")
        manifest_path = summary_path.with_name("artifact-manifest.json")
        if not manifest_path.is_file():
            raise ValueError("medium shard artifact manifest is missing")
        identity = (
            summary.get("corpusRevision"),
            summary.get("corpusDigest"),
            summary.get("targetProfile"),
            summary.get("targetProfileDigest"),
        )
        if common is None:
            common = identity
        elif identity != common:
            raise ValueError("medium shard authority differs across lanes")
        observed_packages.add(package_id)
        records.append({
            "packageId": package_id,
            "workloadRevision": package.get("workloadRevision"),
            "generationId": package.get("generationId"),
            "elapsedSeconds": summary["elapsedSeconds"],
            "nativeSeconds": native.get("seconds"),
            "rssGrowthBytes": native.get("rssGrowthBytes"),
            "summaryDigest": sha256_file(summary_path),
            "artifactManifestDigest": sha256_file(manifest_path),
        })
    if observed_packages != set(expected_packages):
        raise ValueError("medium shard package set is incomplete")
    records.sort(key=lambda item: expected_packages.index(item["packageId"]))
    assert common is not None
    return {
        "schema": "stoner.production-medium-aggregate",
        "schemaVersion": 1,
        "profile": "medium",
        "corpusRevision": common[0],
        "corpusDigest": common[1],
        "targetProfile": common[2],
        "targetProfileDigest": common[3],
        "packageIds": expected_packages,
        "maximumLaneSeconds": max(
            record["elapsedSeconds"] for record in records
        ),
        "packages": records,
        "passed": True,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--profile", type=Path, required=True)
    parser.add_argument("--target-profile", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    result = aggregate_medium_shards(
        args.root, args.profile, args.target_profile
    )
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(
        json.dumps(result, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
