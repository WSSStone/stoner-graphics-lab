#include "MetalLifecycleStressTests.h"

#include "Core/FPlatformMemory.h"
#include "Core/SGPlatform.h"
#include "FMetalDeviceOwnerState.h"
#include "FMetalInspection.h"
#include "MetalRHI/FMetalDeviceFactory.h"
#include "RHI/RHIMinimal.h"

#include <algorithm>
#include <iostream>
#include <vector>

namespace
{

using namespace Stoner;
using namespace Stoner::Backend::Metal;
using namespace Stoner::Backend::Metal::Private;

void Record(
    FMetalLifecycleStressTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

Core::uint64 Median(std::vector<Core::uint64> Values)
{
    if (Values.empty()) return 0;
    std::sort(Values.begin(), Values.end());
    return Values[Values.size() / 2];
}

bool RunLogicalCycle(Core::uint32 Iteration)
{
    auto Owner = Core::MakeShared<FMetalDeviceOwnerState>(Iteration);
    bool bBalanced = Owner->TryRegisterObject(
        EMetalOwnershipCategory::Resource);
    bBalanced = bBalanced && Owner->TryRegisterObject(
        EMetalOwnershipCategory::Pipeline);
    bBalanced = bBalanced && Owner->TryRegisterObject(
        EMetalOwnershipCategory::Command);
    bBalanced = bBalanced && Owner->TryBeginSubmission();
    Owner->EndSubmission();
    Owner->ReleaseObject(EMetalOwnershipCategory::Command);
    Owner->ReleaseObject(EMetalOwnershipCategory::Pipeline);
    Owner->ReleaseObject(EMetalOwnershipCategory::Resource);
    Owner->StopAdmission();
    Owner->AdvanceGeneration();
    Owner->ReleaseDeviceOwnership();
    return bBalanced && HasZeroMetalOwnership(Owner->Inspect());
}

bool RunNativeCycle(
    const Core::TSharedPtr<RHI::IRHIDevice>& Device)
{
    RHI::FRHIBufferDesc BufferDesc;
    BufferDesc.SizeInBytes = 256;
    BufferDesc.Usage = RHI::ERHIBufferUsage::CopySource |
        RHI::ERHIBufferUsage::CopyDestination;
    BufferDesc.MemoryAccess = RHI::ERHIMemoryAccess::HostVisible;
    RHI::FRHIPipelineLayoutDesc LayoutDesc;
    LayoutDesc.Bindings.push_back({
        0, 0, RHI::ERHIDescriptorType::UniformBuffer, 1,
        RHI::ERHIShaderStageFlags::Vertex});

    auto Buffer = Device->CreateBuffer(BufferDesc);
    auto Pipeline = Device->CreatePipelineLayout(LayoutDesc);
    auto Commands = Device->CreateCommandBuffer(RHI::ERHIQueueType::Graphics);
    if (!Buffer.Succeeded() || !Pipeline.Succeeded() || !Commands.Succeeded())
        return false;
    FMetalBackendInspection Live;
    if (!InspectMetalDevice(Device, Live) ||
        Live.ResourceOwnershipCount == 0 ||
        Live.PipelineOwnershipCount == 0 ||
        Live.CommandOwnershipCount == 0)
        return false;
    Buffer.Object.reset();
    Pipeline.Object.reset();
    Commands.Object.reset();
    FMetalBackendInspection Released;
    return InspectMetalDevice(Device, Released) &&
        Released.LiveObjectCount == 0 &&
        Released.ResourceOwnershipCount == 0 &&
        Released.PipelineOwnershipCount == 0 &&
        Released.CommandOwnershipCount == 0;
}

} // namespace

FMetalLifecycleStressTestResult RunMetalLifecycleStressTests(
    const FMetalTestOptions& Options)
{
    FMetalLifecycleStressTestResult Result;
    std::vector<Core::uint64> Samples;
    bool bBalanced = true;
    Core::TSharedPtr<RHI::IRHIDevice> NativeDevice;
#if SG_PLATFORM_MAC
    const auto Created = CreateMetalDevice();
    if (Created.Succeeded()) NativeDevice = Created.Device;
    else if (Options.bRequestNative)
        bBalanced = false;
#else
    if (Options.bRequestNative) bBalanced = false;
#endif
    std::cout << "[EVIDENCE] metal-lifecycle-native "
              << (NativeDevice ? "native" : "unavailable") << '\n';
    for (Core::uint32 Iteration = 1;
         Iteration <= Options.LifecycleIterations; ++Iteration)
    {
        bBalanced = bBalanced && (NativeDevice
            ? RunNativeCycle(NativeDevice)
            : RunLogicalCycle(Iteration));

        if (Iteration >= 1100 && Iteration % 100 == 0)
        {
            const auto Memory = Core::FPlatformMemory::QueryProcessMemory();
            if (Memory.bAvailable)
            {
                Samples.push_back(Memory.ResidentBytes);
                std::cout << "[EVIDENCE] metal-lifecycle-rss iteration="
                          << Iteration << " bytes=" << Memory.ResidentBytes
                          << '\n';
            }
        }
    }
    if (NativeDevice)
    {
        const auto Shutdown = NativeDevice->Shutdown();
        FMetalBackendInspection Inspection;
        bBalanced = bBalanced && Shutdown == RHI::ERHIResult::Success &&
            InspectMetalDevice(NativeDevice, Inspection) &&
            HasZeroMetalOwnership(Inspection);
    }
    Record(Result, bBalanced,
        "resource, pipeline, command, submission, and shutdown cycles balance");

    bool bRssPassed = true;
    if (Options.LifecycleIterations >= 10000 && Samples.size() == 90)
    {
        const Core::uint64 First = Median(std::vector<Core::uint64>(
            Samples.begin(), Samples.begin() + 10));
        const Core::uint64 Final = Median(std::vector<Core::uint64>(
            Samples.end() - 10, Samples.end()));
        const Core::uint64 Threshold = std::max<Core::uint64>(
            16ull * 1024ull * 1024ull, First / 20ull);
        bRssPassed = Final <= First || Final - First <= Threshold;
        const Core::uint64 Growth = Final > First ? Final - First : 0;
        std::cout << "[EVIDENCE] metal-lifecycle-summary samples=90 first="
                  << First << " final=" << Final << " growth=" << Growth
                  << " allowed=" << Threshold << " passed="
                  << (bRssPassed ? "true" : "false") << '\n';
    }
    else if (Options.LifecycleIterations >= 1100)
    {
        bRssPassed = !Samples.empty();
    }
    Record(Result, bRssPassed,
        "post-warm-up RSS sampling follows the bounded growth protocol");
    return Result;
}
