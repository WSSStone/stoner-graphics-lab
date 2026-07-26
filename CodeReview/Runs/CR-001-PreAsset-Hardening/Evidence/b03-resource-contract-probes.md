# B03-S07 Buffer, Texture, And Sampler Probe Evidence

## Purpose

This strict standalone probe exercises the public resource validators and the
Feature 008 mock device factory with values omitted by maintained negative
coverage. It does not modify production or maintained test sources.

Source:

`Evidence/Probes/b03-resource-contract-probe.cpp`

## Command

```text
clang++ -std=c++20 -Wall -Wextra -Werror \
  -I. \
  -ISource/Core/Public \
  -ISource/RHI/Public \
  -ITests \
  Evidence/Probes/b03-resource-contract-probe.cpp \
  -o /tmp/cr001_b03_s07_resource_probe

/tmp/cr001_b03_s07_resource_probe
```

## Observed Output

```text
undefined_domains_accepted=1
invalid_mip_counts_accepted=1
format_usage_validator_mismatch=1
classification=resource-contract-defects
```

The probe exits zero only when all three current defect classes are
reproduced.

## Undefined Domains

Each of these descriptions passes its public validator and returns a usable
mock object:

- buffer usage bit 31, which has no RHI meaning;
- `ERHIMemoryAccess(255)`;
- texture usage bit 31;
- texture usage `Vertex`, which is absent from the texture contract's valid
  categories;
- `ERHISampleCount(3)`, while the recognized values are 1, 2, 4, and 8;
- sampler min filter and W address mode value 255.

Maintained coverage rejects `ReservedPresent`, color plus depth attachment,
and one supported-enum sampler combination, but does not exercise unnamed
bits or out-of-domain enum values.

## Invalid Mip Counts

Each of these descriptions passes `IsValidRHITextureDesc` and returns a usable
mock texture:

- 1x1 2D texture with 2 mip levels;
- 64x64 2D texture with `UINT32_MAX` mip levels;
- two-sample 2D texture with 2 mip levels.

The maintained invalid-mip assertion tests only zero. It does not cover the
geometric maximum, the first value above it, representability extremes, or
the one-mip multisample constraint.

## Format And Usage Contradiction

The public helper reports both descriptions valid:

- `R8G8B8A8_UNorm` with only `DepthStencilAttachment`;
- `D32_Float` with only `ColorAttachment`.

The mock factory rejects the same descriptions through a separate test-local
format helper. This is direct evidence that the public validity predicate and
authoritative factory do not agree.

Focused call-site search also shows the Vulkan device consumes the public
buffer, texture, and sampler validators. Backend-specific behavior remains in
B04/B05 scope and was not inspected in this packet.

