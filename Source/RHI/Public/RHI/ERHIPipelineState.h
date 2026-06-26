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

} // namespace Stoner::RHI
