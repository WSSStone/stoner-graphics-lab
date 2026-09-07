#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIResult.h"

namespace Stoner::Renderer
{

// These tags describe the color interpretation of a UI texture. They are
// deliberately independent of render-graph domains: UI RGB is either decoded
// from encoded sRGB Rec.709, sampled as linear Rec.709, or treated as linear
// coverage data.
enum class EUITextureColorDomain
{
    SRGBRec709,
    LinearRec709,
    AlphaCoverage
};

enum class EUITextureOperation
{
    Create,
    Update,
    Destroy
};

// Source tags are retained for Renderer-private texture records. They are
// intentionally absent from FUITextureRequest: renderer-produced resources
// use the separate private registry path.
enum class EUITextureSourceKind
{
    CPUUpload,
    RendererProduced
};

enum class EUITextureState
{
    Requested,
    Prepared,
    UploadQueued,
    Ready,
    Retiring,
    Destroyed,
    Rejected
};

struct FUITextureId
{
    static constexpr Stoner::Core::uint32 MinimumSlot = 1;
    static constexpr Stoner::Core::uint32 MaximumSlot = 256;

    Stoner::Core::uint32 Slot = 0;
    // Generation is intentionally not capped by the 512-record budget. It is
    // a monotonic identity and remains valid until it wraps the chosen type.
    Stoner::Core::uint64 Generation = 0;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return Slot >= MinimumSlot && Slot <= MaximumSlot && Generation != 0;
    }

    [[nodiscard]] bool IsNewerThan(const FUITextureId& Other) const noexcept
    {
        return IsValid() && Other.IsValid() && Slot == Other.Slot &&
            Generation > Other.Generation;
    }

    [[nodiscard]] friend bool operator==(
        const FUITextureId& Left,
        const FUITextureId& Right) noexcept = default;
};

struct FUITextureRequest
{
    static constexpr Stoner::Core::uint32 MaximumDimension = 2048;
    static constexpr Stoner::Core::uint64 MaximumPayloadBytes =
        16ull * 1024ull * 1024ull;

    Stoner::Core::uint64 RequestId = 0;
    EUITextureOperation Operation = EUITextureOperation::Create;

    // LogicalSlot is the requested slot for creation. TextureId identifies
    // the existing generation for update/destroy; no native handle is stored.
    Stoner::Core::uint32 LogicalSlot = 0;
    FUITextureId TextureId;
    Stoner::Core::uint64 ExpectedGeneration = 0;

    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    Stoner::RHI::ERHIFormat Format = Stoner::RHI::ERHIFormat::Unknown;
    EUITextureColorDomain ColorDomain = EUITextureColorDomain::SRGBRec709;
    // The public request is a CPU upload value. PixelBytes owns a complete
    // copied RGBA8 payload; renderer-produced resources use a private registry
    // path and are intentionally not represented by this request.
    Stoner::Core::TArray<Stoner::Core::uint8> PixelBytes;

    [[nodiscard]] bool IsValid() const noexcept;
};

struct FUITextureResult
{
    Stoner::Core::uint64 RequestId = 0;
    Stoner::RHI::ERHIResult Result = Stoner::RHI::ERHIResult::Failed;
    EUITextureState State = EUITextureState::Rejected;
    FUITextureId TextureId;
    Stoner::Core::FString Diagnostic;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] bool Succeeded() const noexcept
    {
        return IsValid() && Result == Stoner::RHI::ERHIResult::Success &&
            (State == EUITextureState::Prepared ||
             State == EUITextureState::UploadQueued ||
             State == EUITextureState::Ready ||
             State == EUITextureState::Retiring ||
             State == EUITextureState::Destroyed);
    }
};

[[nodiscard]] const char* ToString(EUITextureColorDomain Domain) noexcept;
[[nodiscard]] const char* ToString(EUITextureOperation Operation) noexcept;
[[nodiscard]] const char* ToString(EUITextureSourceKind Source) noexcept;
[[nodiscard]] const char* ToString(EUITextureState State) noexcept;

} // namespace Stoner::Renderer
