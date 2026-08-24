#include "MetalPipelineTests.h"

#include "Core/SGPlatform.h"
#if SG_PLATFORM_MAC
#include "FMetalBindingMapValidator.h"
#include "FMetalDescriptorSet.h"
#include "FMetalDeviceOwnerState.h"
#include "FMetalPipelineLayout.h"
#include "FMetalRasterizationConvention.h"
#include "MetalRHI/FMetalDeviceFactory.h"
#endif
#include "RHI/FRHIDeviceCapabilities.h"
#include "RHI/FRHINativeBindingMap.h"
#include "RHI/FRHIPipelineLayoutDesc.h"
#include "RHI/FRHIRenderPassDesc.h"

#include <iostream>

namespace
{

using namespace Stoner;
using namespace Stoner::Core;
using namespace Stoner::RHI;

void Record(FMetalPipelineTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

#if SG_PLATFORM_MAC
FRHIDeviceCapabilities MakeCapabilities()
{
    FRHIDeviceCapabilities Capabilities;
    Capabilities.bSupportsGraphicsQueue = true;
    Capabilities.bSupportsComputeQueue = true;
    Capabilities.bSupportsTransferQueue = true;
    Capabilities.bSupportsSynchronization = true;
    Capabilities.MaxInFlightFrames = 3;
    Capabilities.MaxCommandBuffersPerQueue = 64;
    Capabilities.MaxQueuesPerType = 1;
    Capabilities.MaxBufferSizeBytes = 1u << 20u;
    Capabilities.MaxResourceSizeBytes = 1u << 21u;
    Capabilities.MaxTextureDimension1D = 4096;
    Capabilities.MaxTextureDimension2D = 4096;
    Capabilities.MaxTextureDimension3D = 2048;
    Capabilities.MaxTextureArrayLayers = 256;
    Capabilities.MaxPerStageBufferBindings = 31;
    Capabilities.MaxPerStageTextureBindings = 128;
    Capabilities.MaxPerStageSamplerBindings = 16;
    Capabilities.MaxConstantRangeBytes = 4096;
    Capabilities.MaxConstantDataBytesPerStage = 4096;
    Capabilities.MaxComputeThreadgroupSizeX = 1024;
    Capabilities.MaxComputeThreadgroupSizeY = 1024;
    Capabilities.MaxComputeThreadgroupSizeZ = 64;
    Capabilities.MaxComputeThreadsPerThreadgroup = 1024;
    Capabilities.MaxComputeDispatchGroupsX = 65535;
    Capabilities.MaxComputeDispatchGroupsY = 65535;
    Capabilities.MaxComputeDispatchGroupsZ = 65535;
    Capabilities.SupportedSampleCounts =
        static_cast<uint32>(ERHISampleCount::One) |
        static_cast<uint32>(ERHISampleCount::Four);
    Capabilities.Formats = {
        MakeRHIFormatCapabilities(ERHIFormat::R8G8B8A8_UNorm),
        MakeRHIFormatCapabilities(ERHIFormat::D32_Float)};
    return Capabilities;
}

FRHIShaderInterfaceMetadata MakeInterface()
{
    FRHIShaderInterfaceMetadata Interface;
    Interface.Bindings.push_back({
        0, 3, ERHIDescriptorType::UniformBuffer, 2,
        ERHIShaderStageFlags::Vertex});
    Interface.DebugName = FString("metal-binding-test");
    return Interface;
}

FRHIPipelineLayoutDesc MakeLayout()
{
    FRHIPipelineLayoutDesc Layout;
    Layout.Bindings.push_back({
        0, 3, ERHIDescriptorType::UniformBuffer, 2,
        ERHIShaderStageFlags::Vertex});
    return Layout;
}

FRHINativeBindingMap MakeMap()
{
    FRHINativeBindingMap Map;
    Map.PolicyVersion = FString("metal-direct-binding-v1");
    Map.Entries = {
        {ERHIShaderStage::Vertex, 0, 3,
         ERHIDescriptorType::UniformBuffer, 0,
         ERHINativeResourceClass::Buffer, 7},
        {ERHIShaderStage::Vertex, 0, 3,
         ERHIDescriptorType::UniformBuffer, 1,
         ERHINativeResourceClass::Buffer, 11}};
    Map.ReservedRanges = {
        {ERHIShaderStage::Vertex, ERHINativeResourceClass::Buffer,
         0, 1, FString("vertex-input")},
        {ERHIShaderStage::Vertex, ERHINativeResourceClass::Buffer,
         30, 1, FString("constant-data")}};
    Map.LimitSnapshot = {
        {ERHIShaderStage::Vertex, ERHINativeResourceClass::Buffer, 31},
        {ERHIShaderStage::Vertex, ERHINativeResourceClass::Texture, 128},
        {ERHIShaderStage::Vertex, ERHINativeResourceClass::Sampler, 16}};
    (void)FinalizeRHINativeBindingMapDigest(Map);
    return Map;
}

void TestBindingEvidence(FMetalPipelineTestResult& Result)
{
    Record(Result,
        Backend::Metal::Private::ResolveMetalFrontFace(
            ERHIFrontFace::Clockwise) ==
                ERHIFrontFace::CounterClockwise &&
        Backend::Metal::Private::ResolveMetalFrontFace(
            ERHIFrontFace::CounterClockwise) ==
                ERHIFrontFace::Clockwise,
        "Metal adapts canonical RHI winding once after SPIRV-Cross vertex Y flip");

    const auto Capabilities = MakeCapabilities();
    const auto Interface = MakeInterface();
    const auto Layout = MakeLayout();
    auto Map = MakeMap();
    const auto* Element =
        Backend::Metal::Private::FindMetalNativeBinding(
            Map, ERHIShaderStage::Vertex, 0, 3,
            ERHIDescriptorType::UniformBuffer, 1);
    Record(Result,
        IsValidRHIDeviceCapabilities(Capabilities) &&
            Backend::Metal::Private::ValidateMetalBindingMap(
                Map, Interface, Layout, Capabilities) == ERHIResult::Success &&
            Element && Element->NativeIndex == 11,
        "binding validation consumes authoritative native indices");

    Map.Entries.pop_back();
    (void)FinalizeRHINativeBindingMapDigest(Map);
    Record(Result,
        Backend::Metal::Private::ValidateMetalBindingMap(
            Map, Interface, Layout, Capabilities) == ERHIResult::InvalidState,
        "binding validation rejects incomplete array evidence");

    Map = MakeMap();
    Map.Entries.front().NativeIndex = 9;
    Record(Result,
        Backend::Metal::Private::ValidateMetalBindingMap(
            Map, Interface, Layout, Capabilities) == ERHIResult::InvalidState,
        "binding validation rejects digest mismatch");

    Map = MakeMap();
    Map.LimitSnapshot.front().MaxCount = 32;
    (void)FinalizeRHINativeBindingMapDigest(Map);
    Record(Result,
        Backend::Metal::Private::ValidateMetalBindingMap(
            Map, Interface, Layout, Capabilities) == ERHIResult::Unsupported,
        "binding validation rejects cooked limits beyond the device");

    FRHIShaderInterfaceMetadata CombinedInterface;
    CombinedInterface.Bindings.push_back({
        2, 0, ERHIDescriptorType::CombinedTextureSampler, 1,
        ERHIShaderStageFlags::Fragment});
    FRHIPipelineLayoutDesc CombinedLayout;
    CombinedLayout.Bindings.push_back({
        2, 0, ERHIDescriptorType::CombinedTextureSampler, 1,
        ERHIShaderStageFlags::Fragment});
    FRHINativeBindingMap CombinedMap;
    CombinedMap.PolicyVersion = FString("metal-direct-binding-v1");
    CombinedMap.Entries = {
        {ERHIShaderStage::Fragment, 2, 0,
         ERHIDescriptorType::CombinedTextureSampler, 0,
         ERHINativeResourceClass::Texture, 0},
        {ERHIShaderStage::Fragment, 2, 0,
         ERHIDescriptorType::CombinedTextureSampler, 0,
         ERHINativeResourceClass::Sampler, 0}};
    CombinedMap.ReservedRanges = {{
        ERHIShaderStage::Fragment, ERHINativeResourceClass::Buffer,
        30, 1, FString("constant-data")}};
    CombinedMap.LimitSnapshot = {
        {ERHIShaderStage::Fragment, ERHINativeResourceClass::Buffer, 31},
        {ERHIShaderStage::Fragment, ERHINativeResourceClass::Texture, 128},
        {ERHIShaderStage::Fragment, ERHINativeResourceClass::Sampler, 16}};
    const bool bCombinedFinalized =
        FinalizeRHINativeBindingMapDigest(CombinedMap);
    const bool bCombinedAccepted = bCombinedFinalized &&
        Backend::Metal::Private::ValidateMetalBindingMap(
            CombinedMap, CombinedInterface, CombinedLayout,
            Capabilities) == ERHIResult::Success;
    CombinedMap.Entries.pop_back();
    (void)FinalizeRHINativeBindingMapDigest(CombinedMap);
    Record(Result,
        bCombinedAccepted &&
            Backend::Metal::Private::ValidateMetalBindingMap(
                CombinedMap, CombinedInterface, CombinedLayout,
                Capabilities) == ERHIResult::InvalidState,
        "binding validation requires paired combined texture and sampler entries");
}

void TestLayoutAndNativeProbe(FMetalPipelineTestResult& Result)
{
    auto Owner = MakeShared<Backend::Metal::Private::FMetalDeviceOwnerState>(91);
    auto Layout = MakeShared<Backend::Metal::Private::FMetalPipelineLayout>(
        Owner, MakeLayout());
    auto Set = MakeShared<Backend::Metal::Private::FMetalDescriptorSet>(
        Owner, Layout, 0);
    const bool Lifecycle = Layout->GetSetCount() == 1 &&
        Layout->FindBinding(0, 3) != nullptr &&
        Set->GetBoundResourceCount() == 0 &&
        Layout->Invalidate() == ERHIResult::Success &&
        Set->UpdateBuffer(3, 0, nullptr) == ERHIResult::InvalidState;
    Record(Result, Lifecycle,
        "layout invalidation blocks descriptor mutation before native binding");

    const auto Created = Backend::Metal::CreateMetalDevice();
    if (!Created.Succeeded())
    {
        Record(Result, Created.Result == ERHIResult::Unavailable,
            "native pipeline scope is controlled unavailable without a device");
        return;
    }
    FRHIRenderPassDesc InvalidPass;
    const auto Rejected = Created.Device->CreateRenderPass(InvalidPass);
    Record(Result,
        !Rejected.Succeeded() && Rejected.Result == ERHIResult::InvalidState,
        "native device rejects invalid render scope before allocation");
}
#endif

} // namespace

FMetalPipelineTestResult RunMetalPipelineTests()
{
    FMetalPipelineTestResult Result;
#if SG_PLATFORM_MAC
    TestBindingEvidence(Result);
    TestLayoutAndNativeProbe(Result);
#else
    Record(Result, true, "Metal pipeline implementation is excluded off macOS");
#endif
    return Result;
}
