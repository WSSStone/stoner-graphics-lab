# B04-S01 Instance, Adapter, Device, And Capabilities Probe Evidence

## Purpose

These standalone probes exercise Feature 009 initialization, adapter
selection, capability publication, and lifecycle edges omitted by maintained
coverage. They do not modify production or maintained test sources.

Sources:

- `Evidence/Probes/b04-instance-device-inspection-probe.cpp`
- `Evidence/Probes/b04-adapter-null-name-probe.cpp`

## Summary Probe

The summary probe links against the current strict Debug libraries:

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -ISource/Core/Public \
  -ISource/RHI/Public \
  -ISource/Backend/Vulkan/Public \
  Evidence/Probes/b04-instance-device-inspection-probe.cpp \
  -LBuild/Mac/Debug/Backend/Vulkan \
  -LBuild/Mac/Debug/RHI \
  -LBuild/Mac/Debug/Core \
  -lVulkanRHI -lRHI -lCore \
  -framework Cocoa -framework IOKit -framework CoreFoundation \
  -o /tmp/cr001-b04-s01-instance-device-probe

/tmp/cr001-b04-s01-instance-device-probe summary
```

Observed output:

```text
fallback_masquerades_as_available=1
selected_identity_aliases_caller_storage=1
depth_capability_overclaimed=1
uninitialized_shutdown_reports_success=1
classification=instance-device-contract-defects
```

The probe exits zero only when all four current behaviors are reproduced.
Three are accepted findings; uninitialized shutdown is retained as a
contract/coverage gap.

Running the same binary with `null-name` without sanitizers terminated with
exit code 139 and no output.

## Null-Identity Sanitizer Probe

The minimized probe was compiled directly with the adapter-selection
implementation:

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -fsanitize=address,undefined \
  -fno-omit-frame-pointer \
  -fno-sanitize-recover=all \
  -ISource/Core/Public \
  -ISource/RHI/Public \
  -ISource/Backend/Vulkan/Public \
  Evidence/Probes/b04-adapter-null-name-probe.cpp \
  Source/Backend/Vulkan/Private/FVulkanPhysicalDevice.cpp \
  -o /tmp/cr001-b04-s01-adapter-null-sanitized

/tmp/cr001-b04-s01-adapter-null-sanitized
```

Observed result:

```text
exit_code=134
AddressSanitizer: SEGV on address 0x000000000000
frame: _platform_strlen
frame: std::string_view(char const*)
frame: SelectBestAdapter comparator
summary: zero-address read while ordering a null adapter name
```

## Runtime Discovery Trace

The focused source search was:

```text
rg -n \
  "vkEnumeratePhysicalDevices|vkCreateInstance|FVulkanNativeContext|MakeDefaultCandidates|bUsedRuntimeFallback|Availability" \
  Source/Backend/Vulkan/Private/FVulkanInstance.cpp \
  Source/Backend/Vulkan/Private/FVulkanDevice.cpp \
  Source/Backend/Vulkan/Public/VulkanRHI/FVulkanInstance.h \
  Source/Backend/Vulkan/Public/VulkanRHI/FVulkanDevice.h
```

The inspected device path contains `MakeDefaultCandidates`, unconditional
`bUsedRuntimeFallback = true`, and availability assignment after synthetic
selection. It contains no `vkCreateInstance`, `vkEnumeratePhysicalDevices`, or
`FVulkanNativeContext` integration. The allocator separately interprets
`bUsedRuntimeFallback` as runtime unavailable, while public diagnostics report
the same device as `Available`.

## Maintained Coverage Trace

Focused searches of `Tests/VulkanBackendTests.cpp` and
`Tests/VulkanNativeIntegrationTests.cpp` confirm:

- deterministic adapter selection uses non-null string literals;
- forced unsupported runtime and no-compatible-adapter cases exist;
- public capability coverage checks a default color format but not the
  selected candidate's depth-format restriction;
- native integration tests exercise `FVulkanNativeContext` separately from
  `FVulkanDevice`;
- initialized shutdown and repeated factory cycles are covered, but shutdown
  from `Uninitialized` is not.

## Interpretation

The evidence separates three contract defects:

1. the public instance/device abstraction claims a real available backend for
   deterministic simulation;
2. adapter identity is neither owned nor normalized;
3. the capability snapshot is not a faithful projection of the selected
   adapter.

The probes are retained so B04-S02 can be inverted into regression checks and
B04-S03 can independently verify the repaired behavior.
