#include "Renderer/FUIDrawSnapshot.h"
#include "FUIDrawValidator.h"
#include <algorithm>

#include <cmath>

namespace Stoner::Renderer
{

namespace
{

constexpr Stoner::Core::uint32 MaximumTextureGenerations = 512;

[[nodiscard]] bool IsFinite(const Stoner::Core::FVector2& Value) noexcept
{
    return std::isfinite(Value.X) && std::isfinite(Value.Y);
}

[[nodiscard]] bool IsFinite(const Stoner::Core::FVector4& Value) noexcept
{
    return std::isfinite(Value.X) && std::isfinite(Value.Y) &&
        std::isfinite(Value.Z) && std::isfinite(Value.W);
}

[[nodiscard]] bool IsValidOperation(EUIDrawOperation Operation) noexcept
{
    switch (Operation)
    {
    case EUIDrawOperation::Draw:
    case EUIDrawOperation::ResetState:
        return true;
    }
    return false;
}

} // namespace

FUIDrawSnapshot::FUIDrawSnapshot(
    Stoner::Core::uint64 InSessionId,
    Stoner::Core::uint64 InFrameId,
    Stoner::Core::uint64 InSettingsRevision,
    Stoner::Core::uint64 InDisplayGeneration) noexcept
    : SessionId(InSessionId)
    , FrameId(InFrameId)
    , SettingsRevision(InSettingsRevision)
    , DisplayGeneration(InDisplayGeneration)
{
}

bool FUIDrawSnapshot::SetIdentity(
    Stoner::Core::uint64 InSessionId,
    Stoner::Core::uint64 InFrameId,
    Stoner::Core::uint64 InSettingsRevision,
    Stoner::Core::uint64 InDisplayGeneration) noexcept
{
    if (bPublished)
    {
        return false;
    }
    SessionId = InSessionId;
    FrameId = InFrameId;
    SettingsRevision = InSettingsRevision;
    DisplayGeneration = InDisplayGeneration;
    return true;
}

bool FUIDrawSnapshot::SetDisplay(
    const Stoner::Core::FVector2& InDisplayPos,
    const Stoner::Core::FVector2& InDisplaySize,
    const Stoner::Core::FVector2& InFramebufferScale) noexcept
{
    if (bPublished)
    {
        return false;
    }
    DisplayPos = InDisplayPos;
    DisplaySize = InDisplaySize;
    FramebufferScale = InFramebufferScale;
    return true;
}

bool FUIDrawSnapshot::SetVertices(
    std::span<const FUIVertex> InVertices)
{
    if (bPublished || InVertices.size() > MaximumVertices)
    {
        return false;
    }
    Vertices.assign(InVertices.begin(), InVertices.end());
    return true;
}

bool FUIDrawSnapshot::SetIndices(
    std::span<const Stoner::Core::uint32> InIndices)
{
    if (bPublished || InIndices.size() > MaximumIndices)
    {
        return false;
    }
    Indices.assign(InIndices.begin(), InIndices.end());
    return true;
}

bool FUIDrawSnapshot::SetCommands(
    std::span<const FUIDrawCommand> InCommands)
{
    if (bPublished || InCommands.size() > MaximumCommands)
    {
        return false;
    }
    Commands.assign(InCommands.begin(), InCommands.end());
    return true;
}

bool FUIDrawSnapshot::SetTextureIds(
    std::span<const FUITextureId> InTextureIds)
{
    if (bPublished || InTextureIds.size() > MaximumTextureGenerations)
    {
        return false;
    }
    for (const FUITextureId& TextureId : InTextureIds)
    {
        if (!TextureId.IsValid())
        {
            return false;
        }
    }
    TextureIds.assign(InTextureIds.begin(), InTextureIds.end());
    return true;
}

bool FUIDrawSnapshot::SetTextureLeases(std::span<const FUITextureLease> InLeases)
{
    if (bPublished || InLeases.size() > MaximumTextureGenerations) return false;
    Stoner::Core::TArray<FUITextureId> Ids;
    Ids.reserve(InLeases.size());
    for (const auto& Lease : InLeases)
    {
        if (!Lease.IsValid() || std::find(Ids.begin(), Ids.end(), Lease.GetId()) != Ids.end()) return false;
        Ids.push_back(Lease.GetId());
    }
    Stoner::Core::TArray<FUITextureLease> Owners(InLeases.begin(), InLeases.end());
    TextureIds = std::move(Ids); TextureLeases = std::move(Owners);
    return true;
}

bool FUIDrawSnapshot::ValidateOwnedGeometry(Stoner::Core::uint32 Width,
    Stoner::Core::uint32 Height) const
{
    if (TextureIds.size() != TextureLeases.size()) return false;
    FUIDrawValidationContext Context{SessionId, SettingsRevision, DisplayGeneration, 0, Width, Height,
        [this](FUITextureId Id)
        {
            return std::any_of(TextureLeases.begin(), TextureLeases.end(),
                [Id](const auto& Lease) { return Lease.IsValid() && Lease.GetId() == Id; });
        }};
    return FUIDrawValidator::Validate(*this, Context).bValid;
}

bool FUIDrawSnapshot::Publish() noexcept
{
    if (bPublished || !IsValid())
    {
        return false;
    }
    bPublished = true;
    return true;
}

bool FUIDrawSnapshot::IsValid() const noexcept
{
    if (SessionId == 0 || FrameId == 0 || SettingsRevision == 0 ||
        DisplayGeneration == 0 || !IsFinite(DisplayPos) ||
        !IsFinite(DisplaySize) || DisplaySize.X <= 0.0f ||
        DisplaySize.Y <= 0.0f || !IsFinite(FramebufferScale) ||
        FramebufferScale.X <= 0.0f || FramebufferScale.Y <= 0.0f ||
        Vertices.size() > MaximumVertices || Indices.size() > MaximumIndices ||
        Commands.size() > MaximumCommands ||
        TextureIds.size() > MaximumTextureGenerations)
    {
        return false;
    }

    if (!TextureLeases.empty())
    {
        if (TextureLeases.size() != TextureIds.size()) return false;
        for (std::size_t I = 0; I < TextureLeases.size(); ++I)
            if (!TextureLeases[I].IsValid() || TextureLeases[I].GetId() != TextureIds[I]) return false;
    }

    for (const FUITextureId& TextureId : TextureIds)
    {
        if (!TextureId.IsValid())
        {
            return false;
        }
    }

    for (const FUIVertex& Vertex : Vertices)
    {
        if (!IsFinite(Vertex.Position) || !IsFinite(Vertex.UV))
        {
            return false;
        }
    }

    for (const FUIDrawCommand& Command : Commands)
    {
        if (!IsValidOperation(Command.Operation) ||
            !IsFinite(Command.ClipRect))
        {
            return false;
        }
        if (Command.Operation == EUIDrawOperation::Draw &&
            !Command.TextureId.IsValid())
        {
            return false;
        }
    }
    return true;
}

const char* ToString(EUIDrawOperation Operation) noexcept
{
    switch (Operation)
    {
    case EUIDrawOperation::Draw: return "Draw";
    case EUIDrawOperation::ResetState: return "ResetState";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
