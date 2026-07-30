#pragma once

#include "Asset/FAssetDiagnostics.h"
#include "Asset/FKTX2TextureArtifact.h"

#include <span>

namespace Stoner::Asset::Private
{

struct FKTX2PreflightResult
{
    Core::uint32 VkFormat = 0;
    Core::uint32 TypeSize = 0;
    Core::uint32 Width = 0;
    Core::uint32 Height = 0;
    Core::uint32 LevelCount = 0;
    Core::uint32 Supercompression = 0;
    Core::uint64 DfdOffset = 0;
    Core::uint64 DfdLength = 0;
    Core::uint64 KvdOffset = 0;
    Core::uint64 KvdLength = 0;
    Core::uint64 SgdOffset = 0;
    Core::uint64 SgdLength = 0;
    Core::TArray<FKTX2Level> Levels;
};

[[nodiscard]] EAssetResult PreflightKTX2(
    std::span<const Core::uint8> Bytes,
    const FTextureCookLimits& Limits,
    FKTX2PreflightResult& OutResult,
    FAssetDiagnosticList* OutDiagnostics = nullptr);

} // namespace Stoner::Asset::Private
