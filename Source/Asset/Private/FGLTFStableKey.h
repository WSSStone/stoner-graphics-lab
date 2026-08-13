#pragma once

#include "Asset/EAssetResult.h"
#include "Core/FString.h"

namespace Stoner::Asset::Private
{

[[nodiscard]] EAssetResult MakeGLTFStableKey(
    const char* ExtrasJson,
    const Core::FString& FallbackKey,
    Core::FString& OutKey,
    bool& OutExplicit);

} // namespace Stoner::Asset::Private
