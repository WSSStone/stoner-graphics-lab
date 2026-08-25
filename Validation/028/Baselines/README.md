# Feature 028 Baseline Evidence

This directory indexes reviewed production image baseline policy and hardware
evidence. Candidate captures remain ignored until semantic probes pass,
twenty-capture calibration rejects the mutation set, and a maintainer changes
the matching record to `accepted`.

## Accepted macOS Apple8 baselines

The corrected native winding exposed that the former Lantern
`production-content-v1` image showed the back face and relied on a light placed
behind the intended surface. Those records are retained as `superseded` and
cannot be consumed. Revision `production-content-lantern-v2` preserves the
frozen identity camera, validates the intended approximately -X world normal,
and moves the key light to the camera-facing side. Its Metal and
MoltenVK/Vulkan records were explicitly accepted by the maintainer after their
twenty same-revision captures produced zero FLIP error and the calibration
suite rejected blank, stale-frame, origin, missing-geometry, material-swap,
color-space, and opposite-normal mutations.

| Backend | Device class | Baseline ID | Calibration evidence SHA-256 |
| --- | --- | --- | --- |
| Metal | `macos.apple8.metal.rgba8` | `production-content-lantern-v2.macos.apple8.metal.rgba8.v1` | `dc90dcf5e4c60a28084a0518ff9aad1be00ca745c0860aa601ebbcad24702b4a` |
| Vulkan (MoltenVK) | `macos.apple8.moltenvk.rgba8` | `production-content-lantern-v2.macos.apple8.moltenvk.rgba8.v1` | `bc32bb96fb422300ccb00d32072bf1ca523e4924085e327d542c8690448104af` |

Both records use reference image SHA-256
`f208b18d3db701282955066716e3ab6bcdee3de830b342dcf0f0e5baac798a42`
and remain selected only by exact workload, backend, registry-derived device
class, and capability-signature equality. The `StateFixtures/` records retain
all four non-accepted lifecycle states against a fixture-only workload so the
production selector must continue to reject them.

Feature 028 freezes `sampleCount=1` and performs no anti-aliasing or general
post-processing. That visibly aliased output is the reviewed v2 authority for
this phase; a future post-processing/anti-aliasing feature must advance the
workload revision and repeat calibration rather than silently replacing it.

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

## Final local consumption evidence

Final implementation revision
`c82e51790643fc6583a80240239f6f4123cc0df1` consumed the Lantern v2 Metal and
MoltenVK records without fallback in post-prime 20-frame visible gates. Each
reported 20 semantic probes, zero FLIP error, 40 captures, seven readbacks, zero
terminal owners, and stale-handle rejection. Its clean predecessor
`426d8617fe8558114110b09a60260de3895da82f` consumed all four accepted Apple8
records without fallback and rendered 1,000
visible lifecycle cycles per workload and reported 20 semantic probes plus zero
FLIP error for both Lantern v2 and Sponza v2. Independent consumer verification
accepted 4,369 artifacts with manifest SHA-256
`5cd36ce6a287e40884d77f538b838a5f06bce3edd23c76258a1b16cd3918bfdb`
and summary SHA-256
`6c456e04daf300a02fc5e7668034094d5760c859ca795923059e1a22e72c1c83`.

The final pre-prime binary also passed separate 20-frame visible MoltenVK gates
for both workloads with exact registry-derived baseline selection. Final
physical CI remains responsible for the same-revision 1,000-cycle Vulkan and
Metal closeout record.
