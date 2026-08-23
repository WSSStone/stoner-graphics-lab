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
