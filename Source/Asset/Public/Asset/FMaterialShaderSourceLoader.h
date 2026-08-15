#pragma once

#include "Asset/FAssetDiagnostics.h"
#include "Asset/FAssetExtensionRegistry.h"
#include "Asset/FAssetId.h"
#include "Asset/FAssetImportRequest.h"
#include "Asset/FAssetMetadata.h"
#include "Asset/FAssetPayload.h"
#include "Asset/FAssetRegistry.h"
#include "Asset/FMaterialShaderAssetLimits.h"

namespace Stoner::Asset
{

struct FMaterialShaderLoadRequest
{
    FAssetId ExpectedId;
    const FAssetExtensionRegistry* Extensions = nullptr;
    FAssetSourceDescriptor Descriptor;
    FAssetSourceLease Source;
    FMaterialShaderAssetLimits Limits;
    bool bLoadDependencies = true;
};

class FMaterialShaderImportParameters final : public FAssetImportParameters
{
public:
    FAssetId ExpectedId;
    Core::TSharedPtr<const FAssetExtensionRegistry> Extensions;
    FMaterialShaderAssetLimits Limits;
    bool bLoadDependencies = true;
};

struct FMaterialShaderLoadResult
{
    EAssetResult Result = EAssetResult::InvalidInput;
    Core::TArray<Core::TSharedPtr<const FAssetPayload>> Payloads;
    Core::TArray<FAssetMetadata> Metadata;
    Core::TArray<FAssetDependency> Dependencies;
    Core::FString CanonicalDefinition;
    Core::uint64 RegistryRevision = 0;
    FAssetDiagnosticList Diagnostics;

    [[nodiscard]] bool Succeeded() const noexcept
    {
        return Result == EAssetResult::Success;
    }
};

class FMaterialShaderSourceLoader
{
public:
    [[nodiscard]] static FMaterialShaderLoadResult Load(
        const FMaterialShaderLoadRequest& Request);
};

class FMaterialShaderImportService
{
public:
    [[nodiscard]] static FMaterialShaderLoadResult ImportAndRegister(
        const FAssetExtensionRegistry& Extensions,
        FAssetRegistry& Registry,
        const FAssetImportRequest& Request);
};

[[nodiscard]] EAssetResult RegisterMaterialShaderDefinitionImporter(
    FAssetExtensionRegistry& Registry,
    FAssetRegistrationToken& OutToken);

} // namespace Stoner::Asset
