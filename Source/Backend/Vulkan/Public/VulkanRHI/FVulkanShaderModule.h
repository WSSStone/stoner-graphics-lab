#pragma once

#include "RHI/IRHIShaderModule.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanShaderModule final : public Stoner::RHI::IRHIShaderModule
{
public:
    FVulkanShaderModule(Stoner::RHI::FRHIShaderModuleDesc InDesc, const char* InDiagnosticsReason) noexcept;

    [[nodiscard]] const Stoner::RHI::FRHIShaderModuleDesc& GetDesc() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIShaderStage GetStage() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept override;
    [[nodiscard]] const Stoner::RHI::FRHIShaderInterfaceMetadata& GetInterfaceMetadata() const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIRuntimeObjectMode GetRuntimeMode() const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIShaderBytecodeValidationMode GetValidationMode() const noexcept;
    [[nodiscard]] const char* GetDiagnosticsReason() const noexcept;

    Stoner::RHI::ERHIResult Invalidate() override;

private:
    Stoner::RHI::FRHIShaderModuleDesc Desc;
    const char* DiagnosticsReason = "";
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

} // namespace Stoner::Backend::Vulkan
