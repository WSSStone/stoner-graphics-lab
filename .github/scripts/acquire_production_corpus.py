#!/usr/bin/env python3
"""Acquire one hash-pinned external Feature 028 package without fallback."""

import argparse
import os
from pathlib import Path
import re
import shutil
import sys
import tempfile
from urllib.parse import quote, urlparse
from urllib.request import Request, urlopen


SCRIPT_DIR = Path(__file__).parent
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from production_content_manifest import (  # noqa: E402
    ManifestError, find_package, load_json, validate_manifest, verify_package,
)


IMMUTABLE_GIT_REVISION = re.compile(r"^[0-9a-f]{40}$")


class AcquisitionError(RuntimeError):
    def __init__(self, category, subject):
        self.category = category
        self.subject = subject
        super().__init__(f"{category}: {subject}")


def _raw_url(package, relative_path):
    parsed = urlparse(package["sourceLocation"])
    parts = [part for part in parsed.path.removesuffix(".git").split("/") if part]
    if parsed.scheme != "https" or parsed.netloc != "github.com" or len(parts) != 2:
        raise AcquisitionError("unsupported-source", package["sourceLocation"])
    source_path = quote(package["sourcePath"], safe="/")
    file_path = quote(relative_path, safe="/")
    return f"https://raw.githubusercontent.com/{parts[0]}/{parts[1]}/{package['revision']}/{source_path}/{file_path}"


def _download(url, destination):
    request = Request(url, headers={"User-Agent": "stoner-production-corpus/1"})
    with urlopen(request, timeout=60) as response, destination.open("wb") as output:
        shutil.copyfileobj(response, output, length=1024 * 1024)


def _next_quarantine_path(destination):
    for index in range(1, 1000):
        candidate = destination.with_name(f"{destination.name}.quarantine-{index:03d}")
        if not candidate.exists():
            return candidate
    raise AcquisitionError("quarantine-exhausted", destination.name)


def acquire_package(manifest_path, package_id, content_root, fetcher=None):
    try:
        document = validate_manifest(load_json(manifest_path))
        package = find_package(document, package_id)
    except ManifestError as error:
        raise AcquisitionError(error.category, error.subject) from error
    if package["tier"] != "medium":
        raise AcquisitionError("package-not-external", package_id)
    if not IMMUTABLE_GIT_REVISION.fullmatch(package["revision"]):
        raise AcquisitionError("revision-not-immutable", package["revision"])

    content_root = Path(content_root)
    destination = content_root / package["packageRoot"]
    destination.parent.mkdir(parents=True, exist_ok=True)
    if destination.exists() or destination.is_symlink():
        try:
            summary = verify_package(package, destination)
            return {"status": "Reused", **summary}
        except ManifestError:
            os.replace(destination, _next_quarantine_path(destination))

    partial = Path(tempfile.mkdtemp(
        prefix=f".{package_id}.partial-", dir=destination.parent,
    ))
    fetch = fetcher or _download
    try:
        for record in package["files"]:
            output = partial / record["path"]
            output.parent.mkdir(parents=True, exist_ok=True)
            try:
                fetch(_raw_url(package, record["path"]), output)
            except AcquisitionError:
                raise
            except Exception as error:
                raise AcquisitionError("source-unavailable", record["path"]) from error
        try:
            summary = verify_package(package, partial)
        except ManifestError as error:
            raise AcquisitionError(error.category, error.subject) from error
        os.replace(partial, destination)
        return {"status": "Acquired", **summary}
    except Exception:
        shutil.rmtree(partial, ignore_errors=True)
        raise


def parse_args(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=Path("Content/ProductionAcceptance/Corpus/corpus-v1.json"))
    parser.add_argument("--package", required=True)
    parser.add_argument("--content-root", type=Path, default=Path("Content/ProductionAcceptance"))
    return parser.parse_args(argv)


def main(argv=None):
    args = parse_args(argv)
    try:
        result = acquire_package(args.manifest, args.package, args.content_root)
    except AcquisitionError as error:
        print(str(error), file=sys.stderr)
        return 1
    print(f"status={result['status']} package={result['packageId']} files={result['fileCount']} bytes={result['aggregateBytes']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
