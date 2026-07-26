# B03-S08 Resource Contract Fix Evidence

## Fix Commit

`ca68ed4 fix(rhi): validate resource descriptor domains`

## Positive Fix Probe

Source:

`Evidence/Probes/b03-resource-contract-fix-probe.cpp`

Command:

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -I. \
  -ISource/Core/Public \
  -ISource/RHI/Public \
  -ITests \
  Evidence/Probes/b03-resource-contract-fix-probe.cpp \
  -o /tmp/cr001_b03_s08_fix_probe

/tmp/cr001_b03_s08_fix_probe
```

Observed output:

```text
closed_domains=1
exact_mip_rules=1
shared_format_usage_rules=1
classification=fixed
```

The probe exits zero only when all repaired helper and factory invariants hold.

## Original Defect Probe On Current Code

The unmodified B03-S07 reproduction probe was compiled against current code.
It exits `3`, as designed when the original defect conjunction is absent, and
reported:

```text
undefined_domains_accepted=0
invalid_mip_counts_accepted=0
format_usage_validator_mismatch=0
classification=unexpected
```

This is the inverse of its retained pre-fix evidence, where all three defect
indicators were `1`.

## Maintained Regression Coverage

`TestResourceDescriptionsAndFactories` now checks both public helpers and the
mock device factory for all repaired domains and boundaries:

- closed buffer, texture, memory, sample, and sampler value domains;
- exact 1x1 and 64x64 mip boundaries plus `UINT32_MAX`;
- one-level-only multisample textures;
- matching color/depth attachment format decisions.

The strict graphics-disabled full suite passed without a failure record.

## Gate Records

- `Evidence/gate-fallback-strict.json`: passed strict Debug compile and full
  graphics-disabled deterministic suite at `2026-07-26T10:18:31Z`.
- `Evidence/gate-strict-debug.json`: passed native-capable strict Debug build
  at `2026-07-26T10:19:05Z`.
- `Evidence/gate-strict-release.json`: passed native-capable strict Release
  build at `2026-07-26T10:19:28Z`.
- `Evidence/gate-sanitizers.json`: passed strict ASan/UBSan build and full
  non-optional test suite at `2026-07-26T10:20:31Z`.

The sanitizer profile retains the established
`STONER_SKIP_OPTIONAL_DEFERRED_NATIVE=1` boundary for Accepted finding
`CR001-B08-F001`; it does not skip RHI core coverage.
