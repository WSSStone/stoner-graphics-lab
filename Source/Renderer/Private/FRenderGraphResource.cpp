#include "Renderer/FRenderGraphResource.h"

#include <utility>

namespace Stoner::Renderer
{

bool FRenderGraphTextureSidecar::IsValid() const noexcept
{
    return bIsTyped && Stoner::RHI::IsValidRHIFormat(Format) &&
        Stoner::RHI::IsValidRHISampleCount(SampleCount) &&
        Usage != Stoner::RHI::ERHITextureUsage::None &&
        Stoner::RHI::HasOnlyRHIFlags(
            Usage, Stoner::RHI::RHITextureUsageValidMask) &&
        ColorDomain != ERenderGraphColorDomain::Unspecified;
}

FRenderGraphResourceDesc FRenderGraphResourceDesc::Buffer(Stoner::Core::FString InName, Stoner::Core::uint64 SizeInBytes)
{
    FRenderGraphResourceDesc Desc;
    Desc.Name = std::move(InName);
    Desc.Kind = ERenderGraphResourceKind::Buffer;
    Desc.SizeInBytes = SizeInBytes;
    return Desc;
}

FRenderGraphResourceDesc FRenderGraphResourceDesc::Texture2D(Stoner::Core::FString InName, Stoner::Core::uint32 Width, Stoner::Core::uint32 Height)
{
    FRenderGraphResourceDesc Desc;
    Desc.Name = std::move(InName);
    Desc.Kind = ERenderGraphResourceKind::Texture;
    Desc.Width = Width;
    Desc.Height = Height;
    return Desc;
}

FRenderGraphResourceDesc FRenderGraphResourceDesc::TypedTexture2D(
    Stoner::Core::FString InName,
    Stoner::Core::uint32 Width,
    Stoner::Core::uint32 Height,
    Stoner::RHI::ERHIFormat Format,
    Stoner::RHI::ERHISampleCount SampleCount,
    Stoner::RHI::ERHITextureUsage Usage,
    ERenderGraphColorDomain ColorDomain)
{
    FRenderGraphResourceDesc Desc = Texture2D(std::move(InName), Width, Height);
    Desc.FormatId = static_cast<Stoner::Core::uint32>(Format);
    Desc.Texture.Format = Format;
    Desc.Texture.SampleCount = SampleCount;
    Desc.Texture.Usage = Usage;
    Desc.Texture.ColorDomain = ColorDomain;
    Desc.Texture.bIsTyped = true;
    return Desc;
}

FRenderGraphResourceDesc FRenderGraphResourceDesc::ImportedBuffer(Stoner::Core::FString InName, Stoner::Core::uint64 SizeInBytes, bool bReadOnly)
{
    FRenderGraphResourceDesc Desc = Buffer(std::move(InName), SizeInBytes);
    Desc.Ownership = ERenderGraphResourceOwnership::Imported;
    Desc.InitialState = ERenderGraphResourceState::External;
    Desc.AliasPolicy = ERenderGraphAliasPolicy::Disabled;
    Desc.bReadOnlyImported = bReadOnly;
    return Desc;
}

bool IsValidRenderGraphResourceDesc(const FRenderGraphResourceDesc& Desc) noexcept
{
    if (Desc.Name.IsEmpty())
    {
        return false;
    }

    if (Desc.Ownership == ERenderGraphResourceOwnership::Imported && Desc.InitialState == ERenderGraphResourceState::Unknown)
    {
        return false;
    }

    switch (Desc.Kind)
    {
    case ERenderGraphResourceKind::Buffer:
    case ERenderGraphResourceKind::External:
        return Desc.SizeInBytes > 0;
    case ERenderGraphResourceKind::Texture:
        return Desc.Width > 0 && Desc.Height > 0 && Desc.Depth > 0 &&
            (Desc.Texture.bIsTyped ? Desc.Texture.IsValid() : Desc.FormatId != 0);
    }
    return false;
}

const char* ToString(ERenderGraphColorDomain Domain) noexcept
{
    switch (Domain)
    {
    case ERenderGraphColorDomain::Unspecified: return "Unspecified";
    case ERenderGraphColorDomain::SceneLinearRec709D65: return "SceneLinearRec709D65";
    case ERenderGraphColorDomain::DisplayLinearRec709D65: return "DisplayLinearRec709D65";
    case ERenderGraphColorDomain::DisplayLinearRec2020D65: return "DisplayLinearRec2020D65";
    case ERenderGraphColorDomain::EncodedSrgb: return "EncodedSrgb";
    case ERenderGraphColorDomain::EncodedBt709: return "EncodedBt709";
    case ERenderGraphColorDomain::EncodedGamma22: return "EncodedGamma22";
    case ERenderGraphColorDomain::EncodedPqRec2020D65: return "EncodedPqRec2020D65";
    case ERenderGraphColorDomain::ExtendedSrgbLinear: return "ExtendedSrgbLinear";
    }
    return "Unknown";
}

const char* ToString(ERenderGraphResourceKind Kind) noexcept
{
    switch (Kind)
    {
    case ERenderGraphResourceKind::Buffer: return "Buffer";
    case ERenderGraphResourceKind::Texture: return "Texture";
    case ERenderGraphResourceKind::External: return "External";
    }
    return "Unknown";
}

const char* ToString(ERenderGraphResourceOwnership Ownership) noexcept
{
    switch (Ownership)
    {
    case ERenderGraphResourceOwnership::Transient: return "Transient";
    case ERenderGraphResourceOwnership::Imported: return "Imported";
    case ERenderGraphResourceOwnership::Exported: return "Exported";
    }
    return "Unknown";
}

const char* ToString(ERenderGraphResourceState State) noexcept
{
    switch (State)
    {
    case ERenderGraphResourceState::Unknown: return "Unknown";
    case ERenderGraphResourceState::Read: return "Read";
    case ERenderGraphResourceState::Write: return "Write";
    case ERenderGraphResourceState::ReadWrite: return "ReadWrite";
    case ERenderGraphResourceState::External: return "External";
    }
    return "Unknown";
}

const char* ToString(ERenderGraphAliasReason Reason) noexcept
{
    switch (Reason)
    {
    case ERenderGraphAliasReason::NonOverlappingCompatible: return "NonOverlappingCompatible";
    case ERenderGraphAliasReason::OverlappingLifetime: return "OverlappingLifetime";
    case ERenderGraphAliasReason::IncompatibleDescription: return "IncompatibleDescription";
    case ERenderGraphAliasReason::ImportedResource: return "ImportedResource";
    case ERenderGraphAliasReason::ExportedExternalOwnership: return "ExportedExternalOwnership";
    case ERenderGraphAliasReason::ExplicitNoAlias: return "ExplicitNoAlias";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
