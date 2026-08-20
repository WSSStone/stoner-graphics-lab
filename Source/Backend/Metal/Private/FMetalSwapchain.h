#pragma once

#include "FMetalNativeObject.h"
#include "FMetalPresentationSurface.h"
#include "RHI/FRHISwapchainDesc.h"
#include "RHI/IRHISwapchain.h"

#include <mutex>

namespace Stoner::Backend::Metal::Private
{

class FMetalSwapchain final
    : public RHI::IRHISwapchain,
      public FMetalNativeObject
{
public:
    FMetalSwapchain(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        Core::TSharedPtr<FMetalPresentationSurface> Surface,
        RHI::FRHISwapchainDesc Desc) noexcept;
    ~FMetalSwapchain() override;

    [[nodiscard]] RHI::ERHISwapchainState GetState() const noexcept override;
    [[nodiscard]] Core::uint32 GetFrameCount() const noexcept override;
    [[nodiscard]] Core::uint32 GetCurrentFrameIndex() const noexcept override;
    [[nodiscard]] Core::TSharedPtr<RHI::IRHITexture> GetImage(
        Core::uint32 FrameIndex) const override;
    [[nodiscard]] Core::uint64 GetGeneration() const noexcept override;
    RHI::ERHIResult AcquireNextFrame(Core::uint32& OutFrameIndex) override;
    RHI::ERHIResult AcquireNextFrame(
        Core::uint32& OutFrameIndex,
        const Core::TSharedPtr<RHI::IRHISemaphore>& SignalSemaphore) override;
    RHI::ERHIResult Present(Core::uint32 FrameIndex) override;
    RHI::ERHIResult Present(
        Core::uint32 FrameIndex,
        const Core::TSharedPtr<RHI::IRHISemaphore>& WaitSemaphore) override;

private:
    mutable std::mutex Mutex_;
    Core::TSharedPtr<FMetalPresentationSurface> Surface_;
    RHI::FRHISwapchainDesc Desc_;
    RHI::ERHISwapchainState State_ = RHI::ERHISwapchainState::Ready;
    Core::uint32 CurrentFrameIndex_ = 0;
    Core::uint64 AcquiredGeneration_ = 0;
    Core::TArray<Core::TSharedPtr<RHI::IRHITexture>> Images_;
};

} // namespace Stoner::Backend::Metal::Private
