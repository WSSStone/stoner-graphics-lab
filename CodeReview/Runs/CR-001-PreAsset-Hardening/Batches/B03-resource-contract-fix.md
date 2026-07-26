# B03-S08: Buffer, Texture, And Sampler Resource Fix

## Scope

This packet fixes the three Accepted S2 findings from B03-S07:

- `CR001-B03-F006`: resource validators accept undefined usage bits and enum
  values.
- `CR001-B03-F007`: texture validation accepts impossible mip chains.
- `CR001-B03-F008`: public texture validity and the mock factory disagree on
  color/depth attachment compatibility.

The production change is limited to five RHI public headers. Maintained
regressions remain in `Tests/RHICoreTests.cpp`. No Vulkan implementation,
device capability limit, feature specification, or Feature 020 asset code
changed.

## Implementation

### Closed Portable Value Domains

- Buffer and texture usage validators now admit only named portable bits.
  `ReservedPresent`, texture `Vertex`, and every unnamed bit fail closed.
- Buffer memory access and texture sample count use explicit recognized-value
  predicates.
- Sampler min/mag filters, mip filter, all three address axes, and compare
  mode must each be a recognized enum value.
- Existing named flag composition and the documented compare/no-mip
  incompatibility remain intact.

### Exact Mip Rules

`GetRHIMaxMipLevels` computes the finite chain from the largest dimension by
right-shifting an unsigned value. This is constexpr, overflow-safe, accepts
the exact `floor(log2(max dimension)) + 1` boundary, and rejects larger
counts. Multisampled textures are restricted to one mip level.

### Shared Format And Usage Rules

`IsValidRHITextureDesc` now rejects:

- a depth/stencil format with `ColorAttachment`;
- a non-depth format with `DepthStencilAttachment`.

The mock device keeps its capability-specific format support check but no
longer duplicates portable format/usage policy. Helper and factory therefore
share one authoritative validity decision.

## Regression Coverage

Maintained tests cover:

- unknown buffer/texture bits, memory access, sample count, and every sampler
  field;
- texture `Vertex` usage;
- 1x1 one-level acceptance and two-level rejection;
- 64x64 seven-level acceptance, eight-level rejection, and `UINT32_MAX`;
- one-level multisample acceptance and multisample-chain rejection;
- helper/factory parity for both color-as-depth and depth-as-color.

## Local Verification

- Strict standalone positive fix probe: passed.
- Original B03-S07 defect probe: all three defect indicators changed from
  `1` to `0`.
- Full graphics-disabled strict Debug build and test suite: passed.
- Native-capable strict Debug build: passed.
- Native-capable strict Release build: passed.
- Strict ASan/UBSan build and non-optional test suite: passed.
- `git diff --check`: passed.

Authoritative commands and outputs are recorded in
`Evidence/b03-resource-contract-fix-probes.md`.

## Finding State

- `CR001-B03-F006`: Fixed at `ca68ed4`.
- `CR001-B03-F007`: Fixed at `ca68ed4`.
- `CR001-B03-F008`: Fixed at `ca68ed4`.

Independent parent/current verification remains B03-S09's responsibility.
No push or GitHub Actions run occurred in this packet.
