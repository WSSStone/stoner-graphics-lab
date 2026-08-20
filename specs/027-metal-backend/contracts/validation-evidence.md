# Contract: Feature 027 Validation Evidence

## Evidence Tiers

| Tier | Required proof | May satisfy native gate |
|---|---|---|
| `deterministic` | Pure mapping, lifecycle, diagnostics, and MSL derivation | No |
| `native-offscreen` | Real Metal device, committed commands, completion, GPU readback | Yes |
| `visible-manual` | Real window/layer presentation, lifecycle log, accepted capture | Yes, presentation only |
| `cross-backend` | Independent native Metal and Vulkan evidence plus comparison | Yes |

Semantic expected-value calculation may assist comparison but cannot be emitted
as the native result.

## Required CI Jobs

1. Windows Debug shared build/tests and deterministic MSL derivation.
2. Windows strict Release shared build/tests and deterministic derivation.
3. Linux Debug shared build/tests and deterministic MSL derivation.
4. Linux strict Release shared build/tests and deterministic derivation.
5. macOS arm64 Debug plus native shader cook and Metal-device probe; native
   offscreen runs only when the probe succeeds.
6. macOS arm64 strict Release plus native shader cook, Metal-device probe, and
   Vulkan regression; native offscreen runs only when the probe succeeds.
7. Linux ASan/UBSan shared ownership/lifecycle suites.
8. Linux TSan shared concurrency suites.
9. macOS Intel Debug plus native shader cook and Metal-device probe; native
   offscreen runs only when the probe succeeds.
10. macOS Intel strict Release plus native shader cook, Metal-device probe, and
    Vulkan regression; native offscreen runs only when the probe succeeds.
11. Required physical M4 Pro self-hosted arm64 native-offscreen conformance,
    strict-cooked shader, triangle/deferred, failure, and lifecycle gates.
12. Required GitHub-hosted `macos-26-intel` x86_64 equivalent full native gates.

The macOS labels are pinned to `macos-26` and `macos-26-intel` for CPU-
architecture build/cook coverage. Runner/toolchain identity and Metal-device
probe results are recorded in artifacts. A hosted result without a real device
is `unavailable`, not a native pass. Job 11 uses
`[self-hosted, macOS, metal, arm64]`; job 12 uses `macos-26-intel`. Both are
required and fail on an unavailable Metal device or missing GPU readback. Job
11 additionally owns the required Metal/Vulkan cross-backend comparison. Job
12 executes the equivalent full Metal-only workload because MoltenVK 1.4.2 is
not usable on the hosted Intel `Apple Paravirtual device`; this does not relax
any Metal offscreen, strict-cooked, presentation-smoke, failure, or lifecycle
gate. A physical Intel Mac run is optional and manual evidence cannot replace
either automated native-offscreen job.

## Native Workloads

- Buffer/texture upload, graphics, compute, transfer, barrier, synchronization,
  and readback conformance.
- Strict-cooked native shader-library load and graphics/compute pipeline use.
- Triangle and deferred offscreen semantic probes.
- Failure injection at all FR-037 boundaries.
- 10,000-iteration Release lifecycle stress: iterations 1-1,000 warm up; sample
  RSS every 100 iterations from 1,100-10,000; retain all samples and compare the
  first ten versus final ten medians. Growth must be at most
  `max(16 MiB, 5% of first median)`.
- Visible 3,000-frame and 20-cycle presentation acceptance on real hardware.

## Comparison Normalization

Comparisons declare orientation, channel/colorspace conversion, row-padding
removal, StandardZ/ReversedZ interpretation, world-space normal decoding, and
per-probe numeric/image tolerances. Entity/material/light identity and Shader
Asset versions must match. A tolerance change is reviewed evidence, not a hidden
test adjustment.

The frozen Feature 027 tolerance set is:

| Semantic | Pass threshold |
|---|---|
| Final LDR/sRGB color | Absolute decoded error `<= 2/255` per channel |
| Linear HDR color or scalar lighting | Absolute error `<= max(1e-3, 1e-3 * abs(expected))` per component |
| Normalized depth | Absolute error `<= 1e-4` after StandardZ/ReversedZ normalization |
| Decoded world-space normal | Dot product of normalized vectors `>= 0.999` |
| Metallic or roughness | Absolute error `<= 1e-3` |
| 8-bit UNorm ambient occlusion | Absolute error `<= 2e-3` |
| Orientation and probe identity | Exact coordinate/semantic match after declared origin conversion |
| Row-padding removal | Exact logical row bytes and extent after padding is stripped |
| Whole accepted LDR capture | At least `99.5%` of compared pixels within `2/255` per channel and every pixel within `4/255` |

Any non-finite value, missing probe, identity mismatch, undeclared conversion,
or threshold breach fails the comparison. The report records both the tolerance
set version `metal-vulkan-tolerance-v1` and every observed maximum/error ratio.

## Artifact Rules

Every report conforms to `metal-validation-report.schema.json`, records the Git
revision and exact device/profile/shader evidence, and references artifacts by
relative path and SHA-256. Large raw logs and captures are CI artifacts; checked-
in `Validation/027/` stores normalized summaries, accepted small evidence, and
artifact digests. Unstable native pointer values and absolute developer paths
are forbidden.

## Completion Gate

Feature 027 closes only when all applicable public RHI operations are classified
and pass or are tied to a genuine reported device limitation; both Mac
architectures have passing required automated full native jobs under the
physical-M4/hosted-Intel policy;
all shared jobs pass; visible acceptance is
recorded; Vulkan/MoltenVK affected regressions pass; and no accepted native pass
originates from deterministic or semantic-oracle output.
