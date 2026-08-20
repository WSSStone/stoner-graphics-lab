#pragma once

#include "Asset/FAssetPayload.h"
#include "Asset/FAssetTargetProfile.h"
#include "Asset/FMaterialShaderTypes.h"
#include "Core/TArray.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult ValidateShaderPayloadBytes(
    const Core::TArray<Core::uint8>& Bytes,
    EShaderPayloadFormat Format,
    EShaderStage Stage,
    const Core::FString& EntryPoint);

[[nodiscard]] EAssetResult ValidateStrictCookedShaderPayload(
    const FAssetTargetProfile& Profile,
    Core::uint32 CodecVersion,
    Core::uint32 PayloadSchemaVersion,
    const FAssetPayload& Payload) noexcept;

} // namespace Stoner::Asset::Private
