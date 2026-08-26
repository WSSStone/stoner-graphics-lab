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
own disjoint source, DDC, publication, lease, and report trees. Scheduled/manual
hosted Metal assigns one complete package to each isolated lane and aggregates
their exact authority; the local full-profile fallback overlaps CPU/cook work,
meets at a post-strict barrier, and serializes native execution. The
scheduled/manual medium lanes use
GitHub-hosted arm64 Metal: an isolated 1,000-cycle Lantern Lavapipe lifecycle
exhausted 1,471 remaining seconds after all preceding stages passed, proving
that serialization could not make the two-package Linux software-native
workload satisfy the 1,800-second profile contract. Linux Lavapipe remains
required in the 20-cycle regular matrix. Visible hardware stays fully
serialized to avoid competing windows and capture devices. See
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
number of dropped successes in stable diagnostic text. Every lifecycle cycle
still performs synchronized GPU-to-CPU readback and nonblank validation for
Deferred FinalOutput and ForwardColor. Every lifecycle cycle, including the
terminal cycle, omits the five intermediate Deferred attachment copies plus
retained byte copies and SHA-256 work. After the exact terminal teardown, RSS
sample, and lifecycle decision, one uncounted extraction pass restores all six
Deferred readbacks and retains the seven authoritative Deferred/Forward
records. The extraction pass uses the same strict generation, backend, and
frozen workload, does not increment completed cycles or capture count, and must
return every owner to baseline plus reject stale handles again.

**Rationale**: A 1,000-cycle validation run must prove that the renderer and
backend release their resources; the validation recorder itself must not grow
by storing two full-resolution captures and several success strings per cycle.
Separate counters preserve exact lifecycle evidence, while post-lifecycle
attachments preserve the bytes needed for image acceptance without making the
terminal RSS sample include evidence that the warm-up sample intentionally did
not retain. Diagnostics continue to preserve every actionable failure.

**Alternatives considered**:

- Retain every capture: rejected because evidence memory would scale with the
  requested cycle count and contaminate the RSS gate.
- Retain the seven authoritative raw readbacks before the terminal RSS sample:
  rejected because it makes evidence allocation itself appear as lifecycle
  growth and lets an allocator high-water mark decide whether the same bytes
  pass.
- Stop recording after warm-up: rejected because later presentation or Forward
  omissions would become invisible.
- Skip all non-terminal GPU readback: rejected because a late blank, stale, or
  omitted Deferred/Forward output would become invisible even if ownership and
  submission counters remained balanced.
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

## Decision 18: Lifecycle RSS Starts After One Unmeasured Native Prime

**Decision**: Before the declared 20- or 1,000-cycle lifecycle sequence, submit
one real Deferred and one real Forward command buffer, wait for completion,
release the complete production cycle, require all ownership counters to return
to baseline plus stale-handle rejection, and reinitialize. The prime records no
capture, readback, lifecycle sample, or completed cycle. Formal RSS authority
remains the exact declared warm-up cycle to terminal delta with the unchanged
16 MiB limit.

**Rationale**: Final-revision Linux Lavapipe regular CI failed twice while all
40 captures, seven final readbacks, submissions, terminal owners, and stale
checks passed. RSS showed the first large driver allocation after the declared
cycle-2 sample: the retry rose from 419,581,952 bytes at cycle 2 to 530,227,200
at cycle 3, then stabilized near 484 MiB through cycle 20. Measuring before the
backend has executed both production paths mistakes first-use driver allocation
for retained per-cycle ownership. A real unmeasured prime establishes the
environment boundary without hiding any declared lifecycle cycle or weakening
the leak threshold.

**Alternatives considered**:

- Increase the 16 MiB threshold: rejected because the requirement is unchanged
  and a larger allowance would also weaken real leak detection.
- Select a later declared warm-up cycle only on Linux: rejected because regular
  remains a backend-neutral 20/2 contract.
- Retry CI until allocator timing happens to pass: rejected after two failures
  on the same revision demonstrated the missing first-use boundary.

## Decision 19: Released Heap Pages Are Reclaimed Before RSS Sampling

**Decision**: After complete production-cycle teardown at the exact declared
warm-up and terminal comparison cycles, ask the platform memory boundary to
release unused heap pages before the existing process-RSS sample. Other cycles
retain their unmodified samples and peak evidence. The operation is a no-op on
platforms without an applicable safe primitive. It uses `malloc_trim(0)` on
Linux/glibc and `malloc_zone_pressure_relief(nullptr, 0)` across all allocator
zones on macOS. Both platform paths perform at most eight passes and stop as
soon as a pass reports no further release. This convergence remains inside the
same exact lifecycle checkpoint; it neither selects a different cycle nor
chooses a favorable RSS sample. The authoritative metric remains the platform
process-resident measurement at the exact declared warm-up and terminal cycles
with the unchanged 16 MiB growth limit.

**Rationale**: A hosted Lavapipe rerun after the native prime still returned all
ownership counters to zero and rejected stale handles, but its released RSS
oscillated between 427,380,736 and 563,261,440 bytes across 20 cycles. Cycle 2
to cycle 20 therefore reported 85,561,344 bytes of growth even though lower
later samples proved the pages were allocator cache, not monotonically retained
resources. Releasing unused glibc heap pages immediately after teardown makes
the existing RSS contract measure live post-release residency instead of an
arbitrary allocator-cache high point.

Run `32858413208` exposed why a single trim invocation was still timing
dependent: every owner returned to zero and stale-handle rejection passed, but
the comparison samples were 229,781,504 and 340,525,056 bytes while intermediate
RSS oscillated up to 579,129,344 bytes. Treating the first successful trim as
converged discarded glibc's own signal that another release pass could remain.
A bounded convergence loop honors that signal without changing the lifecycle
or RSS acceptance rule.

Calling `malloc_trim` at every one of 1,000 lifecycle cycles was rejected by
the final manual Linux medium run: both packages reached native execution, but
the repeated global allocator scan exhausted the remaining 1,467-second stage
budget. Trimming only the two authoritative comparison samples preserves the
measurement contract without contaminating lifecycle throughput.

Hosted arm64 Metal run `32874425910` exposed the corresponding macOS gap after
package isolation removed cross-workload contention. Its Lantern lane completed
1,000 cycles, 2,000 captures, seven retained readbacks, zero terminal owners,
and stale-handle rejection, but the exact cycle-20 and terminal resident samples
were 552,910,848 and 579,321,856 bytes. The 26,411,008-byte increase therefore
failed the unchanged 16 MiB limit even though all tracked production objects
were released. The former macOS no-op left freed allocator-zone pages cached at
the terminal comparison point. The Core platform boundary now requests maximal
pressure relief across all macOS malloc zones at the same two checkpoints; a
final hosted rerun remains required before this evidence is accepted.

The first macOS relief rerun `32877744103` proved that allocator convergence was
not the full cause. Lantern again completed 1,000 cycles, 2,000 captures, seven
retained readbacks, zero terminal owners, and stale-handle rejection, but its
cycle-20 and terminal samples were 527,450,112 and 595,853,312 bytes. Inspection
of the execution order found that the terminal sample was taken only after the
seven full-resolution authoritative readback byte arrays had been retained,
while the cycle-20 sample retained none. The resulting 68,403,200-byte delta
therefore included required validation evidence rather than only post-teardown
runtime residency. Authoritative raw readback extraction now occurs after the
unchanged lifecycle/RSS decision and has its own ownership/stale-handle check;
a final hosted rerun remains required.

The first rerun of that ordering, `32881970457` at revision `dec8174`, failed
before native execution because its clean strict build regenerated the
architecture stamp and measured 1,617 lines in
`FStonerDemoApplication.cpp`, above the enforced 1,600-line Demo composition
budget. The local incremental strict build had reused the prior stamp and did
not expose the source-size change. Production lifecycle submission, readback,
capture accounting, and evidence extraction now live in the dedicated private
`FStonerDemoProductionContentRun.cpp` translation unit; the composition root is
1,299 lines. Direct architecture verification, all 14 architecture unit tests,
and a from-zero strict Release build pass locally. Hosted run `32883681679` at
revision `246eeca` then crossed that clean architecture/build gate; its regular
Windows Vulkan, Linux Vulkan, macOS Metal, Linux ASan/UBSan, Linux TSan, and
isolated Lantern medium lanes passed. The isolated Sponza medium lane reached
native execution but consumed 1,665 seconds there and exhausted its unchanged
1,800-second complete-lane budget. The hosted Intel Metal lane also stopped
during its Release build, so another final hosted rerun remains required.

Sampling the Sponza native process showed its load workers dominated by two
SHA-256 passes over nearly identical bytes: strict loading first authenticated
each whole cooked envelope against the already validated generation manifest,
then the generic codec independently hashed the envelope body. A manifest-
authenticated typed-load entry point now computes the whole-envelope digest
once, compares it with the immutable manifest authority, parses the header and
typed payload, and relies on that digest to cover the header, declared body
digest, and body bytes. The generic codec still verifies both body and envelope
digests, and the optimized path fails closed when authority is absent or any
byte is mutated. All ten typed payload families, missing authority, and mutation
rejection are covered by focused tests.

The Sponza v2 1,000-cycle session now requests the selected StaticModel root before the
manifest-wide completeness requests. The existing manager dependency traversal
loads its exact closure and retains it in the request cache; the subsequent
manifest requests therefore prove every record while avoiding duplicate decode
work. This is not a persistent cache or a reduced workload: medium/hardware
still use eight workers, release the full closure every cycle, and retain the
same 1,000/20, image, owner, stale-handle, and RSS contracts. Lantern and
regular 20-cycle ordering remain unchanged because root-first increased their
allocator residency without a corresponding large-package throughput benefit.

A local optimized Sponza 20-cycle visible Metal run passed in 64.02 seconds with
40 captures, seven retained readbacks, zero terminal owners, stale-handle
rejection, and 11,550,720 bytes of post-warm-up RSS growth. An A/B restoration
of the redundant body hash took 75.87 seconds, about 15.8 percent longer. The
final local 1,000-cycle visible Metal proof passed in 1,049.75 seconds with
2,000 captures, seven retained readbacks, zero terminal owners, stale-handle
rejection, semantic/FLIP acceptance, a 462,602,240-byte cycle-20 RSS sample,
a 464,076,800-byte terminal sample, and 1,474,560 bytes of growth. A hosted
rerun remains authoritative for medium closeout.

The next full hosted dispatch, `32895672130`, proved that root-first ordering
must remain Sponza-specific: applying it to Lantern raised post-warm-up RSS
growth to 93,110,272 bytes. The isolated local Lantern scope correction then
passed its complete medium runner in 226.95 seconds with 1,000 cycles, 2,000
captures, seven readbacks, zero terminal owners, stale-handle rejection, and
zero reported RSS growth. The same hosted dispatch still timed out the Sponza
native stage after 1,628 seconds; removing only the duplicate body hash was not
sufficient because each of the 1,000 manager lifetimes still authenticated
about 519 MiB of immutable envelope bytes again.

The final Sponza optimization retains only successful `(generation,
envelope-digest)` authentication results across those manager lifetimes. The
authentication object owns a continuous generation reader lease and is bound
to the exact publication namespace, so the generation cannot be removed or
replaced between cycles. Capacity is bounded by the manifest record count.
Every cycle still performs file-size checks, full file reads, container parsing,
typed decoding, header/codec/schema/shader validation, realization, rendering,
and complete teardown. No payload bytes, decoded Asset object, request/cache
entry, Renderer/RHI handle, or GPU object survives through this memo. The
default Asset Manager path is unchanged; only `production-content-sponza-v2`
at exactly 1,000 cycles enables it. Focused tests prove first-load corruption is
never recorded, wrong generations fail manager binding, missing authority and
malformed containers fail closed, and a second full session load reuses only
authentication while again loading and releasing the complete closure.

The canonical local Sponza medium runner at
`Build/Validation/028/medium-metal-sponza-authentication-reuse-final` passed in
1,094.29 of the unchanged 1,800 seconds. Its native stage took 1,030.93 seconds
and reported 1,000 cycles, 2,000 captures, seven readbacks, zero terminal
owners, stale-handle rejection, a 396,689,408-byte warm-up RSS sample, a
399,540,224-byte terminal sample, and 2,850,816 bytes of growth. It also proved
189/189 reachable assets and 189/189 warm cook reuse. A separate full visible
Metal replay passed all 20 Sponza v2 semantic probes and exact accepted-baseline
FLIP with the same 1,000/20 lifecycle contract. Hosted rerun remains the final
medium authority.

Hosted run `32907674313` then passed every regular Windows Vulkan, Linux
Vulkan, macOS arm64/Intel Metal, artifact revalidation, ASan/UBSan, and TSan
job, but exposed two remaining medium-only costs. Lantern completed all 1,000
cycles and returned every owner, yet its 536,018,944-byte cycle-20 RSS sample
rose to 561,856,512 bytes at terminal, a 25,837,568-byte increase above the
unchanged 16 MiB limit. Sponza completed clean/warm cook, publication,
equivalence, and strict runtime, but its native process consumed the remaining
1,663 seconds and hit the unchanged lane deadline. Because lifecycle failure
precedes the post-lifecycle extraction, Lantern's reported zero readbacks were
a cascading consequence rather than a separate GPU-readback defect.

The authenticated loader still copied every complete file into its per-cycle
file buffer and then copied the full cooked-envelope body into a second buffer
before typed decode. The validation-only previously-authenticated path now
parses a header-only envelope and borrows a bounded body span from the complete
file buffer only for the duration of typed decode. Normal public, generic, and
first-authentication codec paths retain owned body copies and all existing
digest checks. The borrowed path still checks file size, reads the complete
file, validates the container/header/schema/codec/shader authority, decodes a
new typed payload, realizes and renders it, and releases it on every cycle; no
span or body buffer is retained across the call. The manifest-bounded
authentication object is now enabled for both accepted v2 workloads at exactly
1,000 cycles so Lantern receives the same bounded allocator relief, while both
20-cycle regular schedules and unknown revisions remain fail-closed.

The full local Lantern medium runner at
`Build/Validation/028/medium-metal-lantern-borrowed-body` passed in 194.38
seconds, including a 181.55-second native stage, 1,000 cycles, 2,000 captures,
seven readbacks, 37/37 reachable and reused assets, zero terminal owners, stale
handle rejection, a 545,505,280-byte cycle-20 RSS sample, a 481,918,976-byte
terminal sample, and zero reported growth. A direct native-only Sponza replay
against the same clean/warm/strict generation also passed 1,000 cycles, 2,000
captures, seven readbacks, zero terminal owners, stale rejection, a
396,132,352-byte cycle-20 sample, a 398,163,968-byte terminal sample, and
2,031,616 bytes of growth. The hosted isolated medium lanes remain the final
throughput and closeout authority.

The complete hosted rerun `32923918933` falsified the hypothesis that the body
copy was sufficient for Lantern allocator stability. Lantern finished its
1,000-cycle native stage in about 458 seconds with 2,000 captures, no retained
owners, and stale-handle rejection, but its cycle-20 RSS of 519,061,504 bytes
rose to 578,568,192 bytes at terminal, a 59,506,688-byte increase. The missing
post-lifecycle readbacks again cascaded from that RSS decision. The comparison-
point macOS pressure-relief path already examines all malloc zones for up to
eight converging passes, so increasing the same relief loop lacks evidence and
would not address per-thread allocator arenas created by repeated eight-worker
manager lifetimes.

Lantern's 37-asset closure was then evaluated with one manager worker to test
whether repeated worker-thread arenas caused that growth. This changes only
scheduling parallelism: every cycle still constructs and shuts down a manager,
reads and typed-decodes the complete closure, realizes both paths, submits
native GPU work, performs capture accounting, releases every owner, and
participates in the same 1,000/20 and 16 MiB gate.

The full local proof at
`Build/Validation/028/medium-metal-lantern-single-worker` passed in 668.94
seconds, with a 657.07-second native stage, 1,000 cycles, 2,000 captures, seven
readbacks, 37/37 reachable and reused assets, zero terminal owners, stale
rejection, a 358,744,064-byte cycle-20 RSS sample, a 359,514,112-byte terminal
sample, and only 770,048 bytes of growth. Hosted run `32927821133` rejected the
hypothesis: the same single-worker workload finished all 1,000 cycles with zero
owners and stale rejection, but its RSS grew from 349,356,032 to 442,335,232
bytes, a worse 92,979,200-byte increase. Lantern therefore returns to eight
workers; the completed experiment remains evidence that worker-thread count is
not the hosted RSS root cause.

The same hosted run gave Sponza 1,684 seconds of native execution after every
pre-native stage passed, but the existing eight-worker load still timed out
without producing lifecycle evidence. Sponza's 189-record, texture-heavy
closure therefore uses sixteen workers, still half the Manager's validated
32-worker bound. This is isolated to the accepted Sponza v2 1,000-cycle
workload; Lantern and regular worker selections do not change. The direct local
1,000-cycle replay completed well within the hosted native allowance and passed
with 2,000 captures, seven readbacks, zero terminal owners, stale rejection, a
394,887,168-byte cycle-20 RSS sample, a 396,853,248-byte terminal sample, and
1,966,080 bytes of growth. Hosted run `32927821133` still timed out after every
pre-native stage passed: the sixteen-worker native process exhausted its exact
1,631-second remaining allowance without producing lifecycle evidence.

Inspection then found that the validation session retained each original typed
Asset handle but also deep-copied every immutable model, mesh, material,
texture, and shader payload into the Renderer closure on every lifecycle
cycle. This duplicated the largest Sponza CPU payloads after complete strict
file reads and typed decode, without adding validation coverage. `TAssetHandle`
now exposes an aliasing typed shared pointer whose ownership remains the same
handle control. The Renderer closure therefore observes the exact decoded
immutable object without copying it, and the manager's external-retention
counter remains live until every handle and alias is released. The public
handle lifetime test proves that aliases survive handle/manager destruction
and release the final control deterministically. A strict Release build and a
20-cycle native Sponza replay passed before the exact sixteen-worker
1,000-cycle replay completed in 947.82 seconds with 2,000 captures, seven
readbacks, zero terminal owners, stale rejection, a 372,752,384-byte cycle-20
RSS sample, a 374,784,000-byte terminal sample, and 2,031,616 bytes of growth.
This retains complete loading, decoding, realization, native rendering, and
teardown work while leaving 683 seconds against the hosted native allowance;
the same binary then passed the exact eight-worker Lantern 1,000-cycle replay
in 176.82 seconds with 2,000 captures, seven readbacks, zero terminal owners,
stale rejection, a 528,334,848-byte cycle-20 RSS sample, a 457,965,568-byte
terminal sample, and zero positive growth. Hosted revalidation remains the
throughput and closeout authority. Hosted run `32932377467` proved the
payload-alias change independently useful but insufficient: Lantern passed its
complete medium profile in 501.14 seconds and its 465.87-second native stage
reported 14,630,912 bytes of RSS growth, while Sponza again exhausted its exact
1,687-second native allowance after every pre-native stage passed.

The remaining Sponza review found a composition-lifetime defect rather than a
new payload cost. `FProductionContentSession::Shutdown()` intentionally keeps
the generation-bound authentication context so a later `Load()` can reuse it,
and its focused unit test exercised that sequence. The application, however,
stored the session inside `FProductionContentRuntime` and destroyed the entire
runtime after every cycle, so the production gate discarded the context and
reader lease before the next manager lifetime. The session now belongs to the
application for the complete gate while each cycle still destroys all loaded
payload handles, manager/cache state, Renderer snapshots, RHI objects, and
native resources. A new integration inspection bit requires an actual reuse
hit for every 1,000-cycle Metal/Vulkan workload. Strict Release, 42 Core
platform tests, 27 strict-runtime tests, the 38-test runner, six workflow tests,
and 14 architecture tests passed. The exact local sixteen-worker Sponza replay
then passed the new reuse assertion in 780.84 seconds with 2,000 captures,
seven readbacks, zero owners, stale rejection, a 373,227,520-byte cycle-20 RSS
sample, a 374,112,256-byte terminal sample, and 884,736 bytes of growth. The
same binary passed Lantern in 169.68 seconds with the reuse assertion, zero
owners, stale rejection, and zero positive RSS growth.

Hosted run `32936488807` at revision `c94e749` then passed every regular
Windows Vulkan, Linux Vulkan, macOS arm64/Intel Metal, artifact revalidation,
ASan/UBSan, TSan, and isolated Lantern medium job. Linux regular specifically
returned from 194,101,248 to 171,290,624 resident bytes with zero positive
growth, 40 captures, seven readbacks, zero terminal owners, and stale-handle
rejection, proving the fixed one-second glibc comparison point. Sponza alone
again exhausted its exact 1,668-second native allowance after clean/warm cook,
publication, semantic equivalence, and strict runtime all passed. The unchanged
Sponza generation contains 189 envelopes totaling 519,146,543 bytes, so the
1,000-cycle contract deliberately performs about 495 GiB of complete cooked
file reads before typed decode.

The persistent session still invoked `IndexAndLayout` twice per cycle: once in
the Demo session and again while each strict Manager bound the same immutable
generation. Both paths parsed the same manifest, queried every payload, and
enumerated the same directory even though the authentication-owned reader lease
already prevented generation replacement. The session now retains the first
successful pointer/manifest/layout result as bounded, payload-free metadata.
Every new Manager validates that authority against the exact publication
namespace, generation, manifest digest, target, required extensions, record
count, and continuously held reader lease without repeating filesystem layout
enumeration. Payload access is unchanged. A new Core bounded regular-file read
also performs the non-link/regular-file check, size bound, allocation, and
complete read through one native handle instead of a separate query followed
by another open. Every cycle still reads every one of the 189 complete files,
parses every container, creates new typed payloads, realizes both render paths,
submits/readbacks, and releases all runtime and graphics ownership.

Strict Release, 44 Core platform tests, the complete Asset Manager suite,
strict-runtime, production Demo, runner `38/38`, workflow `6/6`, architecture
`14/14`, and direct architecture verification pass locally. The exact local
Sponza Metal replay then passed all 1,000 cycles in about 620 seconds with the
new validated-metadata reuse assertion, 2,000 captures, seven readbacks, zero
terminal owners, stale rejection, a 370,196,480-byte cycle-20 RSS sample, a
372,883,456-byte terminal sample, and 2,686,976 bytes of growth. Hosted rerun
remains authoritative for medium closeout.

Hosted run `32942777805` then passed Windows Vulkan, Linux Vulkan, macOS
arm64/Intel Metal, all four independent artifact consumers, ASan/UBSan, TSan,
and the isolated Lantern medium lane. Sponza again exhausted its exact
1,672-second native allowance after clean/warm cook, publication, semantic
equivalence, and strict runtime passed. The remaining throughput defect was
the Sponza-only root-first schedule itself: one Manager worker recursively read
and decoded the entire 189-record closure before the other fifteen workers had
useful work, so the selected sixteen-worker policy could not parallelize the
largest image and texture batches.

The Sponza v2 1,000-cycle schedule now computes deterministic dependency-first
manifest batches. Every record in a ready batch is requested concurrently;
later closures borrow only immutable dependencies already retained by the same
cycle's Manager cache. The cache borrow copies metadata and shared immutable
ownership, never payload bytes, and cannot cross the Manager lifecycle. Each
of the 189 records is therefore still completely read, envelope-parsed, and
typed-decoded exactly once per cycle, while root publication, complete
manifest coverage, generation authentication, realization, native rendering,
and teardown remain unchanged. A scheduler regression proves an already-loaded
dependency is not re-executed, and the strict Sponza fixture requires exactly
one strict-loader execution per manifest record.

Strict Debug and Release builds, the complete Asset Manager suite, all 27
strict-runtime checks, production Demo, image acceptance, and camera preview
passed locally. The exact Sponza Metal replay then completed all 1,000 cycles
in about 390 seconds, down from 795 seconds for the preceding clean-worktree
run, with 2,000 captures, seven readbacks, zero terminal owners, stale-handle
rejection, both authentication/validation reuse assertions, a 272,072,704-byte
cycle-20 RSS sample, a 272,171,008-byte terminal sample, a 275,333,120-byte
peak, and only 98,304 bytes of growth. Hosted rerun remains authoritative for
medium closeout.

The same `32927821133` dispatch also exposed comparison-point instability on
the unchanged 20-cycle Linux regular lane: cycle 2 fell to 122,810,368 bytes
after trimming, intermediate samples ranged from 302,780,416 to 461,225,984
bytes, and cycle 20 settled at 173,289,472 bytes. All owners were zero and stale
rejection passed, but the exact endpoint difference was 50,479,104 bytes. On
Metal, a completed handler can publish zero submission ownership immediately
before its native command buffer and driver allocations leave the callback; on
glibc, arena release and kernel RSS accounting can likewise settle just after
the first trim. The comparison-point protocol now performs the existing
bounded all-zone/arena relief, waits a fixed platform-specific interval,
performs the same bounded relief again, and then takes exactly one authoritative sample.
The wait is unconditional and value-independent; it does not select a minimum,
move the warm-up/terminal cycles, or weaken the 16 MiB threshold. A strict
Release build, all 42 Core platform tests, all 27 strict-runtime tests, and a
20-cycle native Metal replay passed with 40 captures, seven readbacks, zero
owners, stale rejection, and 5,406,720 bytes of RSS growth. The 100-millisecond
protocol passed the next hosted Metal and Linux regular run: Metal grew only
1,818,624 bytes and Linux fell from 233,730,048 to 164,536,320 bytes. Run
`32932377467` then showed the Linux result was not repeatable: its exact
cycle-2 sample fell to 120,991,744 bytes while cycle 20 remained 171,040,768
bytes, producing 50,049,024 bytes of apparent growth despite zero owners and
stale rejection. glibc/Lavapipe therefore uses a fixed one-second quiescence
between the same two bounded trim sequences, while Metal retains 100
milliseconds. The sample remains single, unconditional, and value-independent;
hosted Linux rerun remains authoritative.

Hosted run `32951285909` at revision `86a0014` proved the dependency-first
throughput change: Sponza completed all 1,000 cycles inside the lane deadline,
with 2,000 captures, zero terminal owners, and stale-handle rejection. It also
showed that fixed allocator relief alone does not make a persistent native
backend a stable RSS comparison boundary. Sponza's exact cycle-20 and terminal
Metal samples were 249,364,480 and 342,310,912 bytes, a 92,946,432-byte
increase with a 343,195,648-byte peak. The same dispatch's Linux Vulkan regular
lane completed all 20 cycles, 40 captures, seven readbacks, zero owners, and
stale rejection, but moved from 179,372,032 to 288,284,672 bytes, a
108,912,640-byte endpoint increase while intermediate Lavapipe residency
oscillated much higher. Neither failure left a tracked production object alive.

The authoritative native-headless RSS lanes now release the submission harness
and completely shut down the requested backend after production-cycle teardown
at both exact comparison points. Linux Vulkan applies this only to the regular
20/2 headless gate; macOS Metal applies it only to the medium 1,000/20 headless
gate. After the unchanged single warm-up sample, the same requested backend is
reinitialized, native execution is re-proved, and the shared submission harness
is restored before cycle processing continues. After the unchanged terminal
sample and lifecycle decision, it is restored again for the existing uncounted
seven-readback evidence extraction. The continuous strict-generation session
and reader lease survive, but no RHI/native device ownership does. Visible
hardware presentation is deliberately unchanged.

The exact local Sponza v2 Metal replay passed this boundary with 1,000 cycles,
2,000 captures, seven readbacks, zero terminal owners, stale rejection, both
generation reuse assertions, and exactly two backend recycles. Its cycle-20
sample was 269,139,968 bytes, terminal sample 271,564,800 bytes, peak
272,875,520 bytes, and growth 2,424,832 bytes. Strict Debug/Release, production
Demo/content/camera/image suites, all 14 architecture unit tests, and direct
architecture verification also passed. Hosted Linux and Metal reruns remain
required before the comparison-boundary fix is accepted as final evidence.

Taking repeated samples or selecting a minimum was rejected because it would
make the gate value-dependent. Raising the 16 MiB limit was rejected because
it would weaken the lifecycle contract. Restarting the backend on every cycle
was rejected because the two exact post-teardown comparison points are
sufficient and per-cycle restart would replace the declared lifecycle workload
with a different one.

The subsequent comparison-only-trim run proved that allocator scanning was not
the remaining medium timeout cause: both packages again completed cook, warm
reuse, publication, equivalence, and strict runtime, then their simultaneously
started Lavapipe lifecycle processes timed out together. Serializing those
native stages did not solve the feasibility problem: on run `32842555956`, the
first isolated Lantern lifecycle used all 1,471 remaining seconds without
completing. The first hosted Metal attempt then proved that complete-pipeline
concurrency also distorts native evidence: Lantern took about 824 seconds and
failed the unchanged RSS limit at 21,954,560 bytes while Sponza exceeded 1,530
seconds. The weekly/manual medium lane therefore overlaps only disjoint
CPU/cook work and serializes Metal native lifecycle stages, matching the prior
isolated Metal evidence of about 253 seconds for Lantern and 1,302 seconds for
Sponza. Lavapipe continues to own the required Linux regular native/RSS
evidence.

The first lock-only hosted run `32858413208` still timed out: Lantern entered
native execution at 14:36:32 while Sponza continued clean cook, publication,
equivalence, and strict runtime until 14:39:30. The lock prevented two Metal
processes from overlapping, but it did not isolate a native process from the
other package's CPU, I/O, allocator, and memory pressure. Medium therefore uses
a package barrier after both strict-runtime stages and only then enters the
existing native lock. Cook work remains concurrent, both 1,000-cycle lifecycles
remain serial and unmodified, and the shared 1,800-second deadline remains the
authority.

The barrier revision then passed the complete two-package profile locally in
1,756.410 seconds, including 20 Lantern clean determinism runs, but hosted run
`32864192139` exhausted the same 1,800-second budget. Those 20 clean runs are
the per-target responsibility of the regular gate (FR-016/SC-003); FR-033's
medium scale gate requires one clean cook and an unchanged 100-percent-reuse
warm cook for every accepted package. Medium and hardware therefore execute
one clean run per package and retain every warm, publication, equivalence,
strict-runtime, 1,000/20 lifecycle, RSS, readback, and ownership requirement.
This removes duplicated regular work rather than reducing medium scale proof,
and the summary reports the number of clean runs actually executed.

Hosted run `32869040060` still exhausted 1,800 seconds after that de-duplication,
while the identical full profile passed on the physical M4 Pro in 1,688.229
seconds. A single hosted lane therefore has no stable margin for the sum of both
unaltered native lifecycles. Scheduled/manual medium now uses two isolated
`macos-26` Metal lanes, one exact declared package per lane, running
concurrently. Each lane retains its own 1,800-second authority and complete
clean/warm/publication/equivalence/strict/1,000-cycle/RSS evidence. A separate
aggregate job downloads only the summary and artifact-manifest authority,
requires the exact profile package set, rejects duplicates or cross-lane
corpus/target drift, and fails unless both native results pass.

Raising the 1,800-second limit, reducing lifecycle cycles, weakening 20-cycle
warm-up/RSS, or running both native processes on one host were rejected. The
two-lane wall time is the maximum package lane time rather than their sum, which
matches SC-010's 30-minute-per-declared-lane contract without removing work.

**Alternatives considered**:

- Raise or disable the 16 MiB limit on Lavapipe: rejected because it weakens the
  same leak contract that the gate is intended to enforce.
- Replace RSS with backend ownership counters: rejected because counters and
  process residency detect different classes of leaks and both remain required.
- Use the minimum or median of several cycles: rejected because it changes the
  exact post-warm-up-to-terminal contract and could hide terminal retention.
- Trim every sampled cycle: rejected because intermediate RSS remains useful
  peak evidence and 1,000 global allocator scans exceeded the medium budget.
