# Feature 028 US3 Native Production Render Gate

Captured on 2026-08-22 from branch
`028-production-content-acceptance`, based on revision
`66a20cc42881d3747d836f9f45257c37f7f3e039` plus the current Feature 028
implementation worktree.

## Sponza v2 camera and outward-normal correction (2026-08-24)

The maintainer explicitly accepted the frozen Sponza v2 atrium camera and the
corrected outward-facing image. Feature 024's canonical clockwise RHI winding
now adapts once at both native backend boundaries: Vulkan's positive-height
viewport and Metal's SPIRV-Cross `flip_vert_y` path both select native
counter-clockwise front faces for the canonical clockwise declaration.

Twenty visible 512x512 captures per backend were byte-identical within and
across Apple8 Metal and MoltenVK. The accepted reference SHA-256 is
`25cae6c615e49aa53358a602e3d7130d46be98b4dc8e2c12a3651c1f10d0a441`.
Both backends returned the same diagnostic world normal
`(-0.0000304, +0.999512, -0.00919342)`, and the semantic gate now rejects a
Sponza v2 diagnostic normal whose +Y component is below 0.8 before FLIP.

Accepted post-registration runs selected the exact Metal and MoltenVK v2
records, passed 20 semantic probes, and measured zero mean, p95, maximum, and
bad-pixel-fraction FLIP error. All blank, stale-frame, origin,
missing-geometry, material-swap, color-space, and opposite-normal mutations
were rejected. The final bounded visible runs completed with exit code 0 on
both backends and retained zero owners plus stale-handle rejection on every
cycle. Metal observed 9,715,712 bytes of cycle-2-to-cycle-20 RSS growth;
MoltenVK observed 57,278,464 bytes. These 20-frame calibration RSS values are
explicitly observational because Sponza is not a regular-profile root. Its
authoritative lifecycle/RSS acceptance remains the separate 1,000-cycle,
cycle-20-warm-up hardware gate required by T126.

## Scope And Method

The strict cooked Khronos Lantern root was loaded, realized, rendered, and
released for 20 complete cycles on the physical Apple M4 Pro. Cycles 1-2 are
included warm-up cycles. The lifecycle gate compares the cycle-2 RSS sample
with the terminal cycle-20 sample and permits at most 16 MiB of positive
growth.

Each cycle executes the full Deferred production composition and a bounded
Forward smoke from the same strict generation. The native test requires
backend proof and synchronized submission, six Deferred attachment readbacks,
the final composition readback, and the Forward color readback. It also
requires all Asset, Renderer, RHI, native, and presentation ownership counters
to return to zero and rejects the stale handle after release.

## Physical M4 Pro Results

| Backend | Cycles | Captures | Readbacks | Warm-up RSS | Terminal RSS | Positive growth | Result |
|---|---:|---:|---:|---:|---:|---:|---|
| Vulkan through MoltenVK | 20 | 40 | 7 | 662,126,592 B | 605,126,656 B | 0 B | PASS |
| Metal | 20 | 40 | 7 | 476,364,800 B | 479,756,288 B | 3,391,488 B | PASS |

The latest Metal evidence-format verification remains below the 16 MiB limit
of 16,777,216 bytes by 13,385,728 bytes. An earlier passing run on the same
worktree measured 15,925,248 bytes of positive growth, so allocator variation
must still be re-evaluated by the 1,000-cycle hardware closeout gate.

Both backend runs reported:

- native execution through the requested backend with synchronized submission;
- `BaseColorAO`, `NormalRoughness`, `EmissiveMetallic`, `Depth`,
  `LightingAccumulation`, and `FinalOutput` Deferred readbacks;
- one `ForwardColor` readback and 20 Forward captures;
- 20 lifecycle samples with every ownership counter at its terminal baseline;
- stale-handle rejection after each release; and
- exit code 0.

## Reproduction

The Vulkan command uses the prepared strict publication and the
`Mac-Vulkan.json` target profile:

```text
STONER_REQUIRE_VULKAN_PRODUCTION=1 \
STONER_PRODUCTION_VULKAN_PUBLICATION_ROOT=<publication-root> \
STONER_PRODUCTION_VULKAN_LEASE_ROOT=<lease-root> \
STONER_PRODUCTION_VULKAN_GENERATION=<generation-digest> \
STONER_PRODUCTION_VULKAN_TARGET_PROFILE=Config/AssetCooker/Profiles/Production/Mac-Vulkan.json \
Build/Mac/Release/Tests/StonerTest --suite production-content-vulkan-native
```

The Metal command uses the prepared strict publication and the
`Mac-Metal-Arm64.json` target profile:

```text
STONER_REQUIRE_METAL_PRODUCTION=1 \
STONER_PRODUCTION_METAL_PUBLICATION_ROOT=<publication-root> \
STONER_PRODUCTION_METAL_LEASE_ROOT=<lease-root> \
STONER_PRODUCTION_METAL_GENERATION=<generation-digest> \
STONER_PRODUCTION_METAL_TARGET_PROFILE=Config/AssetCooker/Profiles/Production/Mac-Metal-Arm64.json \
Build/Mac/Release/Tests/StonerTest --suite production-content-metal-native
```

Publication and lease paths are represented by relative evidence tokens here;
host-private absolute temporary paths are not part of checked-in evidence.
