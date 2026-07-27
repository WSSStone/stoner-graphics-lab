# B05-S08: Shader Modules And Interfaces Fix

## Repair Target

Implementation commit `d5f1714` repairs:

- `CR001-B05-F007`: incomplete SPIR-V structure and stage/entry-point validation;
- `CR001-B05-F008`: shader/layout construction, provenance, and publication ownership gaps;
- `CR001-B05-F009`: the unreachable RHI real-runtime shader-module path.

The change is limited to RHI shader validation, Vulkan shader/layout ownership,
the optional native shader context, maintained tests, and the Feature 012
contract documents. It does not relabel other deterministic Vulkan objects as
native or alter Renderer policy.

## Validation And Maintained Fixtures

Fallback validation now requires a complete SPIR-V header, supported version,
nonzero bound, zero schema, bounded nonzero instruction word counts, and a
terminated `OpEntryPoint`. Module validation additionally requires the
declared execution model and exact entry-point name to match the RHI stage and
entry point.

Maintained positive tests now build minimal valid vertex, fragment, and compute
modules. Negative tests cover a truncated header, instruction overrun,
wrong-stage execution model, and missing entry point.

## Device Ownership

`FVulkanShaderModule` and `FVulkanPipelineLayout` construction is private to
`FVulkanDevice`. Both retain shared creating-device identity, and pipeline and
descriptor factories reject foreign dependencies.

Shader and layout factories map wrapper or tracking allocation/capacity
failure to `Unavailable`. If publication fails after native shader creation,
the temporary native object is destroyed before failure is returned.

## Native Shader Runtime

An active deterministic `FVulkanDevice` may explicitly enable a native shader
context. The RHI shader factory then creates and retains a real
`VkShaderModule`; only that wrapper reports `RealRuntime` and runtime
validation. Default device objects and all unrelated resource/pipeline
factories remain explicit deterministic fallback, preserving the corrected
B04 runtime contract.

Explicit invalidation and device shutdown destroy tracked native shader
objects and invalidate wrappers. Feature 012 quickstart, data model, and
contract documents record this ownership and runtime behavior.

## Local Verification

Fresh local results are recorded in
`Evidence/b05-shader-module-interface-fix.md`:

- strict Debug build: passed with project warnings treated as errors;
- strict deterministic fallback build and complete maintained tests: passed;
- strict Release build: passed with project warnings treated as errors;
- graphics-enabled maintained tests compiled and all four new native shader
  assertions passed;
- `git diff --check`: passed.

The graphics-enabled run returned only the three assertions already tracked by
accepted finding `CR001-B08-F001`; this packet does not alter deferred native
readback behavior.

## Finding State

- `CR001-B05-F007`: Fixed at `d5f1714`.
- `CR001-B05-F008`: Fixed at `d5f1714`.
- `CR001-B05-F009`: Fixed at `d5f1714`.

Independent verification remains assigned to B05-S09. No push or GitHub
Actions run occurred in this packet.
