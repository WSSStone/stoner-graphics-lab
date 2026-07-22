#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIResult.h"
#include "RHI/IRHISemaphore.h"

namespace Stoner::RHI
{

class IRHITexture;

enum class ERHISwapchainState
{
    Ready,
    Acquired,
    ResizeRequired,
    Unavailable
};

class IRHISwapchain
{
public:
    virtual ~IRHISwapchain() = default;

    [[nodiscard]] virtual ERHISwapchainState GetState() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::uint32 GetFrameCount() const noexcept = 0;
    [[nodiscard]] virtual Stoner::Core::uint32 GetCurrentFrameIndex() const noexcept = 0;

    virtual ERHIResult AcquireNextFrame(Stoner::Core::uint32& OutFrameIndex) = 0;
    virtual ERHIResult Present(Stoner::Core::uint32 FrameIndex) = 0;
    [[nodiscard]] virtual Stoner::Core::TSharedPtr<IRHITexture> GetImage(Stoner::Core::uint32) const { return nullptr; }
    [[nodiscard]] virtual Stoner::Core::uint64 GetGeneration() const noexcept { return 1; }
    virtual ERHIResult AcquireNextFrame(Stoner::Core::uint32& OutFrameIndex, const Stoner::Core::TSharedPtr<IRHISemaphore>& SignalSemaphore)
    {
        const ERHIResult Result = AcquireNextFrame(OutFrameIndex);
        if (Result == ERHIResult::Success && SignalSemaphore)
        {
            return SignalSemaphore->Signal();
        }
        return Result;
    }
    virtual ERHIResult Present(Stoner::Core::uint32 FrameIndex, const Stoner::Core::TSharedPtr<IRHISemaphore>& WaitSemaphore)
    {
        if (WaitSemaphore && WaitSemaphore->Consume() != ERHIResult::Success)
        {
            return ERHIResult::InvalidState;
        }
        return Present(FrameIndex);
    }
};

} // namespace Stoner::RHI
