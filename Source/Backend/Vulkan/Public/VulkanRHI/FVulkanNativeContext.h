#pragma once

#include "RHI/RHIMinimal.h"
#include "Core/FPlatformWindow.h"

#include <memory>

namespace Stoner::Backend::Vulkan
{

class FVulkanNativeContext
{
public:
    FVulkanNativeContext();
    ~FVulkanNativeContext();
    FVulkanNativeContext(const FVulkanNativeContext&) = delete;
    FVulkanNativeContext& operator=(const FVulkanNativeContext&) = delete;

    [[nodiscard]] Stoner::RHI::ERHIResult Initialize(Stoner::RHI::ERHIRuntimeMode Mode,
        const Stoner::Core::FPlatformWindow& PlatformWindow = {});
    [[nodiscard]] Stoner::RHI::ERHIResult ExecuteOffscreenTriangle(
        const Stoner::Core::FString& VertexShaderPath,
        const Stoner::Core::FString& FragmentShaderPath);
    [[nodiscard]] Stoner::RHI::ERHIResult PrepareVisibleTriangle(
        const Stoner::Core::FString& VertexShaderPath,
        const Stoner::Core::FString& FragmentShaderPath,
        Stoner::Core::uint32 Width,
        Stoner::Core::uint32 Height);
    [[nodiscard]] Stoner::RHI::ERHIResult DrawVisibleFrame();
    [[nodiscard]] Stoner::RHI::ERHIResult RecreateVisiblePresentation(
        Stoner::Core::uint32 Width,
        Stoner::Core::uint32 Height);
    [[nodiscard]] Stoner::RHI::ERHIResult Shutdown();
    [[nodiscard]] const Stoner::RHI::FRHIRuntimeSnapshot& GetSnapshot() const noexcept;
    [[nodiscard]] bool IsAvailable() const noexcept;

private:
    struct FImpl;
    std::unique_ptr<FImpl> Impl;
};

} // namespace Stoner::Backend::Vulkan
