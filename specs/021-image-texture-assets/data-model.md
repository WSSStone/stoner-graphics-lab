# Data Model: Image & Texture Asset Foundation

## Shared Types

### FImageExtent2D

A non-zero Width and Height in texels.

- Width and Height are uint32 and each lies in 1..MaxDimension.
- Row, level, and chain byte sizes use checked uint64 arithmetic.
- Next mip extent is max(1, floor(previous / 2)) per axis.

### EImageTexelFormat

Asset-owned canonical storage. It never aliases RHI::ERHIFormat.

| Value | Channels | Bytes/texel | Primary use |
|---|---:|---:|---|
| R8_UNorm | 1 | 1 | linear data or gray color |
| R8G8_UNorm | 2 | 2 | grayscale plus straight alpha, linear data, or tangent-normal XY |
| R8G8B8_UNorm | 3 | 3 | LDR color or tangent-normal XYZ |
| R8G8B8A8_UNorm | 4 | 4 | LDR color/data/normal plus straight alpha |
| R32G32B32_Float | 3 | 12 | explicit no-alpha HDR |
| R16G16B16A16_Float | 4 | 8 | default HDR |
| R32G32B32A32_Float | 4 | 16 | explicit high-precision HDR |

### Semantic Enums

| Type | Values | Invariant |
|---|---|---|
| ETextureSemantic | Color, Normal, Data | Caller declares meaning; never inferred from channels. |
| EImageColorSpace | Linear, SRGB | Normal and Data must be Linear. |
| EImageAlphaMode | None, Straight | Premultiplied is out of scope. |
| EImageOrigin | TopLeft | Fixed DX/Unreal convention. |
| EImageMipPolicy | FullChain, BaseOnly | FullChain is default. |
| EHDRLayout | DefaultRGBA16F, RGBA32F, RGB32F | Contributes to content evidence. |

## Import Configuration

### FImageImportLimits

| Field | Default | Rule |
|---|---:|---|
| MaxDimension | 16,384 | Each decoded dimension must not exceed it. |
| MaxSourceBytes | 256 MiB | Full source must fit before decode. |
| MaxMipBytes | 512 MiB | Every canonical mip must fit. |
| MaxDecodedChainBytes | 1 GiB | Sum of all retained mips must fit. |

A caller may raise a positive field but cannot use zero/unbounded limits. The
complete limits value is content-version input.

### FImageImportSettings

| Field | Default | Rule |
|---|---|---|
| Semantic | Required | Color, Normal, or Data. |
| ColorSpace | Source-derived | Color may be Linear/SRGB; Normal/Data require Linear. |
| MipPolicy | FullChain | BaseOnly is explicit. |
| HDRLayout | DefaultRGBA16F | Applies only to Radiance HDR. |
| Limits | FImageImportLimits defaults | Must validate before source read. |

Every field contributes to imported-content version evidence.

### FImageImportParameters

A typed immutable FAssetImportParameters object.

- ImageId: requested Image identity.
- TextureId: requested Texture identity.
- Settings: FImageImportSettings.

Both IDs must be valid and distinct typed identities. TextureId metadata has one
required dependency on ImageId. The generic FAssetImportRequest supplies the
source descriptor and source lease.

## Immutable Payloads

### FImageMip

One tightly packed canonical raster level.

- Extent: FImageExtent2D.
- Format: EImageTexelFormat.
- RowPitchBytes: exact checked Width * bytes-per-texel.
- Bytes: immutable contiguous data with exact RowPitchBytes * Height size.

Feature 021 has depth one, one array layer, no padding, and no native layout.

### FImageAsset

An FAssetPayload with stable asset type Image.

- Represents the decoded normalized source base level only.
- Holds BaseMip, ColorSpace, AlphaMode, and Origin.
- Contains no RHI enum, GPU object, decoder state, or mutable byte store.
- Its content digest covers canonical header fields and base bytes.

### FTextureAsset

An FAssetPayload with stable asset type Texture.

- ImageReference: typed soft reference to required Image identity.
- BaseImage: immutable shared ownership of imported Image data for the immediate
  import result; it is not a registry payload cache.
- Semantic, ColorSpace, AlphaMode, Origin, and MipPolicy.
- Mips: ordered levels; level zero shares BaseImage data, later levels are
  immutable generated mips.

Validation requires exact format/interpretation agreement, a correct extent
recurrence, nonempty bytes, and chain budget compliance. Its content digest
includes settings and every mip byte.

## Generic Import Bridge

### FAssetImportRequest

A backward-compatible generic request:

- Descriptor: Feature 020 FAssetSourceDescriptor.
- Source: Feature 020 FAssetSourceLease.
- Parameters: optional immutable shared FAssetImportParameters object.

The existing IAssetImporter import overload remains callable through a default
bridge. The image importer requires FImageImportParameters.

### FImageAssetImporter

An IAssetImporter Strategy.

- Declares png, jpg, jpeg, and hdr format hints.
- Probe is side-effect-free and <= 64 KiB.
- A supported internally coherent signature gets confidence 100.
- Import emits exactly two payload/metadata outputs: Image and dependent Texture.

## Import Publication States

1. Select: FAssetDispatch chooses one importer under an execution lease.
2. Inspect: source limits and metadata policy validate.
3. Decode: source becomes a candidate immutable Image asset.
4. Transform: orientation/canonicalization/mips become a candidate Texture asset.
5. Validate: IDs, metadata, dependency, payload, digests, and sizes validate.
6. Commit: one FAssetMutationBatch publishes both metadata records.
7. Succeeded: caller receives both payloads and registry revision.
8. Rejected: returned outputs are empty; registry and old records are unchanged.

## Renderer Realization

### FTextureAssetRealizationRequest

Renderer-owned request:

- Device: active RHI::IRHIDevice.
- Texture: validated immutable FTextureAsset.
- Requested usage may add only usage compatible with sampled/copy-destination.

### FTextureAssetRealizationResult

- Result: ERHIResult.
- Texture: non-null only after every mip upload succeeds.
- Diagnostic: Renderer stage ValidateAsset, Plan, Create, Expand, Upload, or
  Finalize; affected mip when applicable; asset identity; stable reason.

### Realization States

1. ValidateAsset checks payload and device capabilities.
2. Plan maps/expands Asset format into portable RHI format and upload plan.
3. Create makes one 2D Sampled|CopyDestination texture.
4. Upload invokes IRHIDevice::UploadTexture for every mip in ascending order.
5. Ready returns the sample-ready resource.
6. Failed releases request-owned resource; CPU asset remains valid.
