#pragma once
#include "FProductionContentSession.h"
#include "Renderer/FShaderAssetConversion.h"

namespace Stoner::Demo
{
// Owned bytes selected only from the session's already authenticated generation.
// No device objects or runtime source loading occurs at this boundary.
struct FInteractiveLabShaders
{
    Asset::FAssetDigest Generation;
    Renderer::FShaderAssetSnapshot Draw;
    Renderer::FShaderAssetSnapshot Copy;
    Renderer::FShaderAssetSnapshot Diagnostic;
};
[[nodiscard]] Asset::EAssetResult PrepareInteractiveLabShaders(
    const FProductionContentLoadedClosure& Closure,
    const Asset::FAssetDigest& ExpectedGeneration,
    const Asset::FAssetTargetProfileEvidence& Target,
    FInteractiveLabShaders& OutShaders,
    Core::FString& OutReason);
}
