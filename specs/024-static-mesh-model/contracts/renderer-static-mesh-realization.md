# Contract: Renderer Static Mesh Realization

## RHI Prerequisites

```cpp
struct FRHIBufferUploadDesc
{
    uint64 DestinationOffset;
    const void* Data;
    uint64 DataSizeBytes;
};

struct FRHIIndexedDrawArguments
{
    uint32 IndexCount;
    uint32 InstanceCount = 1;
    uint32 FirstIndex = 0;
    int32 VertexOffset = 0;
    uint32 FirstInstance = 0;
};
```

`IRHIDevice::UploadBuffer(Buffer, Desc)` and
`IRHICommandBuffer::RecordDrawIndexed(FRHIIndexedDrawArguments)` are
backend-neutral. Vulkan maps them to staging/copy and `vkCmdDrawIndexed`;
Renderer does not include Vulkan headers or call backend helpers.

`UploadBuffer` mirrors the existing texture-upload contract: it validates the
destination range, performs or schedules the backend transfer according to the
device contract, and does not retain `Data` after the call returns.

## Request And Result

```cpp
struct FStaticMeshRealizationRequest
{
    TSharedPtr<IRHIDevice> Device;
    TSharedPtr<const FStaticMeshAsset> Asset;
    FStaticMeshRealizationProfile Profile;
};

struct FStaticMeshRealizationResult
{
    ERHIResult Result;
    TSharedPtr<const FStaticMeshAssetSnapshot> Snapshot;
    FStaticMeshRealizationDiagnostic Diagnostic;
};
```

## Stages

1. `ValidateAsset`: validate request, device lifecycle, source payload, source
   manifest, and coordinate convention.
2. `Plan`: deterministically choose vertex formats, stream/interleave packing,
   index width, buffer sizes, alignment, and sections.
3. `Allocate`: create all required buffers.
4. `Upload`: transfer all bytes through RHI and establish final states.
5. `Finalize`: validate section ranges, material identities, bounds, and source
   evidence.
6. `Publish`: return an immutable snapshot.

## Atomicity

- No snapshot is visible before Publish.
- A failure invalidates every RHI resource created by the request.
- A failed result carries no partially populated snapshot.
- Diagnostics identify stage, asset ID, primitive key where applicable, RHI
  result, code, and reason.
- Repeating the same asset/profile yields byte-identical packing and section
  order.

## Snapshot

The immutable snapshot contains:

- normalized source manifest;
- vertex and index buffers;
- `FRHIVertexInputDesc`;
- RHI index type;
- ordered primitive sections with first index and vertex offset;
- material asset identities;
- aggregate and per-section bounds;
- realization profile digest.

It does not contain parser documents, source files, Asset mutable builders, or
Vulkan handles.

## Validation

- Mock-RHI tests inject failure at every stage and verify cleanup.
- uint16 and uint32 index paths are covered.
- Multiple primitives share packed buffers without losing section offsets.
- Lifecycle invalidation rejects realization and later drawing.
- Vulkan native evidence draws a transformed non-symmetric model with
  clockwise culling and verifies expected readback.
