# Feature 028 US3 Native Production Render Gate

Captured on 2026-08-22 from branch
`028-production-content-acceptance`, based on revision
`66a20cc42881d3747d836f9f45257c37f7f3e039` plus the current Feature 028
implementation worktree.

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
