#pragma once

#include "RHI/IRHIGraphicsPipeline.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanGraphicsPipeline final : public Stoner::RHI::IRHIGraphicsPipeline
{
public:
    FVulkanGraphicsPipeline(Stoner::RHI::FRHIGraphicsPipelineDesc InDesc, const char* InDiagnosticsReason) noexcept;

    [[nodiscard]] const Stoner::RHI::FRHIGraphicsPipelineDesc& GetDesc() const noexcept override;
    [[nodiscard]] Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout> GetPipelineLayout() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIRuntimeObjectMode GetRuntimeMode() const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIPipelineReuseState GetReuseState() const noexcept;
    [[nodiscard]] const char* GetDiagnosticsReason() const noexcept;

    void SetReuseState(Stoner::RHI::ERHIPipelineReuseState InReuseState) noexcept;
    Stoner::RHI::ERHIResult Invalidate() override;

private:
    Stoner::RHI::FRHIGraphicsPipelineDesc Desc;
    const char* DiagnosticsReason = "";
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

} // namespace Stoner::Backend::Vulkan
