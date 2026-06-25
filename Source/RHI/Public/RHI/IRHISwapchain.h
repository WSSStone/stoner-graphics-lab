#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIResult.h"

namespace Stoner::RHI
{

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
};

} // namespace Stoner::RHI
