#!/usr/bin/env python3
"""Regenerate Feature 021's tiny deterministic image fixture corpus."""

from __future__ import annotations

import binascii
import math
import pathlib
import shutil
import struct
import subprocess
import tempfile
import zlib


ROOT = pathlib.Path(__file__).resolve().parent
VALID = ROOT / "Valid"
INVALID = ROOT / "Invalid"


def chunk(kind: bytes, payload: bytes) -> bytes:
    body = kind + payload
    return struct.pack(">I", len(payload)) + body + struct.pack(
        ">I", binascii.crc32(body) & 0xFFFFFFFF
    )


def exif_orientation(value: int) -> bytes:
    return (
        b"II"
        + struct.pack("<H", 42)
        + struct.pack("<I", 8)
        + struct.pack("<H", 1)
        + struct.pack("<HHI", 0x0112, 3, 1)
        + struct.pack("<H", value)
        + b"\0\0"
        + struct.pack("<I", 0)
    )


def write_png(
    path: pathlib.Path,
    width: int,
    height: int,
    color_type: int,
    bit_depth: int,
    rows: list[bytes],
    extras: list[tuple[bytes, bytes]] | None = None,
) -> None:
    ihdr = struct.pack(">IIBBBBB", width, height, bit_depth, color_type, 0, 0, 0)
    filtered = b"".join(b"\0" + row for row in rows)
    payload = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", ihdr)
    for kind, data in extras or []:
        payload += chunk(kind, data)
    payload += chunk(b"IDAT", zlib.compress(filtered, 9))
    payload += chunk(b"IEND", b"")
    path.write_bytes(payload)


def asymmetric_rgb(width: int, height: int) -> bytes:
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            pixels.extend(
                (
                    (x * 71 + y * 19 + 17) & 0xFF,
                    (x * 23 + y * 83 + 29) & 0xFF,
                    (x * 41 + y * 37 + 53) & 0xFF,
                )
            )
    return bytes(pixels)


def convert_jpeg(source: pathlib.Path, destination: pathlib.Path) -> None:
    if shutil.which("sips") is None:
        raise RuntimeError("fixture regeneration currently requires macOS sips")
    subprocess.run(
        ["sips", "-s", "format", "jpeg", str(source), "--out", str(destination)],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    destination.write_bytes(strip_jpeg_app2(destination.read_bytes()))


def strip_jpeg_app2(jpeg: bytes) -> bytes:
    output = bytearray(jpeg[:2])
    offset = 2
    while offset < len(jpeg):
        marker_start = offset
        if jpeg[offset] != 0xFF:
            output.extend(jpeg[offset:])
            break
        while offset < len(jpeg) and jpeg[offset] == 0xFF:
            offset += 1
        if offset >= len(jpeg):
            output.extend(jpeg[marker_start:])
            break
        marker = jpeg[offset]
        offset += 1
        if marker in (0xD8, 0xD9) or marker == 0x01 or 0xD0 <= marker <= 0xD7:
            output.extend(jpeg[marker_start:offset])
            continue
        if offset + 2 > len(jpeg):
            raise RuntimeError("truncated generated JPEG")
        length = struct.unpack(">H", jpeg[offset : offset + 2])[0]
        segment_end = offset + length
        if segment_end > len(jpeg):
            raise RuntimeError("truncated generated JPEG segment")
        if marker == 0xDA:
            output.extend(jpeg[marker_start:])
            break
        if marker != 0xE2:
            output.extend(jpeg[marker_start:segment_end])
        offset = segment_end
    return bytes(output)


def rgbe(red: float, green: float, blue: float) -> bytes:
    maximum = max(red, green, blue)
    if maximum < 1.0e-32:
        return b"\0\0\0\0"
    mantissa, exponent = math.frexp(maximum)
    scale = mantissa * 256.0 / maximum
    return bytes(
        (
            int(red * scale),
            int(green * scale),
            int(blue * scale),
            exponent + 128,
        )
    )


def main() -> None:
    VALID.mkdir(parents=True, exist_ok=True)
    INVALID.mkdir(parents=True, exist_ok=True)

    gray_rows = [
        bytes(((x * 37 + y * 61 + 9) & 0xFF for x in range(3)))
        for y in range(5)
    ]
    write_png(VALID / "png-gray-3x5.png", 3, 5, 0, 8, gray_rows)

    gray_alpha_rows = []
    for y in range(3):
        row = bytearray()
        for x in range(5):
            row.extend(((x * 43 + y * 17) & 0xFF, (x * 59 + y * 31) & 0xFF))
        gray_alpha_rows.append(bytes(row))
    write_png(
        VALID / "png-gray-alpha-5x3.png", 5, 3, 4, 8, gray_alpha_rows
    )

    rgb = asymmetric_rgb(3, 5)
    write_png(
        VALID / "png-rgb-3x5.png",
        3,
        5,
        2,
        8,
        [rgb[y * 9 : (y + 1) * 9] for y in range(5)],
    )

    rgba_rows = []
    for y in range(3):
        row = bytearray()
        for x in range(5):
            row.extend(
                (
                    (x * 71 + y * 19) & 0xFF,
                    (x * 13 + y * 89) & 0xFF,
                    (x * 47 + y * 29) & 0xFF,
                    (x * 53 + y * 37) & 0xFF,
                )
            )
        rgba_rows.append(bytes(row))
    write_png(VALID / "png-rgba-5x3.png", 5, 3, 6, 8, rgba_rows)

    palette = bytes((255, 0, 0, 0, 255, 0, 0, 0, 255))
    palette_rows = [bytes((0, 1, 2)), bytes((2, 0, 1)), bytes((1, 2, 0))]
    write_png(
        VALID / "png-palette-3x3.png",
        3,
        3,
        3,
        8,
        palette_rows,
        [(b"PLTE", palette), (b"tRNS", bytes((255, 127, 0)))],
    )

    gray16_rows = [
        b"".join(struct.pack(">H", value) for value in values)
        for values in ((0, 32768, 65535), (65535, 1024, 40960))
    ]
    write_png(VALID / "png-gray16-3x2.png", 3, 2, 0, 16, gray16_rows)

    oriented_rgb = asymmetric_rgb(2, 3)
    write_png(
        VALID / "png-exif-o6-2x3.png",
        2,
        3,
        2,
        8,
        [oriented_rgb[y * 6 : (y + 1) * 6] for y in range(3)],
        [(b"eXIf", exif_orientation(6))],
    )

    linear_rgb = asymmetric_rgb(4, 2)
    write_png(
        VALID / "png-linear-4x2.png",
        4,
        2,
        2,
        8,
        [linear_rgb[y * 12 : (y + 1) * 12] for y in range(2)],
        [(b"gAMA", struct.pack(">I", 100000))],
    )

    with tempfile.TemporaryDirectory() as temporary:
        temp = pathlib.Path(temporary)
        ppm = temp / "rgb.ppm"
        ppm.write_bytes(b"P6\n3 5\n255\n" + rgb)
        convert_jpeg(ppm, VALID / "jpeg-rgb-3x5.jpg")

        gray = bytes(((x * 47 + y * 67 + 11) & 0xFF for y in range(3) for x in range(5)))
        pgm = temp / "gray.pgm"
        pgm.write_bytes(b"P5\n5 3\n255\n" + gray)
        convert_jpeg(pgm, VALID / "jpeg-gray-5x3.jpg")

        orientation_ppm = temp / "orientation.ppm"
        orientation_ppm.write_bytes(b"P6\n2 3\n255\n" + oriented_rgb)
        orientation_jpeg = VALID / "jpeg-exif-o6-2x3.jpg"
        convert_jpeg(orientation_ppm, orientation_jpeg)
        jpeg = orientation_jpeg.read_bytes()
        app1_payload = b"Exif\0\0" + exif_orientation(6)
        app1 = b"\xff\xe1" + struct.pack(">H", len(app1_payload) + 2) + app1_payload
        orientation_jpeg.write_bytes(jpeg[:2] + app1 + jpeg[2:])

    hdr_values = (
        (0.25, 0.5, 1.0),
        (2.0, 0.125, 0.75),
        (4.0, 8.0, 0.5),
        (0.01, 0.02, 0.03),
        (16.0, 1.0, 0.25),
        (0.5, 3.0, 6.0),
    )
    hdr = (
        b"#?RADIANCE\nFORMAT=32-bit_rle_rgbe\n\n-Y 2 +X 3\n"
        + b"".join(rgbe(*value) for value in hdr_values)
    )
    (VALID / "hdr-rgb-3x2.hdr").write_bytes(hdr)

    (INVALID / "unsupported.bin").write_bytes(b"STONER-UNSUPPORTED\0")
    (INVALID / "truncated-png.bin").write_bytes(
        b"\x89PNG\r\n\x1a\n\0\0\0\rIHDR\0\0"
    )
    bad_crc = bytearray((VALID / "png-rgb-3x5.png").read_bytes())
    bad_crc[29] ^= 0x01
    (INVALID / "png-bad-crc.png").write_bytes(bad_crc)
    (INVALID / "hdr-bad-header.hdr").write_bytes(
        b"#?RADIANCE\nFORMAT=unknown\n\n-Y 2 +X 2\n"
    )


if __name__ == "__main__":
    main()
