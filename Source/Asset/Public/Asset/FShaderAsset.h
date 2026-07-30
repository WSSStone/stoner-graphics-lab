#pragma once

#include "Asset/FAssetDependency.h"
#include "Asset/FAssetDiagnostics.h"
#include "Asset/FAssetId.h"
#include "Asset/FAssetPayload.h"
#include "Asset/FAssetSourceVersionRecord.h"
#include "Asset/FAssetVersion.h"
#include "Asset/FMaterialShaderTypes.h"
#include "Asset/FShaderPayloadAsset.h"
#include "Core/TSharedPtr.h"

namespace Stoner::Asset
{

struct FShaderAssetDesc
{
    FAssetId Id;
    FAssetVersion Version;
    Core::uint32 SchemaVersion = 1;
    EShaderProgramKind ProgramKind = EShaderProgramKind::Graphics;
    Core::TArray<FShaderSourceReference> Stages;
    Core::TArray<Core::FString> AllowedPermutationFlags;
    Core::TArray<FShaderVariantDefinition> Variants;
    Core::TArray<FShaderRequiredParameter> RequiredParameters;
    Core::TArray<FShaderInterfaceBinding> InterfaceBindings;
    Core::TArray<FShaderConstantRange> ConstantRanges;
    Core::TArray<FAssetDependency> Dependencies;
    Core::FString CanonicalDefinition;
};

class FShaderAsset final : public FAssetPayload
{
public:
    [[nodiscard]] static EAssetResult CreateValidated(
        FShaderAssetDesc Desc,
        FShaderAsset& OutAsset,
        FAssetDiagnosticList* Diagnostics = nullptr);

    [[nodiscard]] Core::FString GetAssetType() const override;
    [[nodiscard]] const FShaderAssetDesc& GetDesc() const noexcept;

private:
    FShaderAssetDesc Desc_;
};

class IShaderPayloadLookup
{
public:
    virtual ~IShaderPayloadLookup() = default;
    [[nodiscard]] virtual Core::TSharedPtr<const FShaderPayloadAsset> Find(
        const FAssetId& Id) const = 0;
};

struct FSelectedShaderStage
{
    EShaderStage Stage = EShaderStage::Vertex;
    Core::TSharedPtr<const FShaderPayloadAsset> Payload;
};

struct FSelectedShaderProgram
{
    FAssetId ShaderId;
    FAssetVersion ShaderVersion;
    EShaderBackendFamily Backend = EShaderBackendFamily::Vulkan;
    Core::FString SelectedProfile;
    FShaderPermutationKey Permutation;
    Core::TArray<FSelectedShaderStage> Stages;
    Core::TArray<FShaderInterfaceBinding> InterfaceBindings;
    Core::TArray<FShaderConstantRange> ConstantRanges;
    Core::TArray<FShaderRequiredParameter> RequiredParameters;
    Core::TArray<FAssetSourceVersionRecord> SourceManifest;
};

[[nodiscard]] EAssetResult SelectShaderProgram(
    const FShaderAsset& Program,
    const FShaderTargetRequest& Request,
    const IShaderPayloadLookup& Payloads,
    FSelectedShaderProgram& OutSelection,
    FAssetDiagnosticList* Diagnostics = nullptr);

} // namespace Stoner::Asset
