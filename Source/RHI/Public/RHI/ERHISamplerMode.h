#pragma once

namespace Stoner::RHI
{

enum class ERHISamplerFilter
{
    Nearest,
    Linear
};

enum class ERHISamplerMipFilter
{
    None,
    Nearest,
    Linear
};

enum class ERHISamplerAddressMode
{
    Repeat,
    MirroredRepeat,
    ClampToEdge,
    ClampToBorder
};

enum class ERHISamplerCompareMode
{
    None,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    Equal,
    NotEqual,
    Always,
    Never
};

[[nodiscard]] constexpr bool IsValidRHISamplerFilter(ERHISamplerFilter Filter) noexcept
{
    switch (Filter)
    {
    case ERHISamplerFilter::Nearest:
    case ERHISamplerFilter::Linear:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr bool IsValidRHISamplerMipFilter(ERHISamplerMipFilter Filter) noexcept
{
    switch (Filter)
    {
    case ERHISamplerMipFilter::None:
    case ERHISamplerMipFilter::Nearest:
    case ERHISamplerMipFilter::Linear:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr bool IsValidRHISamplerAddressMode(ERHISamplerAddressMode Mode) noexcept
{
    switch (Mode)
    {
    case ERHISamplerAddressMode::Repeat:
    case ERHISamplerAddressMode::MirroredRepeat:
    case ERHISamplerAddressMode::ClampToEdge:
    case ERHISamplerAddressMode::ClampToBorder:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr bool IsValidRHISamplerCompareMode(ERHISamplerCompareMode Mode) noexcept
{
    switch (Mode)
    {
    case ERHISamplerCompareMode::None:
    case ERHISamplerCompareMode::Less:
    case ERHISamplerCompareMode::LessEqual:
    case ERHISamplerCompareMode::Greater:
    case ERHISamplerCompareMode::GreaterEqual:
    case ERHISamplerCompareMode::Equal:
    case ERHISamplerCompareMode::NotEqual:
    case ERHISamplerCompareMode::Always:
    case ERHISamplerCompareMode::Never:
        return true;
    }
    return false;
}

} // namespace Stoner::RHI
