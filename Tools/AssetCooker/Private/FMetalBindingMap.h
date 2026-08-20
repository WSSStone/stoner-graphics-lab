#pragma once

#include "Asset/FShaderNativeBindingEvidence.h"

#include <span>

namespace Stoner::AssetCooker::Private
{

struct FMetalBindingLimits
{
    Core::uint32 MaxBufferBindings = 31;
    Core::uint32 MaxTextureBindings = 128;
    Core::uint32 MaxSamplerBindings = 16;

    [[nodiscard]] bool IsValid() const noexcept;
};

struct FMetalBindingMapRequest
{
    Asset::EShaderStage Stage = Asset::EShaderStage::Vertex;
    std::span<const Asset::FShaderInterfaceBinding> InterfaceBindings;
    FMetalBindingLimits Limits;
};

[[nodiscard]] Asset::EAssetResult BuildMetalBindingMap(
    const FMetalBindingMapRequest& Request,
    Asset::FShaderNativeBindingEvidence& OutEvidence) noexcept;

} // namespace Stoner::AssetCooker::Private
