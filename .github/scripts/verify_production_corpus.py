#!/usr/bin/env python3
"""Verify Feature 028 corpus provenance, inventory, and coverage."""

import argparse
import hashlib
import json
from pathlib import Path
import sys


SCRIPT_DIR = Path(__file__).parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from production_content_manifest import (  # noqa: E402
    ManifestError, canonical_bytes, canonical_digest, load_json,
    validate_manifest, verify_package,
)


def _validate_coverage_file(manifest_path, document):
    path = Path(manifest_path).with_name("coverage-v1.json")
    if not path.exists():
        return None
    coverage = load_json(path)
    expected_fields = {"schema", "schemaVersion", "corpusRevision", "coverageClaims"}
    if set(coverage) != expected_fields:
        raise ManifestError("unknown-field", "coverage-v1.json", sorted(expected_fields), sorted(coverage))
    if coverage["schema"] != "stoner.production-coverage" or coverage["schemaVersion"] != 1:
        raise ManifestError("unsupported-schema", "coverage-v1.json", "stoner.production-coverage v1", f"{coverage.get('schema')} v{coverage.get('schemaVersion')}")
    if coverage["corpusRevision"] != document["corpusRevision"]:
        raise ManifestError("coverage-closure", "coverage-v1.json", document["corpusRevision"], coverage["corpusRevision"])
    if coverage["coverageClaims"] != document["coverageClaims"]:
        raise ManifestError("coverage-closure", "coverage-v1.json", "exact manifest coverageClaims", "mismatch")
    return hashlib.sha256(canonical_bytes(coverage)).hexdigest()


def verify_manifest(manifest_path, content_root, tiers=None, package_ids=None):
    manifest_path = Path(manifest_path)
    content_root = Path(content_root)
    try:
        document = validate_manifest(load_json(manifest_path))
        coverage_digest = _validate_coverage_file(manifest_path, document)
        selected_tiers = set(tiers) if tiers else None
        selected_ids = set(package_ids) if package_ids else None
        selected = [
            package for package in document["packages"]
            if (selected_tiers is None or package["tier"] in selected_tiers)
            and (selected_ids is None or package["packageId"] in selected_ids)
        ]
        if not selected:
            raise ManifestError("empty-selection", "manifest.packages", "at least one selected package", "none")
        summaries = [
            verify_package(package, content_root / package["packageRoot"])
            for package in selected
        ]
        canonical = canonical_bytes(document)
        return {
            "result": "Passed",
            "corpusRevision": document["corpusRevision"],
            "manifestDigest": canonical_digest(document),
            "coverageDigest": coverage_digest,
            "canonicalJson": canonical.decode("utf-8"),
            "packages": summaries,
            "firstFailure": None,
        }
    except ManifestError as error:
        return {
            "result": "Failed",
            "corpusRevision": None,
            "manifestDigest": None,
            "coverageDigest": None,
            "canonicalJson": None,
            "packages": [],
            "firstFailure": error.as_dict(),
        }


def parse_args(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=Path("Content/ProductionAcceptance/Corpus/corpus-v1.json"))
    parser.add_argument("--content-root", type=Path, default=Path("Content/ProductionAcceptance"))
    parser.add_argument("--tier", choices=("regular", "medium", "all"), default="regular")
    parser.add_argument("--output", type=Path)
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    tiers = None if args.tier == "all" else {args.tier}
    result = verify_manifest(args.manifest, args.content_root, tiers=tiers)
    output = dict(result)
    output.pop("canonicalJson", None)
    rendered = json.dumps(output, ensure_ascii=False, sort_keys=True, separators=(",", ":")) + "\n"
    if args.output:
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(rendered, encoding="utf-8")
    else:
        print(rendered, end="")
    return 0 if result["result"] == "Passed" else 1


if __name__ == "__main__":
    raise SystemExit(main())
