#pragma once

#include <type_traits>

namespace Stoner::RHI
{

enum class ERHIBufferUsage : unsigned int
{
    None = 0,
    Vertex = 1u << 0,
    Index = 1u << 1,
    Uniform = 1u << 2,
    Storage = 1u << 3,
    CopySource = 1u << 4,
    CopyDestination = 1u << 5,
    Indirect = 1u << 6,
    ReservedPresent = 1u << 7
};

enum class ERHITextureUsage : unsigned int
{
    None = 0,
    Sampled = 1u << 0,
    Storage = 1u << 1,
    ColorAttachment = 1u << 2,
    DepthStencilAttachment = 1u << 3,
    CopySource = 1u << 4,
    CopyDestination = 1u << 5,
    Present = 1u << 6,
    Vertex = 1u << 7
};

template <typename TEnum>
[[nodiscard]] constexpr auto RHIToUnderlying(TEnum Value) noexcept
{
    return static_cast<std::underlying_type_t<TEnum>>(Value);
}

[[nodiscard]] constexpr ERHIBufferUsage operator|(ERHIBufferUsage Left, ERHIBufferUsage Right) noexcept
{
    return static_cast<ERHIBufferUsage>(RHIToUnderlying(Left) | RHIToUnderlying(Right));
}

[[nodiscard]] constexpr ERHIBufferUsage operator&(ERHIBufferUsage Left, ERHIBufferUsage Right) noexcept
{
    return static_cast<ERHIBufferUsage>(RHIToUnderlying(Left) & RHIToUnderlying(Right));
}

constexpr ERHIBufferUsage& operator|=(ERHIBufferUsage& Left, ERHIBufferUsage Right) noexcept
{
    Left = Left | Right;
    return Left;
}

[[nodiscard]] constexpr bool HasRHIFlag(ERHIBufferUsage Value, ERHIBufferUsage Flag) noexcept
{
    return (Value & Flag) != ERHIBufferUsage::None;
}

[[nodiscard]] constexpr ERHITextureUsage operator|(ERHITextureUsage Left, ERHITextureUsage Right) noexcept
{
    return static_cast<ERHITextureUsage>(RHIToUnderlying(Left) | RHIToUnderlying(Right));
}

[[nodiscard]] constexpr ERHITextureUsage operator&(ERHITextureUsage Left, ERHITextureUsage Right) noexcept
{
    return static_cast<ERHITextureUsage>(RHIToUnderlying(Left) & RHIToUnderlying(Right));
}

constexpr ERHITextureUsage& operator|=(ERHITextureUsage& Left, ERHITextureUsage Right) noexcept
{
    Left = Left | Right;
    return Left;
}

[[nodiscard]] constexpr bool HasRHIFlag(ERHITextureUsage Value, ERHITextureUsage Flag) noexcept
{
    return (Value & Flag) != ERHITextureUsage::None;
}

template <typename TEnum>
[[nodiscard]] constexpr bool HasOnlyRHIFlags(TEnum Value, TEnum ValidMask) noexcept
{
    return (RHIToUnderlying(Value) & ~RHIToUnderlying(ValidMask)) == 0;
}

inline constexpr ERHIBufferUsage RHIBufferUsageValidMask =
    ERHIBufferUsage::Vertex |
    ERHIBufferUsage::Index |
    ERHIBufferUsage::Uniform |
    ERHIBufferUsage::Storage |
    ERHIBufferUsage::CopySource |
    ERHIBufferUsage::CopyDestination |
    ERHIBufferUsage::Indirect;

inline constexpr ERHITextureUsage RHITextureUsageValidMask =
    ERHITextureUsage::Sampled |
    ERHITextureUsage::Storage |
    ERHITextureUsage::ColorAttachment |
    ERHITextureUsage::DepthStencilAttachment |
    ERHITextureUsage::CopySource |
    ERHITextureUsage::CopyDestination |
    ERHITextureUsage::Present;

} // namespace Stoner::RHI
