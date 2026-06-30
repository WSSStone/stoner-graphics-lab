#include "VulkanRHI/FVulkanComputePipeline.h"

namespace Stoner::Backend::Vulkan
{

FVulkanComputePipeline::FVulkanComputePipeline(Stoner::RHI::FRHIComputePipelineDesc InDesc, const char* InDiagnosticsReason) noexcept
    : Desc(std::move(InDesc))
    , DiagnosticsReason(InDiagnosticsReason ? InDiagnosticsReason : "")
{
}

const Stoner::RHI::FRHIComputePipelineDesc& FVulkanComputePipeline::GetDesc() const noexcept { return Desc; }
Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout> FVulkanComputePipeline::GetPipelineLayout() const noexcept { return Desc.PipelineLayout; }
Stoner::RHI::ERHIResourceLifecycleState FVulkanComputePipeline::GetLifecycleState() const noexcept { return LifecycleState; }
Stoner::RHI::ERHIRuntimeObjectMode FVulkanComputePipeline::GetRuntimeMode() const noexcept { return Desc.RuntimeMode; }
Stoner::RHI::ERHIPipelineReuseState FVulkanComputePipeline::GetReuseState() const noexcept { return Desc.ReuseState; }
const char* FVulkanComputePipeline::GetDiagnosticsReason() const noexcept { return DiagnosticsReason; }

void FVulkanComputePipeline::SetReuseState(Stoner::RHI::ERHIPipelineReuseState InReuseState) noexcept
{
    Desc.ReuseState = InReuseState;
}

Stoner::RHI::ERHIResult FVulkanComputePipeline::Invalidate()
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    Desc.ReuseState = Stoner::RHI::ERHIPipelineReuseState::Invalidated;
    DiagnosticsReason = "compute pipeline invalidated";
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
