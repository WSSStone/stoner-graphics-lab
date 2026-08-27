# Implementation Plan: Production Content Integration & Acceptance

**Branch**: `028-production-content-acceptance` | **Date**: 2026-08-21 | **Spec**: [spec.md](spec.md)
**Input**: Feature specification from `/specs/028-production-content-acceptance/spec.md`

## Summary

Feature 028 turns the contracts delivered by Features 020-027 into one
production-content acceptance path before Meshlet work begins. A bounded
Lantern GLB is checked in for regular validation, while a hash-pinned Sponza
external glTF package provides the medium scale tier. Both flow through the
existing resolver/importer, real KTX2 cooking, immutable DDC/publication,
strict-cooked `FAssetManager`, transactional Renderer realization, and the same
backend-neutral composition on Vulkan and Metal.

The implementation first corrects the AssetCooker texture producer selection:
an `FTextureAsset` selected for KTX2 MUST invoke `cooker.ktx2` and publish an
`FKTX2TextureArtifact`; a target decision that merely names compression is not
accepted as evidence. Renderer then gains a responsibility-scoped aggregate
static-model realization transaction. Demo/Validation owns dependency loading,
scene composition, lifecycle repetition, semantic probes, CPU LDR-FLIP image
comparison, and evidence orchestration. Deferred is the full image-acceptance
path; Forward is a bounded native readback and visible smoke for the same root.
For workload calibration, a separate strict-cooked native preview loop permits
free-camera navigation and emits candidate View/Projection matrices. Formal
gates consume only exact revision-owned frozen presets; interactive state is
never an acceptance input.

## Technical Context

**Language/Version**: C++20 with traditional public/private headers and sources; Objective-C++20 remains private to Metal; Python 3 standard-library validation scripts
**Primary Dependencies**: Existing Core, Asset, AssetCooker, RHI, Renderer, Application, Vulkan and Metal contracts; cgltf 1.15, stb_image 2.30, KTX-Software 4.4.2, WAMR 2.4.5, yyjson 0.12.0, SPIRV-Cross 0.68.0 lineage; CPU-only NVIDIA FLIP 1.7 single-header implementation pinned to commit `b475eb4bf394ab877c42166c9eb0a84a02cc5b14`; SCons 4.10.1
**Storage**: Checked-in bounded Lantern GLB and corpus metadata; externally staged hash-pinned Sponza medium package; local immutable DDC and cooked generations; checked-in baseline policy/reference images and bounded validation evidence; no database, archive, remote cache, or runtime source fallback
**Testing**: Existing `StonerTest` suites plus corpus, KTX2 integration, semantic-equivalence, transactional realization, native readback, image-acceptance, lifecycle, failure-injection, Python schema/runner tests, Windows/macOS/Linux CI, and required Windows/macOS hardware lanes
**Target Platform**: Windows x64 Vulkan; macOS arm64 Metal and Vulkan/MoltenVK; Linux x64 build, deterministic, sanitizer, and bounded Lavapipe/headless Vulkan validation
**Project Type**: Cross-platform graphics engine, offline asset cooker CLI, desktop demo, and validation tooling
**Performance Goals**: Regular profile within 10 minutes per hosted job; medium profile within 40 minutes per declared hardware lane; serialized visible hardware profile within 60 minutes per declared lane; deterministic reports byte-identical across 20 repetitions
**Constraints**: 20 regular full lifecycle cycles with cycles 1-2 as warm-up; 1,000 medium/hardware cycles with cycles 1-20 as warm-up; warm-up counts toward the total and RSS growth from the post-warm-up sample to the terminal sample is at most 16 MiB; strict-cooked runs invoke no resolver/importer/source decoder/fallback; no full-desktop capture; no license-policy automation
**Scale/Scope**: Two artist-authored source works; regular Lantern GLB is about 9.6 MB with 3 primitives and four 2K textures; medium Sponza external package is about 50 MB with 103 primitives, 25 materials, and 69 mostly 1K textures; at least 30 deterministic negative cases

## Constitution Check

*GATE: Passed before research and re-checked after Phase 1 design.*

- [x] **Spec-Driven Development**: `spec.md` contains 45 functional requirements, 14 measurable success criteria, and five recorded clarification decisions.
- [x] **Decoupled Architecture**: Asset remains CPU-only; Tools performs cooking; Renderer performs RHI realization; Demo/Validation orchestrates the complete workflow; only Backend code calls Vulkan or Metal.
- [x] **Design Pattern Discipline**: Corpus verification, semantic comparison, model realization, image comparison, and evidence rendering are separate services/contracts. No Asset Manager, Demo, or native-context god-class is introduced.
- [x] **Multi-API Support**: One scene contract drives Vulkan and Metal. Backend/device-class baselines allow declared numeric variation without backend-specific content logic.
- [x] **Advanced Graphics Readiness**: Stable model/material/texture identities, bounds, topology coverage, and evidence remain reusable by Meshlet, streaming, ray-tracing, and GI phases.
- [x] **Naming Conventions**: New C++ APIs use PascalCase and project/Unreal-style prefixes; this phase does not rename Core globally.
- [x] **Cross-Platform Compatibility**: Build, deterministic, strict-cooked, and platform-applicable native validation cover Windows, macOS, and Linux with platform code isolated behind existing boundaries.
- [x] **Automated Cross-Platform Validation**: Relevant PR/push regular jobs, weekly/manual medium jobs, and explicit Windows/macOS hardware closeout jobs are part of the design.

### Post-Design Re-check

The data model and contracts preserve every dependency direction from
Constitution v1.4.0. `FStaticModelRealizer` depends on Asset/RHI/Core only;
production orchestration resides in Demo/Validation; AssetCooker produces KTX2
offline; runtime code does not link Tools or FLIP; FLIP is validation-only; and
native proof remains in Vulkan/Metal backend adapters. No exception or
complexity waiver is required.

## Project Structure

### Documentation (this feature)

```text
specs/028-production-content-acceptance/
|-- spec.md
|-- plan.md
|-- research.md
|-- data-model.md
|-- quickstart.md
|-- contracts/
|   |-- production-corpus.schema.json
|   |-- production-validation-profile.schema.json
|   |-- device-class-registry.schema.json
|   |-- image-baseline.schema.json
|   |-- production-acceptance-report.schema.json
|   |-- production-cook-runtime.md
|   |-- static-model-realization.md
|   `-- production-render-acceptance.md
`-- tasks.md                         # Created by /speckit.tasks
```

### Source Code (repository root)

```text
Content/ProductionAcceptance/
|-- Regular/Lantern/                 # Checked-in bounded GLB and corpus record
|-- Corpus/                          # Canonical corpus/coverage manifests
|-- MAINTAINER_NOTES.md              # Out-of-band notes; never a validation input
`-- Baselines/                       # Versioned backend/device-class references

Config/Validation/ProductionContent/
|-- DeviceClasses.json               # Versioned canonical capability registry
`-- {Regular,Medium,Hardware}.json   # Exact workload/profile contracts

Source/Asset/
|-- Public/Asset/                    # Existing KTX2/runtime payload contracts
`-- Private/                         # Existing codecs/loaders; no graphics ownership

Source/Renderer/
|-- Public/Renderer/                 # Aggregate static-model realization contract
`-- Private/                         # Transaction, rollback, immutable snapshot

Tools/AssetCooker/
|-- Public/AssetCooker/              # Existing cook runner request/report surface
`-- Private/                         # Real FTextureAsset -> cooker.ktx2 selection

Demo/StonerDemo/Private/
|-- FProductionContentSession.*      # Strict manager/request/lifecycle coordinator
|-- FProductionContentComposition.*  # Backend-neutral camera/light/draw assembly
|-- FProductionCameraPreset.*        # Exact workload-owned View/Projection authority
`-- FProductionCameraPreview.*       # Calibration-only native free-camera controller

Tests/
|-- ProductionContentCorpusTests.cpp
|-- ProductionTextureCookTests.cpp
|-- StaticModelRealizationTests.cpp
`-- ProductionContentAcceptanceTests.cpp

.github/scripts/
|-- run_production_content_validation.py
|-- verify_production_corpus.py
|-- compare_production_images.py
`-- test_*production*.py

.github/workflows/
|-- feature-028-production-content.yml
`-- feature-028-production-hardware.yml

Validation/028/
|-- README.md
|-- Baselines/
|-- CI/
`-- Hardware/
```

**Structure Decision**: Extend existing modules at their established ownership
boundaries. Production bytes and accepted image baselines are data under
`Content/ProductionAcceptance`; generic engine functionality belongs in Asset,
AssetCooker, or Renderer; workflow composition remains private to Demo and
Validation. No new runtime module is introduced.

## Implementation Strategy

### M0 - Corpus, Policy, and Verifier Foundation

1. Freeze the corpus schema and coverage vocabulary before copying content.
2. Check in the exact Lantern `glTF-Binary/Lantern.glb` from Khronos revision
   `bf2bb4a81c73a7ceb53e80df3dec0105c5a3fdef`; record its SHA-256 and expected
   structural/texture coverage. Keep Sponza external and acquire only its
   `glTF/` package from the same pinned revision into an ignored cache after
   validating every declared file hash.
3. Reject missing, extra, path-escaping, normalization-colliding, or changed
   package files before resolver/importer execution. Any license or attribution
   note lives only in `Content/ProductionAcceptance/MAINTAINER_NOTES.md`, outside
   the corpus manifest, package inventory, package roots, and every validation
   input. The verifier MUST never open, parse, hash, or reject that note and
   MUST contain no license classifier or approval state.
4. Freeze the pinned CPU-only FLIP revision, provenance fields, metric inputs,
   and private Validation-only ownership contract without yet accepting a
   dependency payload or image threshold.
5. Define workload, device-class-registry, reference-image, and threshold
   schemas. The registry contract requires canonical class ordering and unique
   class/signature records, with semantic uniqueness enforced by its loader. A
   versioned device-class registry maps an observed canonical capability
   signature to exactly one stable class. The required signature fields are
   registry version, backend implementation, CPU architecture, adapter family,
   shader profile, color format, depth format, sample count, and texture-format
   family; an arbitrary caller-supplied class token is never authoritative.

### M1 - Real KTX2 Cook Integration

1. Change cook preparation from a single asset-family producer lookup to a
   deterministic payload/profile-specific producer selection. For
   `FTextureAsset`, select `cooker.ktx2`, build validated
   `FTextureCookParameters`, and project only relevant profile/settings into the
   derived key.
2. Register the KTX2 cooker in the AssetCooker composition root. Publish the
   returned `FKTX2TextureArtifact` through the existing envelope/manifest
   contracts; do not serialize the source `FTextureAsset` and label it KTX2.
3. Preserve color/normal/data semantics, mip coverage, target capability
   choice, fallback evidence, deterministic DDC keys, and strict loader type.
4. Add regression tests proving generic payloads still use their existing
   cookers, all three texture semantics take the KTX2 path, corrupted artifacts
   fail closed, and clean/warm cook evidence is deterministic.

### M2 - Production Cook and Strict Runtime Equivalence

1. Cook explicit production model roots from multiple source roots so the glTF
   package and repository-owned deferred surface shader assets form one complete
   graph. Use `Engine/Shaders/Deferred/Surface` from the default glTF material
   mapping profile; unresolved shader dependencies fail graph construction.
2. For every accepted package root and each required target profile, produce a
   complete generation, validate it with the standalone generation validator,
   remove/rename source roots, then request the model and complete required
   closure through strict `FAssetManager` mode.
3. Compare development and cooked payload families with explicit semantic
   comparators: exact identity/dependency/hierarchy/material fields; normalized
   vertex/index values; material parameter/resource bindings; shader interface
   and target selection; texture dimensions/mips/semantic/color space plus
   decoded/transcoded tolerance where compression changes bytes.
4. For every accepted root, prove an unchanged warm cook reuses 100% of
   eligible payloads, strict generation binding invokes zero source
   participants, and development/strict loading passes every payload-family
   semantic comparator. Exercise wrong-target, corruption, substitution,
   missing dependency, and source mutation rejection.

### M3 - Transactional Renderer Static-Model Realization

1. Add `FStaticModelRealizationRequest`, `FStaticModelRenderSnapshot`, and
   `FStaticModelRealizer` in Renderer. The request receives a validated model
   plus an explicit typed dependency set; it never requests Assets itself.
2. Compose existing mesh, KTX2 texture, material, and shader conversion/
   realization paths. Create resources in deterministic dependency order and
   retain them in a private transaction until all primitives are renderable.
3. Commit one immutable snapshot containing node transforms, primitive draw
   records, material bindings, bounds, and owned RHI resources. Any failure
   rolls back in reverse dependency order and returns no snapshot.
4. Validate duplicate/shared dependencies, stale/mismatched identities,
   unsupported formats, upload/descriptor/pipeline failures, exactly-once
   release, and recreation generation safety.

### M4 - Backend-Neutral Production Composition

1. Add a private Demo production session that binds one strict generation,
   requests the stable model root, waits for its dependency closure, and passes
   the typed set into Renderer realization. It MUST NOT link AssetCooker.
2. Add a separate composition builder for normalized model placement, camera,
   directional/point lights, frame state, and render-path inputs. Vulkan and
   Metal consume the same composition values and workload revision.
3. Make Deferred the full production path, including actual material texture
   bindings and GPU attachment readback. Add a bounded Forward smoke using the
   same root and camera with native color readback.
4. Backend proof records selected RHI/backend mode, capability evidence,
   submission/presentation completion, and GPU-produced bytes. Deterministic
   simulation, semantic oracle, or backend substitution cannot satisfy native
   gates.
5. Keep the Demo composition root below its enforced responsibility budget by
   placing production lifecycle submission, synchronized readback, capture
   accounting, and post-lifecycle evidence extraction in a dedicated private
   production run translation unit. The split changes no public interface or
   lifecycle/image contract.

### M5 - Image, Lifecycle, and Failure Acceptance

0. Add a calibration-only native production preview using the same strict
   closure, realization, Deferred, and presentation path. Its free camera may
   emit canonical candidate View/Projection records, but formal validation
   selects only a finite, invertible, affine/orthonormal, convention-correct
   preset by exact workload revision. Camera changes advance the revision and
   require new probes, references, calibration, review, and hardware evidence.
1. Vendor the pinned CPU-only FLIP header as a private Validation dependency,
   record provenance/version, and test metric stability using normalized
   linear/sRGB RGB buffers without introducing CUDA or Python packages.
2. Normalize readback row pitch, channel order, origin, and color transfer into
   a canonical image before any comparison.
3. Require semantic probes first (nonblank, coverage, orientation markers,
   geometry/material regions, finite range, current-frame token). Derive the
   device class by exact canonical capability-signature match in the versioned
   registry, then apply the exact baseline selected by workload/backend/device
   class and its versioned FLIP thresholds. Missing or ambiguous classes or
   baselines fail closed.
4. Calibrate numeric thresholds from 20 accepted same-revision native captures
   plus an intentional mutation set; commit only reviewed thresholds and
   window-only reference captures. Calibration is never a runtime auto-approval
   path. Record mean FLIP, p95 FLIP, maximum FLIP, and the fraction above the
   baseline's declared bad-pixel threshold. Reference and candidate dimensions
   and normalization policy must match exactly.
5. Run 20 regular or 1,000 medium/hardware full manager-realizer-render-release
   cycles. The regular target gate owns the required 20 clean determinism
   repetitions. Independent clean runs retain isolated source, DDC,
   publication, and report roots; the slower hosted x86_64 Metal target may
   execute at most four of those independent runs concurrently so the same 20
   byte-identical results remain inside the unchanged regular deadline.
   Arm64 Metal retains its two-run bound and Vulkan retains its existing
   bounded policy. Medium/hardware run one clean cook and one unchanged
   100-percent-reuse warm cook per selected package rather than duplicating
   those regular repetitions inside their lifecycle budget. Cycles 1-2 are the
   regular warm-up and cycles 1-20 are the medium/hardware warm-up; warm-up
   counts toward the total. Return every
   tracked counter to baseline and enforce at most 16 MiB RSS growth between the
   sample immediately after warm-up and the terminal sample.
   Before each of those two authoritative samples, complete queue idle and
   ownership teardown. The native-headless RSS authority fully shuts down the
   requested backend before allocator relief at both exact comparison points:
   Linux Vulkan regular uses this boundary for 20/2, and macOS Metal medium
   uses it for 1,000/20. Restore and re-prove the same requested native backend
   after the warm-up sample and again before the uncounted evidence extraction;
   visible hardware/presentation paths retain their existing continuous
   backend lifetime. Before each declared native-headless RSS sequence, follow
   the existing unmeasured native Deferred/Forward prime with one unmeasured
   full backend shutdown, platform-specific fixed relief/quiescence, and
   restart. This makes both authoritative Linux regular and Metal medium
   endpoints observe a backend that has already crossed the same restart
   boundary without adding a declared cycle, capture, or RSS sample. Release
   unused heap pages, wait one fixed second on Metal and glibc/Lavapipe for
   native completion and allocator/kernel accounting to quiesce, release
   unused heap pages once more, then take exactly one RSS sample. Do not select
   a minimum or retry the sample based on its value. Start only the exact Linux
   Vulkan native-headless 20/2 child with one glibc arena and start only the
   exact hosted Intel macOS Metal native-headless 1,000/20 child with Apple
   libmalloc space-efficient mode, Nano disabled, and one general allocator
   magazine. Do not claim the hosted RSS authority on arm64 macOS: the default
   Xzone allocator cannot be disabled by a supported production environment
   override. Preserve arm64 authority through regular coverage and the required
   physical M4 1,000-cycle visible hardware lane under the default allocator.
   Space-efficient mode enables aggressive madvise, disables the
   large-allocation cache, and bounds only the medium allocator to one
   magazine; the additional two settings independently bound Nano and the
   tiny/small magazine allocator.
   Visible hardware, other backends, and other lifecycle shapes receive no
   allocator override. Use one runtime
   manager worker for regular allocator stability, eight for the bounded
   Lantern v2 1,000-cycle workload, and sixteen for Sponza medium/hardware
   throughput.
   Scheduled/manual medium assigns each accepted package to its own
   hosted Intel Metal lane with a 2,400-second per-lane deadline. This preserves
   bounded headroom for the required full native-backend RSS comparison
   boundaries without reducing the 1,000/20 lifecycle workload.
   Each lane owns its package's complete clean/warm/strict/equivalence/
   1,000-cycle proof, and an aggregate job requires the exact profile package
   set plus identical corpus, target, and revision authority. Local full-profile
   fallback keeps the post-strict barrier and serialized native execution. The
   Linux Lavapipe lane remains the regular
   software-native gate because one isolated 1,000-cycle Lantern lifecycle
   alone exceeded the complete medium budget. Every lifecycle cycle still
   loads and releases the exact selected root closure. For the 1,000-cycle
   Sponza v2 profile, load the complete manifest in deterministic dependency-
   first batches. Records within one ready batch execute concurrently; each
   later closure borrows only immutable dependencies already retained by the
   same cycle's Manager cache. Every manifest record is still completely read,
   parsed, and typed-decoded exactly once per cycle, while the selected root
   and manifest completeness remain fail-closed.
   The Lantern and regular 20-cycle request schedules are unchanged. When the
   already-validated generation manifest supplies the exact cooked-envelope
   digest, strict typed loading authenticates the complete envelope once and
   does not re-hash its covered body; generic envelope decoding retains the
   independent body digest check, and missing or mismatched manifest authority
   fails closed. For accepted Lantern v2 and Sponza v2 workloads at exactly
   1,000 cycles, retain a manifest-bounded set of successfully authenticated
   envelope digests while an authentication-
   owned reader lease continuously protects the exact publication namespace
   and generation. The validation application owns that session across the
   complete lifecycle gate; each cycle still shuts down and recreates its
   manager, handles, Renderer/RHI realization, and native resources, but it
   must not destroy the payload-free authentication context between cycles.
   Integration inspection must prove at least one cross-cycle reuse hit for
   every accepted 1,000-cycle workload. The same continuous reader lease may
   retain the first successful pointer/manifest/layout validation result as a
   bounded payload-free authority. Every later manager must re-check that
   immutable metadata against the exact publication namespace, generation,
   target profile, required extensions, record count, and held lease, but it
   does not re-enumerate or re-parse the unchanged generation layout. Later
   cycles still query and read every file, parse every
   container, decode every typed payload, realize/render the complete closure,
   and release every runtime owner. Once the exact full envelope has already
   been authenticated, typed decode borrows its body from that cycle's complete
   file buffer instead of allocating a second full body copy; the view cannot
   outlive the decode call. The set contains no payload bytes, decoded assets,
   runtime handles, or graphics objects. Unknown generations, changed
   publication namespaces, first-load corruption, and capacity mismatch fail
   closed. Regular 20-cycle and general Asset Manager callers do not enable
   this validation-only memo, and public/generic codec paths retain owned body
   copies. Strict payload loading combines the bounded regular-file check and
   complete read through one native file handle, rejects links/non-regular
   files and oversize input before allocation, and still compares the exact
   byte count with manifest authority. When the validation session hands an immutable typed payload to the
   Renderer closure, use an aliasing shared pointer that retains the same
   `TAssetHandle` control instead of deep-copying the payload. The alias keeps
   manager external-retention accounting authoritative and cannot outlive its
   payload, while session teardown still requires every alias and handle to be
   released before manager shutdown. Every lifecycle cycle still
   performs synchronized GPU readback and nonblank validation for Deferred
   FinalOutput and ForwardColor. After the exact terminal teardown, RSS sample,
   and lifecycle decision, one uncounted extraction pass copies all six Deferred
   attachments, computes the retained authoritative evidence digests, and must
   return ownership to baseline again. Hardware packages remain fully
   serialized because visible windows and capture devices are host-global
   resources. Keep
   the separate 20-frame Sponza image-calibration run focused on image and
   per-cycle ownership proof; its RSS is observational because Sponza lifecycle
   authority belongs to the 1,000-cycle hardware profile.
6. Build at least 30 stable negative cases spanning corpus, cook, publication,
   strict load, realization, native capability, image, and lifecycle failures.

### M6 - Tiered Automation and Closeout

1. Implement one schema-driven validation runner and reuse it from local and CI
   entry points. Separate deterministic report fields from timing, memory,
   device descriptions, and image observations. Schema validation requires a
   backend, exact registered device class, and measured or structured not-run
   FLIP result for native reports; exactly one structured first failure for
   Failed/Unsupported; failure-only `not-created` when no generation exists;
   and a real generation digest, measured passing FLIP for native execution,
   and no first failure for Passed.
2. Regular workflow runs on relevant PR/push paths for Windows/macOS/Linux. A
   producer job may share immutable cooked artifacts only when each consumer
   still verifies target/profile and performs its platform-specific gates.
3. Medium workflow runs weekly from the default branch and via
   `workflow_dispatch`; it is also manually required at feature/release
   closeout. Hardware workflow is explicit for Windows Vulkan and macOS
   Vulkan/Metal and is required at Feature 028 closeout and reference/render
   path changes.
4. Unsupported lanes name the missing capability and replacement lane. They do
   not become passes. Publish bounded reports/artifacts with digests and no
   absolute paths, credentials, process IDs, pointers, or unrelated screen
   content. Canonical report JSON is at most 1 MiB, references at most 64
   artifacts, caps each artifact at 64 MiB, and caps referenced artifacts at
   256 MiB aggregate; schema handles item/count limits and the runner handles
   serialized/aggregate limits.
5. Run Debug, strict Release, sanitizer, determinism, regular, medium, and
   hardware gates; update `doc/028-production-content-acceptance.html`, roadmap,
   validation index, and project memory only after accepted evidence is tied to
   the final revision.
6. Verify the Feature 028 diff against FR-044 so no new source importer,
   skeletal/editor/hot-reload, package/archive, streaming/residency, Meshlet/
   LOD, virtual-geometry, ray-tracing, or visual-redesign scope enters closeout.

## Dependency and Delivery Order

```text
Setup/contract freeze
 |--> M0 Corpus/Policy ---------|
 `--> M1 Real KTX2 Cook --------+--> M2 Cook/Runtime Equivalence

M2 --> M3 Transactional Realization --> M4 Composition
M4 --> M5 Image/Lifecycle Acceptance --> M6 Automation/Closeout
```

M0 corpus admission and M1 KTX2 foundation may proceed in parallel after Setup;
M2 waits for both. M3 Renderer transaction unit tests may begin once their
contracts freeze, but M4 cannot begin until strict cooked equivalence and
aggregate realization pass. Native image baseline acceptance is intentionally
last because it must validate the real path rather than mask earlier semantic
failures.

## Complexity Tracking

No constitution violations require justification.
