#include "VulkanRHI/FVulkanGraphicsPipeline.h"

#include "VulkanRHI/FVulkanDeviceOwnerState.h"
#include "VulkanRHI/FVulkanNativeContext.h"

namespace Stoner::Backend::Vulkan
{

FVulkanGraphicsPipeline::FVulkanGraphicsPipeline(
    Stoner::RHI::FRHIGraphicsPipelineDesc InDesc,
    Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> InOwner,
    Stoner::Core::TSharedPtr<FVulkanNativeContext> InNativeContext,
    Stoner::Core::uint64 InNativeToken,
    const char* InDiagnosticsReason) noexcept
    : Desc(std::move(InDesc))
    , Owner(std::move(InOwner))
    , NativeContext(std::move(InNativeContext))
    , NativeToken(InNativeToken)
    , DiagnosticsReason(InDiagnosticsReason ? InDiagnosticsReason : "")
{
}

const Stoner::RHI::FRHIGraphicsPipelineDesc& FVulkanGraphicsPipeline::GetDesc() const noexcept { return Desc; }
Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout> FVulkanGraphicsPipeline::GetPipelineLayout() const noexcept { return Desc.PipelineLayout; }
Stoner::RHI::ERHIResourceLifecycleState FVulkanGraphicsPipeline::GetLifecycleState() const noexcept { return LifecycleState; }
Stoner::RHI::ERHIRuntimeObjectMode FVulkanGraphicsPipeline::GetRuntimeMode() const noexcept { return Desc.RuntimeMode; }
Stoner::RHI::ERHIPipelineReuseState FVulkanGraphicsPipeline::GetReuseState() const noexcept { return Desc.ReuseState; }
const char* FVulkanGraphicsPipeline::GetDiagnosticsReason() const noexcept { return DiagnosticsReason; }
bool FVulkanGraphicsPipeline::HasNativeObject() const noexcept
{
    return LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Valid &&
        Owner && Owner->bActive && NativeContext && NativeToken != 0;
}

bool FVulkanGraphicsPipeline::HasValidDependencies() const noexcept
{
    if (LifecycleState != Stoner::RHI::ERHIResourceLifecycleState::Valid ||
        !Owner || !Owner->bActive ||
        !Desc.PipelineLayout ||
        Desc.PipelineLayout->GetLifecycleState() !=
            Stoner::RHI::ERHIResourceLifecycleState::Valid)
    {
        return false;
    }
    for (const auto& Shader : Desc.ShaderModules)
    {
        if (!Shader || Shader->GetLifecycleState() !=
            Stoner::RHI::ERHIResourceLifecycleState::Valid)
        {
            return false;
        }
    }
    return true;
}

bool FVulkanGraphicsPipeline::BelongsTo(
    const Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState>& InOwner) const noexcept
{
    return LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Valid &&
        Owner && Owner->bActive && InOwner && Owner == InOwner;
}

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
    if (NativeContext && NativeToken != 0)
    {
        NativeContext->DestroyOwnedPipeline(NativeToken);
        NativeToken = 0;
    }
    NativeContext.reset();
    Owner.reset();
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    Desc.ReuseState = Stoner::RHI::ERHIPipelineReuseState::Invalidated;
    DiagnosticsReason = "graphics pipeline invalidated";
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
