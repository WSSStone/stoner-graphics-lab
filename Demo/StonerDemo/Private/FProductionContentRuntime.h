#pragma once

#include "FProductionContentComposition.h"
#include "FProductionContentDeferredExecution.h"
#include "FProductionContentSession.h"
#include "FStonerDemoApplication.h"
#include "Renderer/RendererMinimal.h"

namespace Stoner::Demo
{

class FStonerDemoApplication::FProductionContentRuntime
{
public:
    FProductionContentSession Session;
    FProductionContentLoadedClosure LoadedClosure;
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot>
        DeferredRenderSnapshot;
    Core::TSharedPtr<const Renderer::FStaticModelRenderSnapshot>
        ForwardRenderSnapshot;
    Renderer::FStaticModelRealizationInspection DeferredRealizationInspection;
    Renderer::FStaticModelRealizationInspection ForwardRealizationInspection;
    FProductionContentComposition Composition;
    FProductionContentDeferredExecutionResources DeferredResources;
    Renderer::FForwardFramePlan ForwardPlan;
    Renderer::FForwardFrameExecutionBindings ForwardBindings;
};

[[nodiscard]] RHI::ERHIResult ReadProductionBuffer(
    EDemoGraphicsBackend Backend,
    const Core::TSharedPtr<RHI::IRHIDevice>& Device,
    const Core::TSharedPtr<RHI::IRHIBuffer>& Buffer,
    Core::uint64 ByteCount,
    Core::TArray<Core::uint8>& OutBytes);

} // namespace Stoner::Demo
