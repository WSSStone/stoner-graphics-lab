# B03-S11 Pipeline And Framebuffer Fix Evidence

## Fix Commit

`09d1a1b fix(rhi): harden pipeline contract validation`

## Focused Repair Probe

Source:

`Evidence/Probes/b03-pipeline-framebuffer-fix-probe.cpp`

Command:

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -I. -ITests -ISource/Core/Public -ISource/RHI/Public \
  Evidence/Probes/b03-pipeline-framebuffer-fix-probe.cpp \
  -o /tmp/cr001-b03-s11-fix-probe

/tmp/cr001-b03-s11-fix-probe
```

Observed output:

```text
interface_contract_repaired=1
fixed_function_domains_repaired=1
framebuffer_subresources_repaired=1
classification=pipeline-framebuffer-contract-repaired
```

The same source was compiled with
`-fsanitize=address,undefined -fno-omit-frame-pointer
-fno-sanitize-recover=all` and produced the same output with exit code zero.

## Validator Boundary Matrix

Source:

`Evidence/Probes/b03-pipeline-validator-matrix.cpp`

Command:

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -fno-sanitize-recover=all \
  -ISource/Core/Public -ISource/RHI/Public \
  Evidence/Probes/b03-pipeline-validator-matrix.cpp \
  -o /tmp/cr001-b03-s11-validator-matrix

/tmp/cr001-b03-s11-validator-matrix
```

Observed output:

```text
descriptor_domains=1
range_boundaries=1
graphics_domains=1
render_pass_domains=1
mip_extents=1
```

The probe includes adjacent versus overlapping ranges, same-stage versus
disjoint-stage overlap, arithmetic overflow, containment, undefined enums,
and non-power-of-two mip extents.

## Maintained Regression Coverage

`Tests/RHICoreTests.cpp` checks portable helper and mock-factory parity.
`Tests/VulkanBackendTests.cpp` checks the matching deterministic Vulkan
factory behavior. The strict graphics-disabled full suite passed with RHI
core summary `passed=211 failed=0`; the overall test executable returned zero.

## Gate Records

- `Evidence/gate-fallback-strict.json`: passed strict Debug compile and full
  graphics-disabled deterministic suite at `2026-07-26T11:02:03Z`.
- `Evidence/gate-strict-debug.json`: passed native-capable strict Debug build
  at `2026-07-26T11:02:38Z`.
- `Evidence/gate-strict-release.json`: passed native-capable strict Release
  build at `2026-07-26T11:03:11Z`.
- `Evidence/gate-sanitizers.json`: passed strict ASan/UBSan build and full
  non-optional test suite at `2026-07-26T11:04:26Z`.

The sanitizer profile retains the established
`STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1` boundary for Accepted finding
`CR001-B08-F001`; it does not skip RHI core or Vulkan deterministic coverage.
