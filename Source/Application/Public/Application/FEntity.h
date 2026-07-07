#pragma once

#include "Core/CoreMinimal.h"

#include <limits>

namespace Stoner::Application
{

struct FEntity
{
    static constexpr Stoner::Core::uint32 InvalidSlotIndex = std::numeric_limits<Stoner::Core::uint32>::max();

    Stoner::Core::uint32 SlotIndex = InvalidSlotIndex;
    Stoner::Core::uint32 Generation = 0;
    Stoner::Core::uint32 WorldId = 0;

    constexpr FEntity() noexcept = default;
    constexpr FEntity(Stoner::Core::uint32 InSlotIndex,
        Stoner::Core::uint32 InGeneration,
        Stoner::Core::uint32 InWorldId) noexcept
        : SlotIndex(InSlotIndex)
        , Generation(InGeneration)
        , WorldId(InWorldId)
    {
    }

    [[nodiscard]] static constexpr FEntity Invalid() noexcept { return FEntity(); }
    [[nodiscard]] constexpr bool IsSet() const noexcept
    {
        return SlotIndex != InvalidSlotIndex && Generation != 0 && WorldId != 0;
    }
};

[[nodiscard]] constexpr bool operator==(const FEntity& Left, const FEntity& Right) noexcept
{
    return Left.SlotIndex == Right.SlotIndex &&
        Left.Generation == Right.Generation &&
        Left.WorldId == Right.WorldId;
}

[[nodiscard]] constexpr bool operator!=(const FEntity& Left, const FEntity& Right) noexcept
{
    return !(Left == Right);
}

[[nodiscard]] constexpr bool CompareEntityIdentity(const FEntity& Left, const FEntity& Right) noexcept
{
    if (Left.SlotIndex != Right.SlotIndex)
    {
        return Left.SlotIndex < Right.SlotIndex;
    }
    return Left.Generation < Right.Generation;
}

[[nodiscard]] Stoner::Core::FString FormatEntityIdentity(const FEntity& Entity);

} // namespace Stoner::Application
