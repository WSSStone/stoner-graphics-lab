# B03-S05 Command, Synchronization, And Swapchain Fix Evidence

## Fix Commit

`b29f466 fix(rhi): make presentation transitions fail closed`

## Standalone Current-Contract Probe

Source:

`Evidence/Probes/b03-command-sync-swapchain-fix-probe.cpp`

Command:

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -I. \
  -ISource/Core/Public \
  -ISource/RHI/Public \
  -ITests \
  Evidence/Probes/b03-command-sync-swapchain-fix-probe.cpp \
  -o /tmp/cr001_b03_s05_probe

/tmp/cr001_b03_s05_probe
```

Observed output:

```text
surface_fail_closed=1
clear_fail_closed=1
swapchain_fail_closed=1
acquire_atomic=1
queue_wait_atomic=1
queue_signal_atomic=1
classification=fixed
```

The probe exits zero only when every public compatibility fallback and
failure-state invariant matches the repaired contract.

## Maintained Regression Results

The strict graphics-disabled test executable produced 770 result lines, no
`[FAIL]` record, and these focused assertions:

```text
[PASS] IRHIDevice surface-aware swapchain compatibility path fails closed
[PASS] IRHICommandBuffer explicit clear compatibility path fails closed
[PASS] IRHICommandQueue wait preflight fails without partial state transition
[PASS] IRHICommandQueue signal preflight fails without partial submission
[PASS] IRHISwapchain synchronized acquire compatibility path fails closed
[PASS] IRHISwapchain synchronized present compatibility path fails closed
[PASS] IRHISwapchain failed synchronized acquire preserves all states
[PASS] IRHISwapchain synchronized acquire commits image and signal together
[PASS] IRHISwapchain failed synchronized present preserves wait signal
[PASS] IRHISwapchain synchronized present commits wait and frame together
```

## Gate Records

- `Evidence/gate-fallback-strict.json`: passed strict Debug compile and full
  graphics-disabled deterministic suite.
- `Evidence/gate-strict-debug.json`: passed native-capable strict Debug build.
- `Evidence/gate-strict-release.json`: passed native-capable strict Release
  build.
- `Evidence/gate-sanitizers.json`: passed strict ASan/UBSan build and full
  non-optional test suite.

The sanitizer profile uses the established
`STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1` boundary for accepted finding
`CR001-B08-F001`; it does not skip RHI core, Vulkan deterministic, Renderer,
Application, or triangle deterministic coverage.

## Call-Site Audit

- No production caller invokes the surface-aware device overload.
- No production caller invokes synchronized swapchain acquire/present through
  the compatibility default.
- Existing explicit clear callers resolve to maintained mock or Vulkan
  implementations that override the clear-value overload.

The change therefore removes false success without silently redirecting an
active production path.

