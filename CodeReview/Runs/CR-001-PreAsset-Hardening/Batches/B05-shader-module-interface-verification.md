# B05-S09: Shader Modules And Interfaces Verification

## Verification Target

This packet independently verifies the repairs committed at `d5f1714`:

- `CR001-B05-F007`: bounded SPIR-V structure and declared stage/entry-point validation;
- `CR001-B05-F008`: device-owned shader/layout construction, provenance, and failure-atomic publication;
- `CR001-B05-F009`: real native shader ownership through the RHI factory.

No production source or maintained test implementation changed during this
verification packet.

## Authority

Feature 008 and Feature 012 require the active device to own shader modules
and pipeline layouts, malformed or wrong-stage payloads to fail at creation,
fallback validation to remain meaningful, real runtime objects to represent
actual native ownership, and invalidation to release owned state.

The verified B04 runtime rule remains binding: enabling native shader
validation must not relabel unrelated deterministic resources or pipelines as
native.

## Independent Current-State Review

Current source was traced without relying on the fix report:

- the public validator requires a complete SPIR-V header, a supported version,
  nonzero bound, zero schema, bounded nonzero instruction lengths, and a
  terminated entry-point instruction before any entry-point operands are
  consumed;
- module validation requires the SPIR-V execution model and exact entry-point
  name to match the declared RHI stage and entry point;
- repository search finds shader-module and pipeline-layout construction only
  inside `FVulkanDevice`, with private constructors preventing external
  creation;
- shader modules and layouts retain the creating device owner, while graphics,
  compute, and descriptor factories reject foreign dependencies;
- native creation occurs through `vkCreateShaderModule`, publishes a token only
  after ownership tracking succeeds, and destroys the handle if wrapper or
  tracking publication fails;
- explicit shader invalidation destroys its native token, and device shutdown
  invalidates retained wrappers before shutting down the native context;
- only a wrapper with retained native ownership reports `RealRuntime`; other
  device objects preserve deterministic fallback diagnostics.

## Finding Verification

### CR001-B05-F007

Maintained RHI and Vulkan tests reject a truncated four-word header, an
instruction overrun, a mismatched execution model, and a missing declared
entry point. The fallback-strict suite passes with minimal valid vertex,
fragment, and compute fixtures.

### CR001-B05-F008

Compile-time construction checks, source search, and runtime tests confirm
device-only construction. Foreign shaders and layouts are rejected by
pipeline and descriptor factories, and shutdown invalidates retained objects.
Factory inspection confirms native and wrapper/tracking failure paths do not
publish partial ownership.

### CR001-B05-F009

The graphics-enabled maintained suite proves native runtime enablement, a real
RHI-created shader module, wrong-stage rejection before native creation, and
native destruction on explicit invalidation and shutdown. All four assertions
passed at the verification source state.

## Local Gate Evidence

Fresh predefined ordinary gates produced:

- strict Debug build: passed;
- fallback strict Debug build and full deterministic maintained suite: passed;
- strict Release build: passed;
- graphics-enabled maintained suite: all B05 shader assertions passed, while
  the only failures were the three deferred native readback assertions already
  tracked by accepted `CR001-B08-F001`.

Detailed evidence is recorded in
`Evidence/b05-shader-module-interface-verification.md`. F007, F008, and F009
are Verified. No push or remote CI run occurred in this packet.
