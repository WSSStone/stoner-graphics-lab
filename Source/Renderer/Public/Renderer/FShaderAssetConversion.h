#pragma once

#include "Asset/FAssetSourceVersionRecord.h"
#include "Asset/FShaderAsset.h"
#include "Renderer/FMaterialDiagnostics.h"
#include "Renderer/FShaderLibrary.h"
#include "RHI/FRHIShaderModuleDesc.h"

namespace Stoner::Renderer
{

struct FShaderTargetSelection
{
    Asset::EShaderBackendFamily Backend =
        Asset::EShaderBackendFamily::Vulkan;
    Core::FString Profile;
    Core::FString PermutationKey;
};

struct FShaderAssetConversionRequest
{
    const Asset::FSelectedShaderProgram* SelectedProgram = nullptr;
};

struct FShaderAssetSnapshot
{
    Core::TArray<Asset::FAssetSourceVersionRecord> SourceManifest;
    FShaderTargetSelection SelectedTarget;
    Core::TArray<FShaderRecord> ShaderRecords;
    Core::TArray<RHI::FRHIShaderModuleDesc> ModuleDescriptions;
};

[[nodiscard]] EMaterialResult ConvertShaderAsset(
    const FShaderAssetConversionRequest& Request,
    FShaderAssetSnapshot& OutSnapshot,
    FMaterialDiagnosticLog* Diagnostics = nullptr);

} // namespace Stoner::Renderer
