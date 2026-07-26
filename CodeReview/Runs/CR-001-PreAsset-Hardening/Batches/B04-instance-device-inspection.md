# B04-S01: Instance, Adapter, Device, And Capabilities Inspection

## Inspection Budget

The inspection covered one Vulkan initialization-and-capability responsibility
domain and six production files, totaling 785 selected lines:

1. `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanInstance.h`
2. `Source/Backend/Vulkan/Private/FVulkanInstance.cpp`
3. `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanPhysicalDevice.h`
4. `Source/Backend/Vulkan/Private/FVulkanPhysicalDevice.cpp`
5. `Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h`
6. `Source/Backend/Vulkan/Private/FVulkanDevice.cpp`, lines 1-270 and
   820-903

Supporting evidence included Feature 009's specification, plan, research, data
model, contracts and tasks; focused sections of `Tests/VulkanBackendTests.cpp`
and `Tests/VulkanNativeIntegrationTests.cpp`; diagnostics call sites; source
searches for native runtime discovery; and two standalone strict probes.
Diagnostics and tests were supporting reads rather than line-by-line production
inspection. No production or maintained test implementation changed.

## Requirement Mapping

- `009-FR-001`, `009-FR-004`, `009-FR-005`, `009-SC-001`, and
  `009-SC-002`: initialization returns explicit results and forced
  unsupported mode leaves the device inactive, but normal initialization does
  not discover a Vulkan runtime. It selects an invented adapter and reports
  `Available`, `Success`, and `Active`.
- `009-FR-002`, `009-FR-019`, and `009-SC-003`: required graphics,
  transfer, and color-format gates plus deterministic scoring are present, but
  selected adapter identity and rejection diagnostics are borrowed pointers.
  Mutable storage changes the retained identity, and a null name crashes the
  deterministic tie-break.
- `009-FR-003` and the Device Contract: queue and limit fields are mapped from
  the selected candidate, but supported formats are replaced by a fixed
  backend default. Resource factories then validate against the overclaimed
  list instead of the selected adapter.
- Feature 009's Backend Initialization Contract requires missing runtime,
  missing required extensions, no compatible adapter, or missing queues to
  return explicit unsupported/failed results without a partially usable
  device. The default synthetic path violates the runtime portion of this
  contract.

## Reproduction

The retained summary probe produced:

```text
fallback_masquerades_as_available=1
selected_identity_aliases_caller_storage=1
depth_capability_overclaimed=1
uninitialized_shutdown_reports_success=1
classification=instance-device-contract-defects
```

The dedicated null-name probe terminated with exit code 134 under
AddressSanitizer. Its stack reaches `_platform_strlen`,
`std::string_view(char const*)`, and the `SelectBestAdapter` comparator,
confirming a zero-address read during the tie-break.

Full source, commands, output, searches, and interpretation are retained in
`Evidence/b04-instance-device-probes.md`,
`Evidence/Probes/b04-instance-device-inspection-probe.cpp`, and
`Evidence/Probes/b04-adapter-null-name-probe.cpp`.

## Findings

### CR001-B04-F001 - Accepted S1

The normal `FVulkanDevice::Initialize` path performs no real runtime or
physical-device discovery. `FVulkanInstance` unconditionally marks runtime
fallback, manufactures `Stoner Vulkan Compatible Adapter`, and then reports
the backend as `Available`; the device consequently becomes `Active`. A
caller cannot distinguish a real initialized backend from deterministic
simulation through the public result and lifecycle contract.

### CR001-B04-F002 - Accepted S1

`FVulkanAdapterCandidate` and the selected adapter snapshot retain caller-owned
`const char*` names and rejection reasons. The selected identity aliases
mutable caller storage, and a null name reaches `std::string_view` during
sorting and crashes. This violates deterministic, stable adapter identity and
makes diagnostics lifetime-dependent.

### CR001-B04-F003 - Accepted S2

`MapCapabilities` always installs `GetDefaultVulkanSupportedFormats()` instead
of deriving formats from the selected adapter. A candidate that declares no
depth support still advertises `D32_Float` and successfully creates a depth
texture, so capability queries and resource factories overclaim availability.

## Confirmed Strengths

- Adapter gating requires graphics and transfer queues plus color-format
  support before scoring a candidate.
- Compatible non-null candidates use deterministic type/limit scoring and a
  stable name tie-break.
- Missing optional validation layers are reported but do not make otherwise
  supported initialization fail.
- Forced unsupported runtime and no-compatible-adapter paths return explicit
  failures and leave the device inactive.
- Queue-family and basic device-limit fields are propagated into the public
  capability snapshot.
- Normal initialized shutdown invalidates device-owned resources and reaches
  the terminal `Shutdown` state; deeper resource ownership remains in later
  B04 packets.

## Coverage Gaps

- Maintained tests use string literals for adapter identities and do not cover
  mutable, empty, null, or temporary identity storage.
- No maintained test requires adapter-specific format support to agree with
  public capabilities and resource factories.
- Default-path tests accept the synthetic adapter and therefore do not prove
  real runtime discovery or a truthful unsupported result.
- `FVulkanDevice::Shutdown()` from `Uninitialized` returns `Success` and
  transitions directly to `Shutdown`. The lifecycle contract does not clearly
  specify this edge, so it remains a coverage/decision gap rather than a
  fourth finding in this packet.

## B04-S02 Fix Packet

The next packet may repair only these three Accepted findings:

1. Separate real runtime initialization from explicit deterministic fallback.
   The production default must not report `Available` and `Active` without
   real Vulkan runtime/device discovery. Any test fallback must be opt-in and
   unambiguously observable through diagnostics and result/state contracts.
2. Replace borrowed adapter identity and rejection pointers with owned values,
   normalize invalid/empty identities before ordering, and preserve
   deterministic selection and diagnostics.
3. Carry or derive a concrete supported-format set for each selected adapter,
   then use the same set for capability publication and resource factory
   validation.

Add maintained positive and negative tests for runtime-mode observability,
owned/null adapter identity, deterministic ties, and adapter-specific format
acceptance. Do not refactor surface/swapchain, allocation, descriptor, upload,
or native execution ownership reserved for later B04/B05 packets.
