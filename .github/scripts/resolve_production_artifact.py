#!/usr/bin/env python3
"""Resolve the newest immutable producer artifact available to a rerun."""

from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys
from typing import Any, Iterable


def _artifact_pages(document: Any) -> Iterable[dict[str, Any]]:
    pages = document if isinstance(document, list) else [document]
    if not pages or any(not isinstance(page, dict) for page in pages):
        raise ValueError("artifact response must contain object pages")
    for page in pages:
        artifacts = page.get("artifacts")
        if not isinstance(artifacts, list):
            raise ValueError("artifact response page is missing artifacts")
        for artifact in artifacts:
            if not isinstance(artifact, dict):
                raise ValueError("artifact response contains a non-object item")
            yield artifact


def resolve_artifact_name(document: Any, prefix: str, max_attempt: int) -> str:
    if not prefix or "\n" in prefix or "\r" in prefix:
        raise ValueError("artifact prefix is invalid")
    if max_attempt < 1:
        raise ValueError("maximum attempt must be positive")

    pattern = re.compile(rf"{re.escape(prefix)}([1-9][0-9]*)\Z")
    candidates: dict[int, str] = {}
    for artifact in _artifact_pages(document):
        name = artifact.get("name")
        expired = artifact.get("expired")
        if not isinstance(name, str) or not isinstance(expired, bool):
            raise ValueError("artifact response fields are invalid")
        match = pattern.fullmatch(name)
        if expired or match is None:
            continue
        attempt = int(match.group(1))
        if attempt > max_attempt:
            continue
        if attempt in candidates:
            raise ValueError("artifact response contains an ambiguous attempt")
        candidates[attempt] = name

    if not candidates:
        raise ValueError("no eligible producer artifact was found")
    return candidates[max(candidates)]


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--prefix", required=True)
    parser.add_argument("--max-attempt", required=True, type=int)
    parser.add_argument("--github-output", type=Path)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        document = json.load(sys.stdin)
        name = resolve_artifact_name(document, args.prefix, args.max_attempt)
        if args.github_output is None:
            print(name)
        else:
            with args.github_output.open("a", encoding="utf-8", newline="\n") as output:
                output.write(f"name={name}\n")
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"artifact resolution failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
