#pragma once

#include "RHI/RHIMinimal.h"
#include "VulkanRHI/FVulkanNativeContext.h"
#include "VulkanRHI/FVulkanSurface.h"

namespace Stoner::Backend::Vulkan
{

class FVulkanSwapchain final : public Stoner::RHI::IRHISwapchain
{
public:
    using Stoner::RHI::IRHISwapchain::AcquireNextFrame;
    using Stoner::RHI::IRHISwapchain::Present;

    explicit FVulkanSwapchain(
        Stoner::Core::uint32 InFrameCount = 2,
        Stoner::Core::uint32 InMaxFrameCount = 3);
    FVulkanSwapchain(
        Stoner::Core::TSharedPtr<FVulkanSurface> InSurface,
        const Stoner::RHI::FRHISwapchainDesc& InDesc,
        Stoner::Core::uint32 InMaxFrameCount);

    [[nodiscard]] Stoner::RHI::ERHISwapchainState GetState() const noexcept override;
    [[nodiscard]] Stoner::Core::uint32 GetFrameCount() const noexcept override;
    [[nodiscard]] Stoner::Core::uint32 GetCurrentFrameIndex() const noexcept override;
    [[nodiscard]] Stoner::Core::uint64 GetGeneration() const noexcept override { return Generation; }
    [[nodiscard]] const Stoner::RHI::FRHIResolvedPresentationState&
    GetResolvedPresentationState() const noexcept override
    {
        return ResolvedState;
    }
    [[nodiscard]] Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> GetImage(
        Stoner::Core::uint32 ImageIndex) const override;

    Stoner::RHI::ERHIResult AcquireNextFrame(Stoner::Core::uint32& OutFrameIndex) override;
    Stoner::RHI::ERHIResult AcquireNextFrame(
        Stoner::Core::uint64 FrameToken,
        Stoner::RHI::FRHIPresentationFrame& OutFrame) override;
    Stoner::RHI::ERHIResult AcquireNextFrame(
        Stoner::Core::uint32& OutFrameIndex,
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>& SignalSemaphore) override;
    Stoner::RHI::ERHIResult Present(Stoner::Core::uint32 FrameIndex) override;
    Stoner::RHI::ERHIResult Present(
        const Stoner::RHI::FRHIPresentationFrame& Frame) override;
    Stoner::RHI::ERHIResult Present(
        Stoner::Core::uint32 FrameIndex,
        const Stoner::Core::TSharedPtr<Stoner::RHI::IRHISemaphore>& WaitSemaphore) override;
    Stoner::RHI::ERHIResult Recreate(Stoner::Core::uint32 NewFrameCount);
    Stoner::RHI::ERHIResult Reconfigure(
        const Stoner::RHI::FRHISwapchainDesc& Request) override;
    void SimulateResizeRequired() noexcept;
    void SetUnavailable() noexcept;
    void Invalidate() noexcept;
    [[nodiscard]] const FVulkanNativeFrameBindings*
    GetNativeFrameBindings() const noexcept;

private:
    [[nodiscard]] Stoner::RHI::ERHIResult ValidateAcquire() const noexcept;
    [[nodiscard]] Stoner::RHI::ERHIResult ValidatePresent(
        Stoner::Core::uint32 FrameIndex) const noexcept;
    void CommitAcquire(Stoner::Core::uint32& OutFrameIndex) noexcept;
    void CommitPresent() noexcept;
    [[nodiscard]] bool RebuildImages(Stoner::Core::uint32 NewFrameCount);
    [[nodiscard]] bool BuildImages(
        const Stoner::RHI::FRHISwapchainDesc& ImageDesc,
        Stoner::Core::TArray<Stoner::Core::TSharedPtr<
            Stoner::RHI::IRHITexture>>& OutImages) const;
    void InvalidateImages() noexcept;

    Stoner::Core::uint32 FrameCount = 2;
    Stoner::Core::uint32 MaxFrameCount = 3;
    Stoner::Core::uint32 CurrentFrameIndex = 0;
    Stoner::RHI::ERHISwapchainState State = Stoner::RHI::ERHISwapchainState::Ready;
    Stoner::Core::uint64 Generation = 1;
    Stoner::Core::uint64 AcquiredGeneration = 0;
    Stoner::Core::uint64 AcquiredFrameToken = 0;
    Stoner::Core::uint64 NextFrameToken = 1;
    Stoner::Core::TSharedPtr<FVulkanSurface> Surface;
    Stoner::RHI::FRHISwapchainDesc Desc;
    Stoner::RHI::FRHIResolvedPresentationState ResolvedState;
    FVulkanNativeFrameBindings NativeFrameBindings;
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>> Images;
    bool bValid = true;
};

} // namespace Stoner::Backend::Vulkan
