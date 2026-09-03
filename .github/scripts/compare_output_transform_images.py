#!/usr/bin/env python3
"""Exact-dimension SDR v3 image comparison gate.

No alignment, crop, scale, warp, resample, or content-search operation exists
in this module. Dimension mismatch is rejected before any perceptual tool can
run.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import struct
import subprocess
import zlib


PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"
FORBIDDEN_SPATIAL_OPERATIONS = (
    "align", "crop", "scale", "warp", "resample", "resize",
)


def sha256_bytes(payload: bytes) -> str:
    return hashlib.sha256(payload).hexdigest()


def _chunk(kind: bytes, payload: bytes) -> bytes:
    return (struct.pack(">I", len(payload)) + kind + payload +
            struct.pack(">I", zlib.crc32(kind + payload) & 0xffffffff))


def write_lossless_rgb_png(path: Path, width: int, height: int,
                           rgb: bytes) -> tuple[str, str]:
    if width != 512 or height != 512 or len(rgb) != width * height * 3:
        raise ValueError("formal SDR Candidate must be exact 512x512 RGB8")
    rows = b"".join(b"\x00" + rgb[y * width * 3:(y + 1) * width * 3]
                    for y in range(height))
    payload = (PNG_SIGNATURE +
               _chunk(b"IHDR", struct.pack(">IIBBBBB", width, height,
                                             8, 2, 0, 0, 0)) +
               _chunk(b"IDAT", zlib.compress(rows, level=9)) +
               _chunk(b"IEND", b""))
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(payload)
    decoded_width, decoded_height, decoded = decode_png(path)
    if (decoded_width, decoded_height, decoded) != (width, height, rgb):
        raise ValueError("lossless Candidate round-trip changed decoded pixels")
    return sha256_bytes(payload), sha256_bytes(decoded)


def decode_png(path: Path) -> tuple[int, int, bytes]:
    if path.stat().st_size > 64 * 1024 * 1024:
        raise ValueError("PNG exceeds bounded evidence size")
    payload = path.read_bytes()
    if not payload.startswith(PNG_SIGNATURE):
        raise ValueError("invalid PNG signature")
    offset = len(PNG_SIGNATURE)
    width = height = channels = 0
    compressed = bytearray()
    seen_end = False
    while offset + 12 <= len(payload):
        length = struct.unpack(">I", payload[offset:offset + 4])[0]
        kind = payload[offset + 4:offset + 8]
        data_start, data_end = offset + 8, offset + 8 + length
        crc_end = data_end + 4
        if crc_end > len(payload):
            raise ValueError("truncated PNG chunk")
        data = payload[data_start:data_end]
        expected_crc = struct.unpack(">I", payload[data_end:crc_end])[0]
        if zlib.crc32(kind + data) & 0xffffffff != expected_crc:
            raise ValueError("PNG CRC mismatch")
        if kind == b"IHDR":
            if len(data) != 13 or channels or offset != len(PNG_SIGNATURE):
                raise ValueError("invalid PNG IHDR")
            width, height, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", data)
            if depth != 8 or color not in (2, 6) or compression or filtering or interlace:
                raise ValueError("only non-interlaced RGB8/RGBA8 PNG is accepted")
            channels = 3 if color == 2 else 4
        elif kind == b"IDAT":
            compressed.extend(data)
        elif kind == b"IEND":
            if data or crc_end != len(payload):
                raise ValueError("PNG IEND/trailing bytes are invalid")
            seen_end = True
            break
        offset = crc_end
    if not seen_end or width <= 0 or height <= 0 or channels == 0:
        raise ValueError("incomplete PNG")
    stride = width * channels
    decoded_size = height * (stride + 1)
    if decoded_size > 64 * 1024 * 1024:
        raise ValueError("PNG decoded bytes exceed bounded evidence size")
    decoder = zlib.decompressobj()
    try:
        raw = decoder.decompress(bytes(compressed), decoded_size + 1)
    except zlib.error as error:
        raise ValueError("invalid PNG compressed stream") from error
    if (len(raw) != decoded_size or not decoder.eof or
            decoder.unused_data or decoder.unconsumed_tail):
        raise ValueError("unexpected PNG decoded byte count")
    decoded = bytearray(width * height * 3)
    for row in range(height):
        source = row * (stride + 1)
        if raw[source] != 0:
            raise ValueError("formal Candidate PNG must use deterministic filter 0")
        pixels = raw[source + 1:source + 1 + stride]
        if channels == 3:
            decoded[row * width * 3:(row + 1) * width * 3] = pixels
        else:
            for column in range(width):
                rgba = column * 4
                rgb = (row * width + column) * 3
                if pixels[rgba + 3] != 255:
                    raise ValueError("formal Candidate alpha must be opaque")
                decoded[rgb:rgb + 3] = pixels[rgba:rgba + 3]
    return width, height, bytes(decoded)


def compare_exact(candidate: Path, reference: Path,
                  flip_command: list[str] | None = None) -> dict[str, object]:
    candidate_width, candidate_height, candidate_rgb = decode_png(candidate)
    reference_width, reference_height, reference_rgb = decode_png(reference)
    if (candidate_width, candidate_height) != (reference_width, reference_height):
        raise ValueError("dimension mismatch rejected before FLIP")
    if (candidate_width, candidate_height) != (512, 512):
        raise ValueError("formal v3 comparison requires exact 512x512")
    differing = sum(left != right for left, right in
                    zip(candidate_rgb, reference_rgb))
    if flip_command:
        joined = " ".join(flip_command).lower()
        if any(operation in joined for operation in FORBIDDEN_SPATIAL_OPERATIONS):
            raise ValueError("spatial normalization option is forbidden")
        completed = subprocess.run(flip_command, check=False,
                                   capture_output=True, text=True)
        if completed.returncode != 0:
            raise ValueError("FLIP process failed")
    return {
        "schema": "stoner.output-transform-exact-image-comparison",
        "schemaVersion": 1,
        "width": candidate_width,
        "height": candidate_height,
        "candidateDecodedSha256": sha256_bytes(candidate_rgb),
        "referenceDecodedSha256": sha256_bytes(reference_rgb),
        "differingChannelCount": differing,
        "pixelExact": differing == 0,
        "spatialNormalization": "forbidden",
        "dimensionCheckPrecedesFlip": True,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser()
    parser.add_argument("--candidate", required=True)
    parser.add_argument("--reference", required=True)
    parser.add_argument("--output")
    return parser


def main() -> int:
    args, unknown = build_parser().parse_known_args()
    if unknown or any(operation in " ".join(unknown).lower()
                      for operation in FORBIDDEN_SPATIAL_OPERATIONS):
        print("finding: unknown/spatial normalization arguments are forbidden")
        return 2
    try:
        report = compare_exact(Path(args.candidate), Path(args.reference))
    except (OSError, ValueError) as error:
        print(f"finding: {error}")
        return 1
    payload = json.dumps(report, sort_keys=True, indent=2) + "\n"
    if args.output:
        Path(args.output).write_text(payload, encoding="utf-8")
    else:
        print(payload, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
