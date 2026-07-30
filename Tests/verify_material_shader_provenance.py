#!/usr/bin/env python3
"""Verify the private yyjson dependency used by Feature 023."""

from __future__ import annotations

import argparse
import hashlib
from pathlib import Path

EXPECTED_VERSION = "0.12.0"
EXPECTED = {
    "yyjson.c": "ac2e9bbb2e2d9149d90878d40506a1d624fa0b33c979a11b61075c54782c6d6a",
    "yyjson.h": "175867c5493a5df648cec566717fa1c29aa2f6096f5f0cf1efad0b65e1f6d7b3",
    "LICENSE": "45e384d3d52c73cba3a64d6e6c25d47cd738cd8a55c30629e3201046eda62947",
}


def verify(root: Path) -> list[str]:
    errors: list[str] = []
    vendor = root / "ThirdParty" / "yyjson"
    version = vendor / "VERSION"
    if not version.is_file():
        errors.append("missing ThirdParty/yyjson/VERSION")
    elif version.read_text(encoding="ascii").strip() != EXPECTED_VERSION:
        errors.append("yyjson version mismatch")
    upstream = vendor / "UPSTREAM.md"
    if not upstream.is_file():
        errors.append("missing ThirdParty/yyjson/UPSTREAM.md")
    elif "8b4a38dc994a110abaec8a400615567bd996105f" not in upstream.read_text(
        encoding="utf-8"
    ):
        errors.append("yyjson upstream commit mismatch")
    for name, expected in EXPECTED.items():
        path = vendor / name
        if not path.is_file():
            errors.append(f"missing ThirdParty/yyjson/{name}")
            continue
        actual = hashlib.sha256(path.read_bytes()).hexdigest()
        if actual != expected:
            errors.append(f"yyjson checksum mismatch: {name}")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    args = parser.parse_args()
    errors = verify(args.root.resolve())
    if errors:
        for error in errors:
            print(f"ERROR: {error}")
        return 1
    print("yyjson provenance: PASS")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
