#include "VulkanRHI/FVulkanShaderModule.h"

namespace Stoner::Backend::Vulkan
{

FVulkanShaderModule::FVulkanShaderModule(Stoner::RHI::FRHIShaderModuleDesc InDesc, const char* InDiagnosticsReason) noexcept
    : Desc(std::move(InDesc))
    , DiagnosticsReason(InDiagnosticsReason ? InDiagnosticsReason : "")
{
}

const Stoner::RHI::FRHIShaderModuleDesc& FVulkanShaderModule::GetDesc() const noexcept { return Desc; }
Stoner::RHI::ERHIShaderStage FVulkanShaderModule::GetStage() const noexcept { return Desc.Stage; }
Stoner::RHI::ERHIResourceLifecycleState FVulkanShaderModule::GetLifecycleState() const noexcept { return LifecycleState; }
const Stoner::RHI::FRHIShaderInterfaceMetadata& FVulkanShaderModule::GetInterfaceMetadata() const noexcept { return Desc.InterfaceMetadata; }
Stoner::RHI::ERHIRuntimeObjectMode FVulkanShaderModule::GetRuntimeMode() const noexcept { return Desc.RuntimeMode; }
Stoner::RHI::ERHIShaderBytecodeValidationMode FVulkanShaderModule::GetValidationMode() const noexcept { return Desc.ValidationMode; }
const char* FVulkanShaderModule::GetDiagnosticsReason() const noexcept { return DiagnosticsReason; }

Stoner::RHI::ERHIResult FVulkanShaderModule::Invalidate()
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    DiagnosticsReason = "shader module invalidated";
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
