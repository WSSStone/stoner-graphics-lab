# Contract: Image, Texture, and RHI Realization APIs

## Ownership Boundary

| Owner | Owns | Must not own |
|---|---|---|
| Asset | source inspection/decode, CPU bytes, semantics, mips, IDs, metadata, versions, import transaction | RHI types, GPU handles, native API calls, residency |
| RHI | portable formats, texture creation, upload descriptor/result | Asset headers, source codecs, semantic policy |
| Renderer | Asset-to-RHI mapping, RGB expansion, create/upload rollback | codec parsing, registry mutation, native API calls |
| Vulkan Backend | RHI mappings, staging, native copy/transition/completion | Asset payload semantics beyond RHI upload bytes |

## Asset Public API

~~~
enum class EImageTexelFormat : Core::uint8;
enum class ETextureSemantic : Core::uint8;
enum class EImageColorSpace : Core::uint8;
enum class EImageAlphaMode : Core::uint8;
enum class EImageOrigin : Core::uint8;
enum class EImageMipPolicy : Core::uint8;
enum class EHDRLayout : Core::uint8;

struct FImageExtent2D;
struct FImageMip;
struct FImageImportLimits;
struct FImageImportSettings;
class FImageAsset final : public FAssetPayload;
class FTextureAsset final : public FAssetPayload;
class FImageAssetImporter final : public IAssetImporter;
class FAssetImportService;
~~~

Public Asset headers include only Asset/Core vocabulary. The decoder and all RHI
types remain private to their owning layers.

## Import Request and Commit

~~~
class FAssetImportParameters {
public:
    virtual ~FAssetImportParameters() = default;
};

struct FAssetImportRequest {
    FAssetSourceDescriptor Descriptor;
    FAssetSourceLease Source;
    Core::TSharedPtr<const FAssetImportParameters> Parameters;
};

struct FImageImportParameters final : FAssetImportParameters {
    FAssetId ImageId;
    FAssetId TextureId;
    FImageImportSettings Settings;
};
~~~

IAssetImporter receives a request overload while retaining a default bridge to
the Feature 020 overload. The selected importer executes only under its existing
execution lease.

FAssetImportService::ImportAndRegister guarantees:

1. Feature 020 hint filtering, bounded probe, unique selection, ambiguity, and
   registration-lifetime behavior are preserved.
2. The result contains either exactly the requested Image and Texture outputs or
   no output.
3. One registry mutation batch publishes both metadata records only after all
   payload/metadata/dependency validation succeeds.
4. Failure leaves registry state unchanged and releases temporary payload bytes.
5. Texture metadata has a required dependency on Image metadata.

## Canonical Payload Contract

FImageMip is immutable and tightly packed. RowPitchBytes equals checked
Width * bytes-per-texel and Bytes equals RowPitchBytes * Height. Every Asset
payload has top-left origin. Image owns only level zero; Texture owns an ordered
chain beginning with that level.

| Semantic | Color space | Canonical formats | Mip behavior |
|---|---|---|---|
| Color | Linear or sRGB | R8, RG8 gray plus alpha, RGB8, RGBA8, RGB32F, RGBA16F, RGBA32F | Linear-light RGB for sRGB; alpha independently filtered |
| Normal | Linear only | RG8, RGB8, RGBA8 | Decode/filter/renormalize; zero vector becomes +Z |
| Data | Linear only | Canonical source-class formats | Independent channel filtering |

Normal/Data plus sRGB is invalid. Premultiplied alpha is invalid. Default HDR is
RGBA16F; RGBA32F/RGB32F are explicit and version-significant.

## Asset Diagnostics

Asset extends existing generic result/stage vocabulary where necessary.

| Condition | Result | Stage |
|---|---|---|
| Source ends before required bytes | TruncatedSource | Inspect or Decode |
| Source/dimension/mip/chain budget exceeded | ImageLimitExceeded | Inspect, Validate, or Mip |
| ICC or unrepresentable transfer profile | UnsupportedColorProfile | Inspect |
| NaN or infinity in canonical HDR | NonFiniteImageData | Decode or Validate |
| Finite HDR value not fitting requested layout | HDRPrecisionRangeExceeded | Validate |

MalformedSource, Unsupported, NotFound, AccessDenied, and ProcessingFailure keep
their Feature 020 meaning. A normalized diagnostic carries stable code, source
or asset subject, selected participant, relevant field/limit, and no native
error text, address, thread ID, or timing.

## RHI Upload API

~~~
struct FRHITextureUploadDesc {
    Core::uint32 MipLevel = 0;
    Core::uint32 ArrayLayer = 0;
    Core::uint32 X = 0;
    Core::uint32 Y = 0;
    Core::uint32 Z = 0;
    Core::uint32 Width = 0;
    Core::uint32 Height = 0;
    Core::uint32 Depth = 1;
    Core::uint64 RowPitchBytes = 0;
    const void* Data = nullptr;
    Core::uint64 DataSizeBytes = 0;
};

class IRHIDevice {
public:
    virtual ERHIResult UploadTexture(
        const Core::TSharedPtr<IRHITexture>& Texture,
        const FRHITextureUploadDesc& Upload);
};
~~~

The call rejects inactive device, invalid texture, null data, zero/out-of-range
region, invalid layout, missing CopyDestination usage, insufficient data, format
mismatch, or unsupported format. Success means named mip data is complete and
sample-ready under the synchronous contract. It exposes no backend staging type
or async handle.

RHI gains R8G8_UNorm, R8G8B8A8_sRGB, and R32G32B32A32_Float. Each needs
byte-size, descriptor, capability, mock, fallback, and native Vulkan mapping
coverage.

## Renderer Realization API

~~~
struct FTextureAssetRealizationRequest {
    Core::TSharedPtr<RHI::IRHIDevice> Device;
    Core::TSharedPtr<const Asset::FTextureAsset> Asset;
};

struct FTextureAssetRealizationResult {
    RHI::ERHIResult Result;
    Core::TSharedPtr<RHI::IRHITexture> Texture;
    FTextureAssetRealizationDiagnostic Diagnostic;
};

class FTextureAssetRealizer {
public:
    static FTextureAssetRealizationResult Realize(
        const FTextureAssetRealizationRequest& Request);
};
~~~

The realizer creates one 2D Sampled|CopyDestination texture, uploads mips in
ascending order, and returns it only when all uploads succeed. It owns temporary
conversion buffers and never modifies Asset bytes. Failure returns no texture and
releases every request-owned resource.

| Asset layout | Semantic/color | RHI layout | Conversion |
|---|---|---|---|
| R8 | Linear data/normal or gray | R8 UNorm | None |
| R8 | sRGB gray color | RGBA8 sRGB | Replicate R, alpha 1 |
| RG8 | Linear data/normal | RG8 UNorm | None |
| RG8 | Linear or sRGB gray-plus-alpha color | RGBA8 UNorm or sRGB | Replicate R to RGB, G is straight alpha |
| RGB8 | Linear | RGBA8 UNorm | Alpha 1 |
| RGB8 | sRGB color | RGBA8 sRGB | Alpha 1 |
| RGBA8 | Linear | RGBA8 UNorm | None |
| RGBA8 | sRGB Color | RGBA8 sRGB | None |
| RGB32F | Linear | RGBA32F | Alpha 1 |
| RGBA16F | Linear | RGBA16F | None |
| RGBA32F | Linear | RGBA32F | None |

All unlisted combinations fail before RHI resource creation with a Renderer
diagnostic.
