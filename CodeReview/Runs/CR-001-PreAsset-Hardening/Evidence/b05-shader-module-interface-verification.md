# B05-S09 Verification Evidence

## Revisions

- Defective implementation parent: `58ac23a`
- Fix commit: `d5f1714`
- Verification source state: `a4454e5`

Historical scope was checked with Git patch integrity review. Current behavior
was verified by direct source tracing, maintained tests, and predefined
ordinary local gates.

## Requirement Matrix

| Finding | Current evidence | Result |
|---|---|---|
| CR001-B05-F007 | Bounded header/instruction traversal, exact execution-model and entry-point matching, malformed and mismatch regressions | Verified |
| CR001-B05-F008 | Private device-owned construction, shared owner identity, foreign dependency rejection, rollback and shutdown paths | Verified |
| CR001-B05-F009 | Actual `vkCreateShaderModule` ownership through the RHI factory, truthful per-object runtime mode, invalidation/shutdown destruction | Verified |

## Static Signals

- `git show --check --stat d5f1714`: exit 0.
- Repository search found the only `FVulkanShaderModule` and
  `FVulkanPipelineLayout` construction calls in `FVulkanDevice.cpp`.
- Native ownership traces from `CreateOwnedShaderModule` through wrapper token
  retention to `DestroyOwnedShaderModule`.
- Device shutdown invalidates owned wrappers before native-context shutdown.
- No new B05 finding was identified.

## Gate Files

- `gate-fallback-strict.json`
  - recorded `2026-07-27T03:45:39Z`
  - strict graphics-disabled Debug build: exit 0
  - complete maintained test executable: exit 0
- `gate-strict-debug.json`
  - recorded `2026-07-27T03:47:05Z`
  - native-enabled strict Debug build: exit 0
- `gate-tests.json`
  - recorded `2026-07-27T03:48:01Z`
  - build: exit 0
  - maintained test executable: exit 1
  - all four B05 native shader assertions: pass
  - exact failures: the three assertions owned by `CR001-B08-F001`
- `gate-strict-release.json`
  - recorded `2026-07-27T03:48:34Z`
  - strict Release build: exit 0

The exact graphics-enabled failures were:

- `Deferred native validation completes a real Vulkan submission`;
- `Mapped attachment probes are finite, unique, and within semantic tolerances`;
- `Deferred native validation passes semantic probes and releases frame-owned objects`.

## Boundaries

- No production or maintained test source changed in B05-S09.
- No debugger, custom probe, fault trigger, memory-check tool, remote CI, or
  network action was used.
- The failing deferred assertions remain accepted under
  `CR001-B08-F001`; they neither exercise nor contradict F007-F009.
