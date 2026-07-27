#pragma once

#include "RHI/IRHIGraphicsPipeline.h"

#include <memory>

namespace Stoner::Backend::Vulkan
{

class FVulkanDevice;
class FVulkanNativeContext;
struct FVulkanDeviceOwnerState;

class FVulkanGraphicsPipeline final : public Stoner::RHI::IRHIGraphicsPipeline
{
public:
    [[nodiscard]] const Stoner::RHI::FRHIGraphicsPipelineDesc& GetDesc() const noexcept override;
    [[nodiscard]] Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout> GetPipelineLayout() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIRuntimeObjectMode GetRuntimeMode() const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIPipelineReuseState GetReuseState() const noexcept;
    [[nodiscard]] const char* GetDiagnosticsReason() const noexcept;
    [[nodiscard]] bool HasNativeObject() const noexcept;
    [[nodiscard]] bool HasValidDependencies() const noexcept;
    [[nodiscard]] bool BelongsTo(
        const Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState>& InOwner) const noexcept;

    Stoner::RHI::ERHIResult Invalidate() override;

private:
    friend class FVulkanDevice;

    FVulkanGraphicsPipeline(
        Stoner::RHI::FRHIGraphicsPipelineDesc InDesc,
        Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> InOwner,
        Stoner::Core::TSharedPtr<FVulkanNativeContext> InNativeContext,
        Stoner::Core::uint64 InNativeToken,
        const char* InDiagnosticsReason) noexcept;
    void SetReuseState(
        Stoner::RHI::ERHIPipelineReuseState InReuseState) noexcept;

    Stoner::RHI::FRHIGraphicsPipelineDesc Desc;
    Stoner::Core::TSharedPtr<FVulkanDeviceOwnerState> Owner;
    Stoner::Core::TSharedPtr<FVulkanNativeContext> NativeContext;
    Stoner::Core::uint64 NativeToken = 0;
    const char* DiagnosticsReason = "";
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

} // namespace Stoner::Backend::Vulkan
