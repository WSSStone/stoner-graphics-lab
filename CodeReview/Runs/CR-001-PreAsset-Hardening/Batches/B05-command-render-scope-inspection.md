# B05-S01: Command Pools, Buffers, Barriers, And Render Scope Inspection

## Scope

This packet inspected one Feature 011 responsibility domain across exactly
eight production files, 989 lines total:

- `FVulkanCommandPool.h/.cpp`;
- `FVulkanCommandBuffer.h/.cpp`;
- `FVulkanRenderPass.h/.cpp`;
- `FVulkanFramebuffer.h/.cpp`.

The review also read bounded device, queue, and submission call sites, RHI
contracts, Feature 011 authority documents, and maintained tests. No
production source or maintained test changed.

## Authority And Invariants

Feature 011 FR-001 through FR-006, FR-009 through FR-013, FR-016, FR-017,
FR-020, FR-022, SC-002, and SC-003 require:

- command allocation to be device-mediated, queue-capability checked, bounded,
  and explicitly failing without usable partial state;
- recording, submission, completion, reset, and invalidation transitions to
  be controlled by the responsible command, queue, and completion objects;
- transfer records to validate selected subresources, ranges, compatibility,
  and destination byte footprints before entering an executable buffer;
- empty or incompatible render-pass/framebuffer descriptions to return
  explicit failure without a valid object;
- shutdown and later calls on retained objects to reject deterministically
  without crashes.

The RHI device-owned creation contract makes backend wrapper constructors and
state-transition collaborators part of these invariants, not optional calling
conventions.

## Findings

### CR001-B05-F001 — S1 Accepted

`FVulkanCommandBuffer` stores a raw `FVulkanDiagnostics*` into its owning
device, while the public shared command-buffer result may outlive that device.
`FVulkanDevice` does not perform shutdown from its destructor, and command
buffer invalidation does not detach diagnostics. Both accepted and rejected
recording paths can later dereference ownership that no longer exists.

The finding is established from the complete ownership/control-flow chain.
Following the user's safety constraint, this inspection did not deliberately
trigger an invalid access. B05-S02 must replace the raw lifetime assumption and
add a non-faulting maintained regression for retained objects.

### CR001-B05-F002 — S2 Accepted

Command-pool, command-buffer, render-pass, and framebuffer constructors are
public, and queue-owned submission/completion transitions are public as well.
The retained probe consequently:

- allocates a command buffer from a directly constructed Present pool;
- constructs empty render-pass and framebuffer descriptions that report
  `Valid`;
- advances a command buffer through `Submitted` and `Resettable` without a
  queue or completion object.

The device factories also perform wrapper/control-block/tracking allocations
without mapping allocation failure to an RHI result or rolling back partial
pool/tracking state. Capability, validity, lifecycle authority, and explicit-
failure contracts can all be bypassed.

### CR001-B05-F003 — S2 Accepted

The shared local texture-region helper compares every region to base-level
width, height, and depth even when a nonzero mip is selected. Texture copy also
omits source/destination format compatibility. Texture-to-buffer recording
duplicates a format-width switch and multiplies row length, image height,
depth, and texel size without checked arithmetic.

The retained probe accepts an 8x8 copy and readback from mip 1 of an 8x8
texture whose selected extent is 4x4, and accepts an RGBA8-to-R8 texture copy.
Unchecked footprint arithmetic is independently visible in the reviewed
control flow. These invalid records can reach an ended command buffer.

## Barrier And Render-Scope Review

For factory-created valid objects, the inspected render-scope implementation
rejects non-graphics queues, nested/missing scopes, invalidated objects,
framebuffer/render-pass identity mismatches, attachment mismatches, and
incompatible clear values. Buffer range checks use subtraction-based bounds,
and ordinary barrier lifecycle/usage checks are locally consistent.

Barrier descriptions can currently hold both a buffer and texture and silently
prefer the buffer. The authority documents do not define exclusivity strongly
enough to open a separate finding in this packet; B09 should consider making
resource-barrier shape validation an RHI-wide invariant.

## Coverage Gap And Impact

Maintained Feature 011 tests cover ordinary base-mip transfers and factory-
mediated valid/invalid render objects, but not the selected-mip, format,
arithmetic, construction-closure, transition-authority, retained-lifetime, or
allocation-rollback cases above. The focused probe reproduces seven observable
signals under strict ASan/UBSan without a sanitizer report.

These gaps matter before Asset Features 021–025 introduce more mip levels,
formats, cooked payloads, asynchronous ownership, and frequent transfer
recording. The command layer cannot make invalid subresources or hidden device
lifetimes prerequisites of the asset pipeline.

## Validation

- retained strict sanitizer probe: seven defect signals, zero exit, no
  ASan/UBSan report;
- exact requirement and call-site review for Feature 011;
- lifetime finding supported by static ownership/control-flow evidence only;
- no production or maintained-test source changed;
- no full gate or GitHub Actions run was required for this inspection packet.

## Handoff To B05-S02

B05-S02 should repair the three findings as one bounded ownership/validation
migration:

1. give command diagnostics an owner state whose lifetime safely covers
   retained command buffers, and make device destruction enforce shutdown;
2. close direct construction of command/render wrappers and restrict
   submitted/completed transitions to queue/submission collaborators;
3. make command-pool, command-buffer, render-pass, and framebuffer factory
   allocation/tracking failure-atomic with explicit `Unavailable` results;
4. use selected-mip extents, shared format metadata, checked footprints, and
   compatible texture-copy descriptions for transfer validation;
5. add maintained API-closure, lifetime, failure-injection, selected-mip,
   compatibility, and footprint regressions.

No push or GitHub Actions run occurred in this packet.
