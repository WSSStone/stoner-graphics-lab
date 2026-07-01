#include "Renderer/FRenderGraphResource.h"

#include <utility>

namespace Stoner::Renderer
{

FRenderGraphResourceDesc FRenderGraphResourceDesc::Buffer(std::string InName, Stoner::Core::uint64 SizeInBytes)
{
    FRenderGraphResourceDesc Desc;
    Desc.Name = std::move(InName);
    Desc.Kind = ERenderGraphResourceKind::Buffer;
    Desc.SizeInBytes = SizeInBytes;
    return Desc;
}

FRenderGraphResourceDesc FRenderGraphResourceDesc::Texture2D(std::string InName, Stoner::Core::uint32 Width, Stoner::Core::uint32 Height)
{
    FRenderGraphResourceDesc Desc;
    Desc.Name = std::move(InName);
    Desc.Kind = ERenderGraphResourceKind::Texture;
    Desc.Width = Width;
    Desc.Height = Height;
    return Desc;
}

FRenderGraphResourceDesc FRenderGraphResourceDesc::ImportedBuffer(std::string InName, Stoner::Core::uint64 SizeInBytes, bool bReadOnly)
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
    if (Desc.Name.empty())
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
        return Desc.Width > 0 && Desc.Height > 0 && Desc.Depth > 0 && Desc.FormatId != 0;
    }
    return false;
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
