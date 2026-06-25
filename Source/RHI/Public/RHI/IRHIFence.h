#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIResult.h"

namespace Stoner::RHI
{

enum class ERHIFenceState
{
    Unsignaled,
    Signaled,
    Waited
};

class IRHIFence
{
public:
    virtual ~IRHIFence() = default;

    [[nodiscard]] virtual ERHIFenceState GetState() const noexcept = 0;
    [[nodiscard]] virtual bool IsSignaled() const noexcept = 0;

    virtual ERHIResult Wait(Stoner::Core::uint64 TimeoutMicroseconds = 0) = 0;
    virtual ERHIResult Reset() = 0;
    virtual ERHIResult Signal() = 0;
};

} // namespace Stoner::RHI
