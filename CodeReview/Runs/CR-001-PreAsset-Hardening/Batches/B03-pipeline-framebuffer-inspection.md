# B03-S10: Shaders, Descriptors, Pipelines, Render Passes, And Framebuffers Inspection

## Inspection Budget

The inspection covered one RHI declaration-and-compatibility responsibility
domain and eight production headers, totaling 521 lines:

1. `Source/RHI/Public/RHI/FRHIShaderModuleDesc.h`
2. `Source/RHI/Public/RHI/FRHIDescriptorBinding.h`
3. `Source/RHI/Public/RHI/FRHIPipelineLayoutDesc.h`
4. `Source/RHI/Public/RHI/FRHIGraphicsPipelineDesc.h`
5. `Source/RHI/Public/RHI/FRHIComputePipelineDesc.h`
6. `Source/RHI/Public/RHI/FRHIRenderPassDesc.h`
7. `Source/RHI/Public/RHI/FRHIFramebufferDesc.h`
8. `Source/RHI/Public/RHI/IRHIDescriptorSet.h`

Supporting evidence included Features 008 and 012 specifications, data models,
contracts and tasks, focused sections of `Tests/RHICoreTests.cpp`, mock and
Vulkan factory call-site searches, and a standalone strict C++20 probe.
Focused Vulkan reads established consumer behavior but were not line-by-line
production inspection and remain outside this packet's scope. No production
or maintained test implementation changed.

## Requirement Mapping

- `008-FR-008` through `008-FR-010`: bindings retain set, slot, type, array
  count and visibility, while descriptor writes correctly reject wrong
  resource kind, missing binding, out-of-range array index, unsupported
  resource usage and invalidated resources.
- `012-FR-004`, `012-FR-005`, `012-FR-007`, and `012-FR-009`: declaration
  helpers reject duplicate bindings and zero sizes/counts, but do not close
  descriptor/visibility domains, reject incompatible overlapping ranges, or
  consistently compare shader constant ranges with selected layouts.
- `008-FR-011` through `008-FR-014` and `012-FR-006` through `012-FR-009`:
  required shader stages, triangle topology, dynamic viewport/scissor, depth
  target presence and basic attachment formats are checked, but unknown
  fixed-function and render-pass enum values can become usable mock objects.
- `008-FR-015`, `008-FR-016`, and `008-FR-019`: framebuffer count, format,
  sample count, lifecycle and usage are checked, but selected mip/layer bounds
  and mip-relative dimensions are not honored by the maintained mock.
- `012-SC-003` through `012-SC-005` and `008-SC-002`: claimed complete negative
  coverage is contradicted by the retained probe and absent maintained cases.

## Reproduction

The retained strict probe produced:

```text
interface_contract_defects=1
fixed_function_domains_accepted=1
framebuffer_subresource_defects=1
classification=pipeline-framebuffer-contract-defects
```

Full source, command, cases, and interpretation are retained in
`Evidence/b03-pipeline-framebuffer-contract-probes.md` and
`Evidence/Probes/b03-pipeline-framebuffer-contract-probe.cpp`.

## Findings

### CR001-B03-F009 - Accepted S2

Shader and pipeline-layout declaration validation accepts undefined descriptor
and visibility values plus incompatible overlapping constant ranges. The mock
pipeline factories also omit shader constant-range compatibility checks, so a
shader requiring constant data can use a layout that declares none.

### CR001-B03-F010 - Accepted S2

Graphics pipeline and render-pass validation do not establish closed
fixed-function state domains. Unknown raster, blend, depth, sample, attachment
role, load and store values can pass validation and return usable mock objects.

### CR001-B03-F011 - Accepted S2

Framebuffer validation ignores selected array-layer and mip-level bounds and
compares dimensions only to the base texture extent. It therefore accepts
nonexistent subresources while rejecting a valid nonzero mip with its actual
extent.

## Confirmed Strengths

- Shader modules reject unsupported stages, empty identity fields, malformed
  structural SPIR-V payloads, duplicate bindings, zero descriptor arrays,
  absent stage visibility, and ranges with zero size.
- Pipeline layouts reject empty binding lists and duplicate set/binding pairs.
- Descriptor sets retain their layout and set identity and enforce resource
  kind, usage, array bounds, lifecycle, and combined texture-sampler rules.
- Graphics pipelines reject missing/duplicate required stages, compute stages,
  unsupported render-target formats, invalidated dependencies, non-triangle
  topology, missing dynamic viewport/scissor, and depth state without a depth
  target.
- Compute pipelines require exactly one live compute shader and validate
  descriptor binding compatibility.
- Render passes reject empty attachment lists, color/depth format role
  mismatches expressed with recognized roles, and duplicate depth
  attachments.
- Framebuffers reject attachment-count, format, sample-count, usage and
  lifecycle mismatches.
- Public headers expose only Core/RHI vocabulary and no backend API types.

## Coverage Gaps

- Maintained tests do not cover undefined descriptor, visibility,
  fixed-function, attachment-role, load/store, or sample-count values.
- No maintained test covers overlapping constant ranges or a shader constant
  range absent from the selected pipeline layout.
- No maintained framebuffer test selects a nonzero mip or array layer.
- Vertex input coverage does not exercise duplicate locations or an attribute
  whose byte width exceeds the stride. The current `ERHIFormat` contract has
  no public byte-width helper, so this remains a coverage note rather than an
  additional finding in this packet.

## B03-S11 Fix Packet

The next packet may repair only these three Accepted findings:

1. Define shared closed-domain predicates for descriptor types, shader-stage
   flags, constant ranges, fixed-function enums, sample counts, attachment
   roles, and load/store operations. Reject same-stage overlapping ranges and
   arithmetic overflow while preserving compatible disjoint-stage ranges.
2. Make both mock and Vulkan graphics/compute factories validate all shader
   constant-range requirements against the selected layout, using one shared
   compatibility rule where practical.
3. Validate framebuffer layer/mip bounds and require dimensions to match the
   selected mip extent in both maintained mock and Vulkan helper paths.

Add helper-level and factory-level positive and negative tests for every
repaired boundary. Do not refactor native Vulkan render-pass or image-view
ownership reserved for B04/B05.

