#pragma once

#include "FMetalDeviceOwnerState.h"
#include "Core/FPlatformWindow.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHIPresentationCapabilities.h"
#include "RHI/FRHIPresentationSurfaceDesc.h"
#include "RHI/FRHIResolvedPresentationState.h"
#include "RHI/FRHISwapchainDesc.h"
#include "RHI/IRHITexture.h"

#include <memory>

namespace Stoner::Backend::Metal::Private
{

class FMetalSemaphore;
class FMetalTexture;

struct FMetalPresentationLayerPolicy
{
    Core::uint64 PixelFormat = 0;
    RHI::ERHIPresentationColorSpace ColorSpace =
        RHI::ERHIPresentationColorSpace::Unknown;
    RHI::ERHIPresentationDisplayAdaptation DisplayAdaptation =
        RHI::ERHIPresentationDisplayAdaptation::None;
    bool bWantsExtendedDynamicRangeContent = false;
    bool bHasEDRMetadata = false;

    [[nodiscard]] bool IsValid() const noexcept
    {
        return PixelFormat != 0 &&
            RHI::IsValidPresentationColorSpace(ColorSpace);
    }
};

struct FMetalPresentationLayerSnapshot
{
    FMetalPresentationLayerPolicy Policy;
    Core::uint64 ModeGeneration = 0;
    Core::uint32 Width = 0;
    Core::uint32 Height = 0;
    float NativeReferenceWhiteNits = 0.0f;
    float CurrentHeadroom = 1.0f;
    float PotentialHeadroom = 1.0f;
    Core::FString MetadataDigest;
    Core::uint64 LastAcquiredFrameToken = 0;
    Core::uint64 LastSubmittedFrameToken = 0;
    Core::uint64 LastPresentedFrameToken = 0;
};

[[nodiscard]] FMetalPresentationLayerPolicy ResolveMetalPresentationLayerPolicy(
    const RHI::FRHISwapchainDesc& Request) noexcept;

class FMetalPresentationContext final
    : public std::enable_shared_from_this<FMetalPresentationContext>
{
public:
    FMetalPresentationContext(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        void* NativeDevice,
        void* NativeQueue);
    ~FMetalPresentationContext();

    [[nodiscard]] RHI::ERHIResult Attach(
        const Core::FPlatformWindow& Window,
        RHI::ERHIFormat Format,
        Core::uint32 MaximumDrawableCount,
        bool bVSync) noexcept;
    [[nodiscard]] RHI::ERHIResult Attach(
        const Core::FPlatformWindow& Window,
        const RHI::FRHISwapchainDesc& Request) noexcept;
    [[nodiscard]] RHI::ERHIResult Reconfigure(
        const Core::FPlatformWindow& Window,
        const RHI::FRHISwapchainDesc& Request) noexcept;
    [[nodiscard]] RHI::ERHIResult QueryCapabilities(
        const RHI::FRHIPresentationSurfaceDesc& Surface,
        Core::uint64 CapabilityGeneration,
        RHI::FRHIPresentationCapabilities& OutCapabilities) const noexcept;
    [[nodiscard]] bool IsAttached() const noexcept;
    [[nodiscard]] RHI::ERHIResult Acquire(
        Core::uint32 FrameSlot,
        Core::uint64 FrameToken,
        Core::TSharedPtr<RHI::IRHITexture>& OutTexture,
        Core::uint64& OutGeneration) noexcept;
    [[nodiscard]] RHI::ERHIResult Present(
        Core::uint32 FrameSlot,
        Core::uint64 Generation,
        Core::uint64 FrameToken,
        const Core::TSharedPtr<FMetalSemaphore>& WaitSemaphore) noexcept;
    void CancelAcquire(
        Core::uint32 FrameSlot,
        Core::uint64 Generation) noexcept;
    [[nodiscard]] RHI::ERHIResult Shutdown() noexcept;
    [[nodiscard]] Core::uint64 GetGeneration() const noexcept;
    [[nodiscard]] RHI::FRHIResolvedPresentationState
    GetResolvedPresentationState() const noexcept;
    [[nodiscard]] FMetalPresentationLayerSnapshot
    GetLayerSnapshot() const noexcept;

private:
    struct FImpl;
    Core::TUniquePtr<FImpl> Impl_;
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner_;
    void* NativeDevice_ = nullptr;
    void* NativeQueue_ = nullptr;
};

} // namespace Stoner::Backend::Metal::Private
