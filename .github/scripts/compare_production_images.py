#!/usr/bin/env python3
"""Small canonical PPM comparator used before publication of Feature 028 evidence."""

from __future__ import annotations

from pathlib import Path


def _load_ppm(path: Path) -> tuple[int, int, bytes]:
    payload = path.read_bytes()
    if not payload.startswith(b"P6\n"):
        raise ValueError("only canonical P6 images are accepted")
    try:
        _, dimensions, maximum, pixels = payload.split(b"\n", 3)
        width, height = (int(value) for value in dimensions.split())
    except (TypeError, ValueError) as error:
        raise ValueError("invalid P6 image") from error
    if maximum != b"255" or width <= 0 or height <= 0 or len(pixels) != width * height * 3:
        raise ValueError("invalid P6 dimensions")
    return width, height, pixels


def compare_images(reference: Path, candidate: Path) -> dict:
    reference_width, reference_height, reference_pixels = _load_ppm(reference)
    candidate_width, candidate_height, candidate_pixels = _load_ppm(candidate)
    if (reference_width, reference_height) != (candidate_width, candidate_height):
        raise ValueError("image dimensions differ")
    differences = [
        abs(left - right) / 255.0
        for left, right in zip(reference_pixels, candidate_pixels)
    ]
    maximum = max(differences, default=0.0)
    mean = sum(differences) / len(differences) if differences else 0.0
    return {
        "width": reference_width,
        "height": reference_height,
        "meanAbsoluteChannelError": mean,
        "maximumAbsoluteChannelError": maximum,
        "identical": maximum == 0.0,
    }
