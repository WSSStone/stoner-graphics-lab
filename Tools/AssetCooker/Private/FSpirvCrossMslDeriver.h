#pragma once

#include "Asset/FAssetDigest.h"
#include "Asset/FMaterialShaderTypes.h"
#include "Asset/FShaderNativeBindingEvidence.h"
#include "FMetalBindingMap.h"

#include <span>
#include <string_view>

namespace Stoner::AssetCooker::Private
{

struct FSpirvCrossMslRequest
{
    std::span<const Core::uint8> SpirvBytes;
    Asset::EShaderStage Stage = Asset::EShaderStage::Vertex;
    Core::FString EntryPoint;
    std::span<const Asset::FShaderInterfaceBinding> InterfaceBindings;
    FMetalBindingLimits BindingLimits;
};

struct FSpirvCrossMslResult
{
    Core::FString NormalizedMsl;
    Asset::FAssetDigest SpirvDigest;
    Asset::FAssetDigest InterfaceDigest;
    Asset::FAssetDigest OptionsDigest;
    Asset::FAssetDigest NormalizedMslDigest;
    Asset::FShaderNativeBindingEvidence BindingEvidence;

    [[nodiscard]] bool IsValid() const noexcept;
};

[[nodiscard]] Core::FString MakeMetalNativeEntryPoint(
    const Core::FString& LogicalEntryPoint);

[[nodiscard]] Asset::EAssetResult NormalizeMetalShaderSource(
    std::string_view Source,
    Core::FString& OutNormalized) noexcept;

[[nodiscard]] Asset::EAssetResult DeriveMetalShaderSource(
    const FSpirvCrossMslRequest& Request,
    FSpirvCrossMslResult& OutResult) noexcept;

} // namespace Stoner::AssetCooker::Private
