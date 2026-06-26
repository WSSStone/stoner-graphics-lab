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

} // namespace Stoner::RHI
