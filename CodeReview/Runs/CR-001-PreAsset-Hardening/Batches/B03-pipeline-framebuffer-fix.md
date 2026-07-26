# B03-S11: Pipeline And Framebuffer Contract Fix

## Scope

This packet fixes the three Accepted S2 findings from B03-S10:

- `CR001-B03-F009`: shader and pipeline-layout validation omitted closed
  descriptor/visibility domains, constant-range overlap rules, and shared
  shader constant-range compatibility.
- `CR001-B03-F010`: graphics pipeline and render-pass validators accepted
  undefined fixed-function and attachment state.
- `CR001-B03-F011`: framebuffer validation ignored selected mip and
  array-layer semantics.

The production change is limited to public portable RHI validation and the
matching mock/Vulkan consumers. It does not change native Vulkan object
ownership, feature specifications, or Feature 020 asset code.

## Implementation

### Shader Interface And Layout Compatibility

- Descriptor types, shader-stage visibility flags, formats, and constant
  ranges now use explicit closed-domain predicates.
- Constant-range validation rejects zero size, 32-bit end overflow, and
  overlapping ranges that share stage visibility while preserving legal
  overlaps across disjoint stages.
- One shared compatibility helper checks every required binding and constant
  range against a valid selected layout.
- Mock graphics/compute factories and `FVulkanPipelineLayout` now use that
  helper, eliminating the previous backend/mock contract split.

### Graphics And Render-Pass State

- Graphics validation rejects undefined cull, front-face, blend, compare,
  sample-count, vertex-format, and render-target-format values.
- Render-pass validation is shared by mock and Vulkan paths and closes
  attachment role, format, sample-count, load, and store domains.
- Render passes reject role/format mismatches, mixed sample counts, and more
  than one depth/stencil attachment.

### Framebuffer Subresources

- Mock and Vulkan framebuffer validation reject mip and array-layer indices at
  or beyond the texture counts.
- Width and height must exactly match the selected mip extent rather than the
  base texture extent.
- Valid nonzero mip and array-layer selections remain supported.

## Regression Coverage

Maintained RHI and Vulkan tests cover undefined domains, overflowing and
overlapping ranges, disjoint-stage overlap, a missing shader constant range,
mixed attachment sample counts, valid nonzero subresources, and each
framebuffer boundary. Two retained standalone probes cover the repaired
conjunction and helper boundary matrix.

## Local Verification

- Strict graphics-disabled Debug build and full test suite: passed.
- Native-capable strict Debug build: passed.
- Native-capable strict Release build: passed.
- Strict ASan/UBSan build and non-optional full test suite: passed.
- Focused fix probe in ordinary and ASan/UBSan builds: passed.
- Validator boundary matrix under ASan/UBSan: passed.
- `git diff --check`: passed.

Authoritative commands and outputs are recorded in
`Evidence/b03-pipeline-framebuffer-fix-probes.md`.

## Finding State

- `CR001-B03-F009`: Fixed at `09d1a1b`.
- `CR001-B03-F010`: Fixed at `09d1a1b`.
- `CR001-B03-F011`: Fixed at `09d1a1b`.

Independent parent/current verification remains B03-S12's responsibility.
No push or GitHub Actions run occurred in this packet.
