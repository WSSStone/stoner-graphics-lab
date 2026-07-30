#!/usr/bin/env python3
"""Verify the pinned Feature 022 third-party source and module evidence."""

from __future__ import annotations

import hashlib
import pathlib
import sys


EXPECTED = {
    "ktx": ("4.4.2", "4d6fc70eaf62ad0558e63e8d97eb9766118327a6"),
    "wamr": ("2.4.5", "25bd7eb63e828e4bd242cc9b38d260b4b31c6605"),
    "stoner-basis-encoder": (
        "022-v1",
        "d394459dc8f85d2e133045c421b61ef6e080f5890c77f7f048a7258ee77e0b98",
    ),
}


def sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def verify_manifest(directory: pathlib.Path) -> list[str]:
    errors: list[str] = []
    manifest = directory / "SHA256SUMS"
    if not manifest.is_file():
        return [f"{directory}: missing SHA256SUMS"]
    listed: set[pathlib.Path] = set()
    for line_number, line in enumerate(
        manifest.read_text(encoding="ascii").splitlines(), 1
    ):
        try:
            expected_hash, relative_text = line.split("  ", 1)
        except ValueError:
            errors.append(f"{manifest}:{line_number}: malformed entry")
            continue
        relative = pathlib.Path(relative_text.removeprefix("./"))
        path = directory / relative
        listed.add(relative)
        if (
            len(expected_hash) != 64
            or any(character not in "0123456789abcdef" for character in expected_hash)
        ):
            errors.append(f"{manifest}:{line_number}: invalid SHA-256")
        elif not path.is_file():
            errors.append(f"{manifest}:{line_number}: missing {relative.as_posix()}")
        elif sha256(path) != expected_hash:
            errors.append(f"{relative.as_posix()}: checksum mismatch")

    actual = {
        path.relative_to(directory)
        for path in directory.rglob("*")
        if path.is_file() and path.name != "SHA256SUMS"
    }
    for relative in sorted(actual - listed):
        errors.append(f"{directory.name}: unlisted {relative.as_posix()}")
    for relative in sorted(listed - actual):
        errors.append(f"{directory.name}: stale {relative.as_posix()}")
    return errors


def verify(root: pathlib.Path) -> list[str]:
    errors: list[str] = []
    third_party = root / "ThirdParty"
    for name, (version, evidence) in EXPECTED.items():
        directory = third_party / name
        if (directory / "VERSION").read_text(encoding="ascii").strip() != version:
            errors.append(f"{name}: VERSION is not {version}")
        errors.extend(verify_manifest(directory))
        if name == "stoner-basis-encoder":
            module = directory / "stoner_basis_encoder.wasm"
            if not module.is_file() or sha256(module) != evidence:
                errors.append(f"{name}: authoritative module hash mismatch")
        else:
            upstream = (directory / "UPSTREAM.md").read_text(encoding="utf-8")
            if evidence not in upstream:
                errors.append(f"{name}: source revision evidence is missing")
    return errors


def main() -> int:
    root = pathlib.Path(__file__).resolve().parents[1]
    errors = verify(root)
    if errors:
        for error in errors:
            print(f"ERROR: {error}", file=sys.stderr)
        return 1
    print("KTX 4.4.2, WAMR 2.4.5, and encoder module provenance passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
