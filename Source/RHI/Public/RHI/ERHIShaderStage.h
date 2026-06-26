#pragma once

#include <type_traits>

namespace Stoner::RHI
{

enum class ERHIShaderStage
{
    Unknown,
    Vertex,
    Fragment,
    Compute,
    Geometry,
    TessellationControl,
    TessellationEvaluation,
    Mesh,
    Task,
    RayGeneration,
    AnyHit,
    ClosestHit,
    Miss
};

enum class ERHIShaderStageFlags : unsigned int
{
    None = 0,
    Vertex = 1u << 0,
    Fragment = 1u << 1,
    Compute = 1u << 2,
    Geometry = 1u << 3,
    TessellationControl = 1u << 4,
    TessellationEvaluation = 1u << 5,
    Mesh = 1u << 6,
    Task = 1u << 7,
    RayTracing = 1u << 8,
    AllGraphics = Vertex | Fragment | Geometry | TessellationControl | TessellationEvaluation,
    All = AllGraphics | Compute | Mesh | Task | RayTracing
};

[[nodiscard]] constexpr ERHIShaderStageFlags operator|(ERHIShaderStageFlags Left, ERHIShaderStageFlags Right) noexcept
{
    return static_cast<ERHIShaderStageFlags>(
        static_cast<std::underlying_type_t<ERHIShaderStageFlags>>(Left) |
        static_cast<std::underlying_type_t<ERHIShaderStageFlags>>(Right));
}

[[nodiscard]] constexpr ERHIShaderStageFlags operator&(ERHIShaderStageFlags Left, ERHIShaderStageFlags Right) noexcept
{
    return static_cast<ERHIShaderStageFlags>(
        static_cast<std::underlying_type_t<ERHIShaderStageFlags>>(Left) &
        static_cast<std::underlying_type_t<ERHIShaderStageFlags>>(Right));
}

constexpr ERHIShaderStageFlags& operator|=(ERHIShaderStageFlags& Left, ERHIShaderStageFlags Right) noexcept
{
    Left = Left | Right;
    return Left;
}

[[nodiscard]] constexpr bool HasRHIFlag(ERHIShaderStageFlags Value, ERHIShaderStageFlags Flag) noexcept
{
    return (Value & Flag) != ERHIShaderStageFlags::None;
}

} // namespace Stoner::RHI
