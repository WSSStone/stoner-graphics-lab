# Feature 019 Completion Audit

Status: **implementation in progress**

## Verified Locally

- macOS C++20/SCons full build succeeds in the `godot` conda environment.
- `StonerTest` covers deferred surface semantics, world-space normal transforms,
  standard/reversed depth policy, deterministic frame/graph/RHI recording,
  uncapped local-light planning, diagnostics, comparison contracts, forward
  coexistence, and optional native-runtime discovery.
- All checked-in deferred SPIR-V files pass `spirv-val`.
- Renderer and RHI public headers contain no Vulkan API types or backend
  downcasts.
- The Feature 019 validation helper and its parser/failure tests pass locally.

## Open Completion Gates

- `FVulkanNativeOffscreenSession` currently proves a real Vulkan submission but
  labels its probe source
  `NativeVulkanSubmission+DeterministicSemanticOracle`. It does not yet create,
  copy, map, and decode the actual deferred GBuffer and final attachments.
- The required Linux profile accepts only
  `reference_path=NativeDeferredReadback`; therefore the current transition
  implementation cannot produce a false passing artifact.
- Feature-branch CI has not yet run against this working tree. No Linux
  readback or comparison artifact is retained until the native gate and all
  three deterministic jobs pass for the same commit.

## Required Final Evidence

The final audit must record the Git commit, GitHub Actions run identity,
artifact digests, at least 12 passing probes for each depth convention, all four
comparison tiers, passing forward regressions, and
`final_live_objects=0`.
