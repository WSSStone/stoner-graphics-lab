#include "VulkanRHI/FVulkanComputePipeline.h"

#include "VulkanRHI/FVulkanDeviceOwnerState.h"
#include "VulkanRHI/FVulkanNativeContext.h"

namespace Stoner::Backend::Vulkan
{

FVulkanComputePipeline::FVulkanComputePipeline(
    Stoner::RHI::FRHIComputePipelineDesc InDesc,
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

const Stoner::RHI::FRHIComputePipelineDesc& FVulkanComputePipeline::GetDesc() const noexcept { return Desc; }
Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout> FVulkanComputePipeline::GetPipelineLayout() const noexcept { return Desc.PipelineLayout; }
Stoner::RHI::ERHIResourceLifecycleState FVulkanComputePipeline::GetLifecycleState() const noexcept { return LifecycleState; }
Stoner::RHI::ERHIRuntimeObjectMode FVulkanComputePipeline::GetRuntimeMode() const noexcept { return Desc.RuntimeMode; }
Stoner::RHI::ERHIPipelineReuseState FVulkanComputePipeline::GetReuseState() const noexcept { return Desc.ReuseState; }
const char* FVulkanComputePipeline::GetDiagnosticsReason() const noexcept { return DiagnosticsReason; }
bool FVulkanComputePipeline::HasNativeObject() const noexcept
{
    return LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Valid &&
        Owner && Owner->bActive && NativeContext && NativeToken != 0;
}

bool FVulkanComputePipeline::HasValidDependencies() const noexcept
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

bool FVulkanComputePipeline::BelongsTo(
    const Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState>& InOwner) const noexcept
{
    return LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Valid &&
        Owner && Owner->bActive && InOwner && Owner == InOwner;
}

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
    if (NativeContext && NativeToken != 0)
    {
        NativeContext->DestroyOwnedPipeline(NativeToken);
        NativeToken = 0;
    }
    NativeContext.reset();
    Owner.reset();
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    Desc.ReuseState = Stoner::RHI::ERHIPipelineReuseState::Invalidated;
    DiagnosticsReason = "compute pipeline invalidated";
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
