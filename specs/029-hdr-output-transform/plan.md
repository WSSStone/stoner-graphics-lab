# Implementation Plan: Renderer HDR Post-Processing & Output Transform

**Branch**: `029-hdr-output-transform` | **Date**: 2026-09-02 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/029-hdr-output-transform/spec.md`

## Summary

Feature 029 creates one backend-neutral, Render Graph-owned path from Forward
or Deferred RGBA16F scene-referred linear Rec.709/D65 `SceneColor` to a formal
display output. The canonical order is manual exposure, ordered pre-tonemap
operations, one versioned SDR tone map or ACES 2 HDR viewing transform, ordered
post-tonemap operations, one versioned output-device gamut/transfer transform,
then same-frame readback and/or presentation. Renderer owns color policy; RHI
describes surface-specific format/color-space/metadata capability; Vulkan and
Metal adapters perform only the selected native configuration and execution.

The initial matrix contains three SDR output encodings and four HDR targets:
sRGB, BT.709, explicit gamma 2.2, 1000/2000-nit Rec.2020 PQ, and 1000/2000-nit
linear scRGB/Metal EDR. SDR supports Khronos PBR Neutral v1 (default), a
precisely named Narkowicz ACES fit v1, and a Rec.709-luminance Extended Reinhard
v1. HDR never runs those curves; it uses the pinned official ACES 2.0 output
transform lineage. Standard scRGB and Apple EDR have separate native luminance
packers and are compared only after decoding to absolute nits/XYZ.

Validation separates deterministic color/graph contracts, native non-visual
execution, exact 512-by-512 SDR image authority, and macOS Metal live HDR human
review. Feature 028 v2 images and their single-sample/no-post-processing meaning
remain immutable. Changed SDR output creates v3 Candidates and requires fresh
physical M4 Metal and Windows Vulkan review; HDR has no authoritative PNG or
automated perceptual score and closes only through a maintainer-authored live-
view attestation for all four Metal HDR profiles. Windows makes no HDR
validation claim.

## Technical Context

**Language/Version**: C++20 with traditional public/private headers and sources; Objective-C++20 remains private to Metal; GLSL with checked-in SPIR-V; Python 3 standard-library validation scripts

**Primary Dependencies**: Existing Core, RHI, Renderer, Application, Asset, AssetCooker, Vulkan, Metal, Demo, Render Graph, production-content acceptance, SCons 4.10.1, offline GLSL/SPIR-V validation, existing SPIRV-Cross/offline metallib path; official ACES package `v2.0.0+2025.04.04` (`35e1e6a`) is the pinned reference authority for a repository-owned deterministic CPU/GLSL implementation and vectors, not a runtime dependency

**Storage**: Process-local frame plans, graph declarations, presentation capability snapshots, native resources, readback records, and diagnostics; checked-in shader source/SPIR-V, canonical profile/schema files, bounded PNG/JSON evidence, and immutable accepted SDR references; raw readbacks/logs remain under ignored `Build/Validation/029/`; no database, runtime shader compilation, automatic exposure history, or temporal state

**Testing**: Existing `StonerTest` suites plus output-math vectors, insertion/graph contracts, RHI/mock surface capabilities, Vulkan/Metal native readback/presentation probes, resize/failure/lifecycle matrices, SDR v3 calibration/acceptance, Python schema/runner tests, Windows/macOS/Linux CI, fresh physical Windows Vulkan and M4 Metal SDR authority, and M4 Metal human live review for PQ/EDR

**Target Platform**: Windows x64 Vulkan, macOS arm64/Intel Metal, and Linux x64 Vulkan/Lavapipe for deterministic and applicable SDR/native coverage; physical M4 Metal for SDR and HDR; physical Windows Vulkan for fresh SDR only; no Windows HDR validation or authority

**Project Type**: Cross-platform graphics engine, desktop demo, and validation tooling

**Performance Goals**: Frame-local O(width*height) work; the empty-insertion default uses no more than three mandatory fullscreen output passes plus an optional exact readback copy, performs no CPU readback on presentation-only frames, and has bounded stage/resource counts; elapsed time is recorded as observation rather than a hardware-independent qualification

**Constraints**: Canonical SceneColor is RGBA16F linear Rec.709/sRGB primaries D65, sampleCount=1, opaque formal alpha; manual exposure is finite and applied once; no AA/temporal state, bloom, DoF, motion blur, auto exposure, HDR screenshot scoring, runtime shader compilation, silent output fallback, double transfer, alignment, cropping, scaling, warping, or resampling; JSON <=1 MiB, <=64 artifacts, each <=64 MiB, aggregate <=256 MiB

**Scale/Scope**: One shared output subgraph, two renderer producer strategies, seven output-device profiles, three SDR curves, one versioned HDR viewing-transform lineage with 1000/2000-nit presets, two native backends, at least 32 HDR/16-per-profile canonical vectors, 20 deterministic repeats, 100 resize/mode/lifecycle transitions, and two production workloads upgraded from v2 to v3 for formal SDR output

## Constitution Check

*GATE: Passed before research and re-checked after Phase 1 design.*

- [x] **Spec-Driven Development**: `spec.md` contains five prioritized user stories, 46 functional requirements, 16 measurable success criteria, and five recorded clarification decisions with no unresolved marker.
- [x] **Decoupled Architecture**: Renderer owns color and graph policy, RHI owns backend-neutral presentation contracts, and only Vulkan/Metal implementation files call native APIs. Asset remains immutable CPU content.
- [x] **Design Pattern Discipline**: Versioned tone/view/output transforms are Strategy implementations; ordered pre/post operations form a validated Composite; planning, graph declaration, execution, native presentation, and evidence remain separate responsibilities.
- [x] **Multi-API Support**: One SceneColor/output plan drives Vulkan and Metal. Capability discovery may return `Unsupported`; it never selects a different visual policy or backend-private correction.
- [x] **Advanced Graphics Readiness**: The pre-tonemap seam is the Feature 030 TAA replacement point, the post-tonemap seam is its FXAA point, and later GI, Meshlet, and ray-tracing producers feed the same SceneColor contract.
- [x] **Naming Conventions**: Proposed C++ APIs use PascalCase and project/UE5-style `F`, `E`, `I`, and `b` prefixes.
- [x] **Cross-Platform Compatibility**: Deterministic contracts build and run on Windows, macOS, and Linux; platform code stays in backend-private files. The user-selected macOS-only HDR human authority is a capability-correct validation scope, not permission for single-platform Renderer code.
- [x] **Automated Cross-Platform Validation**: CI covers deterministic and SDR duties on all three platforms plus applicable native probes. HDR automation is explicitly non-visual; physical M4 Metal live review is a manual closeout gate, while Windows explicitly makes no HDR validation claim.

### Post-Design Re-check

The data model and contracts preserve Constitution v1.4.0 dependency direction.
`FHDRPostProcessPipeline` consumes a typed SceneColor producer endpoint and emits
a compiled backend-neutral plan; it cannot see Vulkan, Metal, windows, Assets,
or validation files. `IRHIPresentationSurface` exposes dynamic surface-specific
capabilities without acquiring Renderer policy. Native adapters resolve exact
format/color-space pairs and fail closed. Demo/Validation owns workload,
physical-device orchestration, image comparison, and human-review requests.
No exception or complexity waiver is required. The deliberate absence of
Windows HDR validation is recorded in the spec, roadmap, report schema, and
aggregation contract, while Windows still retains all required deterministic
and fresh SDR authority.

## Project Structure

### Documentation (this feature)

```text
specs/029-hdr-output-transform/
|-- spec.md
|-- plan.md
|-- research.md
|-- data-model.md
|-- quickstart.md
|-- checklists/
|   `-- requirements.md
|-- contracts/
|   |-- output-pipeline.md
|   |-- output-device-profiles.md
|   |-- presentation-readback-lifecycle.md
|   |-- validation-evidence.md
|   |-- output-device-profile.schema.json
|   |-- output-validation-report.schema.json
|   |-- sdr-image-baseline-v3.schema.json
|   |-- hdr-live-review-request.schema.json
|   `-- hdr-live-view-attestation.schema.json
`-- tasks.md                         # Created later by /speckit.tasks
```

### Source Code (repository root)

```text
Source/RHI/Public/RHI/
|-- ERHIFormat.h                     # Add packed 10-bit presentation format
|-- ERHIPresentationColorSpace.h     # Native color-space vocabulary only
|-- FRHIHDRMetadata.h                # Versioned mastering/content metadata
|-- FRHIPresentationCapabilities.h   # Surface-specific format/space pairs
|-- FRHIResolvedPresentationState.h  # Actual immutable surface/swapchain state
|-- FRHISwapchainDesc.h              # Requested color space/metadata
|-- IRHIPresentationSurface.h        # Dynamic capability query
`-- IRHISwapchain.h                  # Resolved state and reconfigure generation

Source/Renderer/Public/Renderer/
|-- FHDRSceneColorHandoff.h          # Typed producer endpoint
|-- FOutputTransformSettings.h       # Exposure/version/profile policy
|-- FPostProcessInsertion.h          # Pre/post Strategy + ordered Composite
|-- FOutputTransformPlan.h           # Immutable resolved plan/fingerprint
|-- FOutputTransformGraphDeclaration.h
|-- FHDRPostProcessPipeline.h        # Prepare and declare shared subgraph
|-- FOutputTransformExecutor.h       # RHI-only execution bindings/result
`-- FOutputTransformDiagnostics.h

Source/Renderer/Private/
|-- FOutputTransformSettingsValidator.cpp
|-- FOutputTransformReference.h      # Private CPU oracle/test seam
|-- FOutputTransformReference.cpp    # CPU conformance oracle
|-- FPostProcessInsertion.cpp
|-- FOutputTransformGraphBuilder.cpp
|-- FOutputTransformShaderParameters.cpp
|-- FHDRPostProcessPipeline.cpp
`-- FOutputTransformExecutor.cpp

Content/Shaders/PostProcess/
|-- Fullscreen.vert
|-- Fullscreen.vert.spv
|-- OutputTransform.frag
|-- OutputTransform.frag.spv
`-- *.shader.json

Source/Backend/Vulkan/               # Surface pair query, native swapchain,
Source/Backend/Metal/                # layer format/space/EDR/metadata adapters
Demo/StonerDemo/Private/             # Producer handoff and authority commands
Config/Validation/OutputTransform/   # Profiles, workloads, bounded policies
Tests/                               # Math/graph/RHI/native/evidence tests
.github/scripts/                     # 029 runner, schema and evidence checks
.github/workflows/
`-- feature-029-hdr-output.yml
Validation/029/                      # Bounded reports and human attestations
```

**Structure Decision**: Extend existing ownership boundaries rather than add a
module. Forward and Deferred stop at one typed RGBA16F SceneColor endpoint and
append the same output subgraph. The generic Render Graph compiler receives the
minimal typed format/sample/usage information and an execution visitor needed
to interleave real barriers and passes; existing renderer-specific summary
declarations remain producer diagnostics only. The RHI gains surface-specific
presentation state because color-space and EDR capability can change when a
window moves displays. Renderer profile identities deliberately remain richer
than RHI native color spaces: 1000/2000-nit variants can share a native format
while retaining distinct color transforms and evidence keys.

## Implementation Strategy

### M0 - Freeze Color Authority and Canonical Vectors

1. Pin all public identities and constants from `research.md`: exposure range
   `[-16,+16]` stops, opaque alpha, negative-to-zero boundary policy, the three
   SDR curve implementations, Extended Reinhard `Lwhite=4.0`, explicit gamma
   2.2, ACES package/tag/submodule provenance, PQ constants, gamut matrices,
   the distinct scRGB80/Metal EDR packers, exposure samples
   `{-16,-8,-1,0,+1,+8,+15,+16}`, and the decoded-domain tolerance policies.
2. Implement a double-precision CPU reference oracle and canonical JSON vectors
   before GPU shaders. Preserve normative source/value provenance and generate
   neither expected values nor acceptance decisions at runtime.
3. Freeze decoded comparison domains: SDR linear Rec.709, PQ absolute linear
   Rec.2020 nits/XYZ, scRGB/EDR absolute nits/XYZ. Never compare raw scRGB and
   Metal EDR code values.
4. Add deterministic negative/non-finite/near-limit, monotonicity, one-stop,
   and round-trip decode tests. Narkowicz is never labelled as an Academy ACES
   transform; Unreal pixel parity is a non-goal.
5. Freeze CPU comparison at `max(1e-10,1e-10*abs(expected))`; freeze HDR GPU
   comparison per decoded linear RGB nits component as
   `max(0.02,0.0025*max(1,abs(expected)),M*Qnative)`, using `M=1.5` for packed-10
   PQ and `M=2.0` for FP16 linear HDR, with XYZ error derived by absolute matrix
   propagation plus `1e-6`. These values are authority inputs, never tuned after
   observing an implementation result.

### M1 - Presentation Capability and Resolved-State RHI

1. Add `R10G10B10A2_UNorm`, `ERHIPresentationColorSpace`, versioned HDR
   metadata, and exact supported `(format,colorSpace)` records. Keep target peak
   and viewing transform in Renderer, not RHI.
2. Query capabilities from `IRHIPresentationSurface`, not only static device
   capabilities. Validate unique pairs, metadata support, current EDR reference
   white/headroom, and a monotonic capability generation.
3. Extend swapchain requests and expose immutable resolved extent, format,
   color space, metadata digest, and generation. Reconfigure transactionally;
   unsupported pairs and stale generations fail without SDR fallback.
4. Extend mock RHI tests for display moves, mode changes, zero drawable, exact
   resolution, stale image rejection, and first-failure stability.

### M2 - Shared Renderer Handoff and Ordered Output Subgraph

1. Replace the formal Forward RGBA8 target and Deferred Composition RGBA8
   output with one `FHDRSceneColorHandoff`: RGBA16F linear Rec.709/D65,
   sampleCount=1, exact extent, opaque alpha, strategy/view/frame identity.
   Deferred composition and Forward transparent handoff finish before it.
2. Resolve `FOutputTransformSettings` into one immutable plan. A transform
   Strategy owns one version identity; pre/post operations are a Composite with
   stable IDs, order, dependencies, domains, and bounded resource declarations.
3. Append the output subgraph to the existing `FRenderGraph`. Declare manual
   exposure, optional insertion stages, tone/view transform, output-device
   encoding, formal output, requested readback, and presentation as typed
   resources/passes. External readback/presentation side effects prevent cull.
4. Add the minimal Render Graph typed resource and compiled-schedule visitor
   support required for real interleaved transitions. Do not create a second
   graph or claim the existing token-only executor performs native work.
5. Enforce a single formal writer and output identity. Empty insertion arrays
   preserve the same effective plan; debug bypass emits a separate non-
   authoritative diagnostic output and cannot mutate the formal plan.

### M3 - Deterministic GPU Transform Programs

1. Implement repository-owned GLSL from the frozen CPU oracle, preserving
   stage/version identities in shader constants and pipeline keys. Use
   RGBA16F intermediates and declared UNorm/packed10/FP16 final targets.
2. Offline-compile checked-in SPIR-V, validate it, derive normalized MSL through
   the Feature 027 path, and finalize architecture-specific metallibs. Runtime
   fallback compilation is prohibited.
3. Stage shader assets in `Source/Renderer/SConscript` and add them to strict-
   cooked production closure/DDC inputs so shader or interface changes are
   revision-visible.
4. Compare GPU readback against canonical decoded reference vectors per
   profile; transfer ownership is asserted once from shader through resolved
   presentation state.

### M4 - Native Vulkan and Metal Presentation

1. Vulkan enumerates exact `VkSurfaceFormatKHR` pairs. SDR uses the declared
   nonlinear color space with Renderer-encoded UNorm; PQ requires packed 10-bit
   plus `VK_COLOR_SPACE_HDR10_ST2084_EXT`; linear scRGB requires FP16 plus
   `VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT`. `VK_EXT_hdr_metadata` is applied
   when available but never treated as encoding authority.
2. Consolidate the real native Vulkan swapchain lifecycle behind the surface-
   backed RHI swapchain instead of treating the existing simulated object as
   native proof. Windows/Linux retain required SDR behavior; Windows exposes
   diagnostics but emits no Feature 029 HDR validation claim.
3. Metal configures `CAMetalLayer` per resolved mode. PQ uses
   `BGR10A2Unorm`, the ITU-R 2100 PQ color space, engine-owned encoded values,
   `wantsExtendedDynamicRangeContent`, and `EDRMetadata=nil`; Core Animation
   color management for the declared PQ colorspace is explicitly allowed and
   recorded, but `CAEDRMetadata` system tone mapping is not. EDR uses
   RGBA16Float, extended-linear sRGB,
   `wantsExtendedDynamicRangeContent`, the current native reference-white/
   headroom scale, and `EDRMetadata=nil`, so its engine-owned linear packing
   does not request system tone mapping.
4. Both backends publish actual rather than requested format/color-space/
   extent/native-metadata/generation and the Metal color-management
   disposition. Vulkan may apply HDR10 metadata when the extension is
   available; Metal reports no native metadata for either HDR path. No
   backend applies a second shader tone map, transfer, flip, resize, or image
   alignment.

### M5 - Same-Frame Execution, Resize, and Diagnostics

1. Execute acquire -> producer SceneColor -> output subgraph -> exact copy to
   requested readback -> Present transition -> one ordered submission ->
   completion -> present. Readback and presentation carry the same frame token,
   mode generation, transform fingerprint, extent, and output profile.
2. Permit both readback and presentation in one frame; remove the current
   Forward mutual exclusion and the Deferred path's missing Present transition.
   Do not use Feature 028's CPU aspect-fit upload as Feature 029 authority.
3. Treat extent or output-mode change as a transactional generation change that
   invalidates graph resources, pipelines/render passes/framebuffers,
   swapchain/drawable images, and readback storage. Zero/minimized is `Paused`;
   restore creates a new exact generation.
4. Exercise 100 resize/minimize/restore/profile transitions plus failures at
   allocation, binding, shader/pipeline, acquire, metadata, record, submit,
   completion, copy, present, and teardown. Publish nothing until all required
   terminal operations succeed; retain the stable first actionable failure.
5. Inspect the compiled graph to enforce at most three mandatory fullscreen
   output passes for empty insertion lists, one optional exact GPU readback copy,
   zero CPU readbacks on presentation-only frames, and the bounded 16+16
   insertion/resource policy. Record elapsed time only as an observation.

### M6 - SDR Workload v3 and Image Authority

1. Preserve every Feature 028 v2 file and interpretation byte-for-byte. Add a
   separate baseline schema/loader/registry for v3 keyed by workload revision,
   backend, device class, output-device profile, transform version, exposure,
   and frozen settings digest.
2. Upgrade Lantern and Sponza formal SDR workloads to v3 with Khronos PBR
   Neutral v1 and sRGB v1 as their explicit defaults. Selecting another curve
   or output profile for a formal workload requires its own revision/baseline;
   references never cross curve/profile keys.
3. Reuse exact 512-by-512 semantic probes, one-pixel mutation rejection, FLIP,
   lossless PNG, and Candidate lifecycle. Reject dimension mismatch before
   scoring and prohibit alignment/crop/scale/warp/resample.
4. Generate fresh same-revision Candidates on physical M4 Metal and Windows
   Vulkan. Ordinary automation cannot accept them; the maintainer must review
   and explicitly promote exact records. Feature 028's Windows carry-forward
   cannot be reused.
   Bind each immutable Candidate/PNG/calibration/native-probe set through an
   exact-software-revision `sdr-image-authority` report. Formal producers guard
   clean software inputs before/after fresh capture; evidence-only commits
   retain the tested revision instead of pretending it is their storage commit.

### M7 - macOS Metal HDR Live Human Authority

1. The automated runner performs preflight and non-visual math, format,
   metadata, readback, submission, lifecycle, and digest checks, then emits an
   `hdr-live-review-request.json` whose strongest state is
   `ready-for-live-review`.
2. Display PQ 1000, PQ 2000, EDR 1000, and EDR 2000 live on the physical M4
   Metal authority. The operator observes every settled mode. Target peak is a
   transform/profile declaration, not a claim that the panel was photometrically
   measured at that luminance.
3. Only the maintainer authors the separate attestation. The runner has no
   accept flag, environment variable, prompt default, or writer for `pass`.
   Schemas reject HDR perceptual scores, thresholds, Candidate/reference images,
   or automatic decisions. Missing review remains `manual-review-required`.
4. Corrections append an immutable superseding attestation; they never rewrite
   old evidence. HDR authority contains bounded JSON/digests only, no PNG,
   video, or desktop capture. A `fail` decision remains valid evidence but blocks
   closeout; every one of the four profiles needs a current non-superseded
   `pass` decision before M8 can complete.

### M8 - Cross-Platform CI and Closeout

1. Add a Feature 029 workflow with Windows/macOS/Linux Debug and strict Release,
   Linux ASan/UBSan/TSan, deterministic math/graph/schema tests, applicable SDR
   native probes, Linux Lavapipe readback, and Metal non-visual probes.
2. Separate producer artifacts from consumers; consumers revalidate schema,
   canonical form, manifest, SHA-256, revision, profile, and frame provenance.
   Failure reports remain bounded and uploaded.
3. Aggregate deterministic/native/SDR results without promoting `Unsupported`.
   It may only quote a matching maintainer attestation; it cannot infer HDR
   appearance from readback, metadata, screenshots, or report completeness.
   Require all four physical SDR reports and four linked HDR native reports;
   verify their artifacts, ordered HDR bundle digests, and the request against
   one target software revision. Empty reports or a matching request/attestation
   pair from another revision must fail closed.
4. Close only after all Feature 013/015/018/019/027/028 regressions pass, fresh
   v3 SDR Candidates are explicitly accepted, all four M4 Metal HDR profiles
   have matching current human `pass` decisions, and the final consistency scan reports zero
   numbering, dependency, anchor, task-reference, and old-phase findings.

## Dependency and Delivery Order

```text
M0 color authority
  -> M1 RHI presentation contract
  -> M2 shared Renderer graph
  -> M3 deterministic GPU programs
  -> M4 native adapters
  -> M5 same-frame lifecycle
       |-> M6 SDR v3 authority --------|
       `-> M7 macOS HDR live review ---|-> M8 closeout
```

M1 and the CPU portion of M2 may be implemented in parallel after M0, but M2
cannot freeze graph bindings before M1 resolves the native surface vocabulary.
M6 and M7 share M5 provenance yet remain independent authorities. Feature 030
may begin only after M2's insertion contracts and M5's formal output lifecycle
are stable. Feature 031 Meshlet Derived Data retains only 024/025/026/028 and
does not depend on this plan.

## Complexity Tracking

No constitution violations require justification.
