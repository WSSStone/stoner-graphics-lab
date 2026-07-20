#pragma once

namespace Stoner::RHI
{

enum class ERHIRuntimeMode
{
    Deterministic,
    Native,
    NativeHeadless
};

enum class ERHIRuntimeObjectMode
{
    Unknown,
    RealRuntime,
    DeterministicFallback
};

[[nodiscard]] constexpr const char* ToString(ERHIRuntimeMode Mode) noexcept
{
    switch (Mode)
    {
    case ERHIRuntimeMode::Deterministic: return "Deterministic";
    case ERHIRuntimeMode::Native: return "Native";
    case ERHIRuntimeMode::NativeHeadless: return "NativeHeadless";
    }
    return "Unknown";
}

} // namespace Stoner::RHI
