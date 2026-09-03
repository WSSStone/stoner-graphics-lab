#!/usr/bin/env python3
"""Shared decoded-domain helpers for Feature 029 validation.

These functions only compare already-decoded values. They never generate the
checked-in expected vector authority and never compare raw scRGB/Metal EDR
codes across native encodings.
"""

from __future__ import annotations

import math
from typing import Any


HDR_ABSOLUTE_FLOOR_NITS = 0.02
HDR_RELATIVE_FRACTION = 0.0025
PQ_QUANTIZATION_MULTIPLIER = 1.5
FP16_QUANTIZATION_MULTIPLIER = 2.0
XYZ_PROPAGATION_EPSILON = 1e-6


def _require_finite_vector(value: list[float], name: str) -> None:
    if len(value) != 3 or not all(math.isfinite(component) for component in value):
        raise ValueError(f"{name} must contain three finite components")


def matrix_vector(matrix: list[list[float]], value: list[float]) -> list[float]:
    """Multiply a finite 3x3 matrix by a finite three-component vector."""
    _require_finite_vector(value, "value")
    if len(matrix) != 3 or any(len(row) != 3 for row in matrix):
        raise ValueError("matrix must be 3x3")
    if not all(math.isfinite(component) for row in matrix for component in row):
        raise ValueError("matrix must be finite")
    return [sum(row[index] * value[index] for index in range(3))
            for row in matrix]


def _pq_encode(nits: float) -> float:
    m1, m2 = 0.1593017578125, 78.84375
    c1, c2, c3 = 0.8359375, 18.8515625, 18.6875
    normalized = min(1.0, max(0.0, nits / 10000.0))
    power = normalized ** m1
    return ((c1 + c2 * power) / (1.0 + c3 * power)) ** m2


def _pq_decode(encoded: float) -> float:
    m1, m2 = 0.1593017578125, 78.84375
    c1, c2, c3 = 0.8359375, 18.8515625, 18.6875
    power = min(1.0, max(0.0, encoded)) ** (1.0 / m2)
    numerator = max(power - c1, 0.0)
    denominator = max(c2 - c3 * power,
                      float.fromhex("0x0.0000000000001p-1022"))
    return 10000.0 * (numerator / denominator) ** (1.0 / m1)


def _half_step(native: float) -> float:
    magnitude = abs(native)
    if magnitude < 2.0 ** -14:
        return 2.0 ** -24
    return 2.0 ** (math.floor(math.log2(magnitude)) - 10)


def native_quantization_step_nits(nits: float, encoding: str,
                                  reference_white_nits: float) -> float:
    """Return the frozen native-code quantization bound in decoded nits."""
    if not math.isfinite(nits):
        raise ValueError("nits must be finite")
    if not math.isfinite(reference_white_nits) or reference_white_nits <= 0.0:
        raise ValueError("reference white must be finite and positive")
    if encoding == "pq-rec2020":
        code = _pq_encode(nits) * 1023.0
        lower, upper = math.floor(code), math.ceil(code)
        if lower == upper:
            lower, upper = max(0, lower - 1), min(1023, upper + 1)
        return max(abs(nits - _pq_decode(lower / 1023.0)),
                   abs(_pq_decode(upper / 1023.0) - nits))
    if encoding not in {"scrgb80", "metal-edr"}:
        raise ValueError(f"unknown HDR native encoding {encoding!r}")
    scale = 80.0 if encoding == "scrgb80" else reference_white_nits
    return _half_step(nits / scale) * scale


def decoded_rgb_nits_tolerance(expected_nits: list[float], encoding: str,
                               reference_white_nits: float) -> list[float]:
    """Apply the immutable M0 absolute, relative, and native-step policy."""
    _require_finite_vector(expected_nits, "expected_nits")
    multiplier = (PQ_QUANTIZATION_MULTIPLIER
                  if encoding == "pq-rec2020"
                  else FP16_QUANTIZATION_MULTIPLIER)
    return [max(HDR_ABSOLUTE_FLOOR_NITS,
                HDR_RELATIVE_FRACTION * max(1.0, abs(component)),
                multiplier * native_quantization_step_nits(
                    component, encoding, reference_white_nits))
            for component in expected_nits]


def matrix_propagated_xyz_tolerance(
        matrix: list[list[float]], rgb_nits_tolerance: list[float]) -> list[float]:
    """Propagate independent RGB-nits error bounds into XYZ absolute bounds."""
    _require_finite_vector(rgb_nits_tolerance, "rgb_nits_tolerance")
    if any(component < 0.0 for component in rgb_nits_tolerance):
        raise ValueError("RGB tolerance cannot be negative")
    if len(matrix) != 3 or any(len(row) != 3 for row in matrix):
        raise ValueError("matrix must be 3x3")
    if not all(math.isfinite(component) for row in matrix for component in row):
        raise ValueError("matrix must be finite")
    return [sum(abs(row[index]) * rgb_nits_tolerance[index]
                for index in range(3)) + XYZ_PROPAGATION_EPSILON
            for row in matrix]


def compare_decoded_hdr(actual_nits: list[float], expected_nits: list[float],
                        encoding: str, reference_white_nits: float,
                        xyz_matrix: list[list[float]]) -> dict[str, Any]:
    """Compare decoded HDR values and report RGB/XYZ bounds and outcomes."""
    _require_finite_vector(actual_nits, "actual_nits")
    _require_finite_vector(expected_nits, "expected_nits")
    rgb_tolerance = decoded_rgb_nits_tolerance(
        expected_nits, encoding, reference_white_nits)
    actual_xyz = matrix_vector(xyz_matrix, actual_nits)
    expected_xyz = matrix_vector(xyz_matrix, expected_nits)
    xyz_tolerance = matrix_propagated_xyz_tolerance(
        xyz_matrix, rgb_tolerance)
    return {
        "comparisonDomain": "absolute-nits-xyz",
        "nativeEncoding": encoding,
        "rawCodeComparison": False,
        "rgbTolerance": rgb_tolerance,
        "xyzTolerance": xyz_tolerance,
        "rgbPassed": all(abs(actual_nits[index] - expected_nits[index]) <=
                         rgb_tolerance[index] for index in range(3)),
        "xyzPassed": all(abs(actual_xyz[index] - expected_xyz[index]) <=
                         xyz_tolerance[index] for index in range(3)),
    }
