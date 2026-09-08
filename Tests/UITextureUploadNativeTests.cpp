#include "MetalRHI/FMetalDeviceFactory.h"
#include "FUITextureRegistry.h"
#include "VulkanRHI/FVulkanDevice.h"

#include <array>
#include <cstdlib>
#include <iostream>

namespace
{
using namespace Stoner;
using namespace Stoner::RHI;

int TestUpload(const Core::TSharedPtr<IRHIDevice>& Device, bool bMetal)
{
    int Failed = 0;
    const auto Check = [&](bool Passed, const char* Name)
    {
        std::cout << (Passed ? "[PASS] " : "[FAIL] ")
                  << (bMetal ? "Metal " : "Vulkan ") << Name << '\n';
        if (!Passed) ++Failed;
        return Passed;
    };
    const auto Queue = Device->CreateCommandQueue(ERHIQueueType::Graphics);
    const auto Command = Device->CreateCommandBuffer(ERHIQueueType::Graphics);
    const auto Fence = Device->CreateFence(false);
    const auto Staging = Device->CreateBuffer({1024, ERHIBufferUsage::CopySource,
        ERHIMemoryAccess::HostVisible});
    const auto Readback = Device->CreateBuffer({1024, ERHIBufferUsage::CopyDestination,
        ERHIMemoryAccess::HostVisible});
    FRHITextureDesc TextureDesc;
    TextureDesc.Width = 4; TextureDesc.Height = 3;
    TextureDesc.Format = ERHIFormat::R8G8B8A8_UNorm;
    TextureDesc.Usage = ERHITextureUsage::Sampled | ERHITextureUsage::CopySource |
        ERHITextureUsage::CopyDestination;
    const auto Texture = Device->CreateTexture(TextureDesc);
    if (!Check(Queue.Succeeded() && Command.Succeeded() && Fence.Succeeded() &&
        Staging.Succeeded() && Readback.Succeeded() && Texture.Succeeded(),
        "upload resources created")) return Failed;

    std::array<Core::uint8, 1024> Bytes{};
    for (std::size_t Row = 0; Row < 3; ++Row)
        for (std::size_t Byte = 0; Byte < 16; ++Byte)
            Bytes[256 + Row * 256 + Byte] = static_cast<Core::uint8>(17 + Row * 31 + Byte);
    const auto Before = Device->GetRuntimeSnapshot().NativeOperations;
    if (!Check(Device->UploadBuffer(Staging.Object, {0, Bytes.data(), Bytes.size()}) ==
        ERHIResult::Success && Command.Object->Begin() == ERHIResult::Success,
        "host-visible staging uploaded and command begun")) return Failed;
    FRHIBufferTextureCopyRegion Region;
    Region.Width = 4; Region.Height = 3; Region.SourceOffsetBytes = 256;
    Region.SourceRowLengthTexels = 64;
    const auto Count = Command.Object->GetRecordedCommandCount();
    auto Invalid = Region; Invalid.SourceOffsetBytes = 768;
    Check(Command.Object->RecordBufferToTextureCopy(Staging.Object, Texture.Object, Invalid) !=
        ERHIResult::Success && Command.Object->GetRecordedCommandCount() == Count,
        "out-of-range upload rejected before recording");
    Invalid = Region; Invalid.SourceOffsetBytes = 1;
    Check(Command.Object->RecordBufferToTextureCopy(Staging.Object, Texture.Object, Invalid) !=
        ERHIResult::Success && Command.Object->GetRecordedCommandCount() == Count,
        "unaligned upload rejected before recording");
    Invalid = Region; Invalid.DestinationX = 3;
    Check(Command.Object->RecordBufferToTextureCopy(Staging.Object, Texture.Object, Invalid) !=
        ERHIResult::Success && Command.Object->GetRecordedCommandCount() == Count,
        "out-of-texture upload rejected before recording");
    FRHIResourceBarrierDesc Transition;
    Transition.Texture = Texture.Object;
    Transition.After = ERHIResourceLayout::CopyDestination;
    bool Recorded = Command.Object->RecordLayoutTransition(Transition) == ERHIResult::Success &&
        Command.Object->RecordBufferToTextureCopy(Staging.Object, Texture.Object, Region) == ERHIResult::Success;
    Transition.Before = ERHIResourceLayout::CopyDestination;
    Transition.After = ERHIResourceLayout::ShaderReadOnly;
    Recorded = Recorded && Command.Object->RecordLayoutTransition(Transition) == ERHIResult::Success &&
        Command.Object->End() == ERHIResult::Success;
    if (!Check(Recorded && Queue.Object->SubmitDeferred(Command.Object, {}, {}, Fence.Object) ==
        ERHIResult::Success, "upload submitted through deferred command stream")) return Failed;
    const auto After = Device->GetRuntimeSnapshot().NativeOperations;
    Check(Before.bAvailable && After.bAvailable &&
        After.ImageReadbackCopyCount == Before.ImageReadbackCopyCount &&
        After.ReadbackMapCount == Before.ReadbackMapCount &&
        After.ReadbackWaitCount == Before.ReadbackWaitCount &&
        After.FenceWaitCallCount == Before.FenceWaitCallCount &&
        After.QueueIdleCallCount == Before.QueueIdleCallCount &&
        After.DeviceIdleCallCount == Before.DeviceIdleCallCount,
        "upload submission performs no synchronous wait or image readback");
    if (!Check(Fence.Object->Wait(5000000) == ERHIResult::Success,
        "validation observes upload completion with finite wait")) return Failed;

    const auto ReadCommand = Device->CreateCommandBuffer(ERHIQueueType::Graphics);
    const auto ReadFence = Device->CreateFence(false);
    if (!Check(ReadCommand.Succeeded() && ReadFence.Succeeded(), "readback controls created")) return Failed;
    Transition.Before = ERHIResourceLayout::ShaderReadOnly;
    Transition.After = ERHIResourceLayout::CopySource;
    FRHITextureBufferCopyRegion ReadRegion;
    ReadRegion.Width = 4; ReadRegion.Height = 3;
    ReadRegion.DestinationRowLengthTexels = 64;
    const bool ReadSubmitted = ReadCommand.Object->Begin() == ERHIResult::Success &&
        ReadCommand.Object->RecordLayoutTransition(Transition) == ERHIResult::Success &&
        ReadCommand.Object->RecordTextureToBufferCopy(Texture.Object, Readback.Object, ReadRegion) == ERHIResult::Success &&
        ReadCommand.Object->End() == ERHIResult::Success &&
        Queue.Object->SubmitDeferred(ReadCommand.Object, {}, {}, ReadFence.Object) == ERHIResult::Success &&
        ReadFence.Object->Wait(5000000) == ERHIResult::Success;
    if (!Check(ReadSubmitted, "independent native readback completes")) return Failed;
    Core::TArray<Core::uint8> Actual;
    const auto ReadResult = bMetal
        ? Backend::Metal::ReadMetalBufferForValidation(Device, Readback.Object, 0, 528, Actual)
        : std::dynamic_pointer_cast<Backend::Vulkan::FVulkanDevice>(Device)->ReadbackBufferForTesting(
            Readback.Object, 0, 528, Actual);
    bool Match = ReadResult == ERHIResult::Success && Actual.size() == 528;
    for (std::size_t Row = 0; Match && Row < 3; ++Row)
        for (std::size_t Byte = 0; Byte < 16; ++Byte)
            Match = Match && Actual[Row * 256 + Byte] == Bytes[256 + Row * 256 + Byte];
    Check(Match, "padded nonzero-offset upload roundtrips exact texels");
    {
        using namespace Stoner::Renderer;
        FUITextureRegistry Registry(Device);
        Registry.BeginEligibleFrame(1, true);
        FUITextureRequest Request;
        Request.RequestId = 1; Request.LogicalSlot = 1;
        Request.Width = 4; Request.Height = 3;
        Request.Format = ERHIFormat::R8G8B8A8_UNorm;
        Request.ColorDomain = EUITextureColorDomain::AlphaCoverage;
        Request.PixelBytes.assign(48, 255);
        const auto RegistryBefore = Device->GetRuntimeSnapshot().NativeOperations;
        const auto Prepared = Registry.Prepare(Request);
        auto Lease = Registry.Acquire(Prepared.TextureId);
        auto UploadCommand = Device->CreateCommandBuffer(ERHIQueueType::Graphics).Object;
        auto UploadFence = Device->CreateFence(false).Object;
        FUITextureSubmission Submission;
        const bool Submitted = Prepared.Succeeded() && Lease.IsValid() && UploadCommand && UploadFence &&
            UploadCommand->Begin() == ERHIResult::Success &&
            Registry.RecordSubmission({&Lease, 1}, UploadCommand, Submission) == ERHIResult::Success &&
            UploadCommand->End() == ERHIResult::Success &&
            Queue.Object->SubmitDeferred(UploadCommand, {}, {}, UploadFence) == ERHIResult::Success &&
            Submission.Commit(UploadFence) == ERHIResult::Success;
        if (!Check(Submitted, "registry records and commits a real deferred upload")) return Failed;
        const auto RegistryAfter = Device->GetRuntimeSnapshot().NativeOperations;
        Check(RegistryAfter.ImageReadbackCopyCount == RegistryBefore.ImageReadbackCopyCount &&
            RegistryAfter.ReadbackMapCount == RegistryBefore.ReadbackMapCount &&
            RegistryAfter.ReadbackWaitCount == RegistryBefore.ReadbackWaitCount &&
            RegistryAfter.FenceWaitCallCount == RegistryBefore.FenceWaitCallCount &&
            RegistryAfter.QueueIdleCallCount == RegistryBefore.QueueIdleCallCount &&
            RegistryAfter.DeviceIdleCallCount == RegistryBefore.DeviceIdleCallCount,
            "registry preparation and upload add no synchronous wait or readback");
        FUITextureRequest Destroy;
        Destroy.RequestId = 2; Destroy.Operation = EUITextureOperation::Destroy;
        Destroy.TextureId = Prepared.TextureId; Destroy.ExpectedGeneration = Prepared.TextureId.Generation;
        Check(Registry.Prepare(Destroy).Result == ERHIResult::NotReady,
            "registry native destroy retains prepared snapshot owners");
        const bool Completed = UploadFence->Wait(5000000) == ERHIResult::Success;
        Lease = {}; Submission = {}; UploadCommand.reset(); Registry.Poll();
        Check(Completed && Registry.GetStatistics().Generations == 0 &&
            Registry.GetStatistics().StagingBytes == 0 && Registry.GetStatistics().CPUShadowBytes == 0,
            "registry drains texture shadow and staging after real render completion");
    }
    return Failed;
}
}

int RunUITextureUploadNativeTests()
{
    using namespace Stoner;
    int Failed = 0;
    const bool Required = std::getenv("STONER_REQUIRE_UI_TEXTURE_UPLOAD") != nullptr;
    auto Vulkan = Core::MakeShared<Backend::Vulkan::FVulkanDevice>();
    Backend::Vulkan::FVulkanInstanceDesc Desc;
    Desc.RuntimeMode = Backend::Vulkan::EVulkanInstanceRuntimeMode::DeterministicFallback;
    Desc.bRequestValidation = false;
    if (Vulkan->Initialize(Desc) == RHI::ERHIResult::Success &&
        Vulkan->EnableNativeShaderRuntime() == RHI::ERHIResult::Success)
        Failed += TestUpload(Vulkan, false);
    else
    {
        std::cout << (Required ? "[FAIL] " : "[SKIP] ") << "Vulkan native upload unavailable\n";
        Failed += Required ? 1 : 0;
    }
    (void)Vulkan->Shutdown();
    auto Metal = Backend::Metal::CreateMetalDevice();
    if (Metal.Succeeded())
    {
        Failed += TestUpload(Metal.Device, true);
        (void)Metal.Device->Shutdown();
    }
    else
    {
        const bool MetalRequired = Required && SG_PLATFORM_MAC;
        std::cout << (MetalRequired ? "[FAIL] " : "[SKIP] ") << "Metal native upload unavailable\n";
        Failed += MetalRequired ? 1 : 0;
    }
    return Failed;
}
