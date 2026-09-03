#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/IRHIPresentationSurface.h"

#include <memory>

namespace Stoner::Backend::Vulkan
{

struct FVulkanPresentationOwnerState
{
    bool bActive = false;
};

class FVulkanNativeContext;

class FVulkanSurface final : public Stoner::RHI::IRHIPresentationSurface
{
public:
    FVulkanSurface();

    [[nodiscard]] static Stoner::RHI::ERHIResult Create(
        const Stoner::RHI::FRHIPresentationSurfaceDesc& Desc,
        const std::shared_ptr<FVulkanPresentationOwnerState>& Owner,
        FVulkanSurface& OutSurface,
        Stoner::Core::TSharedPtr<FVulkanNativeContext> NativeContext = {});

    [[nodiscard]] const Stoner::RHI::FRHIPresentationSurfaceDesc& GetDesc() const noexcept override;
    [[nodiscard]] bool IsValid() const noexcept override;
    [[nodiscard]] void* GetNativeHandle() const noexcept;
    [[nodiscard]] const char* GetDiagnosticReason() const noexcept;
    [[nodiscard]] bool BelongsTo(const std::shared_ptr<FVulkanPresentationOwnerState>& Owner) const noexcept;
    [[nodiscard]] Stoner::Core::TSharedPtr<FVulkanNativeContext>
    GetNativeContext() const noexcept;
    Stoner::RHI::ERHIResult QueryCapabilities(
        Stoner::RHI::FRHIPresentationCapabilities& OutCapabilities)
        const override;
    [[nodiscard]] Stoner::Core::uint64 GetCapabilityGeneration()
        const noexcept override;
    Stoner::RHI::ERHIResult NotifyPresentationEnvironmentChanged() override;
    Stoner::RHI::ERHIResult UpdateCapabilities(
        const Stoner::RHI::FRHIPresentationCapabilities& Capabilities);
    Stoner::RHI::ERHIResult Invalidate() override;

private:
    struct FState;
    std::shared_ptr<FState> State;
};

} // namespace Stoner::Backend::Vulkan
