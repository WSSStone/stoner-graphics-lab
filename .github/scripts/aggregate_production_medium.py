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
    expected_lifecycles = profile.get("packageLifecycles")
    expected_policy = {
        "allowedExecutionClasses": [
            "github-hosted", "local-diagnostic"
        ],
        "executionClasses": {
            "github-hosted": {
                "rss": "observed", "timing": "operational",
                "image": "not-required",
            },
            "local-diagnostic": {
                "rss": "observed", "timing": "operational",
                "image": "not-required",
            },
        },
        "physicalPreflight": [],
    }
    if (
        profile.get("schema") != "stoner.production-validation-profile"
        or profile.get("schemaVersion") != 4
        or profile.get("profileId") != "medium"
        or profile.get("timeBudgetSeconds") != 2400
        or profile.get("profileTimeBudgetSeconds") != 2700
        or profile.get("nativeTimeBudgetSeconds") != 1800
        or profile.get("authorityPolicy") != expected_policy
        or not isinstance(expected_packages, list)
        or len(expected_packages) < 2
        or len(set(expected_packages)) != len(expected_packages)
        or not isinstance(expected_lifecycles, list)
        or not all(isinstance(item, dict) for item in expected_lifecycles)
        or [item.get("packageId") for item in expected_lifecycles]
            != expected_packages
    ):
        raise ValueError("medium profile contract is invalid")
    lifecycle_by_package = {
        item["packageId"]: item for item in expected_lifecycles
    }
    if (
        len(lifecycle_by_package) != len(expected_packages)
        or lifecycle_by_package.get("khronos-lantern-glb") != {
            "packageId": "khronos-lantern-glb",
            "purpose": "endurance",
            "cycles": 1000,
            "warmupCycles": 20,
        }
        or lifecycle_by_package.get("khronos-sponza-gltf") != {
            "packageId": "khronos-sponza-gltf",
            "purpose": "scale-lifecycle",
            "cycles": 100,
            "warmupCycles": 10,
        }
    ):
        raise ValueError("medium profile lifecycle split is invalid")

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
            or summary.get("executionClass") != "github-hosted"
            or summary.get("passed") is not True
            or summary.get("determinismRuns") != 1
            or summary.get("timeBudgetSeconds") != 2400
            or summary.get("profileTimeBudgetSeconds") != 2700
            or summary.get("nativeTimeBudgetSeconds") != 1800
            or not isinstance(summary.get("elapsedSeconds"), (int, float))
            or summary["elapsedSeconds"] > 2700
            or summary.get("targetProfileDigest") != target_digest
            or not isinstance(packages, list)
            or len(packages) != 1
        ):
            raise ValueError("medium shard summary contract is invalid")
        package = packages[0]
        native = package.get("nativeLifecycle")
        package_id = package.get("packageId")
        lifecycle = lifecycle_by_package.get(package_id)
        if (
            package_id not in expected_packages
            or package_id in observed_packages
            or package.get("cleanRuns") != 1
            or package.get("reachableAssets") != package.get("reusedAssets")
            or not isinstance(lifecycle, dict)
            or package.get("lifecyclePurpose") != lifecycle["purpose"]
            or not isinstance(native, dict)
            or native.get("result") != "Passed"
            or native.get("executionClass") != "github-hosted"
            or native.get("lifecycleCycles") != lifecycle["cycles"]
            or native.get("warmupCycles") != lifecycle["warmupCycles"]
            or native.get("ownersAtTerminal") != 0
            or native.get("staleHandleRejected") is not True
            or native.get("captureCount") != lifecycle["cycles"] * 2
            or native.get("readbackCount") != 7
            or not isinstance(native.get("seconds"), (int, float))
            or native["seconds"] > 1800
            or native.get("rssDisposition") != "observed"
            or native.get("timingDisposition") != "operational"
            or native.get("imageDisposition") != "not-required"
            or not isinstance(native.get("rssGrowthBytes"), int)
            or native["rssGrowthBytes"] < 0
            or not isinstance(native.get("rssWithinLimit"), bool)
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
            "lifecyclePurpose": lifecycle["purpose"],
            "lifecycleCycles": lifecycle["cycles"],
            "warmupCycles": lifecycle["warmupCycles"],
            "workloadRevision": package.get("workloadRevision"),
            "generationId": package.get("generationId"),
            "elapsedSeconds": summary["elapsedSeconds"],
            "nativeSeconds": native.get("seconds"),
            "rssGrowthBytes": native.get("rssGrowthBytes"),
            "rssWithinLimit": native.get("rssWithinLimit"),
            "rssDisposition": native.get("rssDisposition"),
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
        "executionClass": "github-hosted",
        "measurementDispositions": expected_policy["executionClasses"][
            "github-hosted"
        ],
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
