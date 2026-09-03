# Research: Renderer HDR Post-Processing & Output Transform

This document resolves every technical choice needed by Feature 029. Version
identities below are semantic contracts: changing equations, constants, gamut,
reference white, peak mapping, encoding, or negative-value policy requires a
new identity and affected workload revision.

## Decision 1 - Scene-to-Display Pipeline, Not a Backend Tone Map

**Decision**: Freeze SceneColor handoff -> manual exposure -> pre-tonemap
Composite -> SDR tone map or HDR viewing transform -> post-tonemap Composite ->
output-device gamut/encoding -> same-frame readback/presentation. Renderer owns
color decisions; RHI/native backends expose and execute exact presentation
configurations.

**Rationale**: This follows Unreal's working-space, viewing-transform, and
output-device separation while preserving the engine's RHI boundary. It creates
Feature 030's required seams: TAA before tone mapping and FXAA after it.

**Alternatives considered**: Backend-specific tone maps create policy drift. A
monolithic pass hides extension domains. Unreal pixel parity is a non-goal: the
ordering is intentionally Unreal-like, but the math uses pinned Khronos/ACES
authorities rather than Unreal's legacy ACES 1.x implementation.

Sources: [Unreal HDR output](https://dev.epicgames.com/documentation/unreal-engine/high-dynamic-range-display-output-in-unreal-engine),
[Unreal filmic tonemapper](https://dev.epicgames.com/documentation/unreal-engine/color-grading-and-the-filmic-tonemapper-in-unreal-engine),
[Unreal working color space](https://dev.epicgames.com/documentation/en-us/unreal-engine/working-color-space-in-unreal-engine).

## Decision 2 - SceneColor, Exposure, Negative Values, and Alpha

**Decision**: Canonical SceneColor is RGBA16F, scene-referred linear Rec.709/
sRGB primaries, D65, sampleCount=1. Manual exposure is `2^stops`, accepts
`[-16,+16]`, and runs once. NaN/Inf fails. Finite negative RGB remains visible
to handoff/insertion diagnostics but clamps to zero at the tone/view-transform
boundary; positive over-range stays unclamped until the selected transform.
Formal output alpha is opaque linear `1.0` and is not color transformed.
The canonical finite exposure samples are `{-16,-8,-1,0,+1,+8,+15,+16}` and the
explicit one-stop test pairs are `(-1,0)`, `(0,+1)`, and `(+15,+16)`.

**Rationale**: The format/space is user-frozen and matches current RGBA16F
lighting. Khronos PBR Neutral requires non-negative input, so one named boundary
policy is safer than curve-specific undefined behavior. The exposure range is
broad yet bounded for half-float intermediates.

**Alternatives considered**: Per-curve negative handling is ambiguous. Rejecting
all finite negative values at handoff prevents useful HDR diagnostics. Passing
source alpha through would make presentation blend semantics evidence-relevant.

## Decision 3 - Three Precisely Named SDR Curves

**Decision**:

- `Sdr.KhronosPbrNeutral.v1` is default and uses published Khronos constants
  `F90=0.04`, `Ks=0.76`, and `Kd=0.15`.
- `Sdr.NarkowiczAcesFit.v1` uses
  `clamp((x*(2.51*x+0.03))/(x*(2.43*x+0.59)+0.14),0,1)` per channel after
  exposure. It is a sampled ACES RRT+ODT fit, not an Academy Output Transform.
- `Sdr.ExtendedReinhardRec709.v1` uses Rec.709 luminance
  `Y=dot(rgb,[0.2126,0.7152,0.0722])`,
  `Yd=Y*(1+Y/(Lwhite^2))/(1+Y)`, and zero-safe `rgb*(Yd/Y)`, with the project-
  frozen, non-scene-derived `Lwhite=4.0`, followed by the common SDR
  display-linear `[0,1]` output clamp required by the profile contract.

**Rationale**: These implement the user's A/B/C selection with explicit
provenance. Khronos PBR Neutral is the required default. Fixed Reinhard white
avoids accidentally implementing automatic scene analysis.

**Alternatives considered**: Bare `ACES fitted` is misleading. Per-frame
Reinhard white is automatic exposure. Per-channel Extended Reinhard preserves
hue less reliably than luminance scaling.

Sources: [Khronos PBR Neutral](https://github.com/KhronosGroup/ToneMapping/blob/main/PBR_Neutral/README.md),
[Narkowicz fit](https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/),
[Reinhard paper](https://www-old.cs.utah.edu/docs/techreports/2002/pdf/UUCS-02-001.pdf).

## Decision 4 - Explicit Renderer-Owned SDR Transfer

**Decision**: SDR profiles write explicitly encoded values into UNorm
FinalOutput; native presentation applies no second transfer.

- `Sdr.sRGB.v1`: standard piecewise inverse EOTF with 0.0031308, 12.92,
  1.055, 2.4, and 0.055.
- `Sdr.BT709.v1`: ITU OETF with frozen published rounded constants alpha
  `1.099`, beta `0.018`, slope `4.5`, and exponent `0.45`.
- `Sdr.ExplicitGamma22.v1`: `pow(clamp(L,0,1),1/2.2)`; 2.2 is Stoner policy,
  not a claim about Unreal's unspecified explicit-gamma exponent.

Each profile defines its inverse for comparison. Alpha remains linear.

**Rationale**: One explicit shader transfer plus a resolved native color-space
contract is observable and prevents double encoding.

**Alternatives considered**: `_sRGB` FinalOutput risks implicit duplicate
transfer. Treating BT.709 as sRGB is incorrect because their transfers differ.

Source: [Khronos Data Format Specification](https://registry.khronos.org/DataFormat/specs/1.4/dataformat.1.4.html).

## Decision 5 - Official ACES 2 Lineage for HDR Viewing

**Decision**: Freeze `Hdr.ACES2.0.0_2025-04-04.Rec2020D65.v1` against official
ACES tag `v2.0.0+2025.04.04`, parent commit `35e1e6a`, and recorded submodule
commits. Build a repository-owned deterministic C++/GLSL transliteration and
validate it with canonical 1000/2000-nit Rec.2020 D65 vectors. HDR never passes
through an SDR curve.

**Rationale**: ACES 2 separates rendering appearance from display encoding and
ships appropriate 1000/2000-nit presets. A tag is reproducible; `main` is not.

**Alternatives considered**: Narkowicz is an SDR fit. An unspecified
"ACES-style" shoulder is unversioned. Runtime CTL/OCIO conflicts with offline
shader authority and adds an unnecessary runtime dependency.

Sources: [ACES rendering overview](https://docs.acescentral.com/system-components/output-transforms/technical-details/rendering-overview/),
[ACES output parameters](https://docs.acescentral.com/system-components/output-transforms/parameters/),
[ACES tagged release](https://github.com/aces-aswf/aces/releases/tag/v2.0.0%2B2025.04.04),
[ACES output transforms](https://github.com/aces-aswf/aces-output).

## Decision 6 - PQ Is Absolute Rec.2020 Display Encoding

**Decision**: `Hdr.PQ.Rec2020.1000.v1` and `.2000.v1` receive absolute
display-linear Rec.2020 nits from ACES, bound to target peak, divide by 10,000,
and apply ST-2084 inverse EOTF with `m1=2610/16384`,
`m2=(2523/4096)*128`, `c1=3424/4096`, `c2=(2413/4096)*32`, and
`c3=(2392/4096)*32`. Formal comparison decodes to nits/XYZ, never raw codes.

**Rationale**: PQ is absolute. Vulkan/Metal color-space selection declares
interpretation but does not perform the application's transfer.

**Alternatives considered**: Normalizing PQ by target peak is non-standard.
Comparing encoded values obscures luminance error and quantization.

Sources: [Khronos Data Format Specification PDF](https://registry.khronos.org/DataFormat/specs/1.4/dataformat.1.4.pdf),
[ITU-R BT.2100](https://www.itu.int/rec/R-REC-BT.2100/en).

## Decision 7 - Separate Standard scRGB and Apple EDR Packers

**Decision**: Logical profiles `Hdr.Linear.1000.v1` and `.2000.v1` resolve one
of two native encodings:

- Standard scRGB is linear Rec.709/D65 FP16 with `1.0 = 80 nits`, so 1000 and
  2000 nits encode as 12.5 and 25.0.
- Metal EDR is extended-linear sRGB FP16 with `1.0 = current native SDR/
  reference white`; encode `nits/nativeReferenceWhiteNits` and record queried
  reference white, headroom, and mode generation.

Raw values are never cross-compared; both decode to nits/XYZ. Human review does
not claim the panel was photometrically measured at the profile peak.

**Rationale**: scRGB and EDR share primaries/linearity but not luminance scale.

**Alternatives considered**: Applying 80-nit scRGB scale on Metal contradicts
Apple EDR semantics. Treating display clipping/system adaptation as a hidden
transform would destroy provenance.

Sources: [Microsoft HDR/scRGB guidance](https://learn.microsoft.com/en-us/windows/win32/direct3darticles/high-dynamic-range),
[Apple custom tone mapping](https://developer.apple.com/documentation/metal/performing-your-own-tone-mapping),
[Apple EDR session](https://developer.apple.com/videos/play/wwdc2021/10161/).

### Frozen Non-Visual Conformance Policy

The CPU double oracle uses component tolerance
`max(1e-10,1e-10*abs(expected))`. SDR decoded linear channels use `2/255`.
For every HDR decoded linear RGB nits component, the allowed GPU/native error is

`E=max(0.02 nit,0.0025*max(1 nit,abs(expected)),M*Qnative)`.

`Qnative` is the larger adjacent native-code decoded step bracketing the
expected value. `M=1.5` for packed-10 PQ and `M=2.0` for FP16 scRGB/Metal EDR.
For XYZ comparison, each component allowance is the absolute propagation of the
three RGB `E` values through the frozen RGB-to-XYZ matrix plus `1e-6`. NaN/Inf
always fails. The policy is computed from checked-in expected values and the
declared native encoding, not from observed implementation error, and changing
any constant requires a new profile/vector-set identity.

## Decision 8 - Exact Surface Format/Color-Space Pairs

**Decision**: RHI exposes surface-specific `(format,colorSpace)` pairs,
resolved swapchain state, HDR metadata support, EDR reference/headroom, and a
monotonic mode generation. Renderer profiles are not RHI enums. Unsupported
pairs fail without fallback.

Vulkan PQ requires packed 10-bit plus `VK_COLOR_SPACE_HDR10_ST2084_EXT`;
linear HDR requires FP16 plus `VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT` where
available. Vulkan may apply `VK_EXT_hdr_metadata` when the surface/device path
supports it. Metal PQ uses `BGR10A2Unorm`, ITU-R 2100 PQ, EDR opt-in, and
`EDRMetadata=nil`; the declared colorspace allows Core Animation color
management after Renderer produces final Rec.2020/ST-2084 values, without
enabling the `CAEDRMetadata` system tone mapper. Metal EDR uses RGBA16Float,
extended-linear sRGB, EDR opt-in, and `EDRMetadata=nil`; its engine-owned linear
packing also requests no system tone map. Neither path adds a backend shader
tone map.

**Rationale**: Capability varies by surface/display and can change after a
window move. Format alone cannot identify transfer or target peak.

**Alternatives considered**: Static device booleans become stale. Inferring
profile from format fails when 1000/2000 targets share storage. Simulated
swapchains are not native proof. Non-null Metal `EDRMetadata` was rejected for
the packed-10/PQ path because Apple's documented system-tone-mapping contract
requires an extended-range pixel format and a linear-transfer colorspace; its
reference path is RGBA16Float plus extended-linear ITU-R 2020. Public Unreal
documentation confirms PQ/scRGB output-device choices but does not expose the
current Metal layer configuration, so exact Unreal Metal parity is not claimed.

Sources: [Vulkan color spaces](https://registry.khronos.org/vulkan/specs/latest/man/html/VkColorSpaceKHR.html),
[Vulkan HDR metadata](https://registry.khronos.org/vulkan/specs/latest/man/html/VK_EXT_hdr_metadata.html),
[Apple HDR color spaces](https://developer.apple.com/documentation/metal/using-color-spaces-to-display-hdr-content),
[Apple custom tone mapping](https://developer.apple.com/documentation/metal/performing-your-own-tone-mapping).

## Decision 9 - Existing Render Graph Is the Sole Stage Authority

**Decision**: Forward/Deferred publish one typed SceneColor endpoint and append
the same output subgraph to `FRenderGraph`. Add only typed format/sample/usage
metadata and a compiled-schedule visitor for native execution. Readback and
presentation are explicit external side effects.

**Rationale**: The compiler already validates dependency, cycle, lifetime, and
culling rules, but the current token executor does not execute real GPU work.
The visitor preserves one authority without overstating existing behavior.

**Alternatives considered**: Copying stages into both renderer summaries or a
backend-private graph duplicates policy and violates ownership.

## Decision 10 - One Completed Frame for Readback and Presentation

**Decision**: Copy exact FinalOutput to requested readback before its Present
transition on one ordered submission. Publish only after required operations
succeed. The record shares frame token, mode generation, extent, workload,
settings fingerprint, format, color space, and metadata digest.

**Rationale**: Current Forward makes readback/presentation mutually exclusive;
Deferred readback lacks a following Present transition. Neither proves one
visible frame.

**Alternatives considered**: A separate offscreen rerender breaks provenance.
Feature 028's CPU aspect-fit helper scales and cannot be Feature 029 authority.

## Decision 11 - SDR Image Authority and HDR Human Authority Are Separate

**Decision**: Use four tiers: deterministic contracts, applicable native non-
visual execution, exact SDR v3 image authority, and physical macOS Metal HDR
live human review. HDR automation may verify math/format/metadata/submission/
readback/lifecycle/schema but never score or accept appearance.

The machine writes only `hdr-live-review-request.json`, whose maximum state is
`ready-for-live-review`. A maintainer separately authors
one new attestation file under `Validation/029/HDR/Attestations/` after viewing
all four modes. The runner has no way to generate `pass`. No HDR authority
PNG/video/capture or perceptual field is permitted.

**Rationale**: This directly implements the user's human-eye requirement. The
two-file handshake prevents readiness from masquerading as a human decision.

**Alternatives considered**: HDR PNG/FLIP scoring, screenshots, hosted displays,
Windows substitution, and automatically written `authoredByHuman` flags were
rejected.

A `fail` observation remains immutable evidence and blocks Feature 029 closeout.
After correction, the maintainer appends a linked superseding attestation; M8
requires one current non-superseded `pass` for each of the four profiles.

## Decision 12 - Preserve v2 and Create Fresh SDR v3 Authority

**Decision**: Feature 028 v2 schemas/images/reports remain unchanged. Feature
029 adds v3 keyed by workload/backend/device class/output profile/transform/
exposure/settings. It retains exact 512-by-512 lossless PNG, semantic probes,
one-pixel mutation rejection, FLIP, and explicit Candidate acceptance. No
alignment/crop/scale/warp/resampling. Fresh same-revision M4 Metal and Windows
Vulkan SDR evidence is required; the v2 Windows carry-forward is not reusable.

**Rationale**: Formal pixels intentionally change, so historical correctness
and new review require distinct immutable revisions.

**Alternatives considered**: Replacing v2, auto-promoting Candidates, and
sharing baselines across curves/profiles were rejected.

## Decision 13 - Capability-Correct Cross-Platform Validation

**Decision**: Windows/macOS/Linux run deterministic settings/math/graph/resize/
failure/schema suites and applicable SDR native coverage. Linux retains real
Lavapipe readback. Metal runs non-visual native contracts. Physical M4 Metal
owns SDR and all HDR live review; physical Windows Vulkan owns fresh SDR v3
only. Windows emits no HDR validation claim.

**Rationale**: This preserves cross-platform code quality while honoring the
accepted platform boundary. `Unsupported` and missing review remain incomplete.

**Alternatives considered**: Requiring Windows HDR contradicts the clarification;
dropping cross-platform deterministic HDR math permits platform drift.

## Decision 14 - Bounded, Non-Sensitive Evidence

**Decision**: Canonical JSON <=1 MiB; <=64 artifacts; each <=64 MiB; aggregate
<=256 MiB. SDR authority keeps bounded PNG/JSON; HDR authority keeps only
request/attestation JSON and digests. Raw output/logs stay in ignored
`Build/Validation/029/` or bounded CI retention. Reports use stable class tokens
and capability digests, not serials, usernames, absolute paths, or credentials.

**Rationale**: This continues Feature 028 policy without turning HDR review into
a capture archive.

**Alternatives considered**: Unbounded dumps and desktop captures add privacy
risk and are not authority.
