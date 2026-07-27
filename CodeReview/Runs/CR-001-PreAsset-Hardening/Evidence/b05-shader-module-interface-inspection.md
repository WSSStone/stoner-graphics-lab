# B05-S07 Inspection Evidence

## Source State

- Inspection HEAD: `b151e69`
- Production implementation is unchanged from the source state gated in
  B05-S06.

## Requirement Matrix

| Finding | Requirement | Static evidence | Missing maintained proof | Result |
|---|---|---|---|---|
| CR001-B05-F007 | 012-FR-001, FR-002, FR-003a, SC-002 | Validation checks only four words, format, and magic; positive fixtures are four non-executable words | Complete structure, stage, and entry-point rejection | Accepted S2 |
| CR001-B05-F008 | 008-FR-017, 012-FR-002, FR-018, T034-T035 | Public constructors, no owner identity, no provenance check, uncaught factory allocations | Direct-construction closure, cross-device rejection, rollback | Accepted S2 |
| CR001-B05-F009 | 012-FR-001, FR-003a, SC-001 | RealRuntime initialization is always Unsupported; RHI factory has no native handle; native shader creation lives in separate contexts | RHI real-runtime shader success/rejection/destruction | Accepted S2 |

## Key Locations

- `Source/RHI/Public/RHI/FRHIShaderModuleDesc.h`
  - lines 106-113: magic-only bytecode validation;
  - lines 173-203: metadata and module-description validation.
- `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanShaderModule.h`
  - lines 10-21: public construction and lifecycle mutation.
- `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`
  - lines 632-662: shader factory and unreachable runtime-validation label;
  - lines 745-815: pipeline dependency checks without device provenance.
- `Source/Backend/Vulkan/Private/FVulkanInstance.cpp`
  - lines 62-72: real runtime is rejected; fallback is the only active mode.
- `Tests/VulkanBackendTests.cpp`
  - lines 572-593: accepted four-word shader fixture;
  - lines 631-648: fallback success and limited malformed coverage.
- `Tests/RHICoreTests.cpp`
  - lines 1908-1929: the same accepted four-word fixture;
  - lines 2227-2253: positive factory and metadata-domain coverage.

## Existing Correct Signals

- metadata binding and constant-range closed-domain checks are present;
- interface requirements are compared against layout type, count, visibility,
  and contained constant ranges;
- deterministic fallback is now explicitly selected and does not masquerade as
  an available native runtime;
- tracked factory-created shader wrappers are invalidated during device
  shutdown.

## Gate Reuse

The inspection introduced no production or maintained-test diff. The exact
production source state passed:

- `gate-strict-debug.json`, exit 0;
- `gate-fallback-strict.json`, build and full maintained tests exit 0;
- `gate-strict-release.json`, exit 0.

Those green gates demonstrate current behavior but do not close the listed
coverage gaps because their positive shader fixtures are themselves
structurally invalid.

## Boundaries

- No production or maintained test source changed.
- No custom executable, fault-injection probe, debugger, sanitizer, network
  action, or remote CI was used.
- The existing intermittent deferred-native issue remains separately tracked
  by accepted `CR001-B08-F001`.
