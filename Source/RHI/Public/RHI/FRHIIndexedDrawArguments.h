#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::RHI
{

struct FRHIIndexedDrawArguments
{
    Stoner::Core::uint32 IndexCount = 0;
    Stoner::Core::uint32 InstanceCount = 1;
    Stoner::Core::uint32 FirstIndex = 0;
    Stoner::Core::int32 VertexOffset = 0;
    Stoner::Core::uint32 FirstInstance = 0;
};

[[nodiscard]] constexpr bool IsValidRHIIndexedDrawArguments(
    const FRHIIndexedDrawArguments& Arguments) noexcept
{
    return Arguments.IndexCount > 0 && Arguments.InstanceCount > 0;
}

} // namespace Stoner::RHI
