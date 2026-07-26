# B04-S02 Instance, Adapter, Device, And Capabilities Fix Evidence

## Retained Verifier

Source:

`Evidence/Probes/b04-instance-device-fix-probe.cpp`

The final verifier was compiled against the sanitizer-instrumented strict
Debug libraries:

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -fsanitize=address,undefined \
  -fno-omit-frame-pointer \
  -fno-sanitize-recover=all \
  -ISource/Core/Public \
  -ISource/RHI/Public \
  -ISource/Backend/Vulkan/Public \
  Evidence/Probes/b04-instance-device-fix-probe.cpp \
  -LBuild/Mac/Debug/Backend/Vulkan \
  -LBuild/Mac/Debug/RHI \
  -LBuild/Mac/Debug/Core \
  -lVulkanRHI -lRHI -lCore \
  -framework Cocoa -framework IOKit -framework CoreFoundation \
  -o /tmp/cr001-b04-s02-instance-device-fix-probe-final

env ASAN_OPTIONS=halt_on_error=1 \
  UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=1 \
  /tmp/cr001-b04-s02-instance-device-fix-probe-final
```

Observed output:

```text
real_default_rejected=1
fallback_explicit=1
identity_owned=1
null_identity_rejected=1
format_set_preserved=1
classification=instance-device-contracts-fixed
```

The verifier exits zero only when all five assertions hold.

## Maintained Regression Coverage

`Tests/VulkanBackendTests.cpp` now proves:

- default `RealRuntime` requests return `Unsupported`, remain inactive, do not
  claim fallback, and provide a runtime-mode diagnostic;
- explicit deterministic requests succeed with distinct fallback
  availability and diagnostics;
- selected names do not alias mutable caller storage;
- null names are rejected before deterministic ordering without sanitizer
  failure;
- a color-only selected adapter omits `D32_Float` and rejects a depth texture.

`Tests/DeferredRenderingTests.cpp` and
`Tests/RendererForwardPipelineTests.cpp` opt into deterministic mode rather
than receiving it implicitly.

## Recorded Gates

The final `crctl gate` records are:

- `strict-debug`: passed at `2026-07-26T12:29:53+00:00`;
- `strict-release`: passed at `2026-07-26T12:30:09+00:00`;
- `fallback-strict`: build and full tests passed at
  `2026-07-26T12:29:18+00:00`;
- `sanitizers`: build and full tests passed at
  `2026-07-26T12:31:17+00:00` with
  `STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1`.

The authoritative command arrays, exit codes, and output tails are retained in
`gate-strict-debug.json`, `gate-strict-release.json`,
`gate-fallback-strict.json`, and `gate-sanitizers.json`.

## Known Cross-Batch Native Failure

After the real-graphics strict Debug build, two unfiltered test executions
returned exit code 1 with these same failures:

```text
[FAIL] Deferred native validation completes a real Vulkan submission
[FAIL] Mapped attachment probes are finite, unique, and within semantic tolerances
[FAIL] Deferred native validation passes semantic probes and releases frame-owned objects
```

The runs still passed the real Vulkan instance/device, offscreen triangle,
frame-local release, and zero-live-object context tests. This matches the
accepted `CR001-B08-F001` failure signature and is retained as cross-batch
evidence, not waived or attributed to B04.
