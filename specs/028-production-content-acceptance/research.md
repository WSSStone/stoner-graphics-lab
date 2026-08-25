# Research: Production Content Integration & Acceptance

## Decision 1: Use Lantern for Regular and Sponza for Medium

**Decision**: Pin Khronos `glTF-Sample-Assets` revision
`bf2bb4a81c73a7ceb53e80df3dec0105c5a3fdef`. Check in only
`Models/Lantern/glTF-Binary/Lantern.glb` for the regular profile. Acquire the
complete `Models/Sponza/glTF/` directory into an ignored external cache for the
medium profile and verify every file against the corpus manifest before use.

**Rationale**: Lantern is a bounded, single-file GLB of about 9.6 MB with three
primitives and four 2K PBR textures covering RGB/RGBA color, normal, emissive,
and metallic/roughness data. Sponza contributes a realistic external-dependency
graph of about 50 MB with 103 primitives, 25 materials, and 69 mostly 1K
textures. Together they cover embedded and external storage, multi-primitive,
multi-material, shared files, and the required 1K/2K semantics while keeping
ordinary checkout and CI bounded. The Khronos repository is a curated glTF
sample collection and publishes separate glTF/GLB variants and model metadata:
[repository](https://github.com/KhronosGroup/glTF-Sample-Assets),
[Lantern](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/bf2bb4a81c73a7ceb53e80df3dec0105c5a3fdef/Models/Lantern),
[Sponza](https://github.com/KhronosGroup/glTF-Sample-Assets/tree/bf2bb4a81c73a7ceb53e80df3dec0105c5a3fdef/Models/Sponza).

This decision is technical coverage research, not permission to redistribute
or use either work. Per the clarified specification, maintainers make all
license/compliance decisions outside the system. The corpus verifier neither
reads nor evaluates license metadata. Keeping the medium package outside Git
also avoids making it part of the regular repository distribution.

**Alternatives considered**:

- DamagedHelmet plus Lantern: compact and offers external/embedded variants,
  but both are predominantly 2K and provide much less scale/multi-material
  coverage.
- Check in Sponza: gives offline medium execution but adds roughly 50 MB and 71
  files to every clone and ordinary CI.
- Download regular content in CI: rejected because regular validation must work
  from a clean checkout without an unpinned or availability-sensitive fetch.
- Generate synthetic production content: rejected because Feature 028 exists
  specifically to expose integration behavior using artist-authored content.

## Decision 2: Integrity Manifest Is Canonical; License Is Out of Scope

**Decision**: Use a canonical JSON corpus manifest containing source work,
package, pinned revision/location, acquisition date, file inventory/SHA-256,
root AssetId, tier, and coverage claims. Validation rejects undeclared or
changed bytes and unsafe paths. The schema intentionally defines no license,
approval, policy, SPDX, or legal-status field. Maintainers may keep
`Content/ProductionAcceptance/MAINTAINER_NOTES.md` for out-of-band attribution
or license notes, but it is outside the manifest, package inventory, package
roots, and validation inputs. Automated validation never opens, parses, hashes,
or rejects that file.

**Rationale**: Reproducibility requires provenance and byte integrity, while the
user explicitly excluded license validation. Omitting policy fields prevents a
future report from accidentally treating missing or changed license text as a
technical acceptance result.

**Alternatives considered**:

- License strings inside the manifest but ignored by gates: rejected because
  their presence invites callers to interpret non-authoritative metadata as an
  acceptance decision.
- SPDX classification and allowlists: explicitly outside the clarified scope.
- Directory hash only: insufficient for stable first-failure reporting and
  undeclared-file detection.

## Decision 3: Fix Producer Selection Before End-to-End Cooking

**Decision**: Extend AssetCooker preparation with deterministic
payload/profile-specific cooker selection. `FTextureAsset` must select
`cooker.ktx2`, pass `FTextureCookParameters`, and publish the returned
`FKTX2TextureArtifact`. Other asset families continue using their existing
family-mapped cookers.

**Rationale**: Current target profiles contain `cooker.ktx2` settings and the
KTX2 cooker is implemented, but `FAssetCookRunner::PrepareCookNode` selects the
generic image/texture family cooker. The generic cooker serializes the typed
source payload; a compressed target-decision label is not KTX2 content. Feature
028 must validate the real Feature 022 product, not an equivalent-looking
manifest.

**Alternatives considered**:

- Keep generic cooked texture bytes and defer KTX2 integration: violates
  FR-014 and the purpose of Phase 028.
- Let the runtime convert generic textures to KTX2: violates strict-cooked and
  offline Tools/runtime separation.
- Make every cooker inspect every payload: weakens deterministic dispatch and
  scales poorly. A small selection policy with explicit participants is clearer.

## Decision 4: Cook Explicit Roots Across Multiple Source Roots

**Decision**: Production cooking uses explicit model roots and source roots for
both the admitted model package and repository-owned shader/material content.
The default glTF material profile's
`ShaderProgram:Engine/Shaders/Deferred/Surface` dependency must resolve into the
same cook graph and published generation.

**Rationale**: `MakeDefaultGLTFMaterialMappingProfile` binds every generated
glTF material to the deferred surface shader AssetId. Cooking only the model
directory can therefore produce an incomplete graph. Existing cook requests
already support multiple source roots and explicit roots, preserving generic
importer behavior without package-specific shader injection.

**Alternatives considered**:

- Hard-code shader bytes in Demo: violates Asset identity and strict-cooked
  dependency closure.
- Add shader payloads to each third-party package: mutates authoritative source
  content and creates duplicate authority.
- Cook all repository content: increases unrelated work and can hide missing
  root dependency declarations.

## Decision 5: Add One Aggregate Renderer Realization Transaction

**Decision**: Add a Renderer-owned `FStaticModelRealizer` that consumes an
explicit typed dependency set and returns one immutable
`FStaticModelRenderSnapshot`. It composes existing mesh, KTX2 texture,
material, and shader paths, holds newly created RHI resources privately, and
commits only after every primitive is usable.

**Rationale**: Existing APIs realize one mesh or texture at a time. Demo-level
manual sequencing would publish partial scenes and duplicate rollback logic.
Renderer is the constitutional owner of Asset-to-RHI translation, while Asset
Manager request orchestration remains outside Renderer.

**Alternatives considered**:

- Make `FAssetManager` realize GPU resources: violates `Asset -> Core`.
- Put aggregate realization in Demo: leaks reusable Renderer responsibility and
  grows the composition root into a god-class.
- Add a new runtime module: unnecessary; the responsibility already belongs to
  Renderer.

## Decision 6: Compare Normalized Semantics, Not Universal Bytes

**Decision**: Exact comparison applies to identities, dependency roles/order,
hierarchy, material associations, shader interfaces, and lossless geometry
values. Texture comparison uses dimensions, mip structure, semantic/color
space, target decision, and decoded/transcoded values within a semantic-specific
tolerance when compression changes physical bytes.

**Rationale**: KTX2/Basis and backend-targeted shader payloads intentionally
change representation. A single byte-equality rule would reject correct cooks;
an unstructured visual check would miss wrong identities or dependencies.

**Alternatives considered**:

- Require all cooked bytes to equal source payload bytes: incompatible with
  compression and target shader finalization.
- Compare only manifest digests: proves deterministic packaging, not semantic
  correctness.
- Use rendered images as the only oracle: too late and too coarse to identify
  payload-family failures.

## Decision 7: Use CPU LDR-FLIP After Mandatory Semantic Probes

**Decision**: Pin NVIDIA FLIP v1.7 commit
`b475eb4bf394ab877c42166c9eb0a84a02cc5b14` and vendor only the CPU single-header
implementation as a private Validation dependency. Feed canonical normalized
RGB image buffers directly to LDR-FLIP. Do not add CUDA or the Python package.

**Rationale**: FLIP is designed for rendered-image error evaluation and its
official repository provides LDR/HDR variants plus a pure C++ single-header
implementation since v1.3: [NVLabs FLIP](https://github.com/NVlabs/flip). The
engine already exposes readback bytes, so decoding images solely to run the
metric is unnecessary. Semantic probes remain mandatory and run first because
perceptual similarity alone can tolerate a structurally wrong image.

**Alternatives considered**:

- Pixel-perfect comparison: too sensitive to expected backend/driver numeric
  differences.
- SSIM/PSNR only: useful diagnostics, but less aligned with rendered-image
  perceptual differences; they may be recorded later but are not the gate.
- Python `flip-evaluator`: adds a package/runtime dependency to every validation
  host and duplicates image transfer.
- HDR-FLIP: deferred until an HDR presentation/reference workflow is required;
  Feature 028 acceptance output is normalized LDR.

## Decision 8: Baselines Are Explicit, Versioned, and Calibrated

**Decision**: Select baselines by exact workload revision, backend, and a
device-class token derived by exact match in a versioned registry. Runtime
constructs a canonical capability signature containing registry version,
backend implementation, CPU architecture, adapter family, shader profile,
color format, depth format, sample count, and texture-format family. The runner
accepts neither an arbitrary caller class nor nearest/fallback matching. Each
baseline records canonical dimensions/color transfer, image digest,
mean/p95/max FLIP limits, a bad-pixel FLIP threshold, and maximum bad-pixel
fraction. Missing or ambiguous class/baseline selection fails.

Numeric values are established during M0 by 20 repeated accepted captures for
the exact class and challenged by intentional defects (blank/stale/origin,
missing geometry, material swap, and normal/data color-space mistakes). The
accepted threshold must include repeat noise while rejecting every mutation.
Threshold changes require reference-policy review and hardware rerun.

**Rationale**: One global threshold cannot model MoltenVK, native Metal, and
different Vulkan device classes. Automatically increasing tolerance after a
failure would turn the gate into self-approval. Registry-derived class tokens
avoid unstable marketing names and caller discretion in deterministic identity
while retaining hardware specificity.

**Alternatives considered**:

- One cross-backend reference: too strict for legitimate representation
  differences and too weak if tolerance is widened.
- Backend-only baselines: insufficient when device classes have materially
  different stable numeric behavior.
- Auto-learn tolerance in ordinary CI: rejected because a regression could
  approve itself.

## Decision 9: Separate Regular, Medium, and Hardware Workflows

**Decision**: A reusable schema-driven runner backs all tiers. Relevant PR/push
paths run regular Windows/macOS/Linux jobs. The medium workflow runs weekly on
the default branch and through manual dispatch for feature/release closeout.
Hardware acceptance is explicitly dispatched on proven Windows Vulkan and
macOS Vulkan/Metal hosts at Feature 028 closeout and when references or render
paths change.

**Rationale**: GitHub scheduled workflows run from the default branch, while
feature closeout must validate the candidate revision deliberately. Separating
hardware prevents hosts without a physical display/device from being counted as
native passes. Immutable producer artifacts can reduce duplicated cooking, but
every consumer revalidates target and manifest evidence. Medium package roots
own disjoint source, DDC, publication, lease, and report trees, so up to two may
run concurrently while retaining their manifest order and one shared deadline.
Hardware stays serialized to avoid competing visible windows and capture
devices. See
[GitHub Actions workflow documentation](https://docs.github.com/en/actions/concepts/workflows-and-actions/workflows).

**Alternatives considered**:

- Put 1,000 cycles and visible rendering in every PR job: exceeds regular
  contributor budget and depends on unavailable hardware.
- Run medium only manually: allows long-lived scale regressions.
- Treat unsupported native jobs as skipped success: explicitly forbidden by
  FR-036.

## Decision 10: Freeze Warm-up and RSS Sampling by Profile

**Decision**: Regular runs use cycles 1-2 of 20 as warm-up. Medium and hardware
runs use cycles 1-20 of 1,000 as warm-up. Warm-up cycles count toward the total.
RSS growth is the terminal RSS sample minus the sample taken immediately after
the final warm-up cycle; the gate requires growth at most 16 MiB in addition to
all ownership counters returning to baseline. Arm64 Metal regular uses one
runtime manager worker so worker-local allocation is stable inside two cycles;
other regular targets use four workers, while medium/hardware use eight workers
to meet their declared throughput budgets. Medium remains bounded to 30 minutes;
hardware receives 60 minutes because its two accepted packages must serialize
their visible windows and capture device. The separate 20-frame visible
image-calibration run still requires per-cycle ownership and stale-handle
proof, but treats RSS as an observation: Sponza is not a regular-profile root,
and its authoritative lifecycle/RSS gate is the 1,000-cycle hardware profile.

**Rationale**: A named but variable warm-up lets callers move the measurement
origin until a leak appears acceptable. Exact profile-owned boundaries make the
gate reproducible while preserving most cycles for steady-state observation.

**Alternatives considered**:

- Caller-selected warm-up: rejected because it makes RSS results incomparable.
- No warm-up: too sensitive to one-time allocator and backend initialization.
- Excluding warm-up from the cycle total: unnecessarily increases already
  bounded 20/1,000-cycle workloads and complicates reporting.
## Decision 11: Evidence Is Two-Layered and Privacy-Safe

**Decision**: Emit one canonical deterministic report plus a separate
environment observation section/artifact. Deterministic fields exclude paths,
time, PID, pointers, device marketing names, and thread order. Window-only
captures are verified by dimensions/frame token and expected application
content; full-screen capture is prohibited.

**Rationale**: This permits byte-identical deterministic evidence while still
retaining useful performance, memory, device, and visual observations. It also
prevents an unrelated desktop or secret-bearing window from entering artifacts.

**Alternatives considered**:

- One monolithic report: host observations break deterministic comparison.
- Full-screen screenshots cropped afterward: exposes unrelated desktop content
  before validation.
- Logs as the only evidence: unbounded and difficult to validate or compare.

## Decision 12: Calibrate Freely, Freeze Exact Matrices by Workload

**Decision**: Add a calibration-only strict-cooked native free-camera preview.
It uses right-drag look, W/S/A/D/Q/E movement, Shift acceleration, wheel FOV,
reset, snapshot, and exit controls. Snapshot emits row-major View and Projection
matrices with round-trip float precision. Formal Deferred and Forward rendering
select exactly one code-owned preset by workload revision and derive camera
position, ViewProjection, and inverse ViewProjection from it. Formal modes
offer no caller camera override.

Sponza moves from the rejected exterior-like
`production-content-sponza-v1` candidate to an internal atrium-depth
`production-content-sponza-v2` preset. Every backend/device-class reference for
v2 must be recalibrated and explicitly accepted.

**Rationale**: A single MVP cannot describe a multi-draw model because each
draw has a different Model transform. Separate View and Projection matrices
preserve the user's exact selected pose without coupling it to per-draw model
state. Keeping the preview outside formal modes permits exploratory movement
while preserving deterministic CI authority and backend parity.

**Alternatives considered**:

- Store one MVP: rejected because Sponza has many Model matrices.
- Allow formal CLI camera overrides: rejected because callers could bypass the
  reviewed workload and baseline contract.
- Build a general editor/game camera: deferred as broader runtime/editor scope.
- Keep the first Sponza facade candidate: rejected because it does not provide
  the intended interior depth and material coverage.

## Decision 13: Advance Lantern After Correct Native Winding

**Decision**: Retain the globally correct native front-face adaptation and
advance Lantern from `production-content-v1` to
`production-content-lantern-v2`. Source triangle normals agree with the glTF
vertex normals for every non-degenerate Lantern triangle and overwhelmingly
for Sponza, so the imported model is not repaired or selectively rewound.
Lantern v2 preserves the frozen identity camera, requires the diagnostic world
normal to face approximately -X, and places the key light on that camera-facing
side. The former Lantern v1 image records become `superseded`; v2 receives new
twenty-capture calibration and explicit acceptance on Metal and MoltenVK.

Feature 028 retains the existing `sampleCount=1` render policy with no
anti-aliasing or general post-processing. The maintainer reviewed and accepted
that visibly aliased v2 output for this phase. Adding post-processing or
anti-aliasing is later roadmap work and must advance affected workload
revisions and repeat semantic/FLIP calibration.

**Rationale**: Reverting winding would select the wrong face and reintroduce a
cross-backend geometry error. The unexpectedly dark correct face came from the
workload light direction, not bad asset normals. Treating the light and render
policy as versioned workload authority preserves deterministic acceptance and
makes the visual change reviewable.

**Alternatives considered**:

- Rewind Lantern only: rejected because source winding and vertex normals
  already agree.
- Revert the native front-face fix: rejected because it would make both
  backends consistently wrong.
- Add AA while accepting v2: rejected because it would expand Feature 028 and
  obscure whether the winding/light correction itself passed acceptance.

## Decision 14: Vulkan Devices Track Client-Owned Resources Weakly

**Decision**: Match the RHI ownership contract used by Metal: the Vulkan device
tracks client-owned fences, semaphores, resources, render objects, shaders, and
pipelines with weak references. Creation prunes expired tracking entries, and
pipeline-cache insertion prunes expired or dependency-invalid entries. Device
shutdown still locks every surviving object and invalidates it.

**Rationale**: Heap inspection of a failed MoltenVK 1,000-cycle gate found
thousands of released `FVulkanBuffer`, `FVulkanTexture`, shader, and pipeline
wrappers still reachable from the persistent device. Native counters returned
to zero because their handles had been invalidated, but the device's strong
tracking arrays retained the C++ wrappers, copied shader bytes, pipeline
descriptions, and cache keys until device shutdown. Weak tracking preserves
provenance and shutdown invalidation without becoming an extra resource owner.

**Alternatives considered**:

- Force allocator pressure relief after every cycle: rejected because it did
  not release reachable objects and increased the observed terminal RSS.
- Recreate the device every cycle: rejected because it would evade the intended
  persistent-device resource lifecycle test.
- Add a validation-only Vulkan purge call: rejected because it would hide a
  general ownership bug behind Feature 028-specific behavior.

## Decision 15: Long-Lifecycle Evidence Bookkeeping Is Bounded

**Decision**: Count every native window and Forward capture, but retain image
bytes only for the final authoritative attachment set consumed by semantic and
FLIP acceptance. Retain every failed diagnostic and its global sequence number,
while retaining at most 256 successful diagnostic records and reporting the
number of dropped successes in stable diagnostic text.

**Rationale**: A 1,000-cycle validation run must prove that the renderer and
backend release their resources; the validation recorder itself must not grow
by storing two full-resolution captures and several success strings per cycle.
Separate counters preserve exact lifecycle evidence, while the final retained
attachments preserve the bytes needed for image acceptance and diagnostics
continue to preserve every actionable failure.

**Alternatives considered**:

- Retain every capture: rejected because evidence memory would scale with the
  requested cycle count and contaminate the RSS gate.
- Stop recording after warm-up: rejected because later presentation or Forward
  omissions would become invisible.
- Cap every diagnostic indiscriminately: rejected because a late primary or
  secondary failure must remain inspectable.

## Decision 16: Production Validation Reuses Synchronization and Retires Work

**Decision**: The production lifecycle creates one backend-neutral graphics
queue and fence, submits every cycle through that pair, waits for the fence and
queue to become idle, then resets the fence. Vulkan `WaitIdle` removes completed
or invalidated submission records and remembers that a completed submission was
observed so the existing completion-inspection contract remains stable.

**Rationale**: Creating queues and fences per frame makes the validation harness
itself a source of allocator churn. Reusing them exposed a general Vulkan queue
bug: completed submission records held command buffers indefinitely and
exhausted a bounded command pool after 32 cycles. Explicit retirement makes
queue lifetime independent of submission count and matches the ownership
expected by long-running applications.

**Alternatives considered**:

- Increase the production command-pool capacity: rejected because the queue
  would still grow without bound and merely fail later.
- Recreate the queue periodically: rejected because it hides incorrect
  submission retirement and weakens the persistent-device lifecycle test.
- Add a Vulkan-only Demo purge: rejected because submission retirement belongs
  to the backend queue contract, while the Demo harness remains backend-neutral.

## Decision 17: Visible Production Presentation Has Bounded Recovery

**Decision**: A production window capture retries `ResizeRequired` on either
backend after recreating the presentation extent, and retries Metal
`Unavailable`/`NotReady` drawable results for at most two seconds while pumping
window events. Every other result fails immediately. Success still requires a
presented non-empty window readback and exact pixel equality whenever the
presentation and source extents match; a terminal failure records the RHI
result.

**Rationale**: A clean-checkout 1,000-cycle Metal run completed 955 visible
frames before one drawable acquisition/presentation returned a transient result.
All six Deferred GPU attachments for that cycle were valid, but the former
single-attempt production path collapsed the presentation failure into a generic
GPU-readback error. The existing visible lifecycle already defines a two-second
recovery boundary for resize/drawable churn; production acceptance must apply
the same bounded rule without converting a persistent or unknown failure into
success.

**Alternatives considered**:

- Retry indefinitely: rejected because hardware CI must remain bounded and a
  lost window must fail.
- Treat missing drawable as a successful skipped capture: rejected because the
  gate requires one presented window capture for every selected frame.
- Retry every backend error: rejected because invalid state, device loss, copy,
  synchronization, and data-integrity failures are not presentation churn.
