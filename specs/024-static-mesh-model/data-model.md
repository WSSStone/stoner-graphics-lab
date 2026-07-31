# Data Model: Static Mesh & Model Pipeline

## 1. Coordinate Convention

### `FCoordinateConvention`

Compile-time Core vocabulary describing the one active convention:

| Field | Value |
|---|---|
| `Identity` | `UnrealLH_ZUp_XForward_YRight_Meters_CW` |
| `Handedness` | Left |
| `ForwardAxis` | +X |
| `RightAxis` | +Y |
| `UpAxis` | +Z |
| `LinearUnit` | Meter |
| `CanonicalFrontFace` | Clockwise |
| `ClipDepthRange` | [0, 1] |

This type is convention evidence, not a runtime mode. Component
`UnitX.Cross(UnitY) == UnitZ`, Hamilton quaternion algebra, row-major storage,
column-vector multiplication, and S-R-T composition remain invariant.

### `FShaderMatrix4x4`

Renderer-owned packed shader representation produced explicitly from
`Core::FMatrix4x4`. Its bytes and target layout are never inferred from a Core
struct `memcpy`.

**Invariants**:

- A non-symmetric CPU matrix produces the mathematically identical shader
  transform.
- Backend target packing is explicit and testable.
- Packing does not mutate world handedness or clip-depth policy.

## 2. Import Profile And Limits

### `EStaticMeshNormalPolicy`

- `GenerateFlat`: default for missing normals.
- `RequireSource`: strict rejection.
- Reserved future values may express angle-based smooth/sharp edge generation.

### `EStaticMeshTangentPolicy`

- `GenerateWhenRequired`: default; requires normals and the material-selected
  UV set.
- `RequireSource`: strict rejection when a normal-mapped material needs a
  tangent stream.

### `FStaticModelImportLimits`

Finite values for source bytes, dependency bytes, parser allocations, scene
counts, nodes, depth, meshes, primitives, materials, textures, images, vertices,
indices, decoded geometry bytes, and diagnostics.

### `FStaticModelImportProfile : FAssetImportParameters`

| Field | Type | Rule |
|---|---|---|
| `SchemaVersion` | `uint32` | Starts at 1 |
| `ProfileName` | `FString` | Non-empty canonical token |
| `NormalPolicy` | enum | Explicit |
| `TangentPolicy` | enum | Explicit |
| `MaximumTexCoordSets` | `uint32` | 1 or 2 in Feature 024 |
| `MaterialMappingProfile` | `FString` | Versioned repository profile |
| `CoordinateConvention` | convention ID | Must equal active Core convention |
| `Limits` | struct | All finite and non-zero where applicable |

Every field contributes to import version evidence. A policy or limit change
cannot silently reuse a previous `FAssetVersion`.

## 3. Static Mesh Asset

### `FStaticMeshBounds`

Asset-owned value containing a finite `Core::FBox` and enclosing
`Core::FSphere`. Both representations are derived deterministically from the
same canonical positions and validated against them.

### `FStaticMeshVertexData`

| Field | Type | Required |
|---|---|---|
| `Positions` | `TArray<FVector3>` | Yes |
| `Normals` | `TArray<FVector3>` | Yes after normalization |
| `Tangents` | `TArray<FVector4>` | Only when provided/generated |
| `TexCoords[0..1]` | `TArray<FVector2>` | Per material/use |

**Invariants**:

- Positions are finite and non-empty.
- Normals are finite, normalized within tolerance, and match position count.
- An optional stream is either absent or exactly matches position count.
- Tangent XYZ is finite/normalized and W is exactly `-1` or `+1`.
- UV values are finite and use top-left origin semantics.
- Flat-normal generation duplicates vertices deterministically at face
  boundaries; generated arrays remain index aligned.

### `FStaticMeshIndexData`

Variant of `TArray<uint16>` and `TArray<uint32>`.

**Invariants**:

- Count is non-zero and divisible by 3.
- Every index is less than vertex count.
- The narrowest lossless index width is selected deterministically.
- Non-indexed source primitives become sequential canonical indices.
- Imported source order is preserved under the canonical clockwise convention.

### `FStaticMeshPrimitive`

| Field | Type | Rule |
|---|---|---|
| `StableKey` | `FString` | Unique within mesh |
| `Vertices` | vertex data | Valid |
| `Indices` | index data | Valid triangle list |
| `MaterialSlotIndex` | `uint32` | In range |
| `LocalBounds` | `FStaticMeshBounds` | Encloses all positions |
| `SourcePrimitiveIndex` | `uint32` | Evidence only |

### `FStaticMeshMaterialSlot`

| Field | Type | Rule |
|---|---|---|
| `StableKey` | `FString` | Unique within mesh |
| `Material` | `TSoftAssetRef<FMaterialAsset>` | Typed and valid |

### `FStaticMeshAssetDesc`

| Field | Type |
|---|---|
| `Id` | `FAssetId` |
| `Version` | `FAssetVersion` |
| `SchemaVersion` | `uint32 = 1` |
| `Primitives` | `TArray<FStaticMeshPrimitive>` |
| `MaterialSlots` | `TArray<FStaticMeshMaterialSlot>` |
| `Bounds` | `FStaticMeshBounds` |
| `Dependencies` | `TArray<FAssetDependency>` |
| `SourceManifest` | `TArray<FAssetSourceVersionRecord>` |
| `ImportProfileDigest` | `FAssetDigest` |

### `FStaticMeshAsset : FAssetPayload`

Immutable validated payload with asset type `StaticMesh`.

**Invariants**:

- At least one primitive.
- Primitive and material-slot keys are unique.
- Aggregate bounds enclose every primitive bound.
- Dependencies exactly cover material and source references.
- Source manifest is sorted, duplicate-free, and complete.
- Asset version is derived from canonical payload, source manifest, parser/
  generator versions, and import profile.

## 4. Static Model Asset

### `FStaticModelNode`

| Field | Type | Rule |
|---|---|---|
| `StableKey` | `FString` | Unique within model |
| `DisplayName` | `FString` | Never identity |
| `LocalTransform` | `FTransform` | Finite canonical Unreal-space TRS |
| `Children` | `TArray<uint32>` | Valid unique node indices |
| `Mesh` | optional `TSoftAssetRef<FStaticMeshAsset>` | Typed |
| `SourceNodeIndex` | `uint32` | Evidence |
| `bNegativeDeterminant` | `bool` | Derived parity evidence |

### `FStaticModelAssetDesc`

| Field | Type |
|---|---|
| `Id` | `FAssetId` |
| `Version` | `FAssetVersion` |
| `SchemaVersion` | `uint32 = 1` |
| `SceneStableKey` | `FString` |
| `bSourceDefaultScene` | `bool` |
| `Nodes` | `TArray<FStaticModelNode>` |
| `RootNodeIndices` | `TArray<uint32>` |
| `Bounds` | `FStaticMeshBounds` |
| `Dependencies` | `TArray<FAssetDependency>` |
| `SourceManifest` | `TArray<FAssetSourceVersionRecord>` |
| `ImportProfileDigest` | `FAssetDigest` |

### `FStaticModelAsset : FAssetPayload`

Immutable validated payload with asset type `StaticModel`.

**Invariants**:

- One asset is produced per glTF scene.
- Nodes have at most one parent, are acyclic, and do not exceed maximum depth.
- Roots are exactly nodes without a parent.
- Traversal order is roots in source order and children in source insertion
  order.
- Bounds include all referenced mesh bounds transformed through hierarchy.
- Default-scene evidence is recorded without changing identity.
- Unreferenced glTF meshes still produce independent mesh assets.

## 5. Stable Subresource Identity

### `FStaticModelSubresourceKey`

Canonical string with one of two prefixes:

- `key.<normalized-extras-value>`
- `idx.<typed-structural-path>`

Examples:

```text
key.hero-body
idx.scene.0
idx.mesh.3
idx.mesh.3.primitive.1
idx.material.2
```

**Rules**:

- `extras.stonerAssetId` is preferred when present.
- Explicit key values are normalized and validated with Asset identity rules.
- Explicit keys are unique within output type.
- Structural keys use source indices and type labels.
- Display `name` never participates.
- Reordering keyed objects preserves their identities; reordering fallback-only
  objects is allowed to change their structural identities.

## 6. Material Texture Binding Schema v2

### Asset sampler enums

- `EAssetSamplerFilter`: `Nearest`, `Linear`, `Automatic`
- `EAssetSamplerMipFilter`: `None`, `Nearest`, `Linear`, `Automatic`
- `EAssetSamplerAddressMode`: `Repeat`, `MirroredRepeat`, `ClampToEdge`

### `FMaterialSamplerIntent`

| Field | Type |
|---|---|
| `MinFilter` | `EAssetSamplerFilter` |
| `MagFilter` | `EAssetSamplerFilter` |
| `MipFilter` | `EAssetSamplerMipFilter` |
| `AddressU` | `EAssetSamplerAddressMode` |
| `AddressV` | `EAssetSamplerAddressMode` |

### `FMaterialTextureBinding`

| Field | Type | Rule |
|---|---|---|
| `Texture` | `TSoftAssetRef<FTextureAsset>` | Valid typed reference |
| `TexCoordSet` | `uint32` | 0 or 1 |
| `Sampler` | `FMaterialSamplerIntent` | Valid |

### Schema transition

`EMaterialAssetParameterType` gains `TextureBinding`; the value variant gains
`FMaterialTextureBinding`.

- v1 texture ID -> v2 in-memory binding with UV0, repeat, automatic-linear
  defaults.
- Existing v1 input can be serialized identically as v1.
- glTF-derived materials and instances serialize as schema v2.
- Non-default v2 binding cannot serialize as v1.
- Dependency extraction uses `Binding.Texture`.

## 7. glTF Import Scratch Model

### `FGLTFImportScratchPackage`

Request-scoped mutable assembly state:

- bounded source and dependency bytes
- parser document lease
- canonical source manifest
- planned subresource IDs
- decoded mesh payload builders
- scene/model builders
- material/image/texture payload builders
- deterministic diagnostics
- aggregate budget counters

It is never returned publicly. `Commit()` is permitted only after
`FGLTFPackageValidator` succeeds.

### Import state transitions

```text
Empty
  -> SourcePreflighted
  -> DocumentParsed
  -> DependenciesResolved
  -> SemanticsValidated
  -> GeometryNormalized
  -> PackageAssembled
  -> PackageValidated
  -> Published
```

Any error transitions to `Failed`; `Failed` owns no public output. URI
resolution and decoding cannot publish intermediate assets.

## 8. Renderer Static Mesh Snapshot

### `FStaticMeshRealizationProfile`

Versioned Renderer packing policy:

- vertex semantic order/formats
- interleaving policy
- index packing policy
- buffer usage
- upload policy
- coordinate convention identity

### `FStaticMeshSection`

| Field | Type |
|---|---|
| `FirstIndex` | `uint32` |
| `IndexCount` | `uint32` |
| `VertexOffset` | `int32` |
| `Material` | `FAssetId` |
| `Bounds` | `FStaticMeshBounds` |
| `SourcePrimitiveKey` | `FString` |

### `FStaticMeshAssetSnapshot`

| Field | Type |
|---|---|
| `SourceManifest` | normalized version records |
| `VertexBuffers` | RHI buffer references |
| `IndexBuffer` | RHI buffer reference |
| `VertexInput` | `FRHIVertexInputDesc` |
| `IndexType` | RHI index type |
| `Sections` | `TArray<FStaticMeshSection>` |
| `Bounds` | `FStaticMeshBounds` |
| `RealizationProfileDigest` | `FAssetDigest` |

### Realization transitions

```text
ValidateAsset
  -> Plan
  -> Allocate
  -> Upload
  -> Finalize
  -> Published
```

Failure at any stage invalidates every newly created RHI object, clears the
snapshot, and returns stage-specific deterministic diagnostics.

## 9. RHI Transfer And Draw Values

### `FRHIBufferUploadDesc`

- destination byte offset
- immutable source data pointer
- source byte count

`IRHIDevice::UploadBuffer(Buffer, Desc)` mirrors the existing texture-upload
shape. All ranges use checked arithmetic and must fit the destination
description; the device does not retain the source pointer after return.

### `FRHIIndexedDrawArguments`

- `IndexCount`
- `InstanceCount`
- `FirstIndex`
- `VertexOffset`
- `FirstInstance`

The command buffer validates recording state and non-zero required counts.
Backend implementations map these fields without semantic reinterpretation.

## 10. Validation Relationships

```text
FStaticModelImportProfile
  -> governs FGLTFImportScratchPackage
  -> contributes to every output FAssetVersion

FStaticModelAsset
  -> soft references FStaticMeshAsset

FStaticMeshAsset
  -> soft references FMaterialAsset v2

FMaterialAsset v2
  -> FMaterialTextureBinding
  -> soft references FTextureAsset

FStaticMeshAsset + FStaticMeshRealizationProfile
  -> FStaticMeshAssetSnapshot
  -> RHI buffers and indexed sections
```

No Asset entity owns or references an RHI object. No Renderer snapshot owns a
parser document or source buffer.
