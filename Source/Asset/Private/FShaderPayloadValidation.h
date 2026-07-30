#pragma once

#include "Asset/FMaterialShaderTypes.h"
#include "Core/TArray.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult ValidateShaderPayloadBytes(
    const Core::TArray<Core::uint8>& Bytes,
    EShaderPayloadFormat Format,
    EShaderStage Stage,
    const Core::FString& EntryPoint);

} // namespace Stoner::Asset::Private
