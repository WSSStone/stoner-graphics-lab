#include "FMetalGraphicsPipeline.h"

#include "FMetalBindingMapValidator.h"
#include "FMetalFormat.h"
#include "FMetalPipelineLayout.h"
#include "FMetalShaderLibrary.h"

#include <iomanip>
#include <new>
#include <sstream>

namespace Stoner::Backend::Metal::Private
{
namespace
{

MTLVertexFormat ToVertexFormat(RHI::ERHIFormat Format) noexcept
{
    switch (Format)
    {
    case RHI::ERHIFormat::R32_Float: return MTLVertexFormatFloat;
    case RHI::ERHIFormat::R32G32_Float: return MTLVertexFormatFloat2;
    case RHI::ERHIFormat::R32G32B32_Float: return MTLVertexFormatFloat3;
    case RHI::ERHIFormat::R32G32B32A32_Float: return MTLVertexFormatFloat4;
    case RHI::ERHIFormat::R8G8B8A8_UNorm: return MTLVertexFormatUChar4Normalized;
    default: return MTLVertexFormatInvalid;
    }
}

MTLBlendFactor ToBlendFactor(RHI::ERHIBlendFactor Factor) noexcept
{
    switch (Factor)
    {
    case RHI::ERHIBlendFactor::Zero: return MTLBlendFactorZero;
    case RHI::ERHIBlendFactor::One: return MTLBlendFactorOne;
    case RHI::ERHIBlendFactor::SourceAlpha: return MTLBlendFactorSourceAlpha;
    case RHI::ERHIBlendFactor::OneMinusSourceAlpha:
        return MTLBlendFactorOneMinusSourceAlpha;
    }
    return MTLBlendFactorZero;
}

MTLBlendOperation ToBlendOperation(RHI::ERHIBlendOp Operation) noexcept
{
    switch (Operation)
    {
    case RHI::ERHIBlendOp::Add: return MTLBlendOperationAdd;
    case RHI::ERHIBlendOp::Subtract: return MTLBlendOperationSubtract;
    case RHI::ERHIBlendOp::ReverseSubtract:
        return MTLBlendOperationReverseSubtract;
    }
    return MTLBlendOperationAdd;
}

MTLCompareFunction ToCompareFunction(RHI::ERHICompareOp Operation) noexcept
{
    switch (Operation)
    {
    case RHI::ERHICompareOp::Never: return MTLCompareFunctionNever;
    case RHI::ERHICompareOp::Less: return MTLCompareFunctionLess;
    case RHI::ERHICompareOp::LessEqual: return MTLCompareFunctionLessEqual;
    case RHI::ERHICompareOp::Equal: return MTLCompareFunctionEqual;
    case RHI::ERHICompareOp::GreaterEqual:
        return MTLCompareFunctionGreaterEqual;
    case RHI::ERHICompareOp::Greater: return MTLCompareFunctionGreater;
    case RHI::ERHICompareOp::NotEqual: return MTLCompareFunctionNotEqual;
    case RHI::ERHICompareOp::Always: return MTLCompareFunctionAlways;
    }
    return MTLCompareFunctionNever;
}

void AppendDigest(
    std::ostringstream& Stream,
    const RHI::FRHISha256Digest& Digest)
{
    Stream << std::hex << std::setfill('0');
    for (const Core::uint8 Byte : Digest.Bytes)
        Stream << std::setw(2) << static_cast<unsigned int>(Byte);
    Stream << std::dec;
}

bool CollectShaders(
    const RHI::FRHIGraphicsPipelineDesc& Desc,
    const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner,
    const RHI::FRHIDeviceCapabilities& Capabilities,
    const FMetalPipelineLayout& Layout,
    Core::TSharedPtr<FMetalShaderLibrary>& OutVertex,
    Core::TSharedPtr<FMetalShaderLibrary>& OutFragment) noexcept
{
    if (Desc.ShaderModules.size() != 2) return false;
    for (const auto& Shader : Desc.ShaderModules)
    {
        auto Native = std::dynamic_pointer_cast<FMetalShaderLibrary>(Shader);
        if (!Native || !Native->IsCompatible(Owner) ||
            ValidateMetalBindingMap(
                Native->GetDesc().NativeBindingMap,
                Native->GetDesc().InterfaceMetadata,
                Layout.GetDesc(), Capabilities) != RHI::ERHIResult::Success)
            return false;
        if (Native->GetStage() == RHI::ERHIShaderStage::Vertex && !OutVertex)
            OutVertex = std::move(Native);
        else if (Native->GetStage() == RHI::ERHIShaderStage::Fragment &&
                 !OutFragment)
            OutFragment = std::move(Native);
        else
            return false;
    }
    return OutVertex && OutFragment;
}

} // namespace

FMetalGraphicsPipeline::FMetalGraphicsPipeline(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    RHI::FRHIGraphicsPipelineDesc Desc,
    id<MTLRenderPipelineState> Pipeline,
    id<MTLDepthStencilState> DepthStencil) noexcept
    : FMetalNativeObject(
          std::move(Owner), EMetalOwnershipCategory::Pipeline),
      Desc_(std::move(Desc)),
      Pipeline_(Pipeline), DepthStencil_(DepthStencil)
{
}

FMetalGraphicsPipeline::~FMetalGraphicsPipeline() { (void)Invalidate(); }
const RHI::FRHIGraphicsPipelineDesc& FMetalGraphicsPipeline::GetDesc()
    const noexcept { return Desc_; }
Core::TSharedPtr<RHI::IRHIPipelineLayout>
FMetalGraphicsPipeline::GetPipelineLayout() const noexcept
{
    return Desc_.PipelineLayout;
}
RHI::ERHIResourceLifecycleState FMetalGraphicsPipeline::GetLifecycleState()
    const noexcept { return GetLifecycle(); }
RHI::ERHIResult FMetalGraphicsPipeline::Invalidate()
{
    const auto Result = InvalidateObject();
    if (Result == RHI::ERHIResult::Success)
        Desc_.ReuseState = RHI::ERHIPipelineReuseState::Invalidated;
    return Result;
}
id<MTLRenderPipelineState> FMetalGraphicsPipeline::GetNativePipeline()
    const noexcept { return Pipeline_; }
id<MTLDepthStencilState> FMetalGraphicsPipeline::GetNativeDepthStencil()
    const noexcept { return DepthStencil_; }
void FMetalGraphicsPipeline::MarkReused() noexcept
{
    Desc_.ReuseState = RHI::ERHIPipelineReuseState::Reused;
}

Core::FString BuildMetalGraphicsPipelineKey(
    const RHI::FRHIGraphicsPipelineDesc& Desc)
{
    std::ostringstream Stream;
    Stream << "metal-graphics-v1|";
    for (const auto& Shader : Desc.ShaderModules)
    {
        Stream << static_cast<int>(Shader->GetStage()) << ':';
        AppendDigest(Stream, Shader->GetDesc().Payload.PayloadDigest);
        Stream << ':';
        AppendDigest(Stream, Shader->GetDesc().NativeBindingMap.CanonicalDigest);
        Stream << '|';
    }
    const auto& Layout = Desc.PipelineLayout->GetDesc();
    for (const auto& Binding : Layout.Bindings)
        Stream << Binding.SetIndex << ',' << Binding.BindingSlot << ','
               << static_cast<int>(Binding.DescriptorType) << ','
               << Binding.ArrayCount << ','
               << static_cast<unsigned int>(Binding.Visibility) << ';';
    Stream << '|' << Desc.VertexInput.Stride << '|';
    for (const auto& Attribute : Desc.VertexInput.Attributes)
        Stream << Attribute.Location << ',' << static_cast<int>(Attribute.Format)
               << ',' << Attribute.Offset << ';';
    Stream << '|' << static_cast<int>(Desc.Topology) << ','
           << static_cast<int>(Desc.Rasterizer.CullMode) << ','
           << static_cast<int>(Desc.Rasterizer.FrontFace) << ','
           << Desc.Rasterizer.bDepthClampEnabled << ','
           << Desc.Blend.bEnabled << ','
           << static_cast<int>(Desc.Blend.SourceColor) << ','
           << static_cast<int>(Desc.Blend.DestinationColor) << ','
           << static_cast<int>(Desc.Blend.ColorOp) << ','
           << Desc.DepthStencil.bDepthTestEnabled << ','
           << Desc.DepthStencil.bDepthWriteEnabled << ','
           << static_cast<int>(Desc.DepthStencil.DepthCompare) << ','
           << static_cast<int>(Desc.Multisample.SampleCount) << '|';
    for (const auto Format : Desc.RenderTargets.ColorFormats)
        Stream << static_cast<int>(Format) << ',';
    Stream << static_cast<int>(Desc.RenderTargets.DepthStencilFormat);
    return Core::FString(Stream.str());
}

RHI::TRHIObjectResult<RHI::IRHIGraphicsPipeline>
CreateMetalGraphicsPipeline(
    const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner,
    void* NativeDevice,
    const RHI::FRHIDeviceCapabilities& Capabilities,
    const RHI::FRHIGraphicsPipelineDesc& Desc) noexcept
{
    const auto Layout =
        std::dynamic_pointer_cast<FMetalPipelineLayout>(Desc.PipelineLayout);
    if (!Owner || NativeDevice == nullptr ||
        !RHI::IsValidRHIGraphicsPipelineState(Desc) || !Layout ||
        !Layout->IsCompatible(Owner) ||
        Desc.RuntimeMode != RHI::ERHIRuntimeObjectMode::RealRuntime ||
        Desc.Rasterizer.bDepthClampEnabled ||
        Desc.Multisample.bSampleShadingEnabled)
        return {RHI::ERHIResult::InvalidState, nullptr};

    for (const auto Format : Desc.RenderTargets.ColorFormats)
        if (!Capabilities.SupportsFormatUsage(
                Format, RHI::ERHIFormatCapability::ColorAttachment))
            return {RHI::ERHIResult::Unsupported, nullptr};
    if (Desc.RenderTargets.DepthStencilFormat != RHI::ERHIFormat::Unknown &&
        !Capabilities.SupportsFormatUsage(
            Desc.RenderTargets.DepthStencilFormat,
            RHI::ERHIFormatCapability::DepthStencilAttachment))
        return {RHI::ERHIResult::Unsupported, nullptr};
    if (!Capabilities.SupportsSampleCount(Desc.Multisample.SampleCount) ||
        Desc.RenderTargets.ColorFormats.size() > 8)
        return {RHI::ERHIResult::Unsupported, nullptr};

    Core::TSharedPtr<FMetalShaderLibrary> Vertex;
    Core::TSharedPtr<FMetalShaderLibrary> Fragment;
    if (!CollectShaders(Desc, Owner, Capabilities, *Layout, Vertex, Fragment))
        return {RHI::ERHIResult::InvalidState, nullptr};

    @autoreleasepool
    {
        id<MTLDevice> Device = (__bridge id<MTLDevice>)NativeDevice;
        MTLRenderPipelineDescriptor* NativeDesc =
            [[MTLRenderPipelineDescriptor alloc] init];
        NativeDesc.vertexFunction = Vertex->GetNativeFunction();
        NativeDesc.fragmentFunction = Fragment->GetNativeFunction();
        NativeDesc.sampleCount = static_cast<NSUInteger>(
            Desc.Multisample.SampleCount);

        MTLVertexDescriptor* VertexDesc = [[MTLVertexDescriptor alloc] init];
        VertexDesc.layouts[0].stride = Desc.VertexInput.Stride;
        VertexDesc.layouts[0].stepFunction = MTLVertexStepFunctionPerVertex;
        for (const auto& Attribute : Desc.VertexInput.Attributes)
        {
            const MTLVertexFormat Format = ToVertexFormat(Attribute.Format);
            if (Format == MTLVertexFormatInvalid || Attribute.Location >= 31)
                return {RHI::ERHIResult::Unsupported, nullptr};
            VertexDesc.attributes[Attribute.Location].format = Format;
            VertexDesc.attributes[Attribute.Location].offset = Attribute.Offset;
            VertexDesc.attributes[Attribute.Location].bufferIndex = 0;
        }
        NativeDesc.vertexDescriptor = VertexDesc;

        for (Core::usize Index = 0;
             Index < Desc.RenderTargets.ColorFormats.size(); ++Index)
        {
            auto* Attachment = NativeDesc.colorAttachments[Index];
            Attachment.pixelFormat = static_cast<MTLPixelFormat>(
                ToMetalPixelFormat(Desc.RenderTargets.ColorFormats[Index]));
            Attachment.blendingEnabled = Desc.Blend.bEnabled;
            Attachment.sourceRGBBlendFactor =
                ToBlendFactor(Desc.Blend.SourceColor);
            Attachment.destinationRGBBlendFactor =
                ToBlendFactor(Desc.Blend.DestinationColor);
            Attachment.rgbBlendOperation =
                ToBlendOperation(Desc.Blend.ColorOp);
            Attachment.sourceAlphaBlendFactor =
                ToBlendFactor(Desc.Blend.SourceColor);
            Attachment.destinationAlphaBlendFactor =
                ToBlendFactor(Desc.Blend.DestinationColor);
            Attachment.alphaBlendOperation =
                ToBlendOperation(Desc.Blend.ColorOp);
        }
        if (Desc.RenderTargets.DepthStencilFormat != RHI::ERHIFormat::Unknown)
        {
            const auto Format = static_cast<MTLPixelFormat>(ToMetalPixelFormat(
                Desc.RenderTargets.DepthStencilFormat));
            NativeDesc.depthAttachmentPixelFormat = Format;
            if (Desc.RenderTargets.DepthStencilFormat ==
                    RHI::ERHIFormat::D24_UNorm_S8_UInt ||
                Desc.RenderTargets.DepthStencilFormat ==
                    RHI::ERHIFormat::S8_UInt)
                NativeDesc.stencilAttachmentPixelFormat = Format;
        }

        NSError* Error = nil;
        id<MTLRenderPipelineState> Pipeline =
            [Device newRenderPipelineStateWithDescriptor:NativeDesc
                                                   error:&Error];
        if (Pipeline == nil) return {RHI::ERHIResult::Failed, nullptr};

        MTLDepthStencilDescriptor* DepthDesc =
            [[MTLDepthStencilDescriptor alloc] init];
        DepthDesc.depthCompareFunction =
            ToCompareFunction(Desc.DepthStencil.DepthCompare);
        DepthDesc.depthWriteEnabled = Desc.DepthStencil.bDepthWriteEnabled;
        id<MTLDepthStencilState> Depth =
            [Device newDepthStencilStateWithDescriptor:DepthDesc];
        if (Depth == nil) return {RHI::ERHIResult::Failed, nullptr};

        try
        {
            auto RuntimeDesc = Desc;
            RuntimeDesc.ReuseState = RHI::ERHIPipelineReuseState::Created;
            RuntimeDesc.CompatibilitySummary =
                Core::FString("metal-native-graphics-v1");
            auto Object = Core::MakeShared<FMetalGraphicsPipeline>(
                Owner, std::move(RuntimeDesc), Pipeline, Depth);
            return {RHI::ERHIResult::Success, std::move(Object)};
        }
        catch (const std::bad_alloc&)
        {
            return {RHI::ERHIResult::Failed, nullptr};
        }
    }
}

} // namespace Stoner::Backend::Metal::Private
