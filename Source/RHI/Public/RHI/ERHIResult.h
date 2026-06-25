#pragma once

namespace Stoner::RHI
{

enum class ERHIResult
{
    Success,
    InvalidState,
    Unsupported,
    Timeout,
    NotReady,
    ResizeRequired,
    Unavailable,
    Failed
};

[[nodiscard]] inline bool RHISucceeded(ERHIResult Result) noexcept
{
    return Result == ERHIResult::Success;
}

[[nodiscard]] inline bool RHIFailed(ERHIResult Result) noexcept
{
    return !RHISucceeded(Result);
}

} // namespace Stoner::RHI
