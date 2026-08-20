# Research: Native Metal Backend

## 1. Native Language And Framework Boundary

**Decision**: Implement the backend in private Objective-C++20 `.mm` units with
ARC. Public Metal backend headers remain ordinary C++ and expose no Objective-C
or Apple framework types. Link Metal, QuartzCore, Cocoa, and Foundation only in
the macOS Metal target.

**Rationale**: `CAMetalLayer`, Cocoa view attachment, and Metal protocols are
native Objective-C APIs. A private Objective-C++ boundary is direct, auditable,
and avoids introducing a second wrapper ownership model. It also keeps Windows
and Linux consumers independent of Apple SDKs.

**Alternatives considered**: Apple metal-cpp was evaluated but still requires
Objective-C++ for Cocoa/layer integration and adds wrapper lifetime conventions;
a C bridge would create substantial glue without improving layering; exposing
native handles in RHI would violate the constitution.

## 2. Deployment And Device Families

**Decision**: Set the desktop baseline to macOS 12.0 and MSL 2.4. Treat arm64
Apple Silicon and x86_64 Intel Metal Macs as first-class. Query actual GPU family,
format, sample, threadgroup, binding, and memory capabilities at runtime instead
of inferring them from CPU architecture.

SCons sets `MACOSX_DEPLOYMENT_TARGET=12.0` for every Metal, test, demo, and
AssetCooker compile, link, and finalization action. Objective-C++ builds treat
unguarded availability diagnostics as errors. The architecture verifier checks
emitted arguments and profile JSON so an accidental newer minimum cannot pass.

**Rationale**: macOS 12.0 supports both Apple Silicon and the required Intel
population and is the first compatible deployment baseline for MSL 2.4. Runtime
feature queries remain the authority for actual device support. Apple's
[Metal capability tables](https://developer.apple.com/metal/capabilities/) and
[`MTLGPUFamily`](https://developer.apple.com/documentation/metal/mtlgpufamily)
are the canonical capability sources.

**Alternatives considered**: A current-OS-only baseline would abandon the Intel
goal; architecture-based assumptions are incorrect for discrete/integrated
memory and GPU-family differences; targeting a newer language merely for syntax
provides no Feature 027 benefit.

## 3. Adapter Selection

**Decision**: Enumerate compatible devices into stable records. An explicit
registry ID wins. Otherwise reject devices missing required baseline features,
then rank by non-removable/high-performance suitability, capability score,
registry ID, and normalized name. Record all candidates and the selected reason.

**Rationale**: Native enumeration order is not an engine contract. Explicit
selection supports reproducibility and diagnostics; a final stable identity
tie-break prevents changing enumeration order from changing results.

**Alternatives considered**: `MTLCreateSystemDefaultDevice` alone cannot explain
or reproduce multi-GPU selection; name-only selection is not unique; preferring
low power by default conflicts with graphics-engine workloads but can be added
as a later explicit policy.

## 4. Ownership And Backend Decomposition

**Decision**: Use a shared `FMetalDeviceOwnerState` containing device identity,
validity, submission generation, and atomic live/in-flight counters. Buffers,
textures, descriptors, pipelines, commands, queues, synchronization, and
presentation are distinct classes retaining this state. A small factory creates
the device and exposes inspection; no single native context implements all RHI
objects.

**Rationale**: Shared owner identity makes foreign/stale checks and shutdown
ordering consistent while responsibility-specific classes avoid repeating the
large Vulkan native-context pattern.

**Alternatives considered**: One god context obscures ownership and failure
unwind; independent raw device references make invalidation races likely; a new
global backend service would create hidden lifetime coupling.

## 5. Resource Storage And Coherency

**Decision**: Select storage from RHI visibility and usage. Host-visible data
uses shared storage on unified-memory devices and managed storage where required
on discrete Intel/AMD devices, with explicit `didModifyRange` and synchronize
operations. Device-local/render-target resources use private storage and bounded
staging/blit paths. Feature 027 does not use memoryless storage.

**Rationale**: Apple documents different best practices for unified and discrete
memory in [resource storage modes](https://developer.apple.com/documentation/metal/setting-resource-storage-modes).
The policy preserves RHI upload/readback semantics on both required Mac families.
Avoiding memoryless attachments keeps deferred cross-pass use and readback valid.

**Alternatives considered**: Shared-everywhere wastes discrete-GPU bandwidth and
can violate intended visibility; private-everywhere complicates host-visible
buffers; memoryless render targets cannot satisfy the current readback contract.

## 6. Commands, Queues, Barriers, And Synchronization

**Decision**: Maintain logical graphics, compute, and transfer queues using
separate `MTLCommandQueue` instances; presentation is submitted on a compatible
graphics queue. RHI command buffers record validated backend-neutral commands
and encode them at submission. Normal resources use Metal's tracked hazards;
RHI barriers validate logical states and conservatively end/split encoders when
required. Monotonic shared-event values plus command completion handlers provide
cross-queue and CPU-visible fence/semaphore semantics without busy waiting.

**Rationale**: Metal command encoders impose stronger scope boundaries than the
RHI command list. Recording first preserves RHI validation and lets submission
select legal encoder transitions. Apple's [resource synchronization](https://developer.apple.com/documentation/metal/resource-synchronization),
[events](https://developer.apple.com/documentation/metal/about-synchronization-events),
and [fences](https://developer.apple.com/documentation/metal/synchronizing-passes-with-a-fence)
support this mapping.

**Alternatives considered**: Encoding directly during public recording makes
state errors hard to recover; untracked hazards increase proof burden without a
measured need; CPU polling violates bounded-wait and power requirements.

## 7. Descriptor And Binding Policy

**Decision**: Use direct Metal buffer/texture/sampler binding in Feature 027.
Binding-map policy v1 deterministically maps `(stage, set, binding, descriptor
type, array element)` to native indices, reserves documented buffer ranges for
vertex and constant data, rejects overflow against reported device limits, and
has one canonical Tools implementation. Asset stores the resulting immutable
entries and digest as cooked shader evidence. Renderer copies that evidence into
backend-neutral RHI native-binding metadata; Metal validates and consumes the
metadata without importing Tools or independently assigning slots. The digest
enters shader evidence, DDC keys, pipeline validation, and reuse keys.

**Rationale**: Direct binding covers the current public descriptor contract and
is available across the deployment range. A single versioned policy prevents
the offline MSL compiler and runtime encoder from silently disagreeing.

**Alternatives considered**: Argument buffers add family/tier variation and are
an optimization rather than a correctness requirement; source rewriting by
ad-hoc string replacement is unsafe; assigning slots independently at runtime
would make cooked payloads incompatible.

## 8. Shader Transformation Dependency

**Decision**: Vendor exactly `spirv_cross.cpp`, `spirv_cross_parsed_ir.cpp`,
`spirv_parser.cpp`, `spirv_cfg.cpp`, `spirv_glsl.cpp`, and `spirv_msl.cpp` plus the headers those
translation units include, privately under `ThirdParty/spirv-cross`, pinned to commit
`a0fba56c34a6700f1724bf9b751da5b488a3775c` (Vulkan SDK 1.4.335 lineage,
SPIRV-Cross 0.68.0), with Apache-2.0 license, provenance, source hashes, and
dedicated deterministic tests. Link it only into AssetCooker.

The GLSL translation unit is required even though GLSL is not an output target:
upstream `CompilerMSL` derives from `CompilerGLSL`, and the pinned upstream CMake
graph requires the GLSL library whenever MSL support is enabled.

**Rationale**: Khronos [SPIRV-Cross](https://github.com/KhronosGroup/SPIRV-Cross)
provides a maintained C++ API for SPIR-V reflection and MSL generation including
explicit MSL resource binding. Pinning source avoids relying on a developer's
Vulkan SDK executable and makes all three hosts reproducible.

**Alternatives considered**: Runtime conversion violates strict cooked mode;
shelling out to an unpinned SDK tool is not reproducible; hand-translating SPIR-V
is far outside scope; making SPIRV-Cross an Asset dependency reverses layering.

## 9. Canonical MSL Derivation

**Decision**: Validate SPIR-V structure, entry point, stage, interface, and
binding policy before transformation. Emit canonical UTF-8 MSL with LF endings,
stable options, no volatile path/comment data, and a separately canonicalized
derivation record. Repeat derivation on Windows, macOS, and Linux and compare
MSL plus evidence digests.

**Rationale**: Cross-platform derivation is useful only when normalization
removes host-path and line-ending differences. Complete input and option digests
make a mismatch attributable rather than merely observable.

**Alternatives considered**: Comparing only compiled `metallib` bytes is not
portable across Apple compiler versions; source text without option evidence is
insufficient; accepting per-host whitespace differences weakens SC-004.

## 10. Native Metal Library Cooking

**Decision**: On macOS, AssetCooker launches `/usr/bin/xcrun` with an explicit
executable/argv process contract to run `metal` to AIR and `metallib` to a native
library. It captures bounded output, timeout, exit status, Xcode/SDK/compiler,
deployment target, MSL version, architecture/profile, and output digest. No shell
is involved. The local machine currently lacks the optional Metal Toolchain;
`xcodebuild -downloadComponent MetalToolchain` is therefore a documented
prerequisite, not an implicit install step.

**Rationale**: Apple's [Metal libraries](https://developer.apple.com/documentation/metal/metal-libraries)
and [offline tool flow](https://developer.apple.com/library/archive/documentation/Miscellaneous/Conceptual/MetalProgrammingGuide/Dev-Technique/Dev-Technique.html)
define AIR plus metallib as the production path. Explicit argv avoids quoting
and injection bugs and is reusable by other offline tools.

**Alternatives considered**: `newLibraryWithSource` violates the no-runtime-
compile requirement; shell command strings are unsafe and nondeterministic;
checking generated libraries into source would detach them from cook inputs.

## 11. Payload And Profile Versioning

**Decision**: Generalize RHI shader modules to typed byte payloads with explicit
`SPIRV` and `MetalLibrary` formats. Retain `MSL` in Asset evidence as the
intermediate source format, but add `MetalLibrary` as the final runtime payload.
Introduce v2 target-profile and cooked shader-payload schemas that can describe
native libraries while continuing to read all v1 Vulkan profiles/generations.
New Mac Metal arm64/x86_64 profiles use v2; no old generation is rewritten.

**Rationale**: Existing `FRHIShaderBytecodeDesc` assumes 32-bit SPIR-V words and
cannot represent arbitrary metallib bytes. Treating a binary library as MSL
would make validation ambiguous. Compatible readers protect Feature 025/026
published content and Vulkan regressions.

**Alternatives considered**: Padding metallib into words leaks SPIR-V assumptions;
overloading `MSL` hides source/binary distinctions; an incompatible in-place v1
change would invalidate existing cooked evidence.

## 12. Presentation Ownership And Lifecycle

**Decision**: `FMetalPresentationContext` creates and exclusively owns a
`CAMetalLayer`, attaches it to the Cocoa view returned by GLFW's
`glfwGetCocoaView`, and detaches it on the main thread before the borrowed window
expires. It derives `drawableSize` from framebuffer pixels, tracks scale and
format, acquires one frame-scoped drawable, and presents it on the rendering
command buffer. Zero extent or `nextDrawable == nil` yields a bounded paused or
temporarily unavailable result, never a retry loop.

**Rationale**: [`CAMetalLayer`](https://developer.apple.com/documentation/quartzcore/cametallayer)
owns drawable production while Application must retain window/view ownership.
GLFW's [native Cocoa access](https://www.glfw.org/docs/latest/group__native.html)
provides the existing platform bridge without putting Cocoa in Application's
public API.

**Alternatives considered**: Application-owned layers leak Metal into a higher
layer; retaining drawables across frames violates their lifecycle; a hidden
offscreen fallback would misreport presentation success.

## 13. Validation Architecture

**Decision**: Separate evidence into deterministic/mock, native-offscreen,
visible-manual, and cross-backend comparison tiers. Native gates require Metal
GPU readback or presentation artifacts and record device, GPU family, capability,
shader Asset/version, binding-policy digest, and validation tier. Semantic
oracles may calculate expectations but cannot satisfy a native gate. Normalize
row padding, orientation, depth, colorspace, and floating-point tolerances before
Metal/Vulkan comparison.

**Rationale**: Tiering prevents a deterministic substitute from being mistaken
for actual backend execution and preserves explainable cross-API tolerance.

**Alternatives considered**: Screenshot-only evidence misses transfer/compute/
sync behavior; one opaque pass/fail report cannot prove native execution;
bit-identical cross-vendor images are not a realistic semantic contract.

## 14. CI Matrix And Hardware Evidence

**Decision**: Keep Windows/macOS/Linux Debug and strict Release plus Linux
ASan/UBSan and TSan for shared code. The arm64 jobs use `macos-26`; add Intel
build/cook jobs on `macos-26-intel`. Standard jobs probe for a real `MTLDevice`;
an absent device records `unavailable` and cannot satisfy a native criterion.
Add a separate required hardware workflow with GPU-capable arm64 and x86_64
runner labels. Each hardware job fails unless the probe succeeds and all native
offscreen gates produce GPU readback. Feature closeout requires both hardware
artifacts. Visible 3,000-frame/20-cycle acceptance remains a manual real-hardware
gate because desktop interaction is distinct from offscreen automation.

**Rationale**: GitHub's current official
[runner image table](https://github.com/actions/runner-images) lists both CPU
architecture labels, but GitHub's
[larger-runner reference](https://docs.github.com/en/actions/reference/runners/larger-runners)
only explicitly advertises GPU acceleration for arm64 XLarge macOS runners.
Therefore CPU architecture availability proves compilation/cooking coverage,
not native GPU availability. Required dedicated hardware labels make the
automated native obligation explicit and keep `unavailable` hosted probes from
turning into false success.

**Alternatives considered**: `macos-latest` alone currently selects arm64 and
does not prove Intel compilation; assuming every macOS VM has Metal would create
false native passes; requiring hosted GUI interaction would produce fragile or
false presentation evidence; making paid XLarge capacity mandatory is avoided
unless the repository explicitly enables it.

## 15. Scope Boundary

**Decision**: Feature 027 stops at existing RHI conformance, native desktop
presentation, strict cooked shader consumption, triangle/deferred equivalence,
and hardening. iOS lifecycle, argument-buffer optimization, mesh shaders, ray
tracing, meshlets, streaming/residency, and Asset GPU ownership remain future
work.

**Rationale**: These are independent platform or advanced-rendering systems.
Implementing them here would obscure whether the basic backend is correct and
would violate the roadmap's staged dependencies.

**Alternatives considered**: Folding iOS into the desktop backend adds a second
application lifecycle; implementing advanced Metal features before baseline RHI
conformance creates capability without a stable foundation.
