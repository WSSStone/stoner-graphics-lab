#pragma once

#include "Asset/IAssetCooker.h"
#include "Asset/FAssetDerivedKey.h"
#include "Asset/FShaderAsset.h"
#include "FMetalLibraryCompiler.h"
#include "FSpirvCrossMslDeriver.h"

#include <optional>

namespace Stoner::AssetCooker::Private
{

struct FMetalShaderCookParameters final : Asset::FAssetCookParameters
{
    Asset::FAssetId ShaderAssetId;
    Asset::FAssetDigest ShaderAssetVersion;
    std::optional<Asset::FAssetDigest> GlslDigest;
    Core::TArray<Asset::FShaderInterfaceBinding> InterfaceBindings;
    Core::FString WorkingDirectory;
    Core::FString Architecture;
    FMetalToolchainEvidence ToolchainEvidence;
    IMetalToolExecutor* ToolExecutor = nullptr;
};

class FMetalShaderCooker final : public Asset::IAssetCooker
{
public:
    [[nodiscard]] static Asset::FAssetParticipantId ParticipantId();
    [[nodiscard]] static Asset::FAssetProducerVersion ProducerVersion();

    [[nodiscard]] Asset::FAssetExtensionCapability GetCapability() const override;
    [[nodiscard]] Asset::EAssetResult GetRelevantProfileEvidence(
        const Asset::FAssetTargetProfileEvidence& Profile,
        Asset::FAssetProfileProjectionEvidence& OutEvidence) const override;
    [[nodiscard]] Asset::FAssetCookResult Cook(
        const Asset::FAssetCookRequest& Request) override;
};

[[nodiscard]] Asset::EAssetResult BuildMetalShaderDerivedEvidence(
    const Asset::FShaderPayloadAsset& Payload,
    const FMetalShaderCookParameters& Parameters,
    const Asset::FAssetTargetProfileEvidence& Profile,
    FSpirvCrossMslResult& OutDerivation,
    Core::TArray<Asset::FAssetDerivedNamedEvidence>& OutEvidence) noexcept;

} // namespace Stoner::AssetCooker::Private
