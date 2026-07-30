# Contract: Compressed RHI Formats and KTX2 Realization

## RHI Format Information

```cpp
struct FRHIFormatInfo
{
    ERHIFormat Format = ERHIFormat::Unknown;
    Core::uint8 BlockWidth = 0;
    Core::uint8 BlockHeight = 0;
    Core::uint8 BlockDepth = 0;
    Core::uint8 BytesPerBlock = 0;
    bool bCompressed = false;
    bool bSRGB = false;
    bool bDepthStencil = false;

    [[nodiscard]] bool IsValid() const noexcept;
};

[[nodiscard]] constexpr FRHIFormatInfo GetRHIFormatInfo(
    ERHIFormat Format) noexcept;

[[nodiscard]] bool TryGetRHITextureFootprint(
    ERHIFormat Format,
    Core::uint32 Width,
    Core::uint32 Height,
    Core::uint32 Depth,
    FRHITextureFootprint& OutFootprint) noexcept;
```

`FRHITextureFootprint` contains block counts, tight row bytes, slice bytes, and
total bytes. Every multiplication is checked. `GetRHIFormatByteSize` remains
valid only for 1x1x1 uncompressed formats and returns zero for compressed
members.

## Required ERHIFormat Members

```text
BC1_RGBA_UNorm,       BC1_RGBA_sRGB
BC3_RGBA_UNorm,       BC3_RGBA_sRGB
BC4_R_UNorm
BC5_RG_UNorm
BC7_RGBA_UNorm,       BC7_RGBA_sRGB
ETC2_RGB8_UNorm,      ETC2_RGB8_sRGB
ETC2_RGBA8_UNorm,     ETC2_RGBA8_sRGB
EAC_R11_UNorm
EAC_RG11_UNorm
ASTC_4x4_RGBA_UNorm,  ASTC_4x4_RGBA_sRGB
```

BC4, BC5, EAC R, and EAC RG are linear-only. All listed formats are color-data
formats, not depth/stencil.

## Usage Capabilities

```cpp
enum class ERHIFormatCapability : Core::uint32
{
    None = 0,
    SampledImage = 1 << 0,
    CopySource = 1 << 1,
    CopyDestination = 1 << 2,
    ColorAttachment = 1 << 3,
    DepthStencilAttachment = 1 << 4
};

struct FRHIFormatCapabilities
{
    ERHIFormat Format = ERHIFormat::Unknown;
    ERHIFormatCapability Capabilities =
        ERHIFormatCapability::None;
};

struct FRHIDeviceCapabilities
{
    Core::TArray<FRHIFormatCapabilities> Formats;

    [[nodiscard]] bool SupportsFormat(ERHIFormat Format) const;
    [[nodiscard]] bool SupportsFormatUsage(
        ERHIFormat Format,
        ERHIFormatCapability Required) const;
};
```

`Formats` is the only source of truth. Duplicate or invalid records make a
capability snapshot invalid. Stable sorting is for diagnostics only and does not
imply preference.

## Upload Footprint Contract

`TryGetRHITextureUploadRequiredBytes` uses `FRHIFormatInfo`:

1. validate the logical region against the mip;
2. require block-aligned compressed offsets;
3. require each compressed extent to be block-aligned unless its end reaches
   the logical mip edge;
4. calculate block-rounded width/height/depth;
5. require row pitch >= tight block-row bytes and divisible by bytes per block;
6. calculate final required bytes with checked arithmetic;
7. require DataSizeBytes >= required bytes.

A full 1x1 BC7 upload has logical extent 1x1, one block row, row pitch 16, and
required bytes 16. It is valid.

All existing callers that size texture buffers, staging records, copy regions,
or readback data must use the new footprint helpers. A zero result from
`GetRHIFormatByteSize` must no longer be interpreted as an unsupported texture
when `FRHIFormatInfo` is valid and compressed.

## Renderer Target Profile

```cpp
struct FTextureTargetProfile
{
    Core::FString Name;
    Core::TArray<RHI::ERHIFormat> PreferredFormats;
    bool bAllowUncompressedFallback = true;

    [[nodiscard]] RHI::ERHIResult Validate() const;
    [[nodiscard]] static FTextureTargetProfile DesktopDefault(
        const Asset::FKTX2TextureInfo& Info);
};

struct FTextureTargetSelection
{
    RHI::ERHIResult Result;
    RHI::ERHIFormat SelectedFormat = RHI::ERHIFormat::Unknown;
    Asset::ETextureTranscodeFormat TranscodeFormat;
    Core::TArray<FTextureTargetCandidateDiagnostic> Candidates;
};
```

Selection traverses `PreferredFormats` exactly once in listed order. A candidate
is accepted only if:

- it is a sampled color format;
- it preserves required transfer;
- it preserves alpha and semantic-required channels;
- the artifact codec can transcode to it;
- device capabilities include both SampledImage and CopyDestination.

The final decision cannot depend on `ERHIFormat` ordinal, capability record
order, Vulkan format value, adapter registration order, or hash/container
iteration.

Default desktop orders:

| Content class | Ordered targets |
|---|---|
| Opaque Color | BC1, BC7, ASTC 4x4, ETC2 RGB, RGBA8 |
| Straight-alpha Color | BC7, BC3, ASTC 4x4, ETC2 RGBA, RGBA8 |
| Two-channel Normal/Data | BC5, ASTC 4x4, EAC RG, RG8 |
| One-channel Data | BC4, EAC R, R8 |

Each color-capable token resolves to linear or sRGB before capability lookup.
The fallback is produced from the authoritative Basis payload; no duplicate
uncompressed LDR artifact is consulted.

## KTX2 Realization API

```cpp
enum class EKTX2TextureRealizationStage
{
    ValidateArtifact,
    Select,
    Transcode,
    Create,
    Upload,
    Finalize
};

struct FKTX2TextureRealizationRequest
{
    Core::TSharedPtr<RHI::IRHIDevice> Device;
    Core::TSharedPtr<const Asset::FKTX2TextureArtifact> Artifact;
    FTextureTargetProfile TargetProfile;
};

struct FKTX2TextureRealizationResult
{
    RHI::ERHIResult Result;
    Core::TSharedPtr<RHI::IRHITexture> Texture;
    FTextureTargetSelection Selection;
    FKTX2TextureRealizationDiagnostic Diagnostic;

    [[nodiscard]] bool Succeeded() const noexcept;
};

class FKTX2TextureRealizer
{
public:
    [[nodiscard]] static FKTX2TextureRealizationResult Realize(
        const FKTX2TextureRealizationRequest& Request);
};
```

The class is a sibling of `FTextureAssetRealizer`. It does not add KTX2 branches
to the Feature 021 raw-texture path.

## Realization Transaction

1. Validate active device and immutable artifact.
2. Validate target profile.
3. Select one target from a snapshot of device capabilities.
4. Request one complete Asset transcode payload, or use validated uncompressed
   levels.
5. Cross-check every Asset level against RHI format footprints.
6. Create one Texture2D with all mips and
   `Sampled|CopyDestination`.
7. Upload every mip synchronously in ascending order.
8. Return the texture only after the final successful upload.

Before resource creation, failure returns no RHI object. After creation, any
failure releases the request-owned texture exactly once. The CPU artifact
remains unchanged. The transcode payload is released when the request no longer
needs it and is never retained globally.

## Vulkan Contract

The Vulkan backend maps each required RHI format to the corresponding
`VkFormat`. Native capability records derive from optimal-tiling format
properties:

- SampledImage requires sampled-image feature support.
- CopyDestination requires transfer-destination support.
- only advertised combinations may be selected.

Tightly packed compressed upload uses zero `bufferRowLength` and
`bufferImageHeight`, logical texel `imageExtent`, and the RHI-validated block
byte count. Vulkan code must not recompute a conflicting footprint.

Native evidence runs only for a format supporting both required usages.
Unavailable support produces an explicit unavailable/skip result. Synthetic
fallback mode may test deterministic contracts but cannot satisfy the native
evidence gate.
