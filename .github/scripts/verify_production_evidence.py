#!/usr/bin/env python3
"""Privacy and freshness checks for Feature 028 evidence artifacts."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys


MAX_TEXT_BYTES = 1024 * 1024
CAPTURE_FIELDS = {
    "captureScope", "width", "height", "workloadRevision", "backend",
    "frameToken", "expectedFrameToken", "sha256", "captureStartedNs",
}
PRIVATE_PATTERNS = tuple(re.compile(pattern, re.IGNORECASE) for pattern in (
    r"(?:^|[\s=:'\"])/(?:Users|home)/[^\s]+",
    r"(?:^|[\s=:'\"])[A-Z]:\\Users\\[^\s]+",
    r"\buser(?:name)?\s*[=:]\s*[^\s]+",
    r"https?://[^/@\s:]+:[^/@\s]+@",
    r"\bauthorization\s*:\s*(?:bearer|basic)\s+\S+",
    r"\b(?:AWS_SECRET_ACCESS_KEY|SECRET|TOKEN|PASSWORD)\s*=\s*\S+",
    r"\bHOME\s*=\s*\S+",
    r"\b(?:pid|process-id)\s*[=:]\s*\d+",
    r"\b(?:native|pointer|address)\s*[=:]\s*0x[0-9a-f]+",
    r"\b(?:deviceName|adapter)\s*=\s*.+",
))


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_text(text: str) -> None:
    if not isinstance(text, str) or len(text.encode("utf-8")) > MAX_TEXT_BYTES:
        raise ValueError("evidence text is not bounded to 1 MiB")
    if any(pattern.search(text) for pattern in PRIVATE_PATTERNS):
        raise ValueError("evidence contains private host data")


def _ppm_dimensions(path: Path) -> tuple[int, int]:
    payload = path.read_bytes()
    if not payload.startswith(b"P6\n"):
        raise ValueError("capture is not a canonical P6 image")
    try:
        _, dimensions, maximum, pixels = payload.split(b"\n", 3)
        width, height = (int(value) for value in dimensions.split())
    except (ValueError, TypeError) as error:
        raise ValueError("capture header is invalid") from error
    if maximum != b"255" or width <= 0 or height <= 0 or len(pixels) != width * height * 3:
        raise ValueError("capture dimensions are invalid")
    return width, height


def validate_window_capture(path: Path, metadata: dict) -> None:
    if not isinstance(metadata, dict) or set(metadata) != CAPTURE_FIELDS:
        raise ValueError("window capture metadata fields are invalid")
    if metadata.get("captureScope") != "application-window":
        raise ValueError("capture must contain only the application window")
    if metadata.get("backend") not in ("vulkan", "metal"):
        raise ValueError("capture backend is invalid")
    if not isinstance(metadata.get("workloadRevision"), str) or not metadata["workloadRevision"]:
        raise ValueError("capture workload revision is invalid")
    if metadata.get("frameToken") != metadata.get("expectedFrameToken"):
        raise ValueError("capture is stale")
    if not path.is_file():
        raise ValueError("capture file is missing")
    if path.stat().st_mtime_ns < metadata.get("captureStartedNs", -1):
        raise ValueError("capture is stale")
    width, height = _ppm_dimensions(path)
    if (width, height) != (metadata.get("width"), metadata.get("height")):
        raise ValueError("capture dimensions differ")
    if sha256_file(path) != metadata.get("sha256"):
        raise ValueError("capture digest differs")


def validate_evidence_tree(root: Path) -> dict:
    root = root.resolve()
    if not root.is_dir():
        raise ValueError("evidence root is missing")
    text_count = 0
    capture_count = 0
    for path in sorted(root.rglob("*")):
        if path.is_symlink():
            raise ValueError("evidence tree contains a symbolic link")
        if not path.is_file():
            continue
        if path.suffix.lower() in (".json", ".txt", ".log", ".md"):
            text = path.read_text(encoding="utf-8")
            validate_text(text)
            text_count += 1
            if path.suffix.lower() == ".json":
                value = json.loads(text)
                if isinstance(value, dict) and set(value) == CAPTURE_FIELDS:
                    validate_window_capture(path.with_suffix(".ppm"), value)
                    capture_count += 1
    return {
        "result": "Passed",
        "textArtifactCount": text_count,
        "windowCaptureCount": capture_count,
    }


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    args = parser.parse_args()
    try:
        result = validate_evidence_tree(args.root)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(str(error), file=sys.stderr)
        return 1
    print(json.dumps(result, sort_keys=True))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
