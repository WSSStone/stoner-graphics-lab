# Data Model: KTX2 Cooking & Compression

## Asset Cook Configuration

### ETextureCompressionPolicy

| Value | Meaning | Valid source |
|---|---|---|
| DefaultBySemantic | Resolve through the immutable semantic policy | Any validated Feature 021 texture |
| ETC1S | BasisLZ/ETC1S portable LDR payload | LDR Color only |
| UASTC | UASTC portable LDR payload | LDR Color/Normal; Data only with lossy opt-in |
| Uncompressed | Explicit KTX2 texel representation | Any compatible LDR/HDR texture |

`DefaultBySemantic` resolves to a concrete policy before version serialization.
The artifact records only the resolved policy.

### ETextureCookQuality

Initial values are `Balanced` and `High`. Portable profile v1 fixes their
low-level mapping rather than inheriting host-library defaults:

| Policy | Balanced | High |
|---|---|---|
| ETC1S | quality 192, compression level 2 | quality 255, compression level 2 |
| UASTC | level 2, RDO disabled | level 3, RDO disabled |

Every Basis mode uses one logical worker. Feature 022 uses Balanced as default.
A future mapping change requires a new portable profile or producer version.

### FTextureCookLimits

| Field | Default | Validation |
|---|---:|---|
| MaxDimension | 16,384 | Both base dimensions lie in 1..MaxDimension |
| MaxArtifactBytes | 512 MiB | Bounded source read and final artifact |
| MaxMetadataBytes | 1 MiB | Aggregate DFD, KVD, and SGD preflight budget |
| MaxKeyValuePairs | 64 | Counted before metadata allocation |
| MaxLevelBytes | 512 MiB | Every stored or target level |
| MaxTargetPayloadBytes | 1 GiB | Sum of all target mip bytes |
| MaxMipLevels | 15 | Positive and no greater than geometric maximum |

Every field is positive. Callers may raise a field but cannot disable checked
arithmetic or use an unbounded sentinel.

### FTextureCookSettings

- CompressionPolicy: defaults to `DefaultBySemantic`.
- Quality: defaults to `Balanced`.
- bAllowLossyData: defaults false and is meaningful only for Data plus UASTC.
- PortableProfile: defaults to `stoner.ktx2.portable.v1`.
- ProducerVersion: fixed implementation version `022-v1`.

The source texture's semantic, color space, alpha mode, origin, mip policy, and
ordered mip extents remain authoritative. Settings cannot override them. After
policy resolution all fields contribute to cook revision evidence.

### FTextureCookParameters

A typed immutable `FAssetCookParameters`:

- TextureId: must exactly match the input `FTextureAsset`.
- Settings: immutable `FTextureCookSettings`.
- Limits: validated `FTextureCookLimits`; operational limits do not change cook
  revision when the same input is accepted.

The generic `FAssetCookRequest` continues to carry metadata and payload. Its
optional Parameters points to this type for the KTX2 cooker.

## Canonical Artifact

### EKTX2BasisModel

`None`, `ETC1S`, or `UASTC`. It describes the DFD/Basis payload model and never
aliases a native KTX enum.

### EKTX2Supercompression

`None` or `BasisLZ`. ETC1S uses BasisLZ and its global data. UASTC v1 uses
`None`; UASTC is not itself a KTX2 supercompression scheme.

### FKTX2Level

Normalized description of one logical mip:

- MipLevel: zero-based; zero is the base level.
- Extent: `FImageExtent2D`, matching Feature 021 recurrence.
- ByteOffset: checked range in artifact bytes.
- ByteLength: stored level length.
- UncompressedByteLength: KTX2 level-index value.
- ExpectedTargetFootprint: absent until a transcode target is requested.

Levels are ordered by logical mip index even if physical payload ranges appear
in a different order. Ranges must be in bounds, correctly aligned, and
non-overlapping with protected structures or one another.

### FKTX2TextureInfo

Normalized inspectable facts:

- TextureId.
- SourceDigest, ContentDigest, CookRevision, ArtifactDigest.
- ProducerVersion and PortableProfile.
- CompressionPolicy, BasisModel, and Supercompression.
- Semantic, ColorSpace, AlphaMode, Origin, MipPolicy.
- BaseExtent and ordered Levels.
- StoredTexelFormat for uncompressed artifacts.
- DFD transfer/model facts.
- KTXwriter value.

Validation invariants:

1. TextureId and all digests are valid.
2. Origin is TopLeft and `KTXorientation` is exactly `rd`.
3. only Color may be sRGB.
4. HDR and lossless-required Data are uncompressed.
5. ETC1S is Color only; UASTC Data requires `bAllowLossyData`.
6. Mip count/extents exactly match the source texture and mip policy.
7. Required metadata is present exactly once and agrees with the DFD and level
   descriptions.

### FKTX2TextureArtifact

An immutable `FAssetPayload` with asset type `Texture`:

- Id: same logical `FAssetId` as the source `FTextureAsset`.
- Info: validated `FKTX2TextureInfo`.
- Bytes: complete immutable KTX2 file.

The runtime C++ type is distinct, but `GetAssetType()` and
`TAssetTypeTraits<FKTX2TextureArtifact>` return `Texture` so cooking never
changes the Feature 020 identity type. The object owns no libktx handle, source
image payload, transcode cache, RHI format, GPU resource, native format, or file
path. Its `ArtifactDigest` hashes the exact bytes. Its cook revision hashes
canonical source evidence, resolved cook settings, portable profile, producer
version, and encoder-module hash.

## Generic Contract Extensions

### FAssetCookParameters and FAssetLoadParameters

Polymorphic immutable parameter bases analogous to Feature 021
`FAssetImportParameters`. Existing requests default their pointer to null.
Feature 020 participants remain valid.

### FAssetCookResult

- Result.
- TargetProfile retained for Feature 020 compatibility.
- Artifact bytes retained for generic callers.
- CookDigest.
- Payload: optional immutable typed output; KTX2 returns
  `FKTX2TextureArtifact`.
- Diagnostics: normalized list.

Success requires matching bytes, payload bytes, and digest. Failure returns
empty Artifact/Payload and a zero/default digest.

### FAssetLoadResult

- Result.
- Payload.
- Diagnostics.

KTX2 loading requires typed limits and expected identity. Failure returns no
payload.

### Diagnostic Extensions

New stable results:

| Result | Meaning |
|---|---|
| KTX2LimitExceeded | Structural, metadata, level, or target payload budget exceeded |
| MalformedContainer | Header/index/DFD/KVD/SGD structure is invalid or contradictory |
| CorruptPayload | Container structure is valid but Basis or level data is corrupt |
| CookFailure | Canonical encoder or writer failed after valid input |
| TranscodeFailure | Requested target could not be produced completely |
| UnsupportedCompression | Policy, artifact codec, or target is unsupported |

New stages are `Container` and `Transcode`. Existing `Cook`, `Inspect`,
`Validate`, `Load`, and `Registry` keep their current meanings. A diagnostic may
name a mip level, field, actual value, and configured limit but never includes a
host path, native address, thread ID, timing, or raw backend error text.

## Transcode Model

### ETextureTranscodeFormat

Asset-owned target values:

- BC1_RGBA_UNorm, BC1_RGBA_SRGB
- BC3_RGBA_UNorm, BC3_RGBA_SRGB
- BC4_R_UNorm
- BC5_RG_UNorm
- BC7_RGBA_UNorm, BC7_RGBA_SRGB
- ETC2_RGB8_UNorm, ETC2_RGB8_SRGB
- ETC2_RGBA8_UNorm, ETC2_RGBA8_SRGB
- EAC_R11_UNorm, EAC_RG11_UNorm
- ASTC_4x4_RGBA_UNorm, ASTC_4x4_RGBA_SRGB
- R8_UNorm, R8G8_UNorm, R8G8B8A8_UNorm, R8G8B8A8_SRGB

No Vulkan or RHI numeric value is serialized. Linear-only formats have no sRGB
member.

### FTextureTranscodeRequest

- Artifact: immutable `FKTX2TextureArtifact`.
- TargetFormat: one `ETextureTranscodeFormat`.
- Limits: immutable `FTextureCookLimits`.

The artifact contract determines semantics and channels through its normalized
semantic and original source channel count. The request cannot override alpha,
transfer, origin, channel count, or mip policy.

### FTranscodedTextureMip

- MipLevel and logical Extent.
- BlockWidth, BlockHeight, and BytesPerBlock.
- RowPitchBytes: exact tightly packed block row.
- Bytes: immutable target data.

For uncompressed targets the block is 1x1. For compressed Feature 022 targets it
is 4x4. Byte length equals:

`ceil(width / blockWidth) * ceil(height / blockHeight) * bytesPerBlock`.

### FTranscodedTexturePayload

- TargetFormat.
- Semantic, ColorSpace, AlphaMode, Origin.
- ordered immutable Mips.

It exists only for one realization request. It is not an Asset payload, registry
record, static cache value, or soft-reference target.

### Transcode States

1. ValidateArtifact checks normalized metadata and exact artifact digest.
2. ValidateTarget checks codec, transfer, alpha, and channel compatibility.
3. Preflight computes all target footprints and aggregate budget.
4. Transcode creates private temporary level buffers.
5. Verify checks every level's exact footprint.
6. Publish moves all levels into one immutable result.
7. Reject drops every temporary level and returns no payload.

## RHI Format Model

### FRHIFormatInfo

- Format.
- BlockWidth, BlockHeight, BlockDepth.
- BytesPerBlock.
- bCompressed.
- bSRGB.
- bDepthStencil.

The table is total for every valid `ERHIFormat`. Unknown returns an invalid
zeroed description. Uncompressed formats use a 1x1x1 block. New compressed
members:

| Family | Members | Block | Bytes |
|---|---|---:|---:|
| BC1 | linear, sRGB RGBA | 4x4 | 8 |
| BC3 | linear, sRGB RGBA | 4x4 | 16 |
| BC4 | linear R | 4x4 | 8 |
| BC5 | linear RG | 4x4 | 16 |
| BC7 | linear, sRGB RGBA | 4x4 | 16 |
| ETC2 RGB | linear, sRGB | 4x4 | 8 |
| ETC2 RGBA | linear, sRGB | 4x4 | 16 |
| EAC R | linear | 4x4 | 8 |
| EAC RG | linear | 4x4 | 16 |
| ASTC 4x4 | linear, sRGB RGBA | 4x4 | 16 |

Checked helpers return block counts, tight row bytes, slice bytes, and total
bytes. `GetRHIFormatByteSize` remains an uncompressed convenience and returns
zero for compressed formats.

### ERHIFormatCapability

Bit flags initially include:

- SampledImage.
- CopySource.
- CopyDestination.
- ColorAttachment.
- DepthStencilAttachment.

### FRHIFormatCapabilities

- Format.
- Capabilities flags.

`FRHIDeviceCapabilities` stores one normalized record per valid advertised
format, sorted by enum solely for stable inspection. `SupportsFormatUsage`
checks flags; Renderer target order is independent from record order.

### Compressed Upload Validation

For a compressed upload:

- X/Y/Z offsets are block aligned.
- an extent is block aligned unless its end equals the logical mip edge.
- RowPitchBytes is at least tight block-row bytes and is a multiple of
  BytesPerBlock.
- DataSizeBytes covers all block rows and slices with checked arithmetic.
- full 1x1, 2x2, or 3x3 terminal mips require one 4x4 block.

Feature 022 realization uploads full mips at offset zero, but the generic RHI
contract remains correct for legal edge regions.

## Renderer Model

### FTextureTargetProfile

- Name: stable diagnostic/profile name.
- PreferredFormats: explicit ordered `ERHIFormat` values with no duplicates.
- bAllowUncompressedFallback.

The default desktop profile is generated by semantic/alpha class and follows
BC, ASTC, ETC2/EAC, uncompressed family order. Unknown, depth/stencil,
transfer-incompatible, and duplicate members make a profile invalid.

### FTextureTargetSelection

- Result.
- SelectedRHIFormat.
- SelectedAssetTranscodeFormat.
- Ordered candidate decisions with stable rejection reason.

Selection input is artifact info, profile, and immutable device capabilities.
The first semantically compatible format supporting SampledImage and
CopyDestination wins.

### FKTX2TextureRealizationRequest

- Device: active `IRHIDevice`.
- Artifact: immutable `FKTX2TextureArtifact`.
- TargetProfile: immutable valid profile.

### FKTX2TextureRealizationResult

- Result.
- Texture: non-null only after every mip upload succeeds.
- Selection.
- Diagnostic stage: ValidateArtifact, Select, Transcode, Create, Upload, or
  Finalize.

### Realization States

1. ValidateArtifact.
2. SelectTarget.
3. Transcode or expose uncompressed levels.
4. ValidateFootprints against RHI format info.
5. Create one 2D Sampled|CopyDestination texture.
6. Upload all mips in ascending order.
7. Ready returns the texture and releases transient CPU output after the device
   no longer reads the synchronous upload.
8. Failed releases the request-owned texture exactly once and returns no
   partial GPU resource.
