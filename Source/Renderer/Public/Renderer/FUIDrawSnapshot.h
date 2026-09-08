#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FUITextureRequest.h"
#include "Renderer/FUITextureLease.h"

#include <cstddef>
#include <span>

namespace Stoner::Renderer
{

struct FUIVertex
{
    // PackedRGBA8 is a value-level word with fixed component bits: R[0..7],
    // G[8..15], B[16..23], and A[24..31]. Backends must preserve this mapping.
    static constexpr Stoner::Core::uint32 PackedRedShift = 0;
    static constexpr Stoner::Core::uint32 PackedGreenShift = 8;
    static constexpr Stoner::Core::uint32 PackedBlueShift = 16;
    static constexpr Stoner::Core::uint32 PackedAlphaShift = 24;
    static constexpr Stoner::Core::uint32 PackedRedMask = 0x000000FFu;
    static constexpr Stoner::Core::uint32 PackedGreenMask = 0x0000FF00u;
    static constexpr Stoner::Core::uint32 PackedBlueMask = 0x00FF0000u;
    static constexpr Stoner::Core::uint32 PackedAlphaMask = 0xFF000000u;

    Stoner::Core::FVector2 Position;
    Stoner::Core::FVector2 UV;
    // RGB is packed as sRGB bytes and A is linear coverage.
    Stoner::Core::uint32 PackedRGBA8 = 0;
};

static_assert(sizeof(FUIVertex) == 20,
    "FUIVertex must remain the bounded 20-byte UI vertex");

enum class EUIDrawOperation
{
    Draw,
    ResetState
};

struct FUIDrawCommand
{
    Stoner::Core::uint32 FirstIndex = 0;
    Stoner::Core::uint32 IndexCount = 0;
    Stoner::Core::int32 BaseVertex = 0;
    Stoner::Core::FVector4 ClipRect;
    FUITextureId TextureId;
    EUIDrawOperation Operation = EUIDrawOperation::Draw;
};

// A snapshot is assembled by copying value ranges before publication. Once
// Publish succeeds, its identity and arrays are exposed only through const
// accessors, so queued Renderer work cannot observe Application-side mutation.
class FUIDrawSnapshot
{
public:
    FUIDrawSnapshot() = default;

    FUIDrawSnapshot(
        Stoner::Core::uint64 InSessionId,
        Stoner::Core::uint64 InFrameId,
        Stoner::Core::uint64 InSettingsRevision,
        Stoner::Core::uint64 InDisplayGeneration) noexcept;

    [[nodiscard]] bool SetIdentity(
        Stoner::Core::uint64 InSessionId,
        Stoner::Core::uint64 InFrameId,
        Stoner::Core::uint64 InSettingsRevision,
        Stoner::Core::uint64 InDisplayGeneration) noexcept;

    [[nodiscard]] bool SetDisplay(
        const Stoner::Core::FVector2& InDisplayPos,
        const Stoner::Core::FVector2& InDisplaySize,
        const Stoner::Core::FVector2& InFramebufferScale) noexcept;

    [[nodiscard]] bool SetVertices(
        std::span<const FUIVertex> InVertices);
    [[nodiscard]] bool SetIndices(
        std::span<const Stoner::Core::uint32> InIndices);
    [[nodiscard]] bool SetCommands(
        std::span<const FUIDrawCommand> InCommands);
    [[nodiscard]] bool SetTextureIds(
        std::span<const FUITextureId> InTextureIds);

    [[nodiscard]] bool SetTextureLeases(std::span<const FUITextureLease> InLeases);
    [[nodiscard]] const Stoner::Core::TArray<FUITextureLease>& GetTextureLeases() const noexcept { return TextureLeases; }
    // Used by the Application extraction boundary before exposing a snapshot.
    // Native preparation additionally validates against the current session.
    [[nodiscard]] bool ValidateOwnedGeometry(Stoner::Core::uint32 DrawableWidth,
        Stoner::Core::uint32 DrawableHeight) const;

    // Publication performs structural value checks and freezes the snapshot.
    // Full index, texture-lease, and native-preparation validation belongs to
    // T050; a published snapshot makes no GPU-readiness claim.
    [[nodiscard]] bool Publish() noexcept;
    [[nodiscard]] bool IsPublished() const noexcept { return bPublished; }
    [[nodiscard]] bool IsValid() const noexcept;

    [[nodiscard]] Stoner::Core::uint64 GetSessionId() const noexcept
    {
        return SessionId;
    }
    [[nodiscard]] Stoner::Core::uint64 GetFrameId() const noexcept
    {
        return FrameId;
    }
    [[nodiscard]] Stoner::Core::uint64 GetSettingsRevision() const noexcept
    {
        return SettingsRevision;
    }
    [[nodiscard]] Stoner::Core::uint64 GetDisplayGeneration() const noexcept
    {
        return DisplayGeneration;
    }
    [[nodiscard]] const Stoner::Core::FVector2& GetDisplayPos() const noexcept
    {
        return DisplayPos;
    }
    [[nodiscard]] const Stoner::Core::FVector2& GetDisplaySize() const noexcept
    {
        return DisplaySize;
    }
    [[nodiscard]] const Stoner::Core::FVector2&
        GetFramebufferScale() const noexcept
    {
        return FramebufferScale;
    }
    [[nodiscard]] const Stoner::Core::TArray<FUIVertex>&
        GetVertices() const noexcept
    {
        return Vertices;
    }
    [[nodiscard]] const Stoner::Core::TArray<Stoner::Core::uint32>&
        GetIndices() const noexcept
    {
        return Indices;
    }
    [[nodiscard]] const Stoner::Core::TArray<FUIDrawCommand>&
        GetCommands() const noexcept
    {
        return Commands;
    }
    [[nodiscard]] const Stoner::Core::TArray<FUITextureId>&
        GetTextureIds() const noexcept
    {
        return TextureIds;
    }

private:
    static constexpr std::size_t MaximumVertices = 262144;
    static constexpr std::size_t MaximumIndices = 786432;
    static constexpr std::size_t MaximumCommands = 4096;

    Stoner::Core::uint64 SessionId = 0;
    Stoner::Core::uint64 FrameId = 0;
    Stoner::Core::uint64 SettingsRevision = 0;
    Stoner::Core::uint64 DisplayGeneration = 0;
    Stoner::Core::FVector2 DisplayPos;
    Stoner::Core::FVector2 DisplaySize;
    Stoner::Core::FVector2 FramebufferScale;
    Stoner::Core::TArray<FUIVertex> Vertices;
    Stoner::Core::TArray<Stoner::Core::uint32> Indices;
    Stoner::Core::TArray<FUIDrawCommand> Commands;
    Stoner::Core::TArray<FUITextureId> TextureIds;
    Stoner::Core::TArray<FUITextureLease> TextureLeases;
    bool bPublished = false;
};

[[nodiscard]] const char* ToString(EUIDrawOperation Operation) noexcept;

} // namespace Stoner::Renderer
