#include "Tests/RHICoreTests.cpp"

#include <cstring>
#include <iostream>

namespace
{

FRHIGraphicsPipelineDesc MakeGraphicsDesc(
    const TSharedPtr<IRHIPipelineLayout>& Layout,
    const TSharedPtr<IRHIShaderModule>& Vertex,
    const TSharedPtr<IRHIShaderModule>& Fragment)
{
    FRHIGraphicsPipelineDesc Desc;
    Desc.PipelineLayout = Layout;
    Desc.ShaderModules = {Vertex, Fragment};
    Desc.VertexInput.Stride = 16;
    Desc.VertexInput.Attributes = {{0, ERHIFormat::R32_Float, 0}};
    Desc.RenderTargets.ColorFormats = {ERHIFormat::R8G8B8A8_UNorm};
    return Desc;
}

bool PipelineLayoutRejected(
    FMockDevice& Device,
    const FRHIPipelineLayoutDesc& Desc)
{
    return !Device.CreatePipelineLayout(Desc).Succeeded();
}

bool ShaderRejected(
    FMockDevice& Device,
    const FRHIShaderModuleDesc& Desc)
{
    return !Device.CreateShaderModule(Desc).Succeeded();
}

bool GraphicsRejected(
    FMockDevice& Device,
    const FRHIGraphicsPipelineDesc& Desc)
{
    return !Device.CreateGraphicsPipeline(Desc).Succeeded();
}

bool RenderPassRejected(
    FMockDevice& Device,
    const FRHIRenderPassDesc& Desc)
{
    return !Device.CreateRenderPass(Desc).Succeeded();
}

bool FramebufferRejected(
    FMockDevice& Device,
    const FRHIFramebufferDesc& Desc)
{
    return !Device.CreateFramebuffer(Desc).Succeeded();
}

} // namespace

int main(int ArgCount, char** Args)
{
    if (ArgCount != 2 ||
        (std::strcmp(Args[1], "parent") != 0 &&
            std::strcmp(Args[1], "current") != 0))
    {
        return 2;
    }

    FMockDevice Device;

    int InterfaceRejections = 0;
    FRHIPipelineLayoutDesc InvalidLayout = MakePipelineLayoutDesc();
    InvalidLayout.Bindings[0].DescriptorType =
        static_cast<ERHIDescriptorType>(255);
    InterfaceRejections += PipelineLayoutRejected(Device, InvalidLayout);

    InvalidLayout = MakePipelineLayoutDesc();
    InvalidLayout.Bindings[0].Visibility =
        static_cast<ERHIShaderStageFlags>(1u << 31);
    InterfaceRejections += PipelineLayoutRejected(Device, InvalidLayout);

    InvalidLayout = MakePipelineLayoutDesc();
    InvalidLayout.ConstantRanges.push_back(
        {8, 16, ERHIShaderStageFlags::Compute});
    InterfaceRejections += PipelineLayoutRejected(Device, InvalidLayout);

    FRHIShaderModuleDesc InvalidShader =
        MakeShaderDesc(ERHIShaderStage::Vertex, "verify_type", "verify_type");
    InvalidShader.InterfaceMetadata.Bindings[0].DescriptorType =
        static_cast<ERHIDescriptorType>(255);
    InterfaceRejections += ShaderRejected(Device, InvalidShader);

    InvalidShader =
        MakeShaderDesc(
            ERHIShaderStage::Vertex,
            "verify_visibility",
            "verify_visibility");
    InvalidShader.InterfaceMetadata.Bindings[0].Visibility =
        ERHIShaderStageFlags::Vertex |
        static_cast<ERHIShaderStageFlags>(1u << 31);
    InterfaceRejections += ShaderRejected(Device, InvalidShader);

    InvalidShader =
        MakeShaderDesc(
            ERHIShaderStage::Compute,
            "verify_overlap",
            "verify_overlap");
    InvalidShader.InterfaceMetadata.ConstantRanges.push_back(
        {8, 16, ERHIShaderStageFlags::Compute});
    InterfaceRejections += ShaderRejected(Device, InvalidShader);

    FRHIPipelineLayoutDesc MissingRangeDesc = MakePipelineLayoutDesc();
    MissingRangeDesc.ConstantRanges.clear();
    const auto MissingRangeLayout =
        Device.CreatePipelineLayout(MissingRangeDesc);
    const auto Compute = Device.CreateShaderModule(
        MakeShaderDesc(
            ERHIShaderStage::Compute,
            "verify_compute",
            "verify_compute"));
    FRHIComputePipelineDesc MissingRangeCompute;
    MissingRangeCompute.PipelineLayout = MissingRangeLayout.Object;
    MissingRangeCompute.ShaderModules = {Compute.Object};
    InterfaceRejections +=
        !Device.CreateComputePipeline(MissingRangeCompute).Succeeded();

    int ValidInterfacePaths = 0;
    FRHIPipelineLayoutDesc DisjointRanges = MakePipelineLayoutDesc();
    DisjointRanges.ConstantRanges = {
        {0, 32, ERHIShaderStageFlags::Vertex},
        {16, 32, ERHIShaderStageFlags::Fragment}};
    ValidInterfacePaths +=
        Device.CreatePipelineLayout(DisjointRanges).Succeeded();
    const auto ValidLayout =
        Device.CreatePipelineLayout(MakePipelineLayoutDesc());
    FRHIComputePipelineDesc ValidCompute;
    ValidCompute.PipelineLayout = ValidLayout.Object;
    ValidCompute.ShaderModules = {Compute.Object};
    ValidInterfacePaths +=
        Device.CreateComputePipeline(ValidCompute).Succeeded();

    const auto Vertex = Device.CreateShaderModule(
        MakeShaderDesc(
            ERHIShaderStage::Vertex,
            "verify_vertex",
            "verify_vertex"));
    const auto Fragment = Device.CreateShaderModule(
        MakeShaderDesc(
            ERHIShaderStage::Fragment,
            "verify_fragment",
            "verify_fragment"));
    const FRHIGraphicsPipelineDesc ValidGraphics =
        MakeGraphicsDesc(ValidLayout.Object, Vertex.Object, Fragment.Object);

    int StateRejections = 0;
    FRHIGraphicsPipelineDesc InvalidGraphics = ValidGraphics;
    InvalidGraphics.Rasterizer.CullMode = static_cast<ERHICullMode>(255);
    StateRejections += GraphicsRejected(Device, InvalidGraphics);
    InvalidGraphics = ValidGraphics;
    InvalidGraphics.Rasterizer.FrontFace = static_cast<ERHIFrontFace>(255);
    StateRejections += GraphicsRejected(Device, InvalidGraphics);
    InvalidGraphics = ValidGraphics;
    InvalidGraphics.Blend.SourceColor = static_cast<ERHIBlendFactor>(255);
    StateRejections += GraphicsRejected(Device, InvalidGraphics);
    InvalidGraphics = ValidGraphics;
    InvalidGraphics.Blend.DestinationColor =
        static_cast<ERHIBlendFactor>(255);
    StateRejections += GraphicsRejected(Device, InvalidGraphics);
    InvalidGraphics = ValidGraphics;
    InvalidGraphics.Blend.ColorOp = static_cast<ERHIBlendOp>(255);
    StateRejections += GraphicsRejected(Device, InvalidGraphics);
    InvalidGraphics = ValidGraphics;
    InvalidGraphics.DepthStencil.DepthCompare =
        static_cast<ERHICompareOp>(255);
    StateRejections += GraphicsRejected(Device, InvalidGraphics);
    InvalidGraphics = ValidGraphics;
    InvalidGraphics.Multisample.SampleCount =
        static_cast<ERHISampleCount>(3);
    InvalidGraphics.RenderTargets.SampleCount =
        static_cast<ERHISampleCount>(3);
    StateRejections += GraphicsRejected(Device, InvalidGraphics);

    FRHIRenderPassDesc ValidPassDesc;
    ValidPassDesc.Attachments = {{
        ERHIAttachmentRole::Color,
        ERHIFormat::R8G8B8A8_UNorm,
        ERHISampleCount::One,
        ERHIAttachmentLoadOp::Clear,
        ERHIAttachmentStoreOp::Store}};

    FRHIRenderPassDesc InvalidPass;
    InvalidPass.Attachments = {{
        static_cast<ERHIAttachmentRole>(255),
        ERHIFormat::D32_Float,
        ERHISampleCount::One,
        ERHIAttachmentLoadOp::Clear,
        ERHIAttachmentStoreOp::Store}};
    StateRejections += RenderPassRejected(Device, InvalidPass);
    InvalidPass = ValidPassDesc;
    InvalidPass.Attachments[0].SampleCount =
        static_cast<ERHISampleCount>(3);
    StateRejections += RenderPassRejected(Device, InvalidPass);
    InvalidPass = ValidPassDesc;
    InvalidPass.Attachments[0].LoadOp =
        static_cast<ERHIAttachmentLoadOp>(255);
    StateRejections += RenderPassRejected(Device, InvalidPass);
    InvalidPass = ValidPassDesc;
    InvalidPass.Attachments[0].StoreOp =
        static_cast<ERHIAttachmentStoreOp>(255);
    StateRejections += RenderPassRejected(Device, InvalidPass);
    InvalidPass = ValidPassDesc;
    InvalidPass.Attachments.push_back({
        ERHIAttachmentRole::Color,
        ERHIFormat::R8G8B8A8_UNorm,
        ERHISampleCount::Two,
        ERHIAttachmentLoadOp::Clear,
        ERHIAttachmentStoreOp::Store});
    StateRejections += RenderPassRejected(Device, InvalidPass);

    int ValidStatePaths = Device.CreateGraphicsPipeline(ValidGraphics).Succeeded();
    const auto ValidPass = Device.CreateRenderPass(ValidPassDesc);
    ValidStatePaths += ValidPass.Succeeded();

    FRHITextureDesc ArrayTextureDesc = MakeColorTextureDesc();
    ArrayTextureDesc.Dimension = ERHITextureDimension::Texture2DArray;
    ArrayTextureDesc.ArrayLayers = 2;
    ArrayTextureDesc.MipLevels = 2;
    const auto ArrayTexture = Device.CreateTexture(ArrayTextureDesc);

    FRHIFramebufferDesc BaseFramebuffer;
    BaseFramebuffer.RenderPass = ValidPass.Object;
    BaseFramebuffer.Attachments = {{ArrayTexture.Object, 0, 0}};
    BaseFramebuffer.Width = 64;
    BaseFramebuffer.Height = 64;

    int BaselineSubresourceAcceptances =
        Device.CreateFramebuffer(BaseFramebuffer).Succeeded();
    FRHIFramebufferDesc LayerOne = BaseFramebuffer;
    LayerOne.Attachments[0].ArrayLayer = 1;
    BaselineSubresourceAcceptances +=
        Device.CreateFramebuffer(LayerOne).Succeeded();

    int InvalidSubresourceRejections = 0;
    FRHIFramebufferDesc InvalidSubresource = BaseFramebuffer;
    InvalidSubresource.Attachments[0].ArrayLayer = 2;
    InvalidSubresourceRejections +=
        FramebufferRejected(Device, InvalidSubresource);
    InvalidSubresource = BaseFramebuffer;
    InvalidSubresource.Attachments[0].MipLevel = 2;
    InvalidSubresourceRejections +=
        FramebufferRejected(Device, InvalidSubresource);
    InvalidSubresource = BaseFramebuffer;
    InvalidSubresource.Attachments[0].MipLevel = 1;
    InvalidSubresourceRejections +=
        FramebufferRejected(Device, InvalidSubresource);

    FRHIFramebufferDesc SelectedMip = BaseFramebuffer;
    SelectedMip.Attachments[0] = {ArrayTexture.Object, 1, 1};
    SelectedMip.Width = 32;
    SelectedMip.Height = 32;
    const int SelectedMipAcceptances =
        Device.CreateFramebuffer(SelectedMip).Succeeded();

    const bool bExpectParent = std::strcmp(Args[1], "parent") == 0;
    const bool bMatchesExpected =
        ValidInterfacePaths == 2 &&
        ValidStatePaths == 2 &&
        BaselineSubresourceAcceptances == 2 &&
        (bExpectParent
                ? InterfaceRejections == 0 &&
                    StateRejections == 0 &&
                    InvalidSubresourceRejections == 0 &&
                    SelectedMipAcceptances == 0
                : InterfaceRejections == 7 &&
                    StateRejections == 12 &&
                    InvalidSubresourceRejections == 3 &&
                    SelectedMipAcceptances == 1);

    std::cout
        << "interface_rejections=" << InterfaceRejections << "/7\n"
        << "valid_interface_paths=" << ValidInterfacePaths << "/2\n"
        << "state_rejections=" << StateRejections << "/12\n"
        << "valid_state_paths=" << ValidStatePaths << "/2\n"
        << "invalid_subresource_rejections="
        << InvalidSubresourceRejections << "/3\n"
        << "baseline_subresource_acceptances="
        << BaselineSubresourceAcceptances << "/2\n"
        << "selected_mip_acceptances=" << SelectedMipAcceptances << "/1\n"
        << "classification="
        << (bMatchesExpected
                ? (bExpectParent
                        ? "parent-defects"
                        : "current-fixed")
                : "unexpected")
        << '\n';
    return bMatchesExpected ? 0 : 3;
}
