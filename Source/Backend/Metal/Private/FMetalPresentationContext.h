#pragma once

#include "FMetalDeviceOwnerState.h"
#include "Core/FPlatformWindow.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIResult.h"
#include "RHI/IRHITexture.h"

#include <memory>

namespace Stoner::Backend::Metal::Private
{

class FMetalSemaphore;
class FMetalTexture;

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
    [[nodiscard]] bool IsAttached() const noexcept;
    [[nodiscard]] RHI::ERHIResult Acquire(
        Core::uint32 FrameSlot,
        Core::TSharedPtr<RHI::IRHITexture>& OutTexture,
        Core::uint64& OutGeneration) noexcept;
    [[nodiscard]] RHI::ERHIResult Present(
        Core::uint32 FrameSlot,
        Core::uint64 Generation,
        const Core::TSharedPtr<FMetalSemaphore>& WaitSemaphore) noexcept;
    void CancelAcquire(
        Core::uint32 FrameSlot,
        Core::uint64 Generation) noexcept;
    [[nodiscard]] RHI::ERHIResult Shutdown() noexcept;
    [[nodiscard]] Core::uint64 GetGeneration() const noexcept;

private:
    struct FImpl;
    Core::TUniquePtr<FImpl> Impl_;
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner_;
    void* NativeDevice_ = nullptr;
    void* NativeQueue_ = nullptr;
};

} // namespace Stoner::Backend::Metal::Private
