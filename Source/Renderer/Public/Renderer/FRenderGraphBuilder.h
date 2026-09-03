#pragma once

#include "Renderer/FRenderGraphPass.h"

namespace Stoner::Renderer
{

class FRenderGraph;

class FRenderGraphBuilder
{
public:
    explicit FRenderGraphBuilder(FRenderGraph& InGraph);

    [[nodiscard]] FRenderGraphResourceHandle CreateResource(const FRenderGraphResourceDesc& Desc);
    [[nodiscard]] FRenderGraphResourceHandle ImportResource(const FRenderGraphResourceDesc& Desc);
    [[nodiscard]] FRenderGraphResourceHandle CreateTexture(
        Stoner::Core::FString Name,
        Stoner::Core::uint32 Width,
        Stoner::Core::uint32 Height,
        Stoner::RHI::ERHIFormat Format,
        Stoner::RHI::ERHISampleCount SampleCount,
        Stoner::RHI::ERHITextureUsage Usage,
        ERenderGraphColorDomain ColorDomain);
    [[nodiscard]] FRenderGraphPassHandle AddPass(const FRenderGraphPassDesc& Desc);
    [[nodiscard]] ERenderGraphResult AddAccess(FRenderGraphPassHandle Pass, FRenderGraphResourceHandle Resource, ERenderGraphAccessType Access);
    [[nodiscard]] ERenderGraphResult AddDependency(FRenderGraphPassHandle Before, FRenderGraphPassHandle After);
    [[nodiscard]] ERenderGraphResult MarkOutput(FRenderGraphResourceHandle Resource);

private:
    FRenderGraph& Graph;
};

} // namespace Stoner::Renderer
