#pragma once

#include "Core/CoreMinimal.h"

#include <utility>

namespace Stoner::Application
{

struct FMeshComponent
{
    Stoner::Core::FString MeshId;
    Stoner::Core::FString MaterialId;
    Stoner::Core::int32 OptionalSortKey = 0;
    bool bHasSortKey = false;

    FMeshComponent() = default;
    explicit FMeshComponent(Stoner::Core::FString InMeshId,
        Stoner::Core::FString InMaterialId = Stoner::Core::FString())
        : MeshId(std::move(InMeshId))
        , MaterialId(std::move(InMaterialId))
    {
    }

    [[nodiscard]] bool IsValid() const noexcept;
};

} // namespace Stoner::Application
