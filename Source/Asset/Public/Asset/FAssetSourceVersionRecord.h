#pragma once

#include "Asset/FAssetId.h"
#include "Asset/FAssetVersion.h"
#include "Core/TArray.h"

namespace Stoner::Asset
{

enum class EAssetSourceRole : Core::uint8
{
    Program,
    Source,
    Payload,
    Material,
    Parent,
    Texture
};

struct FAssetSourceVersionRecord
{
    FAssetId Id;
    FAssetVersion Version;
    EAssetSourceRole Role = EAssetSourceRole::Source;
    [[nodiscard]] bool operator==(const FAssetSourceVersionRecord&) const = default;
};

[[nodiscard]] EAssetResult NormalizeSourceManifest(
    Core::TArray<FAssetSourceVersionRecord>& Manifest);

} // namespace Stoner::Asset
