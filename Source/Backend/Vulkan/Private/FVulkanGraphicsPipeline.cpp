#include "VulkanRHI/FVulkanGraphicsPipeline.h"

namespace Stoner::Backend::Vulkan
{

FVulkanGraphicsPipeline::FVulkanGraphicsPipeline(Stoner::RHI::FRHIGraphicsPipelineDesc InDesc, const char* InDiagnosticsReason) noexcept
    : Desc(std::move(InDesc))
    , DiagnosticsReason(InDiagnosticsReason ? InDiagnosticsReason : "")
{
}

const Stoner::RHI::FRHIGraphicsPipelineDesc& FVulkanGraphicsPipeline::GetDesc() const noexcept { return Desc; }
Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout> FVulkanGraphicsPipeline::GetPipelineLayout() const noexcept { return Desc.PipelineLayout; }
Stoner::RHI::ERHIResourceLifecycleState FVulkanGraphicsPipeline::GetLifecycleState() const noexcept { return LifecycleState; }
Stoner::RHI::ERHIRuntimeObjectMode FVulkanGraphicsPipeline::GetRuntimeMode() const noexcept { return Desc.RuntimeMode; }
Stoner::RHI::ERHIPipelineReuseState FVulkanGraphicsPipeline::GetReuseState() const noexcept { return Desc.ReuseState; }
const char* FVulkanGraphicsPipeline::GetDiagnosticsReason() const noexcept { return DiagnosticsReason; }

void FVulkanGraphicsPipeline::SetReuseState(Stoner::RHI::ERHIPipelineReuseState InReuseState) noexcept
{
    Desc.ReuseState = InReuseState;
}

Stoner::RHI::ERHIResult FVulkanGraphicsPipeline::Invalidate()
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    Desc.ReuseState = Stoner::RHI::ERHIPipelineReuseState::Invalidated;
    DiagnosticsReason = "graphics pipeline invalidated";
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
