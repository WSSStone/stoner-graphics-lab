# B04-S02: Instance, Adapter, Device, And Capabilities Fix

## Scope

This packet repaired exactly the three Accepted findings authorized by
B04-S01:

- `CR001-B04-F001`: synthetic fallback reported as an available runtime;
- `CR001-B04-F002`: borrowed and null-unsafe adapter identity;
- `CR001-B04-F003`: selected-adapter formats ignored by device capabilities.

The implementation commit is `c603f8a`. No surface/swapchain, allocation,
descriptor, upload, native command execution, or deferred readback ownership
was refactored.

## Runtime Truthfulness

`FVulkanInstanceDesc` now has an explicit `EVulkanInstanceRuntimeMode`:

- `RealRuntime` is the production default. Because `FVulkanDevice` does not
  currently own native Vulkan instance/device resources, it returns
  `Unsupported`, remains inactive, and points callers to
  `FVulkanNativeContext`, which is the existing real-runtime owner.
- `DeterministicFallback` requires explicit caller opt-in. Successful fallback
  initialization reports `DeterministicFallback` availability,
  `bUsedRuntimeFallback=true`, and a non-empty runtime-mode diagnostic.

All deterministic maintained fixtures were migrated explicitly. The stale
module-init call that silently created and discarded the default device was
removed. Feature 009's spec, data model, and contract now record this
architectural distinction without treating deterministic execution as native
proof.

## Adapter Identity Ownership

Adapter names, rejection reasons, and selection reasons now use owned
`FString` values. Empty or null source identities fail the required capability
gate before scoring and receive an owned rejection reason. Deterministic
tie-breaking compares normalized owned name views.

Maintained regressions mutate the caller's original `std::string` after
selection and pass a null identity alongside a valid candidate. The selected
identity remains stable and the null candidate is rejected without a crash.

## Exact Format Capabilities

`FVulkanFormatSupport` now owns a normalized concrete `ERHIFormat` set instead
of color/depth booleans. Invalid values and duplicates are removed at
construction, and the set is encapsulated behind queries. `MapCapabilities`
copies the selected candidate's same set, so texture factory validation and
public capability queries share one authority. The obsolete backend-wide
default-format helper was removed.

A maintained color-only adapter regression confirms that `D32_Float` is
neither published nor accepted by `CreateTexture`.

## Validation

The final source state passed:

- strict Debug build with real graphics dependencies;
- strict Release build with real graphics dependencies;
- strict graphics-disabled Debug build plus the full test executable;
- ASan/UBSan strict Debug build plus the full test executable with only the
  separately tracked optional deferred-native gate skipped;
- sanitizer-backed retained fix verifier covering all three findings.

The retained verifier produced:

```text
real_default_rejected=1
fallback_explicit=1
identity_owned=1
null_identity_rejected=1
format_set_preserved=1
classification=instance-device-contracts-fixed
```

Two unfiltered real-graphics test runs reproduced the existing
`CR001-B08-F001` MoltenVK deferred-native failure at native submission and
semantic probes. The Vulkan native context initialization/triangle tests and
all B04 regressions passed in those runs. The full log is retained locally at
`Evidence/output/b04-s02-native-tests.log`; this packet neither changes nor
claims to verify the B08 finding.

## Handoff To B04-S03

All three findings are `Fixed`, not yet `Verified`. The independent
verification packet must:

1. run the retained pre-fix probe against `0ad2209` and the fix verifier against
   current source, or otherwise establish equivalent before/after evidence;
2. independently inspect default runtime results, explicit fallback
   observability, owned/null identity behavior, and exact format propagation;
3. rerun focused regressions plus required local gates;
4. mark only `CR001-B04-F001` through `F003` Verified if the evidence remains
   independent and reproducible.
