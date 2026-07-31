#pragma once

namespace Stoner::RHI
{

enum class ERHIResourceLifecycleState
{
    Valid,
    Invalidated
};

enum class ERHIPrimitiveTopology
{
    TriangleList,
    TriangleStrip,
    LineList,
    LineStrip,
    PointList
};

enum class ERHICullMode
{
    None,
    Front,
    Back
};

enum class ERHIFrontFace
{
    CounterClockwise,
    Clockwise
};

// Draw submission resolves negative-determinant transform parity once before
// creating or selecting its backend pipeline state.
[[nodiscard]] constexpr ERHIFrontFace ResolveRHIFrontFaceForTransform(
    ERHIFrontFace FrontFace, bool bNegativeDeterminant) noexcept
{
    if (!bNegativeDeterminant)
    {
        return FrontFace;
    }
    return FrontFace == ERHIFrontFace::Clockwise
        ? ERHIFrontFace::CounterClockwise
        : ERHIFrontFace::Clockwise;
}

enum class ERHIBlendFactor
{
    Zero,
    One,
    SourceAlpha,
    OneMinusSourceAlpha
};

enum class ERHIBlendOp
{
    Add,
    Subtract,
    ReverseSubtract
};

enum class ERHICompareOp
{
    Never,
    Less,
    LessEqual,
    Equal,
    GreaterEqual,
    Greater,
    NotEqual,
    Always
};

enum class ERHIAttachmentRole
{
    Color,
    DepthStencil
};

enum class ERHIAttachmentLoadOp
{
    Load,
    Clear,
    DontCare
};

enum class ERHIAttachmentStoreOp
{
    Store,
    DontCare
};

enum class ERHISampleCount
{
    One = 1,
    Two = 2,
    Four = 4,
    Eight = 8
};

[[nodiscard]] constexpr bool IsValidRHIPrimitiveTopology(ERHIPrimitiveTopology Value) noexcept
{
    switch (Value)
    {
    case ERHIPrimitiveTopology::TriangleList:
    case ERHIPrimitiveTopology::TriangleStrip:
    case ERHIPrimitiveTopology::LineList:
    case ERHIPrimitiveTopology::LineStrip:
    case ERHIPrimitiveTopology::PointList:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr bool IsValidRHICullMode(ERHICullMode Value) noexcept
{
    return Value == ERHICullMode::None || Value == ERHICullMode::Front || Value == ERHICullMode::Back;
}

[[nodiscard]] constexpr bool IsValidRHIFrontFace(ERHIFrontFace Value) noexcept
{
    return Value == ERHIFrontFace::CounterClockwise || Value == ERHIFrontFace::Clockwise;
}

[[nodiscard]] constexpr bool IsValidRHIBlendFactor(ERHIBlendFactor Value) noexcept
{
    switch (Value)
    {
    case ERHIBlendFactor::Zero:
    case ERHIBlendFactor::One:
    case ERHIBlendFactor::SourceAlpha:
    case ERHIBlendFactor::OneMinusSourceAlpha:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr bool IsValidRHIBlendOp(ERHIBlendOp Value) noexcept
{
    return Value == ERHIBlendOp::Add || Value == ERHIBlendOp::Subtract || Value == ERHIBlendOp::ReverseSubtract;
}

[[nodiscard]] constexpr bool IsValidRHICompareOp(ERHICompareOp Value) noexcept
{
    switch (Value)
    {
    case ERHICompareOp::Never:
    case ERHICompareOp::Less:
    case ERHICompareOp::LessEqual:
    case ERHICompareOp::Equal:
    case ERHICompareOp::GreaterEqual:
    case ERHICompareOp::Greater:
    case ERHICompareOp::NotEqual:
    case ERHICompareOp::Always:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr bool IsValidRHIAttachmentRole(ERHIAttachmentRole Value) noexcept
{
    return Value == ERHIAttachmentRole::Color || Value == ERHIAttachmentRole::DepthStencil;
}

[[nodiscard]] constexpr bool IsValidRHIAttachmentLoadOp(ERHIAttachmentLoadOp Value) noexcept
{
    return Value == ERHIAttachmentLoadOp::Load || Value == ERHIAttachmentLoadOp::Clear || Value == ERHIAttachmentLoadOp::DontCare;
}

[[nodiscard]] constexpr bool IsValidRHIAttachmentStoreOp(ERHIAttachmentStoreOp Value) noexcept
{
    return Value == ERHIAttachmentStoreOp::Store || Value == ERHIAttachmentStoreOp::DontCare;
}

[[nodiscard]] constexpr bool IsValidRHISampleCount(ERHISampleCount Value) noexcept
{
    switch (Value)
    {
    case ERHISampleCount::One:
    case ERHISampleCount::Two:
    case ERHISampleCount::Four:
    case ERHISampleCount::Eight:
        return true;
    }
    return false;
}

} // namespace Stoner::RHI
