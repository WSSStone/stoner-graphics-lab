#include "VulkanRHI/FVulkanShaderModule.h"

#include "VulkanRHI/FVulkanDeviceOwnerState.h"
#include "VulkanRHI/FVulkanNativeContext.h"

namespace Stoner::Backend::Vulkan
{

FVulkanShaderModule::FVulkanShaderModule(
    Stoner::RHI::FRHIShaderModuleDesc InDesc,
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

const Stoner::RHI::FRHIShaderModuleDesc& FVulkanShaderModule::GetDesc() const noexcept { return Desc; }
Stoner::RHI::ERHIShaderStage FVulkanShaderModule::GetStage() const noexcept { return Desc.Stage; }
Stoner::RHI::ERHIResourceLifecycleState FVulkanShaderModule::GetLifecycleState() const noexcept { return LifecycleState; }
const Stoner::RHI::FRHIShaderInterfaceMetadata& FVulkanShaderModule::GetInterfaceMetadata() const noexcept { return Desc.InterfaceMetadata; }
Stoner::RHI::ERHIRuntimeObjectMode FVulkanShaderModule::GetRuntimeMode() const noexcept { return Desc.RuntimeMode; }
Stoner::RHI::ERHIShaderBytecodeValidationMode FVulkanShaderModule::GetValidationMode() const noexcept { return Desc.ValidationMode; }
const char* FVulkanShaderModule::GetDiagnosticsReason() const noexcept { return DiagnosticsReason; }
bool FVulkanShaderModule::HasNativeObject() const noexcept
{
    return LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Valid &&
        Owner && Owner->bActive && NativeContext && NativeToken != 0;
}

bool FVulkanShaderModule::BelongsTo(
    const Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState>& InOwner) const noexcept
{
    return LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Valid &&
        Owner && Owner->bActive && InOwner && Owner == InOwner;
}

Stoner::RHI::ERHIResult FVulkanShaderModule::Invalidate()
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (NativeContext && NativeToken != 0)
    {
        NativeContext->DestroyOwnedShaderModule(NativeToken);
        NativeToken = 0;
    }
    NativeContext.reset();
    Owner.reset();
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    DiagnosticsReason = "shader module invalidated";
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
