#include "FMetalComputeCommandEncoder.h"

#include "FMetalBuffer.h"
#include "FMetalComputePipeline.h"
#include "FMetalDescriptorSet.h"
#include "FMetalSampler.h"
#include "FMetalShaderLibrary.h"
#include "FMetalTexture.h"

#import <Metal/Metal.h>

namespace Stoner::Backend::Metal::Private
{
namespace
{

bool BindDescriptor(
    id<MTLComputeCommandEncoder> Encoder,
    const FMetalComputePipeline& Pipeline,
    Core::uint32 SetIndex,
    const FMetalDescriptorSnapshot& Snapshot) noexcept
{
    const auto Shader = std::dynamic_pointer_cast<FMetalShaderLibrary>(
        Pipeline.GetDesc().ShaderModules.front());
    if (!Shader) return false;
    for (const auto& Entry : Shader->GetDesc().NativeBindingMap.Entries)
    {
        if (Entry.SetIndex != SetIndex) continue;
        const auto It = Snapshot.find({Entry.BindingSlot, Entry.ArrayElement});
        if (It == Snapshot.end()) return false;
        const auto Buffer =
            std::dynamic_pointer_cast<FMetalBuffer>(It->second.Buffer);
        const auto Texture =
            std::dynamic_pointer_cast<FMetalTexture>(It->second.Texture);
        const auto Sampler =
            std::dynamic_pointer_cast<FMetalSampler>(It->second.Sampler);
        if (Buffer)
            [Encoder setBuffer:Buffer->GetNativeBuffer()
                        offset:0 atIndex:Entry.NativeIndex];
        else if (Texture)
            [Encoder setTexture:Texture->GetNativeTexture()
                         atIndex:Entry.NativeIndex];
        else if (Sampler)
            [Encoder setSamplerState:Sampler->GetNativeSampler()
                              atIndex:Entry.NativeIndex];
        else return false;
    }
    return true;
}

} // namespace

RHI::ERHIResult EncodeMetalComputeCommands(
    void* NativeCommandBuffer,
    std::span<const FMetalCommandRecord> Records,
    const RHI::FRHIDeviceCapabilities& Capabilities,
    Core::usize& OutConsumed) noexcept
{
    OutConsumed = 0;
    if (Records.empty() ||
        Records.front().Type != RHI::ERHISymbolicCommandType::BindComputePipeline)
        return RHI::ERHIResult::InvalidState;
    @autoreleasepool
    {
        id<MTLCommandBuffer> CommandBuffer =
            (__bridge id<MTLCommandBuffer>)NativeCommandBuffer;
        id<MTLComputeCommandEncoder> Encoder =
            [CommandBuffer computeCommandEncoder];
        if (!Encoder) return RHI::ERHIResult::Failed;
        const auto Fail = [Encoder](RHI::ERHIResult Result) {
            [Encoder endEncoding];
            return Result;
        };
        Core::TSharedPtr<FMetalComputePipeline> Pipeline;
        for (Core::usize Index = 0; Index < Records.size(); ++Index)
        {
            const auto& Record = Records[Index];
            if (Record.Type == RHI::ERHISymbolicCommandType::BindComputePipeline)
            {
                Pipeline = std::dynamic_pointer_cast<FMetalComputePipeline>(
                    Record.ComputePipeline);
                if (!Pipeline) return Fail(RHI::ERHIResult::InvalidState);
                [Encoder setComputePipelineState:Pipeline->GetNativePipeline()];
            }
            else if (Record.Type == RHI::ERHISymbolicCommandType::BindDescriptorSet)
            {
                if (!Pipeline || !BindDescriptor(
                        Encoder, *Pipeline, Record.DescriptorSetIndex,
                        Record.DescriptorSnapshot))
                    return Fail(RHI::ERHIResult::InvalidState);
            }
            else if (Record.Type == RHI::ERHISymbolicCommandType::Dispatch)
            {
                if (!Pipeline || Record.A > Capabilities.MaxComputeDispatchGroupsX ||
                    Record.B > Capabilities.MaxComputeDispatchGroupsY ||
                    Record.C > Capabilities.MaxComputeDispatchGroupsZ)
                    return Fail(RHI::ERHIResult::Unsupported);
                [Encoder dispatchThreadgroups:MTLSizeMake(Record.A, Record.B, Record.C)
                         threadsPerThreadgroup:MTLSizeMake(1, 1, 1)];
            }
            else break;
            OutConsumed = Index + 1;
        }
        [Encoder endEncoding];
        return OutConsumed > 0
            ? RHI::ERHIResult::Success : RHI::ERHIResult::InvalidState;
    }
}

} // namespace Stoner::Backend::Metal::Private
