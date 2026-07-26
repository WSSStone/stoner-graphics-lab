#include "Tests/RHICoreTests.cpp"

#include <iostream>

namespace
{

FRHIGraphicsPipelineDesc MakeGraphicsPipelineDesc(
    const TSharedPtr<IRHIPipelineLayout>& Layout,
    const TSharedPtr<IRHIShaderModule>& Vertex,
    const TSharedPtr<IRHIShaderModule>& Fragment)
{
    FRHIGraphicsPipelineDesc Desc;
    Desc.PipelineLayout = Layout;
    Desc.ShaderModules = {Vertex, Fragment};
    Desc.VertexInput.Stride = 16;
    Desc.VertexInput.Attributes = {{0, ERHIFormat::R32_Float, 0}};
    Desc.Topology = ERHIPrimitiveTopology::TriangleList;
    Desc.RenderTargets.ColorFormats = {ERHIFormat::R8G8B8A8_UNorm};
    return Desc;
}

} // namespace

int main()
{
    FMockDevice Device;

    FRHIPipelineLayoutDesc UnknownDeclaration = MakePipelineLayoutDesc();
    UnknownDeclaration.Bindings[0].DescriptorType =
        static_cast<ERHIDescriptorType>(255);
    UnknownDeclaration.Bindings[0].Visibility =
        static_cast<ERHIShaderStageFlags>(1u << 31);
    const bool bUnknownDeclarationAccepted =
        IsValidRHIPipelineLayoutDesc(UnknownDeclaration) &&
        Device.CreatePipelineLayout(UnknownDeclaration).Succeeded();

    FRHIPipelineLayoutDesc OverlappingRanges = MakePipelineLayoutDesc();
    OverlappingRanges.ConstantRanges = {
        {0, 32, ERHIShaderStageFlags::Compute},
        {16, 32, ERHIShaderStageFlags::Compute}};
    const bool bOverlappingRangesAccepted =
        IsValidRHIPipelineLayoutDesc(OverlappingRanges) &&
        Device.CreatePipelineLayout(OverlappingRanges).Succeeded();

    FRHIPipelineLayoutDesc MissingConstantRange = MakePipelineLayoutDesc();
    MissingConstantRange.ConstantRanges.clear();
    const auto MissingRangeLayout =
        Device.CreatePipelineLayout(MissingConstantRange);
    const auto Compute =
        Device.CreateShaderModule(MakeShaderDesc(
            ERHIShaderStage::Compute, "main", "probe-compute"));
    FRHIComputePipelineDesc ComputeDesc;
    ComputeDesc.PipelineLayout = MissingRangeLayout.Object;
    ComputeDesc.ShaderModules = {Compute.Object};
    const bool bMissingShaderRangeAccepted =
        MissingRangeLayout.Succeeded() &&
        Compute.Succeeded() &&
        Device.CreateComputePipeline(ComputeDesc).Succeeded();

    const bool bInterfaceDefectsAccepted =
        bUnknownDeclarationAccepted &&
        bOverlappingRangesAccepted &&
        bMissingShaderRangeAccepted;

    const auto ValidLayout =
        Device.CreatePipelineLayout(MakePipelineLayoutDesc());
    const auto Vertex =
        Device.CreateShaderModule(MakeShaderDesc(
            ERHIShaderStage::Vertex, "main", "probe-vertex"));
    const auto Fragment =
        Device.CreateShaderModule(MakeShaderDesc(
            ERHIShaderStage::Fragment, "main", "probe-fragment"));
    FRHIGraphicsPipelineDesc InvalidFixedFunction =
        MakeGraphicsPipelineDesc(
            ValidLayout.Object, Vertex.Object, Fragment.Object);
    InvalidFixedFunction.Rasterizer.CullMode =
        static_cast<ERHICullMode>(255);
    InvalidFixedFunction.Rasterizer.FrontFace =
        static_cast<ERHIFrontFace>(255);
    InvalidFixedFunction.Blend.SourceColor =
        static_cast<ERHIBlendFactor>(255);
    InvalidFixedFunction.DepthStencil.DepthCompare =
        static_cast<ERHICompareOp>(255);
    InvalidFixedFunction.Multisample.SampleCount =
        static_cast<ERHISampleCount>(3);
    InvalidFixedFunction.RenderTargets.SampleCount =
        static_cast<ERHISampleCount>(3);
    const bool bInvalidFixedFunctionAccepted =
        IsValidRHIGraphicsPipelineState(InvalidFixedFunction) &&
        Device.CreateGraphicsPipeline(InvalidFixedFunction).Succeeded();

    FRHIRenderPassDesc InvalidRenderPass;
    InvalidRenderPass.Attachments = {{
        static_cast<ERHIAttachmentRole>(255),
        ERHIFormat::D32_Float,
        static_cast<ERHISampleCount>(3),
        static_cast<ERHIAttachmentLoadOp>(255),
        static_cast<ERHIAttachmentStoreOp>(255)}};
    const bool bInvalidRenderPassAccepted =
        Device.CreateRenderPass(InvalidRenderPass).Succeeded();
    const bool bStateDomainsAccepted =
        bInvalidFixedFunctionAccepted && bInvalidRenderPassAccepted;

    FRHIRenderPassDesc ColorPassDesc;
    ColorPassDesc.Attachments = {{
        ERHIAttachmentRole::Color,
        ERHIFormat::R8G8B8A8_UNorm,
        ERHISampleCount::One}};
    const auto ColorPass = Device.CreateRenderPass(ColorPassDesc);
    FRHITextureDesc ArrayTextureDesc = MakeColorTextureDesc();
    ArrayTextureDesc.Dimension = ERHITextureDimension::Texture2DArray;
    ArrayTextureDesc.ArrayLayers = 2;
    ArrayTextureDesc.MipLevels = 2;
    const auto ArrayTexture = Device.CreateTexture(ArrayTextureDesc);

    FRHIFramebufferDesc OutOfRangeFramebuffer;
    OutOfRangeFramebuffer.RenderPass = ColorPass.Object;
    OutOfRangeFramebuffer.Attachments = {{ArrayTexture.Object, 2, 2}};
    OutOfRangeFramebuffer.Width = 64;
    OutOfRangeFramebuffer.Height = 64;
    const bool bOutOfRangeSubresourceAccepted =
        ColorPass.Succeeded() &&
        ArrayTexture.Succeeded() &&
        Device.CreateFramebuffer(OutOfRangeFramebuffer).Succeeded();

    FRHIFramebufferDesc ValidMipFramebuffer = OutOfRangeFramebuffer;
    ValidMipFramebuffer.Attachments = {{ArrayTexture.Object, 1, 1}};
    ValidMipFramebuffer.Width = 32;
    ValidMipFramebuffer.Height = 32;
    const bool bValidMipRejected =
        Device.CreateFramebuffer(ValidMipFramebuffer).Result ==
        ERHIResult::InvalidState;
    const bool bFramebufferSubresourceDefects =
        bOutOfRangeSubresourceAccepted && bValidMipRejected;

    std::cout
        << "interface_contract_defects=" << bInterfaceDefectsAccepted << '\n'
        << "fixed_function_domains_accepted=" << bStateDomainsAccepted << '\n'
        << "framebuffer_subresource_defects="
        << bFramebufferSubresourceDefects << '\n'
        << "classification="
        << (bInterfaceDefectsAccepted &&
                    bStateDomainsAccepted &&
                    bFramebufferSubresourceDefects
                ? "pipeline-framebuffer-contract-defects"
                : "unexpected")
        << '\n';

    return bInterfaceDefectsAccepted &&
            bStateDomainsAccepted &&
            bFramebufferSubresourceDefects
        ? 0
        : 3;
}
