#!/usr/bin/env python3
"""Generate bounded Feature 022 malformed KTX2 seed files."""

from __future__ import annotations

import pathlib
import struct


ROOT = pathlib.Path(__file__).resolve().parent
GOLDEN = ROOT / "Golden"
INVALID = ROOT / "Invalid"


def put_u32(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<I", data, offset, value)


def put_u64(data: bytearray, offset: int, value: int) -> None:
    struct.pack_into("<Q", data, offset, value)


def write(name: str, data: bytes | bytearray) -> None:
    INVALID.mkdir(parents=True, exist_ok=True)
    (INVALID / name).write_bytes(bytes(data))


def main() -> None:
    etc1s = bytearray(
        (GOLDEN / "etc1s-color-balanced-full.ktx2").read_bytes()
    )
    uastc = bytearray(
        (GOLDEN / "uastc-color-balanced-full.ktx2").read_bytes()
    )

    bad_identifier = bytearray(etc1s)
    bad_identifier[0] ^= 0xFF
    write("header-bad-identifier.ktx2", bad_identifier)
    write("header-truncated.ktx2", etc1s[:79])
    write("level-index-truncated.ktx2", etc1s[:80])

    dfd_range = bytearray(etc1s)
    put_u32(dfd_range, 48, len(dfd_range) - 4)
    put_u32(dfd_range, 52, 64)
    write("dfd-out-of-range.ktx2", dfd_range)

    kvd_overlap = bytearray(etc1s)
    put_u32(kvd_overlap, 56, 80)
    write("kvd-overlaps-level-index.ktx2", kvd_overlap)

    sgd_range = bytearray(etc1s)
    put_u64(sgd_range, 64, len(sgd_range) - 4)
    put_u64(sgd_range, 72, 64)
    write("sgd-out-of-range.ktx2", sgd_range)

    level_header = bytearray(etc1s)
    put_u64(level_header, 80, 0)
    write("level-overlaps-header.ktx2", level_header)

    level_truncated = bytearray(etc1s)
    put_u64(level_truncated, 88, len(level_truncated))
    write("level-out-of-range.ktx2", level_truncated)

    level_misaligned = bytearray(uastc)
    original_offset = struct.unpack_from("<Q", level_misaligned, 80)[0]
    put_u64(level_misaligned, 80, original_offset + 1)
    write("level-misaligned.ktx2", level_misaligned)

    invalid_faces = bytearray(etc1s)
    put_u32(invalid_faces, 36, 2)
    write("scope-two-faces.ktx2", invalid_faces)

    invalid_depth = bytearray(etc1s)
    put_u32(invalid_depth, 28, 1)
    write("scope-depth.ktx2", invalid_depth)

    corrupt_basis = bytearray(etc1s)
    corrupt_basis[-1] ^= 0x5A
    write("basis-payload-corrupt.ktx2", corrupt_basis)


if __name__ == "__main__":
    main()
