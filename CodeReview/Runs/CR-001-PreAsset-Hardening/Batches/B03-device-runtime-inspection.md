# B03-S01: RHI Device, Runtime, And Results Inspection

## Inspection Budget

The inspection covered one responsibility domain and eight production headers,
totaling 338 lines:

1. `Source/RHI/Public/RHI/ERHIResult.h`
2. `Source/RHI/Public/RHI/ERHIRuntimeMode.h`
3. `Source/RHI/Public/RHI/FRHIRuntimeSnapshot.h`
4. `Source/RHI/Public/RHI/FRHIDeviceCapabilities.h`
5. `Source/RHI/Public/RHI/IRHIDevice.h`
6. `Source/RHI/Public/RHI/ERHIQueueType.h`
7. `Source/RHI/Public/RHI/ERHIFormat.h`
8. `Source/RHI/Public/RHI/RHIMinimal.h`

Supporting evidence included Features 007, 008, and 018 specifications and
contracts, `Tests/RHICoreTests.cpp`, focused runtime-snapshot call-site
searches, and Git history. No production implementation or test was changed.

## Requirement Mapping

- `007-FR-001`: the device exposes lifecycle, immutable capabilities, queue
  support, formats, and implementation limits.
- `007-FR-015`: `ERHIResult` distinguishes success, invalid state,
  unsupported behavior, timeout/readiness, resize, unavailable, and generic
  failure outcomes.
- `007-SC-002`: normal capability and lifecycle paths are covered, but the
  runtime snapshot tests do not cover contradictory proof states or aggregate
  overflow.
- `008-FR-017`: `IRHIDevice` remains the authoritative factory boundary for
  RHI objects.
- `018-FR-003`, `018-T006`, and `018-T009`: runtime request and object modes
  are explicit, but `ProvesNativeExecution()` does not require a native
  request.
- `018-FR-019` and `018-SC-009`: live-object categories are address-free and
  deterministic, but their 32-bit aggregate can wrap to zero.

## Reproduction

A standalone strict C++20 probe exercised two public snapshot states:

1. `LiveInstances=UINT32_MAX` plus `LiveDevices=1`.
2. `RequestedMode=Deterministic`, `ObjectMode=RealRuntime`, and one live
   instance and device.

Observed output:

```text
wrapped_total=0
contradictory_native_proof=1
probe_exit=3
```

The first result can turn non-zero live categories into a false zero-resource
gate. The second conflicts with the Feature 018 validation contract, which
requires a native-required run to fail when runtime proof reports
deterministic mode.

## Findings

### CR001-B03-F001 - Accepted S2

`GetTotalLiveObjectCount()` returns and accumulates `uint32`, so valid category
values can wrap to zero and falsely certify leak-free shutdown.

### CR001-B03-F002 - Accepted S2

`ProvesNativeExecution()` checks the object mode and two live counts but ignores
the requested mode, so a contradictory deterministic request can satisfy the
native proof gate.

## Confirmed Strengths

- `TRHIObjectResult<T>::Succeeded()` requires both a success result and a
  non-null object, preventing `{Success, nullptr}` from being accepted through
  the helper.
- Unknown queue values and absent formats fail closed in capability queries.
- Device capabilities are exposed as an immutable reference and remain a
  backend-owned snapshot rather than an upward dependency.
- Default runtime inspection on legacy devices reports deterministic fallback,
  which fails native proof closed.
- The inspected RHI aggregate header depends only on Core and RHI public
  contracts.

## Non-Findings And Deferred Leads

- Individual live counters remain `uint32`; only aggregate arithmetic needs a
  wider type for this finding. Replacing every counter would add churn without
  improving realistic per-category diagnostics.
- `ERHIResult::NotReady`, `ResizeRequired`, and `Unavailable` are not accepted
  by `RHISucceeded`; callers must handle these explicit non-success outcomes.
- The default surface swapchain overload forwards only `FramesInFlight`.
  Because surface and swapchain behavior belongs to B03-S04, that adapter is a
  recorded lead for the later commands/queues/synchronization/swapchain
  inspection rather than a finding in this packet.
- Full capability consistency and lifecycle matrix verification belongs in
  B03-S03 after the two accepted runtime fixes are implemented.

## B03-S02 Fix Packet

The next step may repair both related findings:

1. Return and compute the live-object aggregate with non-wrapping 64-bit
   arithmetic while retaining compact per-category counters.
2. Require `RequestedMode` to be a native request before accepting real-runtime
   instance/device evidence.
3. Add positive, negative, contradictory, zero-count, and overflow regression
   cases without changing backend-specific ownership.
