#pragma once

#include "RHI/RHIMinimal.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanSwapchain final : public Stoner::RHI::IRHISwapchain
{
public:
    explicit FVulkanSwapchain(Stoner::Core::uint32 InFrameCount = 2) noexcept;

    [[nodiscard]] Stoner::RHI::ERHISwapchainState GetState() const noexcept override;
    [[nodiscard]] Stoner::Core::uint32 GetFrameCount() const noexcept override;
    [[nodiscard]] Stoner::Core::uint32 GetCurrentFrameIndex() const noexcept override;
    [[nodiscard]] Stoner::Core::uint64 GetGeneration() const noexcept override { return Generation; }

    Stoner::RHI::ERHIResult AcquireNextFrame(Stoner::Core::uint32& OutFrameIndex) override;
    Stoner::RHI::ERHIResult Present(Stoner::Core::uint32 FrameIndex) override;
    Stoner::RHI::ERHIResult Recreate(Stoner::Core::uint32 NewFrameCount) noexcept;
    void SimulateResizeRequired() noexcept;
    void SetUnavailable() noexcept;
    void Invalidate() noexcept;

private:
    Stoner::Core::uint32 FrameCount = 2;
    Stoner::Core::uint32 CurrentFrameIndex = 0;
    Stoner::RHI::ERHISwapchainState State = Stoner::RHI::ERHISwapchainState::Ready;
    Stoner::Core::uint64 Generation = 1;
    Stoner::Core::uint64 AcquiredGeneration = 0;
    bool bValid = true;
};

} // namespace Stoner::Backend::Vulkan
