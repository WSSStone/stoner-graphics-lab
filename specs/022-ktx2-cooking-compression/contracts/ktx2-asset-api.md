# Contract: KTX2 Asset Cook, Load, Inspect, and Transcode APIs

## Ownership Boundary

| Owner | Owns | Must not own |
|---|---|---|
| Asset | KTX2 bytes, cook settings, canonical encoding, preflight, inspect/load, metadata, Basis transcode, CPU target bytes | RHI capabilities, GPU handles, Vulkan/native constants, residency cache |
| Renderer | RHI target profile, capability filtering, Asset-to-RHI mapping, request-scoped realization and rollback | KTX parsing, registry mutation, native API calls |
| RHI | compressed format and footprint contracts, usage capabilities, texture creation/upload | Asset semantic types, Basis/KTX APIs |
| Vulkan Backend | native format mapping/query, image creation, staging/copy/readback | KTX metadata or Asset codec policy |

All public Asset headers include only Asset/Core vocabulary. KTX-Software, WAMR,
and encoder ABI headers are private implementation details.

## Generic Request Evolution

```cpp
class FAssetCookParameters
{
public:
    virtual ~FAssetCookParameters() = default;
};

class FAssetLoadParameters
{
public:
    virtual ~FAssetLoadParameters() = default;
};

struct FAssetCookRequest
{
    FAssetMetadata Metadata;
    Core::TSharedPtr<const FAssetPayload> Payload;
    Core::FString TargetProfile; // Feature 020 compatibility
    Core::TSharedPtr<const FAssetCookParameters> Parameters;
};

struct FAssetCookResult
{
    EAssetResult Result = EAssetResult::Unsupported;
    Core::FString TargetProfile;
    Core::TArray<Core::uint8> Artifact;
    FAssetDigest CookDigest;
    Core::TSharedPtr<const FAssetPayload> Payload;
    FAssetDiagnosticList Diagnostics;
};

struct FAssetLoadRequest
{
    FAssetMetadata Metadata;
    FAssetSourceLease Source;
    Core::TSharedPtr<const FAssetLoadParameters> Parameters;
};

struct FAssetLoadResult
{
    EAssetResult Result = EAssetResult::Unsupported;
    Core::TSharedPtr<const FAssetPayload> Payload;
    FAssetDiagnosticList Diagnostics;
};
```

Null Parameters preserve existing Feature 020 behavior. KTX2 participants
require the matching typed object and return `InvalidInput` at `Validate`
otherwise. Existing virtual signatures remain unchanged.

## Texture Cook API

```cpp
enum class ETextureCompressionPolicy : Core::uint8
{
    DefaultBySemantic,
    ETC1S,
    UASTC,
    Uncompressed
};

enum class ETextureCookQuality : Core::uint8
{
    Balanced,
    High
};

struct FTextureCookLimits;
struct FTextureCookSettings;

struct FTextureCookParameters final : FAssetCookParameters
{
    FAssetId TextureId;
    FTextureCookSettings Settings;
    FTextureCookLimits Limits;
};

class FKTX2TextureCooker final : public IAssetCooker
{
public:
    [[nodiscard]] Core::FString GetName() const override;
    [[nodiscard]] Core::int32 GetPriority() const noexcept override;
    [[nodiscard]] FAssetCookResult Cook(
        const FAssetCookRequest& Request) override;
};
```

The cooker contract guarantees:

1. input Payload is a valid immutable `FTextureAsset`;
2. request metadata and typed TextureId match the payload identity;
3. default policy resolves before cook-revision hashing;
4. no registry mutation occurs inside cooking;
5. success returns one `FKTX2TextureArtifact`, matching Artifact bytes and
   CookDigest;
6. failure returns no bytes, payload, or digest and releases every temporary
   encoder/container allocation;
7. the existing extension execution lease remains held for the whole call.

## Artifact and Inspection API

```cpp
enum class EKTX2Supercompression : Core::uint8
{
    None,
    BasisLZ
};

enum class EKTX2BasisModel : Core::uint8
{
    None,
    ETC1S,
    UASTC
};

struct FKTX2Level;
struct FKTX2TextureInfo;

class FKTX2TextureArtifact final : public FAssetPayload
{
public:
    [[nodiscard]] static EAssetResult Create(
        FAssetId Id,
        FKTX2TextureInfo Info,
        Core::TArray<Core::uint8> Bytes,
        FKTX2TextureArtifact& OutArtifact);

    [[nodiscard]] Core::FString GetAssetType() const override;
    [[nodiscard]] const FAssetId& GetId() const noexcept;
    [[nodiscard]] const FKTX2TextureInfo& GetInfo() const noexcept;
    [[nodiscard]] std::span<const Core::uint8> GetBytes() const noexcept;
    [[nodiscard]] const FAssetDigest& GetArtifactDigest() const noexcept;
};

class FKTX2TextureCodec
{
public:
    [[nodiscard]] static EAssetResult Inspect(
        std::span<const Core::uint8> Bytes,
        const FTextureCookLimits& Limits,
        FKTX2TextureInfo& OutInfo,
        FAssetDiagnosticList* OutDiagnostics = nullptr);

    [[nodiscard]] static EAssetResult Open(
        FAssetId ExpectedId,
        std::span<const Core::uint8> Bytes,
        const FTextureCookLimits& Limits,
        FKTX2TextureArtifact& OutArtifact,
        FAssetDiagnosticList* OutDiagnostics = nullptr);
};
```

`FKTX2TextureArtifact::GetAssetType()` and its type trait return `Texture`.
The C++ payload type distinguishes raw and cooked representations without
changing the Feature 020 identity type.

`Inspect` returns normalized metadata but no payload object. `Open` additionally
requires identity/metadata consistency and verifies the exact artifact digest.
Both are CPU-only, thread-safe for immutable input, and leave outputs default on
failure.

## Loader API

```cpp
struct FKTX2LoadParameters final : FAssetLoadParameters
{
    FAssetId ExpectedId;
    FTextureCookLimits Limits;
};

class FKTX2TextureLoader final : public IAssetLoader
{
public:
    [[nodiscard]] Core::FString GetName() const override;
    [[nodiscard]] Core::int32 GetPriority() const noexcept override;
    [[nodiscard]] FAssetLoadResult Load(
        const FAssetLoadRequest& Request) override;
};
```

The loader uses `FAssetSourceLease::ReadBounded(MaxArtifactBytes, ...)`, runs
preflight before image-data allocation, opens once, and returns exactly one
artifact or none. It does not create a file handle, decoded-image cache, or
registry mutation.

## Transcode API

```cpp
enum class ETextureTranscodeFormat : Core::uint8;

struct FTextureTranscodeRequest
{
    Core::TSharedPtr<const FKTX2TextureArtifact> Artifact;
    ETextureTranscodeFormat TargetFormat;
    FTextureCookLimits Limits;
};

struct FTranscodedTextureMip
{
    Core::uint32 MipLevel = 0;
    FImageExtent2D Extent;
    Core::uint32 BlockWidth = 0;
    Core::uint32 BlockHeight = 0;
    Core::uint32 BytesPerBlock = 0;
    Core::uint64 RowPitchBytes = 0;
    Core::TArray<Core::uint8> Bytes;
};

struct FTranscodedTexturePayload
{
    ETextureTranscodeFormat Format;
    ETextureSemantic Semantic;
    EImageColorSpace ColorSpace;
    EImageAlphaMode AlphaMode;
    EImageOrigin Origin;
    Core::TArray<FTranscodedTextureMip> Mips;
};

struct FTextureTranscodeResult
{
    EAssetResult Result = EAssetResult::Unsupported;
    Core::TSharedPtr<const FTranscodedTexturePayload> Payload;
    FAssetDiagnosticList Diagnostics;
};

class FTextureTranscoder
{
public:
    [[nodiscard]] static FTextureTranscodeResult Transcode(
        const FTextureTranscodeRequest& Request);
};
```

The request is synchronous and immutable. Success publishes all mips. Failure
publishes none. The result has no identity and cannot enter `FAssetRegistry`.
The caller's shared ownership is the entire lifetime contract; Feature 022
creates no hidden cache.

## Stable Result Mapping

| Failure | Result | Stage |
|---|---|---|
| Header/index/metadata budget exceeded | KTX2LimitExceeded | Inspect or Container |
| Invalid offsets, alignment, overlap, DFD, or required metadata | MalformedContainer | Container |
| Basis global data or compressed level corrupt | CorruptPayload | Load or Transcode |
| Invalid semantic/policy combination | InvalidInput | Validate or Cook |
| Unsupported artifact/target codec | UnsupportedCompression | Cook or Transcode |
| Canonical encoder/writer failure | CookFailure | Cook or Container |
| Valid artifact cannot produce complete requested target | TranscodeFailure | Transcode |

Diagnostics always carry stable Code, Subject, Participant, Field/level, Limit
when relevant, and actionable Reason. Native KTX/WAMR integer codes may be
translated into stable project codes but are not exposed as the primary result.

## Thread and Registration Contract

- Participant objects are immutable after registration.
- libktx texture handles and WAMR module instances are request-owned.
- The encoder `.wasm` bytes may be immutable process-wide data; executable
  instances and linear memory are never shared across requests.
- KTX/Basis one-time immutable table initialization must use a standard
  thread-safe primitive and cannot affect output ordering.
- unregister prevents new leases but does not invalidate an already-started
  cook/load.
- eight or more simultaneous requests must match serialized outputs exactly.
