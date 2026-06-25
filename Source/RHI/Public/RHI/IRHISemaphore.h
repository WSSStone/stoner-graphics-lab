#pragma once

#include "RHI/ERHIResult.h"

namespace Stoner::RHI
{

enum class ERHISemaphoreState
{
    Unsignaled,
    Signaled,
    Consumed
};

class IRHISemaphore
{
public:
    virtual ~IRHISemaphore() = default;

    [[nodiscard]] virtual ERHISemaphoreState GetState() const noexcept = 0;
    [[nodiscard]] virtual bool IsSignaled() const noexcept = 0;

    virtual ERHIResult Signal() = 0;
    virtual ERHIResult Consume() = 0;
    virtual ERHIResult Reset() = 0;
};

} // namespace Stoner::RHI
