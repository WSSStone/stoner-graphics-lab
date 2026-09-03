#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIPipelineState.h"
#include "RHI/ERHIResourceUsage.h"

namespace Stoner::Renderer
{

struct FRenderGraphResourceHandle
{
    Stoner::Core::uint32 GraphId = 0;
    Stoner::Core::uint32 Index = InvalidIndex;

    static constexpr Stoner::Core::uint32 InvalidIndex = 0xFFFFFFFFu;

    [[nodiscard]] bool IsValid() const noexcept { return GraphId != 0 && Index != InvalidIndex; }
    [[nodiscard]] friend bool operator==(FRenderGraphResourceHandle Left, FRenderGraphResourceHandle Right) noexcept
    {
        return Left.GraphId == Right.GraphId && Left.Index == Right.Index;
    }
    [[nodiscard]] friend bool operator!=(FRenderGraphResourceHandle Left, FRenderGraphResourceHandle Right) noexcept
    {
        return !(Left == Right);
    }
};

enum class ERenderGraphResourceKind
{
    Buffer,
    Texture,
    External
};

enum class ERenderGraphResourceOwnership
{
    Transient,
    Imported,
    Exported
};

enum class ERenderGraphAliasPolicy
{
    Eligible,
    Disabled
};

enum class ERenderGraphAliasDecisionState
{
    Eligible,
    Rejected
};

enum class ERenderGraphAliasReason
{
    NonOverlappingCompatible,
    OverlappingLifetime,
    IncompatibleDescription,
    ImportedResource,
    ExportedExternalOwnership,
    ExplicitNoAlias
};

enum class ERenderGraphResourceState
{
    Unknown,
    Read,
    Write,
    ReadWrite,
    External
};

enum class ERenderGraphColorDomain
{
    Unspecified,
    SceneLinearRec709D65,
    DisplayLinearRec709D65,
    DisplayLinearRec2020D65,
    EncodedSrgb,
    EncodedBt709,
    EncodedGamma22,
    EncodedPqRec2020D65,
    ExtendedSrgbLinear
};

struct FRenderGraphTextureSidecar
{
    Stoner::RHI::ERHIFormat Format = Stoner::RHI::ERHIFormat::Unknown;
    Stoner::RHI::ERHISampleCount SampleCount =
        Stoner::RHI::ERHISampleCount::One;
    Stoner::RHI::ERHITextureUsage Usage = Stoner::RHI::ERHITextureUsage::None;
    ERenderGraphColorDomain ColorDomain = ERenderGraphColorDomain::Unspecified;
    bool bIsTyped = false;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] friend bool operator==(
        const FRenderGraphTextureSidecar& Left,
        const FRenderGraphTextureSidecar& Right) noexcept = default;
};

struct FRenderGraphResourceDesc
{
    Stoner::Core::FString Name;
    ERenderGraphResourceKind Kind = ERenderGraphResourceKind::Buffer;
    ERenderGraphResourceOwnership Ownership = ERenderGraphResourceOwnership::Transient;
    ERenderGraphAliasPolicy AliasPolicy = ERenderGraphAliasPolicy::Eligible;
    ERenderGraphResourceState InitialState = ERenderGraphResourceState::Unknown;
    Stoner::Core::uint64 SizeInBytes = 0;
    Stoner::Core::uint32 Width = 1;
    Stoner::Core::uint32 Height = 1;
    Stoner::Core::uint32 Depth = 1;
    Stoner::Core::uint32 FormatId = 1;
    FRenderGraphTextureSidecar Texture;
    bool bReadOnlyImported = false;

    [[nodiscard]] static FRenderGraphResourceDesc Buffer(Stoner::Core::FString InName, Stoner::Core::uint64 SizeInBytes);
    [[nodiscard]] static FRenderGraphResourceDesc Texture2D(Stoner::Core::FString InName, Stoner::Core::uint32 Width, Stoner::Core::uint32 Height);
    [[nodiscard]] static FRenderGraphResourceDesc TypedTexture2D(
        Stoner::Core::FString InName,
        Stoner::Core::uint32 Width,
        Stoner::Core::uint32 Height,
        Stoner::RHI::ERHIFormat Format,
        Stoner::RHI::ERHISampleCount SampleCount,
        Stoner::RHI::ERHITextureUsage Usage,
        ERenderGraphColorDomain ColorDomain);
    [[nodiscard]] static FRenderGraphResourceDesc ImportedBuffer(Stoner::Core::FString InName, Stoner::Core::uint64 SizeInBytes, bool bReadOnly = true);
};

struct FRenderGraphResourceRecord
{
    FRenderGraphResourceHandle Handle;
    FRenderGraphResourceDesc Desc;
    bool bResolvedDuringExecution = false;
    Stoner::Core::uint32 BackingAllocationId = 0;
};

struct FRenderGraphResourceLifetime
{
    FRenderGraphResourceHandle Resource;
    Stoner::Core::uint32 FirstUsePassIndex = FRenderGraphResourceHandle::InvalidIndex;
    Stoner::Core::uint32 LastUsePassIndex = FRenderGraphResourceHandle::InvalidIndex;
    bool bImported = false;
    bool bExported = false;
};

struct FRenderGraphAliasingDecision
{
    FRenderGraphResourceHandle FirstResource;
    FRenderGraphResourceHandle SecondResource;
    ERenderGraphAliasDecisionState State = ERenderGraphAliasDecisionState::Rejected;
    ERenderGraphAliasReason Reason = ERenderGraphAliasReason::OverlappingLifetime;
};

[[nodiscard]] bool IsValidRenderGraphResourceDesc(const FRenderGraphResourceDesc& Desc) noexcept;
[[nodiscard]] const char* ToString(ERenderGraphResourceKind Kind) noexcept;
[[nodiscard]] const char* ToString(ERenderGraphResourceOwnership Ownership) noexcept;
[[nodiscard]] const char* ToString(ERenderGraphResourceState State) noexcept;
[[nodiscard]] const char* ToString(ERenderGraphAliasReason Reason) noexcept;
[[nodiscard]] const char* ToString(ERenderGraphColorDomain Domain) noexcept;

} // namespace Stoner::Renderer
