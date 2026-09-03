#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FRenderGraphResource.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIPipelineState.h"

namespace Stoner::Renderer
{

enum class EHDRSceneColorProducer
{
    Forward,
    Deferred
};

enum class EHDRSceneColorState
{
    Declared,
    ProducerBound,
    Produced,
    Consumed,
    Failed
};

enum class EOutputColorPrimaries
{
    Rec709,
    Rec2020
};

enum class EOutputWhitePoint
{
    D65
};

enum class EOutputTransferFunction
{
    Linear,
    Srgb,
    Bt709,
    Gamma22,
    St2084,
    ScRgb80,
    MetalEdr
};

enum class EOutputAlphaMode
{
    OpaqueOne
};

struct FHDRSceneColorHandoffDesc
{
    Stoner::Core::uint64 SceneColorId = 0;
    EHDRSceneColorProducer Producer = EHDRSceneColorProducer::Forward;
    Stoner::Core::uint64 ViewId = 0;
    Stoner::Core::uint64 FrameToken = 0;
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    Stoner::RHI::ERHIFormat Format =
        Stoner::RHI::ERHIFormat::R16G16B16A16_Float;
    Stoner::RHI::ERHISampleCount SampleCount =
        Stoner::RHI::ERHISampleCount::One;
    EOutputColorPrimaries Primaries = EOutputColorPrimaries::Rec709;
    EOutputWhitePoint WhitePoint = EOutputWhitePoint::D65;
    EOutputTransferFunction Transfer = EOutputTransferFunction::Linear;
    EOutputAlphaMode AlphaMode = EOutputAlphaMode::OpaqueOne;
    bool bFinite = true;
    bool bOpaqueAlpha = true;
};

class FHDRSceneColorHandoff
{
public:
    [[nodiscard]] static FHDRSceneColorHandoff Declare(
        const FHDRSceneColorHandoffDesc& Desc) noexcept;

    [[nodiscard]] bool BindProducer(FRenderGraphResourceHandle Resource) noexcept;
    [[nodiscard]] bool MarkProduced() noexcept;
    [[nodiscard]] bool MarkConsumed() noexcept;
    void Fail() noexcept;

    [[nodiscard]] bool IsMetadataValid() const noexcept;
    [[nodiscard]] bool IsReadyForConsumption() const noexcept;
    [[nodiscard]] EHDRSceneColorState GetState() const noexcept { return State; }
    [[nodiscard]] Stoner::Core::uint64 GetSceneColorId() const noexcept { return Desc.SceneColorId; }
    [[nodiscard]] EHDRSceneColorProducer GetProducer() const noexcept { return Desc.Producer; }
    [[nodiscard]] Stoner::Core::uint64 GetViewId() const noexcept { return Desc.ViewId; }
    [[nodiscard]] Stoner::Core::uint64 GetFrameToken() const noexcept { return Desc.FrameToken; }
    [[nodiscard]] Stoner::Core::uint32 GetWidth() const noexcept { return Desc.Width; }
    [[nodiscard]] Stoner::Core::uint32 GetHeight() const noexcept { return Desc.Height; }
    [[nodiscard]] Stoner::RHI::ERHIFormat GetFormat() const noexcept { return Desc.Format; }
    [[nodiscard]] Stoner::RHI::ERHISampleCount GetSampleCount() const noexcept { return Desc.SampleCount; }
    [[nodiscard]] EOutputColorPrimaries GetPrimaries() const noexcept { return Desc.Primaries; }
    [[nodiscard]] EOutputWhitePoint GetWhitePoint() const noexcept { return Desc.WhitePoint; }
    [[nodiscard]] EOutputTransferFunction GetTransfer() const noexcept { return Desc.Transfer; }
    [[nodiscard]] EOutputAlphaMode GetAlphaMode() const noexcept { return Desc.AlphaMode; }
    [[nodiscard]] bool IsFinite() const noexcept { return Desc.bFinite; }
    [[nodiscard]] bool HasOpaqueAlpha() const noexcept { return Desc.bOpaqueAlpha; }
    [[nodiscard]] FRenderGraphResourceHandle GetResource() const noexcept { return Resource; }

private:
    FHDRSceneColorHandoffDesc Desc;
    FRenderGraphResourceHandle Resource;
    EHDRSceneColorState State = EHDRSceneColorState::Failed;
};

[[nodiscard]] const char* ToString(EHDRSceneColorProducer Producer) noexcept;
[[nodiscard]] const char* ToString(EHDRSceneColorState State) noexcept;
[[nodiscard]] const char* ToString(EOutputColorPrimaries Primaries) noexcept;
[[nodiscard]] const char* ToString(EOutputTransferFunction Transfer) noexcept;

} // namespace Stoner::Renderer
