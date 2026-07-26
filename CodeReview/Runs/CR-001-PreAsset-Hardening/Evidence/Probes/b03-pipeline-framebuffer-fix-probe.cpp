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
    const bool bUnknownDeclarationRejected =
        !IsValidRHIPipelineLayoutDesc(UnknownDeclaration) &&
        Device.CreatePipelineLayout(UnknownDeclaration).Result ==
            ERHIResult::InvalidState;

    FRHIPipelineLayoutDesc OverlappingRanges = MakePipelineLayoutDesc();
    OverlappingRanges.ConstantRanges = {
        {0, 32, ERHIShaderStageFlags::Compute},
        {16, 32, ERHIShaderStageFlags::Compute}};
    const bool bOverlappingRangesRejected =
        !IsValidRHIPipelineLayoutDesc(OverlappingRanges) &&
        Device.CreatePipelineLayout(OverlappingRanges).Result ==
            ERHIResult::InvalidState;

    FRHIPipelineLayoutDesc DisjointVisibility = MakePipelineLayoutDesc();
    DisjointVisibility.ConstantRanges = {
        {0, 32, ERHIShaderStageFlags::Vertex},
        {16, 32, ERHIShaderStageFlags::Fragment}};
    const bool bDisjointVisibilityAccepted =
        IsValidRHIPipelineLayoutDesc(DisjointVisibility) &&
        Device.CreatePipelineLayout(DisjointVisibility).Succeeded();

    FRHIPipelineLayoutDesc MissingConstantRange = MakePipelineLayoutDesc();
    MissingConstantRange.ConstantRanges.clear();
    const auto MissingRangeLayout =
        Device.CreatePipelineLayout(MissingConstantRange);
    const auto Compute =
        Device.CreateShaderModule(MakeShaderDesc(
            ERHIShaderStage::Compute, "main", "probe-compute-fixed"));
    FRHIComputePipelineDesc ComputeDesc;
    ComputeDesc.PipelineLayout = MissingRangeLayout.Object;
    ComputeDesc.ShaderModules = {Compute.Object};
    const bool bMissingShaderRangeRejected =
        MissingRangeLayout.Succeeded() &&
        Compute.Succeeded() &&
        Device.CreateComputePipeline(ComputeDesc).Result ==
            ERHIResult::InvalidState;

    const bool bInterfaceContractRepaired =
        bUnknownDeclarationRejected &&
        bOverlappingRangesRejected &&
        bDisjointVisibilityAccepted &&
        bMissingShaderRangeRejected;

    const auto ValidLayout =
        Device.CreatePipelineLayout(MakePipelineLayoutDesc());
    const auto Vertex =
        Device.CreateShaderModule(MakeShaderDesc(
            ERHIShaderStage::Vertex, "main", "probe-vertex-fixed"));
    const auto Fragment =
        Device.CreateShaderModule(MakeShaderDesc(
            ERHIShaderStage::Fragment, "main", "probe-fragment-fixed"));
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
    const bool bInvalidFixedFunctionRejected =
        !IsValidRHIGraphicsPipelineState(InvalidFixedFunction) &&
        Device.CreateGraphicsPipeline(InvalidFixedFunction).Result ==
            ERHIResult::InvalidState;

    FRHIRenderPassDesc InvalidRenderPass;
    InvalidRenderPass.Attachments = {{
        static_cast<ERHIAttachmentRole>(255),
        ERHIFormat::D32_Float,
        static_cast<ERHISampleCount>(3),
        static_cast<ERHIAttachmentLoadOp>(255),
        static_cast<ERHIAttachmentStoreOp>(255)}};
    const bool bInvalidRenderPassRejected =
        !Stoner::RHI::IsValidRHIRenderPassDesc(InvalidRenderPass) &&
        Device.CreateRenderPass(InvalidRenderPass).Result ==
            ERHIResult::InvalidState;
    const bool bStateDomainsRepaired =
        bInvalidFixedFunctionRejected && bInvalidRenderPassRejected;

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

    FRHIFramebufferDesc ValidMipFramebuffer;
    ValidMipFramebuffer.RenderPass = ColorPass.Object;
    ValidMipFramebuffer.Attachments = {{ArrayTexture.Object, 1, 1}};
    ValidMipFramebuffer.Width = 32;
    ValidMipFramebuffer.Height = 32;
    const bool bValidMipAccepted =
        ColorPass.Succeeded() &&
        ArrayTexture.Succeeded() &&
        Device.CreateFramebuffer(ValidMipFramebuffer).Succeeded();

    FRHIFramebufferDesc OutOfRangeFramebuffer = ValidMipFramebuffer;
    OutOfRangeFramebuffer.Attachments = {{ArrayTexture.Object, 2, 2}};
    const bool bOutOfRangeSubresourceRejected =
        Device.CreateFramebuffer(OutOfRangeFramebuffer).Result ==
        ERHIResult::InvalidState;
    const bool bFramebufferSubresourcesRepaired =
        bValidMipAccepted && bOutOfRangeSubresourceRejected;

    std::cout
        << "interface_contract_repaired=" << bInterfaceContractRepaired << '\n'
        << "fixed_function_domains_repaired=" << bStateDomainsRepaired << '\n'
        << "framebuffer_subresources_repaired="
        << bFramebufferSubresourcesRepaired << '\n'
        << "classification="
        << (bInterfaceContractRepaired &&
                    bStateDomainsRepaired &&
                    bFramebufferSubresourcesRepaired
                ? "pipeline-framebuffer-contract-repaired"
                : "unexpected")
        << '\n';

    return bInterfaceContractRepaired &&
            bStateDomainsRepaired &&
            bFramebufferSubresourcesRepaired
        ? 0
        : 3;
}
