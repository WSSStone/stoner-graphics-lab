#include "FMetalRenderCommandEncoder.h"

#include "FMetalBindingMapValidator.h"
#include "FMetalBuffer.h"
#include "FMetalDescriptorSet.h"
#include "FMetalFramebuffer.h"
#include "FMetalGraphicsPipeline.h"
#include "FMetalRasterizationConvention.h"
#include "FMetalSampler.h"
#include "FMetalShaderLibrary.h"
#include "FMetalTexture.h"
#include "RHI/IRHIRenderPass.h"

#import <Metal/Metal.h>

namespace Stoner::Backend::Metal::Private
{
namespace
{

MTLLoadAction ToLoad(RHI::ERHIAttachmentLoadOp Op) noexcept
{
    switch (Op)
    {
    case RHI::ERHIAttachmentLoadOp::Load: return MTLLoadActionLoad;
    case RHI::ERHIAttachmentLoadOp::Clear: return MTLLoadActionClear;
    case RHI::ERHIAttachmentLoadOp::DontCare: return MTLLoadActionDontCare;
    }
    return MTLLoadActionDontCare;
}
MTLStoreAction ToStore(RHI::ERHIAttachmentStoreOp Op) noexcept
{
    return Op == RHI::ERHIAttachmentStoreOp::Store
        ? MTLStoreActionStore : MTLStoreActionDontCare;
}
MTLPrimitiveType ToPrimitive(RHI::ERHIPrimitiveTopology Topology) noexcept
{
    return Topology == RHI::ERHIPrimitiveTopology::TriangleStrip
        ? MTLPrimitiveTypeTriangleStrip : MTLPrimitiveTypeTriangle;
}
MTLCullMode ToCull(RHI::ERHICullMode Mode) noexcept
{
    if (Mode == RHI::ERHICullMode::Front) return MTLCullModeFront;
    if (Mode == RHI::ERHICullMode::Back) return MTLCullModeBack;
    return MTLCullModeNone;
}
MTLWinding ToWinding(RHI::ERHIFrontFace Face) noexcept
{
    return Face == RHI::ERHIFrontFace::Clockwise
        ? MTLWindingClockwise : MTLWindingCounterClockwise;
}

bool BindResource(
    id<MTLRenderCommandEncoder> Encoder,
    RHI::ERHIShaderStage Stage,
    RHI::ERHINativeResourceClass NativeClass,
    Core::uint32 NativeIndex,
    const FMetalDescriptorResource& Resource) noexcept
{
    const auto Buffer = std::dynamic_pointer_cast<FMetalBuffer>(Resource.Buffer);
    const auto Texture = std::dynamic_pointer_cast<FMetalTexture>(Resource.Texture);
    const auto Sampler = std::dynamic_pointer_cast<FMetalSampler>(Resource.Sampler);
    if (Buffer)
    {
        if (Stage == RHI::ERHIShaderStage::Vertex)
            [Encoder setVertexBuffer:Buffer->GetNativeBuffer()
                              offset:0 atIndex:NativeIndex];
        else
            [Encoder setFragmentBuffer:Buffer->GetNativeBuffer()
                                offset:0 atIndex:NativeIndex];
        return true;
    }
    if (Texture && NativeClass == RHI::ERHINativeResourceClass::Texture)
    {
        if (Stage == RHI::ERHIShaderStage::Vertex)
            [Encoder setVertexTexture:Texture->GetNativeTexture()
                               atIndex:NativeIndex];
        else
            [Encoder setFragmentTexture:Texture->GetNativeTexture()
                                 atIndex:NativeIndex];
        return true;
    }
    if (Sampler && NativeClass == RHI::ERHINativeResourceClass::Sampler)
    {
        if (Stage == RHI::ERHIShaderStage::Vertex)
            [Encoder setVertexSamplerState:Sampler->GetNativeSampler()
                                   atIndex:NativeIndex];
        else
            [Encoder setFragmentSamplerState:Sampler->GetNativeSampler()
                                     atIndex:NativeIndex];
        return true;
    }
    return false;
}

bool BindDescriptor(
    id<MTLRenderCommandEncoder> Encoder,
    const FMetalGraphicsPipeline& Pipeline,
    Core::uint32 SetIndex,
    const FMetalDescriptorSnapshot& Snapshot) noexcept
{
    for (const auto& ShaderBase : Pipeline.GetDesc().ShaderModules)
    {
        const auto Shader =
            std::dynamic_pointer_cast<FMetalShaderLibrary>(ShaderBase);
        if (!Shader) return false;
        for (const auto& Entry : Shader->GetDesc().NativeBindingMap.Entries)
        {
            if (Entry.SetIndex != SetIndex) continue;
            const auto It = Snapshot.find({Entry.BindingSlot, Entry.ArrayElement});
            if (It == Snapshot.end() ||
                !BindResource(
                    Encoder, Entry.Stage, Entry.NativeClass,
                    Entry.NativeIndex, It->second))
                return false;
        }
    }
    return true;
}

} // namespace

RHI::ERHIResult EncodeMetalRenderCommands(
    void* NativeCommandBuffer,
    std::span<const FMetalCommandRecord> Records,
    Core::usize& OutConsumed) noexcept
{
    OutConsumed = 0;
    if (Records.empty() ||
        Records.front().Type != RHI::ERHISymbolicCommandType::BeginRenderPass)
        return RHI::ERHIResult::InvalidState;
    @autoreleasepool
    {
        id<MTLCommandBuffer> CommandBuffer =
            (__bridge id<MTLCommandBuffer>)NativeCommandBuffer;
        const auto& Begin = Records.front();
        const auto Frame =
            std::dynamic_pointer_cast<FMetalFramebuffer>(Begin.Framebuffer);
        if (!CommandBuffer || !Frame) return RHI::ERHIResult::InvalidState;
        MTLRenderPassDescriptor* NativePass =
            [MTLRenderPassDescriptor renderPassDescriptor];
        Core::usize ColorIndex = 0;
        Core::usize ColorClearIndex = 0;
        for (Core::usize Index = 0;
             Index < Frame->GetDesc().Attachments.size(); ++Index)
        {
            const auto& Attachment = Frame->GetDesc().Attachments[Index];
            const auto Texture =
                std::dynamic_pointer_cast<FMetalTexture>(Attachment.Texture);
            const auto* PassAttachment = Frame->GetRenderPass()->GetAttachment(
                static_cast<Core::uint32>(Index));
            if (!Texture || !PassAttachment) return RHI::ERHIResult::InvalidState;
            if (PassAttachment->Role == RHI::ERHIAttachmentRole::Color)
            {
                auto* Target = NativePass.colorAttachments[ColorIndex++];
                Target.texture = Texture->GetNativeTexture();
                Target.level = Attachment.MipLevel;
                Target.slice = Attachment.ArrayLayer;
                Target.loadAction = ToLoad(PassAttachment->LoadOp);
                Target.storeAction = ToStore(PassAttachment->StoreOp);
                if (PassAttachment->LoadOp == RHI::ERHIAttachmentLoadOp::Clear)
                {
                    const auto& Clear = Begin.ClearValues.Colors[ColorClearIndex++];
                    Target.clearColor = MTLClearColorMake(
                        Clear.Red, Clear.Green, Clear.Blue, Clear.Alpha);
                }
            }
            else
            {
                NativePass.depthAttachment.texture = Texture->GetNativeTexture();
                NativePass.depthAttachment.level = Attachment.MipLevel;
                NativePass.depthAttachment.slice = Attachment.ArrayLayer;
                NativePass.depthAttachment.loadAction = ToLoad(PassAttachment->LoadOp);
                NativePass.depthAttachment.storeAction = ToStore(PassAttachment->StoreOp);
                NativePass.depthAttachment.clearDepth = Begin.ClearValues.Depth;
                if (Texture->GetFormat() == RHI::ERHIFormat::D24_UNorm_S8_UInt ||
                    Texture->GetFormat() == RHI::ERHIFormat::S8_UInt)
                {
                    NativePass.stencilAttachment.texture = Texture->GetNativeTexture();
                    NativePass.stencilAttachment.level = Attachment.MipLevel;
                    NativePass.stencilAttachment.slice = Attachment.ArrayLayer;
                    NativePass.stencilAttachment.loadAction = ToLoad(PassAttachment->LoadOp);
                    NativePass.stencilAttachment.storeAction = ToStore(PassAttachment->StoreOp);
                    NativePass.stencilAttachment.clearStencil = Begin.ClearValues.Stencil;
                }
            }
        }
        id<MTLRenderCommandEncoder> Encoder =
            [CommandBuffer renderCommandEncoderWithDescriptor:NativePass];
        if (!Encoder) return RHI::ERHIResult::Failed;
        const auto Fail = [Encoder](RHI::ERHIResult Result) {
            [Encoder endEncoding];
            return Result;
        };
        Core::TSharedPtr<FMetalGraphicsPipeline> Pipeline;
        id<MTLBuffer> IndexBuffer = nil;
        NSUInteger IndexOffset = 0;
        MTLIndexType IndexType = MTLIndexTypeUInt16;
        for (Core::usize Index = 1; Index < Records.size(); ++Index)
        {
            const auto& Record = Records[Index];
            switch (Record.Type)
            {
            case RHI::ERHISymbolicCommandType::EndRenderPass:
                [Encoder endEncoding]; OutConsumed = Index + 1;
                return RHI::ERHIResult::Success;
            case RHI::ERHISymbolicCommandType::BindGraphicsPipeline:
                Pipeline = std::dynamic_pointer_cast<FMetalGraphicsPipeline>(
                    Record.GraphicsPipeline);
                if (!Pipeline) return Fail(RHI::ERHIResult::InvalidState);
                [Encoder setRenderPipelineState:Pipeline->GetNativePipeline()];
                [Encoder setDepthStencilState:Pipeline->GetNativeDepthStencil()];
                [Encoder setCullMode:ToCull(Pipeline->GetDesc().Rasterizer.CullMode)];
                [Encoder setFrontFacingWinding:
                    ToWinding(ResolveMetalFrontFace(
                        Pipeline->GetDesc().Rasterizer.FrontFace))];
                break;
            case RHI::ERHISymbolicCommandType::BindVertexBuffer:
            {
                const auto Buffer =
                    std::dynamic_pointer_cast<FMetalBuffer>(Record.BufferA);
                if (!Buffer) return Fail(RHI::ERHIResult::InvalidState);
                [Encoder setVertexBuffer:Buffer->GetNativeBuffer()
                                  offset:Record.A atIndex:0];
                break;
            }
            case RHI::ERHISymbolicCommandType::BindIndexBuffer:
            {
                const auto Buffer =
                    std::dynamic_pointer_cast<FMetalBuffer>(Record.BufferA);
                if (!Buffer) return Fail(RHI::ERHIResult::InvalidState);
                IndexBuffer = Buffer->GetNativeBuffer();
                IndexOffset = Record.A;
                IndexType = Record.IndexType == RHI::ERHIIndexType::UInt16
                    ? MTLIndexTypeUInt16 : MTLIndexTypeUInt32;
                break;
            }
            case RHI::ERHISymbolicCommandType::BindDescriptorSet:
            {
                if (!Pipeline || !BindDescriptor(
                        Encoder, *Pipeline, Record.DescriptorSetIndex,
                        Record.DescriptorSnapshot))
                    return Fail(RHI::ERHIResult::InvalidState);
                break;
            }
            case RHI::ERHISymbolicCommandType::SetViewport:
                [Encoder setViewport:{Record.Viewport.X, Record.Viewport.Y,
                    Record.Viewport.Width, Record.Viewport.Height,
                    Record.Viewport.MinDepth, Record.Viewport.MaxDepth}];
                break;
            case RHI::ERHISymbolicCommandType::SetScissor:
                [Encoder setScissorRect:{Record.Scissor.X, Record.Scissor.Y,
                    Record.Scissor.Width, Record.Scissor.Height}];
                break;
            case RHI::ERHISymbolicCommandType::Draw:
                if (!Pipeline) return Fail(RHI::ERHIResult::InvalidState);
                [Encoder drawPrimitives:ToPrimitive(Pipeline->GetDesc().Topology)
                               vertexStart:0 vertexCount:Record.A
                             instanceCount:Record.B];
                break;
            case RHI::ERHISymbolicCommandType::DrawIndexed:
                if (!Pipeline || !IndexBuffer)
                    return Fail(RHI::ERHIResult::InvalidState);
                [Encoder drawIndexedPrimitives:ToPrimitive(Pipeline->GetDesc().Topology)
                                     indexCount:Record.IndexedDraw.IndexCount
                                      indexType:IndexType
                                    indexBuffer:IndexBuffer
                              indexBufferOffset:IndexOffset +
                                  Record.IndexedDraw.FirstIndex *
                                      RHI::GetRHIIndexTypeSize(Record.IndexType)
                                  instanceCount:Record.IndexedDraw.InstanceCount
                                     baseVertex:Record.IndexedDraw.VertexOffset
                                   baseInstance:Record.IndexedDraw.FirstInstance];
                break;
            default:
                return Fail(RHI::ERHIResult::InvalidState);
            }
        }
        [Encoder endEncoding];
        return RHI::ERHIResult::InvalidState;
    }
}

} // namespace Stoner::Backend::Metal::Private
