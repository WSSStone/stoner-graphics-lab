#pragma once

#include "Asset/FAssetDigest.h"
#include "Core/FPlatformTypes.h"
#include "Core/FString.h"

namespace Stoner::Asset
{

struct FCurrentGenerationPointer
{
    static constexpr Core::uint32 CurrentSchemaVersion = 1;

    Core::FString Schema = Core::FString("stoner.asset-current-generation");
    Core::uint32 SchemaVersion = CurrentSchemaVersion;
    FAssetDigest GenerationId;
    Core::FString ManifestLocator;
    FAssetDigest ManifestDigest;

    [[nodiscard]] EAssetResult Validate() const noexcept;
    [[nodiscard]] bool operator==(const FCurrentGenerationPointer&) const = default;
};

} // namespace Stoner::Asset
