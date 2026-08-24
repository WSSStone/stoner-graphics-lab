# Feature 028 Baseline Evidence

This directory indexes reviewed production image baseline policy and hardware
evidence. Candidate captures remain ignored until semantic probes pass,
twenty-capture calibration rejects the mutation set, and a maintainer changes
the matching record to `accepted`.

## Accepted macOS Apple8 baselines

The Metal and MoltenVK/Vulkan records for workload revision
`production-content-v1` were explicitly accepted by the maintainer after their
twenty same-revision captures produced zero FLIP error and the calibration
suite rejected blank, stale-frame, origin, missing-geometry, material-swap,
and color-space mutations.

| Backend | Device class | Baseline ID | Calibration evidence SHA-256 |
| --- | --- | --- | --- |
| Metal | `macos.apple8.metal.rgba8` | `production-content-v1.macos.apple8.metal.rgba8.v1` | `48b280d184ce78234f986b109c5c2511571d32e16ae65fdb99f01319f0af26e1` |
| Vulkan (MoltenVK) | `macos.apple8.moltenvk.rgba8` | `production-content-v1.macos.apple8.moltenvk.rgba8.v1` | `1f7637393a3d88a028f9966c43bb358a0a3727bc4e3165a577f0d0d67bdd9ec4` |

Both records use reference image SHA-256
`52a0f9b1945394186c55d46df6138ed18c82fa3f8c05cc0de04cc232beb4f431`
and remain selected only by exact workload, backend, registry-derived device
class, and capability-signature equality. The `StateFixtures/` records retain
all four non-accepted lifecycle states against a fixture-only workload so the
production selector must continue to reject them.

## Accepted Sponza v2 image baseline

The explicitly accepted camera and corrected outward-facing surface convention
for `production-content-sponza-v2` produced 20 byte-identical captures per
backend on the Apple8 Metal and MoltenVK device classes. Both backends observe
the frozen diagnostic world normal as approximately
`(-0.0000304, +0.999512, -0.00919342)` and all retained GBuffer, depth,
lighting, final-output, and Forward readback digests are byte-identical. The
semantic gate now requires the diagnostic normal's +Y component to be at least
0.8, so a future common backend winding regression fails before FLIP. Blank,
stale-frame, origin, missing-geometry, material-swap, color-space, and
opposite-normal mutations were rejected. The maintainer explicitly accepted
this corrected image on 2026-08-24. The former Sponza v1 record is
`superseded` and cannot be selected by the production registry.

| Backend | Device class | Accepted baseline ID | Calibration evidence SHA-256 |
| --- | --- | --- | --- |
| Metal | `macos.apple8.metal.rgba8` | `production-content-sponza-v2.macos.apple8.metal.rgba8.v1` | `a5052e00761a902b0b8d1fb15a1e85605b38d604f71e00a0b3770dda8d1f94ae` |
| Vulkan (MoltenVK) | `macos.apple8.moltenvk.rgba8` | `production-content-sponza-v2.macos.apple8.moltenvk.rgba8.v1` | `c4136d4b298dee375dc07add43d69472f5091e32985477907b56cee4b6d671b1` |

Both accepted reference images have SHA-256
`25cae6c615e49aa53358a602e3d7130d46be98b4dc8e2c12a3651c1f10d0a441`.
