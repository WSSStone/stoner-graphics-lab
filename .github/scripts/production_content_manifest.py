#!/usr/bin/env python3
"""Canonical Feature 028 production-corpus model and integrity checks."""

import datetime
import hashlib
import json
import os
from pathlib import Path, PurePosixPath, PureWindowsPath
import re
import unicodedata


TOP_LEVEL_FIELDS = {
    "schema", "schemaVersion", "corpusRevision", "packages", "coverageClaims",
}
PACKAGE_FIELDS = {
    "packageId", "workName", "packageName", "publisher", "sourceLocation",
    "revision", "acquiredOn", "tier", "packageRoot", "sourcePath",
    "rootAssetId", "rootFile", "files", "coverageClaims",
}
PACKAGE_REQUIRED_FIELDS = PACKAGE_FIELDS - {"publisher"}
FILE_FIELDS = {"path", "sha256", "sizeBytes", "role"}
CLAIM_FIELDS = {"claimId", "packageId", "subject", "evidence"}
PACKAGE_ID_PATTERN = re.compile(r"^[a-z0-9]+(?:-[a-z0-9]+)*$")
CORPUS_REVISION_PATTERN = re.compile(r"^[a-z0-9][a-z0-9.-]{0,63}$")
SHA256_PATTERN = re.compile(r"^[0-9a-f]{64}$")
ASSET_ID_PATTERN = re.compile(r"^StaticModel:[^\s]+$")
FILE_ROLES = {"model", "buffer", "image", "other-source"}
TIERS = {"regular", "medium"}


class ManifestError(ValueError):
    def __init__(self, category, subject, expected="", observed=""):
        self.category = category
        self.subject = subject
        self.expected = str(expected)
        self.observed = str(observed)
        super().__init__(f"{category}: {subject}")

    def as_dict(self):
        return {
            "category": self.category,
            "subject": self.subject,
            "expected": self.expected,
            "observed": self.observed,
        }


def _object_without_duplicate_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ManifestError("duplicate-field", key, "unique field", "duplicate")
        result[key] = value
    return result


def load_json(path):
    try:
        return json.loads(
            Path(path).read_text(encoding="utf-8"),
            object_pairs_hook=_object_without_duplicate_keys,
        )
    except ManifestError:
        raise
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise ManifestError("manifest-unreadable", str(path), "canonical UTF-8 JSON", type(error).__name__) from error


def canonical_bytes(document):
    return (json.dumps(
        document, ensure_ascii=False, sort_keys=True, separators=(",", ":"),
    ) + "\n").encode("utf-8")


def canonical_digest(document):
    return hashlib.sha256(canonical_bytes(document)).hexdigest()


def _require_object(value, subject):
    if not isinstance(value, dict):
        raise ManifestError("invalid-type", subject, "object", type(value).__name__)


def _require_exact_fields(value, allowed, required, subject):
    _require_object(value, subject)
    unknown = sorted(set(value) - allowed)
    if unknown:
        raise ManifestError("unknown-field", f"{subject}.{unknown[0]}", "declared schema field", "unknown")
    missing = sorted(required - set(value))
    if missing:
        raise ManifestError("missing-field", f"{subject}.{missing[0]}", "required field", "missing")


def validate_relative_path(value, subject):
    if not isinstance(value, str) or not value or len(value.encode("utf-8")) > 512:
        raise ManifestError("unsafe-path", subject, "bounded relative UTF-8 path", repr(value))
    if value != unicodedata.normalize("NFC", value):
        raise ManifestError("normalization-collision", subject, "NFC path", value)
    windows = PureWindowsPath(value)
    path = PurePosixPath(value)
    if (
        "\\" in value or "%" in value or "\x00" in value or
        path.is_absolute() or windows.is_absolute() or windows.drive or
        any(part in {"", ".", ".."} for part in value.split("/"))
    ):
        raise ManifestError("unsafe-path", subject, "plain relative path", value)
    if value.casefold().endswith("maintainer_notes.md"):
        raise ManifestError("out-of-band-note", subject, "package source file", value)
    return value


def _validate_file(record, package_id):
    subject = f"package:{package_id}.file"
    _require_exact_fields(record, FILE_FIELDS, FILE_FIELDS, subject)
    validate_relative_path(record["path"], f"{subject}.path")
    if not isinstance(record["sha256"], str) or not SHA256_PATTERN.fullmatch(record["sha256"]):
        raise ManifestError("invalid-digest", record["path"], "lowercase SHA-256", record["sha256"])
    if isinstance(record["sizeBytes"], bool) or not isinstance(record["sizeBytes"], int) or record["sizeBytes"] < 0:
        raise ManifestError("invalid-size", record["path"], "non-negative integer", record["sizeBytes"])
    if record["role"] not in FILE_ROLES:
        raise ManifestError("invalid-role", record["path"], sorted(FILE_ROLES), record["role"])


def _validate_package(package):
    _require_exact_fields(package, PACKAGE_FIELDS, PACKAGE_REQUIRED_FIELDS, "package")
    package_id = package["packageId"]
    if not isinstance(package_id, str) or not PACKAGE_ID_PATTERN.fullmatch(package_id):
        raise ManifestError("invalid-package-id", str(package_id), "lowercase kebab-case", package_id)
    for field in ("workName", "packageName", "revision"):
        value = package[field]
        if not isinstance(value, str) or not value or len(value) > 128:
            raise ManifestError("invalid-field", f"package:{package_id}.{field}", "non-empty bounded string", value)
    if "publisher" in package and (not isinstance(package["publisher"], str) or not package["publisher"] or len(package["publisher"]) > 128):
        raise ManifestError("invalid-field", f"package:{package_id}.publisher", "non-empty bounded string", package["publisher"])
    if not isinstance(package["sourceLocation"], str) or not package["sourceLocation"].startswith("https://"):
        raise ManifestError("invalid-source-location", package_id, "HTTPS URL", package["sourceLocation"])
    try:
        datetime.date.fromisoformat(package["acquiredOn"])
    except (TypeError, ValueError) as error:
        raise ManifestError("invalid-acquisition-date", package_id, "ISO date", package["acquiredOn"]) from error
    if package["tier"] not in TIERS:
        raise ManifestError("invalid-tier", package_id, sorted(TIERS), package["tier"])
    validate_relative_path(package["packageRoot"], f"package:{package_id}.packageRoot")
    validate_relative_path(package["sourcePath"], f"package:{package_id}.sourcePath")
    validate_relative_path(package["rootFile"], f"package:{package_id}.rootFile")
    if not isinstance(package["rootAssetId"], str) or not ASSET_ID_PATTERN.fullmatch(package["rootAssetId"]):
        raise ManifestError("invalid-root-asset", package_id, "StaticModel AssetId", package["rootAssetId"])
    files = package["files"]
    if not isinstance(files, list) or not files:
        raise ManifestError("invalid-files", package_id, "non-empty file array", type(files).__name__)
    for record in files:
        _validate_file(record, package_id)
    paths = [record["path"] for record in files]
    if paths != sorted(paths):
        raise ManifestError("canonical-order", f"package:{package_id}.files", "ascending path", paths)
    if len(paths) != len(set(paths)):
        raise ManifestError("duplicate-path", package_id, "unique paths", paths)
    folded = {}
    for path in paths:
        key = unicodedata.normalize("NFC", path).casefold()
        if key in folded:
            raise ManifestError("normalization-collision", path, "unique NFC case-folded path", folded[key])
        folded[key] = path
    if package["rootFile"] not in paths:
        raise ManifestError("missing-root-file", package_id, package["rootFile"], paths)
    claims = package["coverageClaims"]
    if not isinstance(claims, list) or not claims or not all(isinstance(item, str) and item for item in claims):
        raise ManifestError("invalid-coverage", package_id, "non-empty claim ID array", claims)
    if claims != sorted(claims) or len(claims) != len(set(claims)):
        raise ManifestError("canonical-order", f"package:{package_id}.coverageClaims", "unique ascending IDs", claims)


def _validate_claim(claim):
    _require_exact_fields(claim, CLAIM_FIELDS, CLAIM_FIELDS, "coverageClaim")
    for field in ("claimId", "packageId"):
        if not isinstance(claim[field], str) or not PACKAGE_ID_PATTERN.fullmatch(claim[field]):
            raise ManifestError("invalid-coverage", field, "lowercase kebab-case", claim[field])
    if not isinstance(claim["subject"], str) or not claim["subject"] or len(claim["subject"].encode("utf-8")) > 512:
        raise ManifestError("invalid-coverage", claim["claimId"], "bounded subject", claim["subject"])
    if claim["evidence"] is None or isinstance(claim["evidence"], list):
        raise ManifestError("invalid-coverage", claim["claimId"], "scalar or object evidence", type(claim["evidence"]).__name__)


def validate_manifest(document):
    _require_exact_fields(document, TOP_LEVEL_FIELDS, TOP_LEVEL_FIELDS, "manifest")
    if document["schema"] != "stoner.production-corpus" or document["schemaVersion"] != 1:
        raise ManifestError("unsupported-schema", "manifest", "stoner.production-corpus v1", f"{document['schema']} v{document['schemaVersion']}")
    revision = document["corpusRevision"]
    if not isinstance(revision, str) or not CORPUS_REVISION_PATTERN.fullmatch(revision):
        raise ManifestError("invalid-corpus-revision", "manifest", "canonical revision", revision)
    packages = document["packages"]
    if not isinstance(packages, list) or len(packages) < 2:
        raise ManifestError("package-independence", "manifest", "at least two packages", len(packages) if isinstance(packages, list) else type(packages).__name__)
    for package in packages:
        _validate_package(package)
    package_ids = [package["packageId"] for package in packages]
    if package_ids != sorted(package_ids) or len(package_ids) != len(set(package_ids)):
        raise ManifestError("canonical-order", "manifest.packages", "unique ascending packageId", package_ids)
    roots = [package["packageRoot"] for package in packages]
    if len({root.casefold() for root in roots}) != len(roots):
        raise ManifestError("normalization-collision", "manifest.packageRoots", "unique roots", roots)
    sources = {(package["workName"].casefold(), package["sourceLocation"]) for package in packages}
    if len(sources) < 2:
        raise ManifestError("package-independence", "manifest.packages", "two independently identified works", sources)

    claims = document["coverageClaims"]
    if not isinstance(claims, list) or not claims:
        raise ManifestError("invalid-coverage", "manifest.coverageClaims", "non-empty array", claims)
    for claim in claims:
        _validate_claim(claim)
    claim_ids = [claim["claimId"] for claim in claims]
    if claim_ids != sorted(claim_ids) or len(claim_ids) != len(set(claim_ids)):
        raise ManifestError("canonical-order", "manifest.coverageClaims", "unique ascending claimId", claim_ids)
    package_id_set = set(package_ids)
    claim_map = {claim["claimId"]: claim for claim in claims}
    for claim in claims:
        if claim["packageId"] not in package_id_set:
            raise ManifestError("coverage-closure", claim["claimId"], "declared package", claim["packageId"])
    referenced = set()
    for package in packages:
        for claim_id in package["coverageClaims"]:
            claim = claim_map.get(claim_id)
            if claim is None or claim["packageId"] != package["packageId"]:
                raise ManifestError("coverage-closure", claim_id, package["packageId"], "missing or foreign claim")
            referenced.add(claim_id)
    if referenced != set(claim_ids):
        missing = sorted(set(claim_ids) - referenced)
        raise ManifestError("coverage-closure", missing[0], "referenced by package", "unreferenced")
    return document


def _has_symlink_component(root, candidate):
    current = candidate
    while current != root:
        if current.is_symlink():
            return True
        current = current.parent
    return root.is_symlink()


def verify_package(package, package_root):
    package_root = Path(package_root)
    if not package_root.exists():
        raise ManifestError("missing-package", package["packageId"], "package directory", "missing")
    if not package_root.is_dir() or package_root.is_symlink():
        raise ManifestError("path-escape", package["packageId"], "ordinary directory", "non-directory or symlink")
    root_resolved = package_root.resolve(strict=True)
    observed = []
    for current, directories, files in os.walk(package_root, followlinks=False):
        current_path = Path(current)
        for name in sorted(directories):
            path = current_path / name
            if path.is_symlink():
                raise ManifestError("path-escape", path.relative_to(package_root).as_posix(), "ordinary directory", "symlink")
        for name in sorted(files):
            path = current_path / name
            relative = path.relative_to(package_root).as_posix()
            validate_relative_path(relative, f"package:{package['packageId']}.observed")
            if path.is_symlink() or _has_symlink_component(package_root, path):
                raise ManifestError("path-escape", relative, "ordinary file", "symlink")
            observed.append(relative)
    declared = [record["path"] for record in package["files"]]
    declared_set = set(declared)
    observed_set = set(observed)
    missing = sorted(declared_set - observed_set)
    if missing:
        raise ManifestError("missing-file", f"{package['packageId']}:{missing[0]}", "declared file", "missing")
    extra = sorted(observed_set - declared_set)
    if extra:
        raise ManifestError("extra-file", f"{package['packageId']}:{extra[0]}", "no undeclared file", "present")
    for record in package["files"]:
        candidate = package_root / record["path"]
        resolved = candidate.resolve(strict=True)
        try:
            resolved.relative_to(root_resolved)
        except ValueError as error:
            raise ManifestError("path-escape", record["path"], "inside package root", str(resolved)) from error
        size = candidate.stat().st_size
        if size != record["sizeBytes"]:
            raise ManifestError("size-mismatch", f"{package['packageId']}:{record['path']}", record["sizeBytes"], size)
        digest = hashlib.sha256(candidate.read_bytes()).hexdigest()
        if digest != record["sha256"]:
            raise ManifestError("digest-mismatch", f"{package['packageId']}:{record['path']}", record["sha256"], digest)
    return {
        "packageId": package["packageId"],
        "fileCount": len(declared),
        "aggregateBytes": sum(record["sizeBytes"] for record in package["files"]),
        "rootAssetId": package["rootAssetId"],
    }


def find_package(document, package_id):
    for package in document["packages"]:
        if package["packageId"] == package_id:
            return package
    raise ManifestError("unknown-package", package_id, "declared package", "missing")
