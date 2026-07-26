# B03-S07: Buffer, Texture, And Sampler Resource Inspection

## Inspection Budget

The inspection covered one RHI resource-description responsibility domain and
eight production headers, totaling 311 lines:

1. `Source/RHI/Public/RHI/IRHIBuffer.h`
2. `Source/RHI/Public/RHI/IRHITexture.h`
3. `Source/RHI/Public/RHI/IRHISampler.h`
4. `Source/RHI/Public/RHI/FRHIBufferDesc.h`
5. `Source/RHI/Public/RHI/FRHITextureDesc.h`
6. `Source/RHI/Public/RHI/FRHISamplerDesc.h`
7. `Source/RHI/Public/RHI/ERHIResourceUsage.h`
8. `Source/RHI/Public/RHI/ERHISamplerMode.h`

Supporting evidence included Feature 008's specification, research, data
model, API contract and tasks, `Tests/RHICoreTests.cpp`, repository history,
focused symbol/call-site searches, and a standalone strict C++20 probe.
Symbol-only searches surfaced recognized sample-count and format helpers plus
validator consumers; those supporting files were not line-by-line inspected
and remain outside this packet's production scope. No production or
maintained test implementation changed.

## Requirement Mapping

- `008-FR-001` through `008-FR-004`: public objects expose requested
  descriptions, resource identity fields, usage, and lifecycle state. Named
  usage combinations are composable.
- `008-FR-005` and `008-FR-005a`: zero and two explicit incompatible
  combinations are rejected, but undefined values and several contract-level
  incompatibilities are accepted.
- `008-FR-002`, the texture data model, API contract, and `008-T033`: dimension
  shape and zero mip/layer cases are covered, but geometrically excessive and
  multisample mip counts are not validated.
- `008-FR-003`, the sampler data model, and `008-T034`: one compare/mip
  combination is rejected, but unrecognized filter, address, mip, and compare
  enum values are accepted.
- `008-FR-018`: objects expose Valid/Invalidated state, remain queryable after
  invalidation, and are rejected by maintained downstream descriptor and
  framebuffer checks.
- `008-FR-019` and `008-SC-002`: the claim of complete public-contract negative
  coverage is contradicted by the retained probe.

## Reproduction

The retained strict probe produced:

```text
undefined_domains_accepted=1
invalid_mip_counts_accepted=1
format_usage_validator_mismatch=1
classification=resource-contract-defects
```

Full source, command, cases, and interpretation are retained in
`Evidence/b03-resource-contract-probes.md` and
`Evidence/Probes/b03-resource-contract-probe.cpp`.

## Findings

### CR001-B03-F006 - Accepted S2

Buffer, texture, and sampler validation does not establish closed value
domains. Undefined usage bits, invalid memory/sample values, invalid sampler
enums, and the texture-only incompatible `Vertex` usage all reach successful
mock factory results.

### CR001-B03-F007 - Accepted S2

Texture validation checks only that `MipLevels` is non-zero. It accepts mip
counts beyond the finite geometric chain and permits multiple mip levels on a
multisampled texture.

### CR001-B03-F008 - Accepted S2

The public texture validity helper accepts color formats as depth attachments
and depth formats as color attachments. The mock factory rejects them through
a separate helper, so identical descriptions have contradictory validity
answers.

## Confirmed Strengths

- Buffer size zero and usage `None` are rejected.
- Named buffer and texture usage flags compose without losing individual bits.
- Buffer `ReservedPresent` and simultaneous texture color/depth attachment
  roles are explicitly rejected.
- Texture dimension validation correctly distinguishes 1D, 2D, 3D, cube,
  1D-array, 2D-array, and cube-array shape/layer rules.
- Unknown texture dimension values reach the switch default and fail closed.
- Unknown and unsupported texture formats are rejected by the authoritative
  mock factory.
- Resource descriptions are retained by value and query methods expose the
  requested size, dimension, format, usage, and sampler behavior.
- Buffer, texture, and sampler invalidation is explicit; objects remain safe
  to query and maintained integration tests reject their downstream use.
- `IRHIBuffer::Upload` defaults to `Unsupported` rather than claiming an
  upload path that a legacy implementation does not provide.
- The eight public headers depend only on Core and RHI public vocabulary.

## Non-Findings And Limits

- The compare-sampler plus no-mip-filter combination is deliberately rejected
  by both Feature 008 and Feature 010 maintained tests. No requirement proves
  it must be accepted, so it is not recorded as a defect.
- Resource alignment and backend allocation limits are explicitly deferred to
  backend features; their absence from these contract-only descriptions is not
  a finding.
- Device-specific format availability and maximum dimensions remain
  capability/backend concerns. B03-S08 should add only portable validity
  rules.
- Copy-region bounds and upload behavior belong to the command/resource
  transfer inspection domain and were not assessed here.
- Vulkan conversion and allocation behavior is reserved for B04/B05. This
  packet records only that backend code calls the public validators.

## B03-S08 Fix Packet

The next packet may repair only these three Accepted findings:

1. Define closed valid masks and recognized enum predicates for buffer usage,
   texture usage, memory access, sample count, and all sampler modes. Reject
   unnamed bits and `ERHITextureUsage::Vertex`.
2. Compute the exact maximum geometric mip count with overflow-safe constexpr
   arithmetic; accept the boundary, reject the first value above it and
   `UINT32_MAX`, and require one mip for multisampled textures.
3. Move portable color/depth attachment format compatibility into
   `IsValidRHITextureDesc`, then remove redundant mock-only policy so helper
   and factory share one authoritative decision.

Add helper-level and factory-level parity tests for every repaired boundary.
Do not add backend capability limits or modify Vulkan implementation internals
in B03-S08.

