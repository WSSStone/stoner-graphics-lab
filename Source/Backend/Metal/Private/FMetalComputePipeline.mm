#include "FMetalComputePipeline.h"

#include "FMetalBindingMapValidator.h"
#include "FMetalPipelineLayout.h"
#include "FMetalShaderLibrary.h"

#include <iomanip>
#include <new>
#include <sstream>

namespace Stoner::Backend::Metal::Private
{
namespace
{

void AppendDigest(
    std::ostringstream& Stream,
    const RHI::FRHISha256Digest& Digest)
{
    Stream << std::hex << std::setfill('0');
    for (const Core::uint8 Byte : Digest.Bytes)
        Stream << std::setw(2) << static_cast<unsigned int>(Byte);
    Stream << std::dec;
}

} // namespace

FMetalComputePipeline::FMetalComputePipeline(
    Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
    RHI::FRHIComputePipelineDesc Desc,
    id<MTLComputePipelineState> Pipeline) noexcept
    : FMetalNativeObject(
          std::move(Owner), EMetalOwnershipCategory::Pipeline),
      Desc_(std::move(Desc)),
      Pipeline_(Pipeline)
{
}

FMetalComputePipeline::~FMetalComputePipeline() { (void)Invalidate(); }
const RHI::FRHIComputePipelineDesc& FMetalComputePipeline::GetDesc()
    const noexcept { return Desc_; }
Core::TSharedPtr<RHI::IRHIPipelineLayout>
FMetalComputePipeline::GetPipelineLayout() const noexcept
{
    return Desc_.PipelineLayout;
}
RHI::ERHIResourceLifecycleState FMetalComputePipeline::GetLifecycleState()
    const noexcept { return GetLifecycle(); }
RHI::ERHIResult FMetalComputePipeline::Invalidate()
{
    const auto Result = InvalidateObject();
    if (Result == RHI::ERHIResult::Success)
        Desc_.ReuseState = RHI::ERHIPipelineReuseState::Invalidated;
    return Result;
}
id<MTLComputePipelineState> FMetalComputePipeline::GetNativePipeline()
    const noexcept { return Pipeline_; }
void FMetalComputePipeline::MarkReused() noexcept
{
    Desc_.ReuseState = RHI::ERHIPipelineReuseState::Reused;
}

Core::FString BuildMetalComputePipelineKey(
    const RHI::FRHIComputePipelineDesc& Desc)
{
    std::ostringstream Stream;
    Stream << "metal-compute-v1|";
    const auto& Shader = Desc.ShaderModules.front()->GetDesc();
    AppendDigest(Stream, Shader.Payload.PayloadDigest);
    Stream << '|';
    AppendDigest(Stream, Shader.NativeBindingMap.CanonicalDigest);
    Stream << '|';
    for (const auto& Binding : Desc.PipelineLayout->GetDesc().Bindings)
        Stream << Binding.SetIndex << ',' << Binding.BindingSlot << ','
               << static_cast<int>(Binding.DescriptorType) << ','
               << Binding.ArrayCount << ','
               << static_cast<unsigned int>(Binding.Visibility) << ';';
    for (const auto& Range : Desc.PipelineLayout->GetDesc().ConstantRanges)
        Stream << Range.OffsetBytes << ',' << Range.SizeBytes << ','
               << static_cast<unsigned int>(Range.Visibility) << ';';
    return Core::FString(Stream.str());
}

RHI::TRHIObjectResult<RHI::IRHIComputePipeline>
CreateMetalComputePipeline(
    const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner,
    void* NativeDevice,
    const RHI::FRHIDeviceCapabilities& Capabilities,
    const RHI::FRHIComputePipelineDesc& Desc) noexcept
{
    const auto Layout =
        std::dynamic_pointer_cast<FMetalPipelineLayout>(Desc.PipelineLayout);
    if (!Owner || NativeDevice == nullptr || !Layout ||
        !Layout->IsCompatible(Owner) || Desc.ShaderModules.size() != 1 ||
        Desc.RuntimeMode != RHI::ERHIRuntimeObjectMode::RealRuntime ||
        !Capabilities.bSupportsComputeQueue)
        return {RHI::ERHIResult::InvalidState, nullptr};
    const auto Shader =
        std::dynamic_pointer_cast<FMetalShaderLibrary>(Desc.ShaderModules[0]);
    if (!Shader || !Shader->IsCompatible(Owner) ||
        Shader->GetStage() != RHI::ERHIShaderStage::Compute ||
        ValidateMetalBindingMap(
            Shader->GetDesc().NativeBindingMap,
            Shader->GetDesc().InterfaceMetadata,
            Layout->GetDesc(), Capabilities) != RHI::ERHIResult::Success)
        return {RHI::ERHIResult::InvalidState, nullptr};

    @autoreleasepool
    {
        id<MTLDevice> Device = (__bridge id<MTLDevice>)NativeDevice;
        NSError* Error = nil;
        id<MTLComputePipelineState> Pipeline =
            [Device newComputePipelineStateWithFunction:
                        Shader->GetNativeFunction()
                                             error:&Error];
        if (Pipeline == nil || Pipeline.maxTotalThreadsPerThreadgroup == 0 ||
            Pipeline.maxTotalThreadsPerThreadgroup >
                Capabilities.MaxComputeThreadsPerThreadgroup)
            return {RHI::ERHIResult::Failed, nullptr};
        try
        {
            auto RuntimeDesc = Desc;
            RuntimeDesc.ReuseState = RHI::ERHIPipelineReuseState::Created;
            RuntimeDesc.CompatibilitySummary =
                Core::FString("metal-native-compute-v1");
            auto Object = Core::MakeShared<FMetalComputePipeline>(
                Owner, std::move(RuntimeDesc), Pipeline);
            return {RHI::ERHIResult::Success, std::move(Object)};
        }
        catch (const std::bad_alloc&)
        {
            return {RHI::ERHIResult::Failed, nullptr};
        }
    }
}

} // namespace Stoner::Backend::Metal::Private
