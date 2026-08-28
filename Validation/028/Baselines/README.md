# Feature 028 Baseline Evidence

This directory indexes reviewed production image baseline policy and hardware
evidence. Candidate captures remain ignored until semantic probes pass,
twenty-capture calibration rejects the mutation set, and a maintainer changes
the matching record to `accepted`.

All formal references and candidates are exactly 512-by-512. Semantic
classification uses workload-versioned 9-by-9 regions (four-pixel radius),
channel medians, minimum matching-sample fractions, and directional normal
coverage rather than single edge pixels. FLIP remains an exact same-size,
same-coordinate comparison with no registration or alignment; the calibration
set rejects a one-pixel whole-image translation.

Reference pixels are stored only as losslessly compressed PNG. The registry
hashes the PNG bytes, while admission separately proves that decoding produces
the exact reviewed RGB pixels. Raw PPM/RGBA captures, hardware packages, DDC,
and cooked generations remain ignored under `Build/Validation/` and are not
source-controlled evidence.

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
color-space, one-pixel translation, and opposite-normal mutations.

| Backend | Device class | Baseline ID | Calibration evidence SHA-256 |
| --- | --- | --- | --- |
| Metal | `macos.apple8.metal.rgba8` | `production-content-lantern-v2.macos.apple8.metal.rgba8.v1` | `b2452dc25e51e8c75c712511b287c871b24d9bc8232ac0b5ed5d901092b6d02f` |
| Vulkan (MoltenVK) | `macos.apple8.moltenvk.rgba8` | `production-content-lantern-v2.macos.apple8.moltenvk.rgba8.v1` | `e53e0fbcc5a7797f4ed4d35c4cb4f59311e4965abfde1a1c75160dbd47468d73` |

Both records use reference image SHA-256
`a3a2f8d402f3ea4c7396006572c8936f675eac47db6e18b04f0e53130a32af6a`
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
semantic gate now requires at least 60 percent of the bounded diagnostic region
to align with +Y at dot 0.8 or greater, so a future common backend winding
regression fails before FLIP without depending on one edge pixel. Blank,
stale-frame, origin, missing-geometry, material-swap, color-space, and
opposite-normal mutations were rejected; exact FLIP also rejects a one-pixel
translation. The maintainer explicitly accepted this corrected image on
2026-08-24. The former Sponza v1 record is
`superseded` and cannot be selected by the production registry.

| Backend | Device class | Accepted baseline ID | Calibration evidence SHA-256 |
| --- | --- | --- | --- |
| Metal | `macos.apple8.metal.rgba8` | `production-content-sponza-v2.macos.apple8.metal.rgba8.v1` | `2029332bd3c908c2e19adbbe2960d1f31d9faad07a3a2d7ac9d394a0d0c2defa` |
| Vulkan (MoltenVK) | `macos.apple8.moltenvk.rgba8` | `production-content-sponza-v2.macos.apple8.moltenvk.rgba8.v1` | `7ff0ed70e15b50851fdfabdc6993dfb8398d5cfd0d0f05ba73071ffd9cce7963` |

Both accepted reference images have SHA-256
`7e98feb07832b7d8dfdf87977a27ac1dbb955070575d2b092c985d1bb3ca8f4f`.

## Accepted Windows Vulkan Lantern v2 baseline

The maintainer synchronized revision
`738d66f150c0cd554267939328d0f8366336a2ff` to the x86_64 Windows device and
ran strict Release plus the hardware profile. The registry derived
`windows.discrete-vulkan.rgba8`; 1,000 lifecycle cycles, 2,000 captures, seven
readbacks, zero terminal owners, and stale-handle rejection completed. The
twenty 512-by-512 application-window captures were byte-identical. The
maintainer explicitly accepted the reviewed Lantern image on 2026-08-28.

| Backend | Device class | Accepted baseline ID | Calibration evidence SHA-256 |
| --- | --- | --- | --- |
| Vulkan | `windows.discrete-vulkan.rgba8` | `production-content-lantern-v2.windows.discrete-vulkan.rgba8.v1` | `6b8465cadf8941395a22225bdd92b866176db1bb1dcb6f8e7c59bb6dbb4ab31c` |

The accepted source PPM had SHA-256
`4147e4c5b0a2380ebcd3177785e06d964b2851132cbf4050c831c7b5dc25c95b`.
Its lossless PNG has SHA-256
`47d0abd805f608148323259c1878fcaee0e6cd9c49ade727960b002320618a61`;
decoded RGB was verified pixel-identical before registration.

This acceptance resolves the image `baseline-missing` result only. The same run
measured RSS growth of 62,119,936 bytes, above the required 16 MiB physical
limit, so the Windows hardware gate remains Failed. Fail-fast therefore did not
run the final Windows Sponza workload; T114 remains open until the RSS issue is
fixed and both workloads pass on the final shared revision.

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
