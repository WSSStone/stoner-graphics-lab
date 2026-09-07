#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIPipelineState.h"
#include "RHI/ERHIResourceUsage.h"
#include "RHI/FRHIShaderModuleDesc.h"

namespace Stoner::RHI
{

class IRHIPipelineLayout;
class IRHIShaderModule;

enum class ERHIColorWriteMask : Stoner::Core::uint32
{
    None = 0,
    Red = 1u << 0,
    Green = 1u << 1,
    Blue = 1u << 2,
    Alpha = 1u << 3,
    RGB = Red | Green | Blue,
    RGBA = RGB | Alpha
};

[[nodiscard]] constexpr ERHIColorWriteMask operator|(
    ERHIColorWriteMask Left,
    ERHIColorWriteMask Right) noexcept
{
    return static_cast<ERHIColorWriteMask>(
        static_cast<Stoner::Core::uint32>(Left) |
        static_cast<Stoner::Core::uint32>(Right));
}

[[nodiscard]] constexpr ERHIColorWriteMask operator&(
    ERHIColorWriteMask Left,
    ERHIColorWriteMask Right) noexcept
{
    return static_cast<ERHIColorWriteMask>(
        static_cast<Stoner::Core::uint32>(Left) &
        static_cast<Stoner::Core::uint32>(Right));
}

constexpr ERHIColorWriteMask& operator|=(
    ERHIColorWriteMask& Left,
    ERHIColorWriteMask Right) noexcept
{
    Left = Left | Right;
    return Left;
}

inline constexpr ERHIColorWriteMask RHIColorWriteMaskValidMask =
    ERHIColorWriteMask::RGBA;

[[nodiscard]] constexpr bool IsValidRHIColorWriteMask(
    ERHIColorWriteMask Value) noexcept
{
    return HasOnlyRHIFlags(Value, RHIColorWriteMaskValidMask);
}

[[nodiscard]] constexpr bool HasRHIFlag(
    ERHIColorWriteMask Value,
    ERHIColorWriteMask Flag) noexcept
{
    return (Value & Flag) != ERHIColorWriteMask::None;
}

struct FRHIVertexAttributeDesc
{
    Stoner::Core::uint32 Location = 0;
    ERHIFormat Format = ERHIFormat::Unknown;
    Stoner::Core::uint32 Offset = 0;
};

struct FRHIVertexInputDesc
{
    Stoner::Core::uint32 Stride = 0;
    Stoner::Core::TArray<FRHIVertexAttributeDesc> Attributes;
};

struct FRHIRasterizerState
{
    ERHICullMode CullMode = ERHICullMode::Back;
    ERHIFrontFace FrontFace = ERHIFrontFace::Clockwise;
    bool bDepthClampEnabled = false;
};

struct FRHIBlendState
{
    bool bEnabled = false;
    ERHIBlendFactor SourceColor = ERHIBlendFactor::One;
    ERHIBlendFactor DestinationColor = ERHIBlendFactor::Zero;
    ERHIBlendOp ColorOp = ERHIBlendOp::Add;
    ERHIColorWriteMask ColorWriteMask = ERHIColorWriteMask::RGBA;
};

struct FRHIDepthStencilState
{
    bool bDepthTestEnabled = false;
    bool bDepthWriteEnabled = false;
    ERHICompareOp DepthCompare = ERHICompareOp::LessEqual;
};

struct FRHIMultisampleState
{
    ERHISampleCount SampleCount = ERHISampleCount::One;
    bool bSampleShadingEnabled = false;
};

struct FRHIDynamicStateRequirements
{
    bool bViewportDynamic = true;
    bool bScissorDynamic = true;
};

struct FRHIRenderTargetCompatibility
{
    Stoner::Core::TArray<ERHIFormat> ColorFormats;
    ERHIFormat DepthStencilFormat = ERHIFormat::Unknown;
    ERHISampleCount SampleCount = ERHISampleCount::One;
};

struct FRHIGraphicsPipelineDesc
{
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<IRHIShaderModule>> ShaderModules;
    Stoner::Core::TSharedPtr<IRHIPipelineLayout> PipelineLayout;
    FRHIVertexInputDesc VertexInput;
    ERHIPrimitiveTopology Topology = ERHIPrimitiveTopology::TriangleList;
    FRHIRasterizerState Rasterizer;
    FRHIBlendState Blend;
    FRHIDepthStencilState DepthStencil;
    FRHIMultisampleState Multisample;
    FRHIDynamicStateRequirements DynamicState;
    FRHIRenderTargetCompatibility RenderTargets;
    ERHIRuntimeObjectMode RuntimeMode = ERHIRuntimeObjectMode::Unknown;
    ERHIPipelineReuseState ReuseState = ERHIPipelineReuseState::NotReusable;
    Stoner::Core::FString CompatibilitySummary;
};

[[nodiscard]] inline bool IsValidRHIVertexInputDesc(const FRHIVertexInputDesc& Desc) noexcept
{
    if (Desc.Stride == 0 || Desc.Attributes.empty())
    {
        return false;
    }
    for (const FRHIVertexAttributeDesc& Attribute : Desc.Attributes)
    {
        if (!IsValidRHIFormat(Attribute.Format) ||
            IsDepthStencilFormat(Attribute.Format) ||
            Attribute.Offset >= Desc.Stride)
        {
            return false;
        }
    }
    return true;
}

[[nodiscard]] constexpr bool IsTriangleReadyRHITopology(ERHIPrimitiveTopology Topology) noexcept
{
    return Topology == ERHIPrimitiveTopology::TriangleList || Topology == ERHIPrimitiveTopology::TriangleStrip;
}

[[nodiscard]] inline bool IsValidRHIRenderTargetCompatibility(const FRHIRenderTargetCompatibility& Desc) noexcept
{
    if (Desc.ColorFormats.empty())
    {
        return false;
    }
    for (ERHIFormat Format : Desc.ColorFormats)
    {
        if (!IsValidRHIFormat(Format) || IsDepthStencilFormat(Format))
        {
            return false;
        }
    }
    return IsValidRHISampleCount(Desc.SampleCount) &&
        (Desc.DepthStencilFormat == ERHIFormat::Unknown ||
            (IsValidRHIFormat(Desc.DepthStencilFormat) && IsDepthStencilFormat(Desc.DepthStencilFormat)));
}

// Depth test or write requires a depth/stencil attachment in the render target scope.
[[nodiscard]] inline bool IsValidRHIDepthStencilCompatibility(const FRHIDepthStencilState& DepthStencil, const FRHIRenderTargetCompatibility& RenderTargets) noexcept
{
    if ((DepthStencil.bDepthTestEnabled || DepthStencil.bDepthWriteEnabled) && RenderTargets.DepthStencilFormat == ERHIFormat::Unknown)
    {
        return false;
    }
    return true;
}

[[nodiscard]] inline bool IsValidRHIGraphicsPipelineState(const FRHIGraphicsPipelineDesc& Desc) noexcept
{
    return Desc.PipelineLayout &&
        IsValidRHIVertexInputDesc(Desc.VertexInput) &&
        IsTriangleReadyRHITopology(Desc.Topology) &&
        IsValidRHICullMode(Desc.Rasterizer.CullMode) &&
        IsValidRHIFrontFace(Desc.Rasterizer.FrontFace) &&
        IsValidRHIBlendFactor(Desc.Blend.SourceColor) &&
        IsValidRHIBlendFactor(Desc.Blend.DestinationColor) &&
        IsValidRHIBlendOp(Desc.Blend.ColorOp) &&
        IsValidRHIColorWriteMask(Desc.Blend.ColorWriteMask) &&
        IsValidRHICompareOp(Desc.DepthStencil.DepthCompare) &&
        IsValidRHISampleCount(Desc.Multisample.SampleCount) &&
        Desc.Multisample.SampleCount == Desc.RenderTargets.SampleCount &&
        Desc.DynamicState.bViewportDynamic && Desc.DynamicState.bScissorDynamic &&
        IsValidRHIDepthStencilCompatibility(Desc.DepthStencil, Desc.RenderTargets) &&
        IsValidRHIRenderTargetCompatibility(Desc.RenderTargets);
}

} // namespace Stoner::RHI
