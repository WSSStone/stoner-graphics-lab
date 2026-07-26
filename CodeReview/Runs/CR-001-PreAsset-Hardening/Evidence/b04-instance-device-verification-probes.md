# B04-S03 Instance, Adapter, Device, And Capabilities Verification Evidence

## Exact-Parent Snapshot

The fix parent was exported without changing the worktree:

```text
git archive --format=tar --output=/tmp/cr001-b04-parent-0ad2209.tar 0ad2209
mkdir /tmp/cr001-b04-parent.PikLm5
tar -xf /tmp/cr001-b04-parent-0ad2209.tar \
  -C /tmp/cr001-b04-parent.PikLm5

conda run -n stoner-cr scons \
  config=debug strict=1 graphics=disabled -j8
```

The build ran from `/tmp/cr001-b04-parent.PikLm5` and passed. The retained
B04-S01 summary probe was compiled against that snapshot's libraries as
`/tmp/cr001-b04-s03-parent-probe`. It produced:

```text
fallback_masquerades_as_available=1
selected_identity_aliases_caller_storage=1
depth_capability_overclaimed=1
uninitialized_shutdown_reports_success=1
classification=instance-device-contract-defects
```

The process exited zero because this retained probe is a defect reproducer.
The parent null-name sanitizer probe was compiled as
`/tmp/cr001-b04-s03-parent-null-sanitized`. It terminated with exit code 134
and reported an ASan zero-address read through:

```text
_platform_strlen
std::string_view(char const*)
SelectBestAdapter comparator
```

## Independent Current Verifier

Source:

`Evidence/Probes/b04-instance-device-verification-probe.cpp`

The verifier was compiled against the current sanitizer-instrumented strict
Debug libraries:

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -fsanitize=address,undefined \
  -fno-omit-frame-pointer \
  -fno-sanitize-recover=all \
  -ISource/Core/Public \
  -ISource/RHI/Public \
  -ISource/Backend/Vulkan/Public \
  Evidence/Probes/b04-instance-device-verification-probe.cpp \
  -LBuild/Mac/Debug/Backend/Vulkan \
  -LBuild/Mac/Debug/RHI \
  -LBuild/Mac/Debug/Core \
  -lVulkanRHI -lRHI -lCore \
  -framework Cocoa -framework IOKit -framework CoreFoundation \
  -o /tmp/cr001-b04-s03-current-verifier

env ASAN_OPTIONS=halt_on_error=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  /tmp/cr001-b04-s03-current-verifier
```

Observed output:

```text
default_factory_truthful=1
fallback_factory_explicit=1
identity_lifetime_stable=1
empty_identity_safe=1
format_set_normalized=1
format_factory_exact=1
tie_break_stable=1
classification=instance-device-contracts-verified
```

The process exited zero. The assertions independently establish:

1. the default factory cannot return a synthetic active runtime;
2. fallback succeeds only when explicitly requested and is externally visible;
3. selected identities outlive temporary source storage;
4. empty/null identity is rejected before sorting with an owned reason;
5. invalid and duplicate format entries are normalized;
6. public capabilities and texture factories accept only the selected set;
7. 20 repeated equal-score selections use the same lexical tie-break.

## Source And Call-Site Search

The verification search covered `Source`, `Tests`, and retained probes:

```text
rg -n \
  "GetDefaultVulkanSupportedFormats|CreateVulkanDevice\\(\\)|\\.Initialize\\(\\)" \
  Source Tests \
  CodeReview/Runs/CR-001-PreAsset-Hardening/Evidence/Probes

rg -n \
  "GetSupportedFormats|MapCapabilities|EVulkanInstanceRuntimeMode|DeterministicFallback" \
  Source/Backend/Vulkan Tests/Vulkan*Tests.cpp
```

No `GetDefaultVulkanSupportedFormats` reference remains. The default
real-runtime calls are confined to the intentional rejection regression and
retained evidence probes. Maintained fallback fixtures explicitly set the
runtime mode. `MapCapabilities` copies the selected adapter's format set.

## Fresh Maintained Gates

Authoritative records:

- `Evidence/gate-fallback-strict.json`: passed at
  `2026-07-26T12:41:21+00:00`;
- `Evidence/gate-sanitizers.json`: passed at
  `2026-07-26T12:42:42+00:00`.

The fallback gate performed a strict graphics-disabled Debug build and ran the
complete maintained test executable with exit code zero. The sanitizer gate
performed a strict ASan/UBSan Debug build and ran the maintained test
executable with exit code zero and
`STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1`.

That skip is scoped to accepted `CR001-B08-F001`. The same sanitizer run
executed and passed the native Vulkan instance/device and offscreen triangle
tests relevant to this packet.
