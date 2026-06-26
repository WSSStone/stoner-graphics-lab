#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIPipelineState.h"

namespace Stoner::RHI
{

class IRHIPipelineLayout;
class IRHIShaderModule;

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
    ERHIFrontFace FrontFace = ERHIFrontFace::CounterClockwise;
    bool bDepthClampEnabled = false;
};

struct FRHIBlendState
{
    bool bEnabled = false;
    ERHIBlendFactor SourceColor = ERHIBlendFactor::One;
    ERHIBlendFactor DestinationColor = ERHIBlendFactor::Zero;
    ERHIBlendOp ColorOp = ERHIBlendOp::Add;
};

struct FRHIDepthStencilState
{
    bool bDepthTestEnabled = false;
    bool bDepthWriteEnabled = false;
    ERHICompareOp DepthCompare = ERHICompareOp::LessEqual;
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
    FRHIRenderTargetCompatibility RenderTargets;
};

} // namespace Stoner::RHI
