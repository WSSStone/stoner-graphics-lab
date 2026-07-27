#pragma once

#include "RHI/IRHIShaderModule.h"

#include <memory>

namespace Stoner::Backend::Vulkan
{

class FVulkanDevice;
class FVulkanNativeContext;
struct FVulkanDeviceOwnerState;

class FVulkanShaderModule final : public Stoner::RHI::IRHIShaderModule
{
public:
    [[nodiscard]] const Stoner::RHI::FRHIShaderModuleDesc& GetDesc() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIShaderStage GetStage() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept override;
    [[nodiscard]] const Stoner::RHI::FRHIShaderInterfaceMetadata& GetInterfaceMetadata() const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIRuntimeObjectMode GetRuntimeMode() const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIShaderBytecodeValidationMode GetValidationMode() const noexcept;
    [[nodiscard]] const char* GetDiagnosticsReason() const noexcept;
    [[nodiscard]] bool HasNativeObject() const noexcept;
    [[nodiscard]] bool BelongsTo(
        const Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState>& InOwner) const noexcept;

    Stoner::RHI::ERHIResult Invalidate() override;

private:
    friend class FVulkanDevice;

    FVulkanShaderModule(
        Stoner::RHI::FRHIShaderModuleDesc InDesc,
        Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> InOwner,
        Stoner::Core::TSharedPtr<FVulkanNativeContext> InNativeContext,
        Stoner::Core::uint64 InNativeToken,
        const char* InDiagnosticsReason) noexcept;

    Stoner::RHI::FRHIShaderModuleDesc Desc;
    Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> Owner;
    Stoner::Core::TSharedPtr<FVulkanNativeContext> NativeContext;
    Stoner::Core::uint64 NativeToken = 0;
    const char* DiagnosticsReason = "";
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

} // namespace Stoner::Backend::Vulkan
