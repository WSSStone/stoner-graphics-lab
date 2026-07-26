#include "RHI/FRHIDescriptorBinding.h"
#include "RHI/FRHIGraphicsPipelineDesc.h"
#include "RHI/FRHIPipelineLayoutDesc.h"
#include "RHI/FRHIRenderPassDesc.h"
#include "RHI/FRHITextureDesc.h"

#include <iostream>
#include <limits>

using namespace Stoner::Core;
using namespace Stoner::RHI;

int main()
{
    const FRHIDescriptorBinding ValidBinding{
        0, 0, ERHIDescriptorType::UniformBuffer, 1,
        ERHIShaderStageFlags::Vertex};
    FRHIDescriptorBinding InvalidBinding = ValidBinding;
    InvalidBinding.DescriptorType = static_cast<ERHIDescriptorType>(255);
    const bool bDescriptorDomains =
        IsValidRHIDescriptorBinding(ValidBinding) &&
        !IsValidRHIDescriptorBinding(InvalidBinding) &&
        !IsValidRHIShaderStageFlags(
            static_cast<ERHIShaderStageFlags>(1u << 31));

    const FRHIShaderConstantRange Left{
        0, 16, ERHIShaderStageFlags::Vertex};
    const FRHIShaderConstantRange Adjacent{
        16, 16, ERHIShaderStageFlags::Vertex};
    const FRHIShaderConstantRange OverlappingSameStage{
        8, 16, ERHIShaderStageFlags::Vertex};
    const FRHIShaderConstantRange OverlappingOtherStage{
        8, 16, ERHIShaderStageFlags::Fragment};
    const FRHIShaderConstantRange Overflowing{
        std::numeric_limits<uint32>::max() - 3u,
        8,
        ERHIShaderStageFlags::Vertex};
    const bool bRangeBoundaries =
        IsValidRHIShaderConstantRange(Left) &&
        !IsValidRHIShaderConstantRange(Overflowing) &&
        !DoRHIShaderConstantRangesOverlap(Left, Adjacent) &&
        HasIncompatibleRHIShaderConstantRangeOverlap(
            {Left, OverlappingSameStage}) &&
        !HasIncompatibleRHIShaderConstantRangeOverlap(
            {Left, OverlappingOtherStage}) &&
        DoesRHIShaderConstantRangeContain(
            {0, 64, ERHIShaderStageFlags::Vertex |
                    ERHIShaderStageFlags::Fragment},
            {16, 16, ERHIShaderStageFlags::Fragment}) &&
        !DoesRHIShaderConstantRangeContain(
            {0, 16, ERHIShaderStageFlags::Vertex},
            {8, 16, ERHIShaderStageFlags::Vertex});

    FRHIGraphicsPipelineDesc Graphics;
    Graphics.PipelineLayout = TSharedPtr<IRHIPipelineLayout>(
        reinterpret_cast<IRHIPipelineLayout*>(1),
        [](IRHIPipelineLayout*) {});
    Graphics.VertexInput.Stride = 16;
    Graphics.VertexInput.Attributes = {
        {0, ERHIFormat::R32_Float, 0}};
    Graphics.RenderTargets.ColorFormats = {
        ERHIFormat::R8G8B8A8_UNorm};
    const bool bGraphicsValid =
        IsValidRHIGraphicsPipelineState(Graphics);
    Graphics.Rasterizer.CullMode =
        static_cast<ERHICullMode>(255);
    const bool bGraphicsDomains =
        bGraphicsValid &&
        !IsValidRHIGraphicsPipelineState(Graphics);

    FRHIRenderPassDesc Pass;
    Pass.Attachments = {{
        ERHIAttachmentRole::Color,
        ERHIFormat::R8G8B8A8_UNorm,
        ERHISampleCount::One,
        ERHIAttachmentLoadOp::Clear,
        ERHIAttachmentStoreOp::Store}};
    const bool bPassValid = IsValidRHIRenderPassDesc(Pass);
    Pass.Attachments[0].LoadOp =
        static_cast<ERHIAttachmentLoadOp>(255);
    const bool bRenderPassDomains =
        bPassValid && !IsValidRHIRenderPassDesc(Pass);

    const bool bMipExtents =
        GetRHIMipExtent(13, 0) == 13 &&
        GetRHIMipExtent(13, 1) == 6 &&
        GetRHIMipExtent(13, 2) == 3 &&
        GetRHIMipExtent(13, 3) == 1 &&
        GetRHIMipExtent(13, 4) == 1 &&
        GetRHIMipExtent(0, 0) == 0 &&
        GetRHIMipExtent(13, 32) == 0;

    std::cout
        << "descriptor_domains=" << bDescriptorDomains << '\n'
        << "range_boundaries=" << bRangeBoundaries << '\n'
        << "graphics_domains=" << bGraphicsDomains << '\n'
        << "render_pass_domains=" << bRenderPassDomains << '\n'
        << "mip_extents=" << bMipExtents << '\n';
    return bDescriptorDomains &&
            bRangeBoundaries &&
            bGraphicsDomains &&
            bRenderPassDomains &&
            bMipExtents
        ? 0
        : 3;
}
