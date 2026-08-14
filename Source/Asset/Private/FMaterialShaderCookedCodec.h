#pragma once

#include "Asset/FAssetCookedPayload.h"
#include "Asset/FAssetPayload.h"
#include "Core/TSharedPtr.h"

#include <span>

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult EncodeMaterialShaderCookedBody(
    const FAssetPayload& Payload,
    const FAssetCookedPayloadLimits& Limits,
    FAssetCookedPayloadHeader& OutHeader,
    Core::TArray<Core::uint8>& OutBody);

[[nodiscard]] EAssetResult DecodeMaterialShaderCookedBody(
    const FAssetCookedPayloadHeader& Header,
    std::span<const Core::uint8> Body,
    Core::TSharedPtr<const FAssetPayload>& OutPayload);

} // namespace Stoner::Asset::Private
