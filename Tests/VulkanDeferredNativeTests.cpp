#include "VulkanRHI/FVulkanDevice.h"
#include "VulkanRHI/FVulkanFence.h"
#include "VulkanRHI/FVulkanNativeContext.h"
#include "VulkanRHI/FVulkanQueue.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <fstream>
#include <iostream>

namespace
{

void Record(bool bPassed, const char* Name, int& Failed)
{
    if (!bPassed)
        ++Failed;
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

Stoner::Core::TArray<Stoner::Core::uint32> ReadShaderWords(
    const char* Path)
{
    std::ifstream Stream(Path, std::ios::binary | std::ios::ate);
    if (!Stream)
        return {};
    const std::streamsize Size = Stream.tellg();
    if (Size <= 0 || (Size % sizeof(Stoner::Core::uint32)) != 0)
        return {};
    Stoner::Core::TArray<Stoner::Core::uint32> Words(
        static_cast<std::size_t>(Size) / sizeof(Stoner::Core::uint32));
    Stream.seekg(0);
    Stream.read(reinterpret_cast<char*>(Words.data()), Size);
    return Stream.good() ? Words
                         : Stoner::Core::TArray<Stoner::Core::uint32>{};
}

Stoner::RHI::FRHIShaderModuleDesc MakeShaderDesc(
    Stoner::RHI::ERHIShaderStage Stage,
    const char* Identity,
    Stoner::Core::TArray<Stoner::Core::uint32> Words)
{
    Stoner::RHI::FRHIShaderModuleDesc Desc;
    Desc.Stage = Stage;
    Desc.EntryPoint = "main";
    (void)Stoner::RHI::SetRHIShaderSpirvWords(
        Desc.Payload, Words, Identity);
    return Desc;
}

} // namespace

int RunVulkanDeferredNativeTests()
{
    using namespace Stoner::Backend::Vulkan;
    using namespace Stoner::RHI;

    int Failed = 0;
    FVulkanDevice Device;
    FVulkanInstanceDesc Desc;
    Desc.RuntimeMode = EVulkanInstanceRuntimeMode::DeterministicFallback;
    Desc.bRequestValidation = false;
    if (Device.Initialize(Desc) != ERHIResult::Success)
    {
        Record(false, "Vulkan deferred test device initialization", Failed);
        return Failed;
    }

    const ERHIResult NativeResult = Device.EnableNativeShaderRuntime();
    const bool bRequireNative = std::getenv(
        "STONER_REQUIRE_VULKAN_DEFERRED") != nullptr;
    if (NativeResult != ERHIResult::Success)
    {
        Record(!bRequireNative,
            "Vulkan deferred native runtime is controlled unavailable", Failed);
        (void)Device.Shutdown();
        return Failed;
    }
    Record(Device.GetCapabilities().bSupportsDeferredSubmission,
        "Vulkan native runtime advertises deferred submission capability", Failed);

    const auto QueueResult = Device.CreateCommandQueue(ERHIQueueType::Graphics);
    const auto Context = Device.GetNativeShaderContext();
    const auto Queue = std::dynamic_pointer_cast<FVulkanQueue>(QueueResult.Object);
    Record(QueueResult.Succeeded() && Queue && Context && Context->IsAvailable(),
        "Vulkan deferred native queue and context are available", Failed);
    if (!QueueResult.Succeeded() || !Queue || !Context || !Context->IsAvailable())
    {
        (void)Device.Shutdown();
        return Failed;
    }

    Context->ConfigureDeferredCompletionInjection(true);
    const auto DelayedCommandResult =
        Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    const auto DelayedFenceResult = Device.CreateFence(false);
    const bool bRecordedDelayed = DelayedCommandResult.Succeeded() &&
        DelayedFenceResult.Succeeded() &&
        DelayedCommandResult.Object->Begin() == ERHIResult::Success &&
        DelayedCommandResult.Object->RecordBarrier() == ERHIResult::Success &&
        DelayedCommandResult.Object->End() == ERHIResult::Success;
    Record(bRecordedDelayed,
        "Vulkan deferred delayed command records before submission", Failed);

    const ERHIResult DelayedSubmitResult = bRecordedDelayed
        ? Queue->SubmitDeferred(DelayedCommandResult.Object, {}, {},
            DelayedFenceResult.Object)
        : ERHIResult::InvalidState;
    const Stoner::Core::uint64 DelayedSubmissionId =
        Queue->GetLastDeferredSubmissionId();
    Record(DelayedSubmitResult == ERHIResult::Success &&
            DelayedSubmissionId != 0 &&
            Context->GetPendingDeferredSubmissionCount() == 1,
        "Vulkan SubmitDeferred accepts and retains a native submission", Failed);
    Record(DelayedSubmitResult == ERHIResult::Success &&
            DelayedCommandResult.Object->GetState() ==
                ERHICommandBufferState::Submitted &&
            !DelayedFenceResult.Object->IsSignaled(),
        "Vulkan deferred injected observation gate leaves command and fence pending", Failed);

    const ERHIResult DelayedFiniteWait = DelayedFenceResult.Object->Wait(1000);
    Record(DelayedSubmitResult != ERHIResult::Success ||
            DelayedFiniteWait == ERHIResult::Timeout,
        "Vulkan deferred injected finite wait returns before native observation", Failed);
    Record(DelayedSubmitResult != ERHIResult::Success ||
            Queue->ObserveLastSubmissionCompletion(1000) == ERHIResult::Timeout,
        "Vulkan deferred queue observation remains gated after real submit", Failed);

    // Both slots contain independently submitted native records while host
    // observation is gated. A third admission is rejected transactionally and
    // leaves its command/fence untouched; retiring one real fence frees a slot.
    const auto SaturatedCommandResult =
        Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    const auto SaturatedFenceResult = Device.CreateFence(false);
    const bool bRecordedSaturated = SaturatedCommandResult.Succeeded() &&
        SaturatedFenceResult.Succeeded() &&
        SaturatedCommandResult.Object->Begin() == ERHIResult::Success &&
        SaturatedCommandResult.Object->RecordBarrier() == ERHIResult::Success &&
        SaturatedCommandResult.Object->End() == ERHIResult::Success;
    const ERHIResult SaturatedSubmitResult = bRecordedSaturated
        ? Queue->SubmitDeferred(SaturatedCommandResult.Object, {}, {},
            SaturatedFenceResult.Object)
        : ERHIResult::InvalidState;
    const auto SaturatedSubmissionId = Queue->GetLastDeferredSubmissionId();
    const auto RejectedCommandResult =
        Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    const auto RejectedFenceResult = Device.CreateFence(false);
    const bool bRecordedRejected = RejectedCommandResult.Succeeded() &&
        RejectedFenceResult.Succeeded() &&
        RejectedCommandResult.Object->Begin() == ERHIResult::Success &&
        RejectedCommandResult.Object->RecordBarrier() == ERHIResult::Success &&
        RejectedCommandResult.Object->End() == ERHIResult::Success;
    const ERHIResult RejectedSubmitResult = bRecordedRejected
        ? Queue->SubmitDeferred(RejectedCommandResult.Object, {}, {},
            RejectedFenceResult.Object)
        : ERHIResult::InvalidState;
    Record(DelayedSubmitResult == ERHIResult::Success &&
            SaturatedSubmitResult == ERHIResult::Success &&
            SaturatedSubmissionId != 0 && RejectedSubmitResult == ERHIResult::NotReady &&
            Context->GetPendingDeferredSubmissionCount() == 2 &&
            RejectedCommandResult.Object->GetState() ==
                ERHICommandBufferState::Completed &&
            !RejectedFenceResult.Object->IsSignaled(),
        "Vulkan deferred two-slot admission rejects without mutating rejected state", Failed);

    const ERHIResult SignalResult = DelayedSubmitResult == ERHIResult::Success
        ? Context->SignalDeferredCompletionForTesting(DelayedSubmissionId)
        : ERHIResult::InvalidState;
    const ERHIResult DelayedCompletedWait = DelayedSubmitResult == ERHIResult::Success
        ? DelayedFenceResult.Object->Wait(1000000)
        : ERHIResult::InvalidState;
    Record(DelayedSubmitResult != ERHIResult::Success ||
            (SignalResult == ERHIResult::Success &&
             DelayedCompletedWait == ERHIResult::Success &&
             Context->GetPendingDeferredSubmissionCount() == 1 &&
             DelayedCommandResult.Object->GetState() ==
                 ERHICommandBufferState::Resettable),
        "Vulkan deferred native fence signal retires retained resources", Failed);
    Record(DelayedSubmitResult != ERHIResult::Success ||
            DelayedCommandResult.Object->Reset() == ERHIResult::Success,
        "Vulkan deferred command buffer is reusable after retirement", Failed);

    const ERHIResult SaturatedSignalResult = SaturatedSubmitResult == ERHIResult::Success
        ? Context->SignalDeferredCompletionForTesting(SaturatedSubmissionId)
        : ERHIResult::InvalidState;
    const ERHIResult SaturatedWaitResult = SaturatedSubmitResult == ERHIResult::Success
        ? SaturatedFenceResult.Object->Wait(1000000)
        : ERHIResult::InvalidState;
    Record(SaturatedSubmitResult != ERHIResult::Success ||
            (SaturatedSignalResult == ERHIResult::Success &&
             SaturatedWaitResult == ERHIResult::Success &&
             SaturatedCommandResult.Object->GetState() ==
                 ERHICommandBufferState::Resettable),
        "Vulkan deferred second native slot retires through its real fence", Failed);
    if (SaturatedCommandResult.Succeeded() &&
        SaturatedCommandResult.Object->GetState() ==
            ERHICommandBufferState::Resettable)
        (void)SaturatedCommandResult.Object->Reset();
    if (RejectedCommandResult.Succeeded() &&
        RejectedCommandResult.Object->GetState() ==
            ERHICommandBufferState::Completed)
        (void)RejectedCommandResult.Object->Reset();

    Context->ConfigureDeferredCompletionInjection(false);
    const FRHIBufferDesc SourceDesc{
        32, ERHIBufferUsage::CopySource | ERHIBufferUsage::CopyDestination,
        ERHIMemoryAccess::HostVisible};
    const FRHIBufferDesc DestinationDesc{
        32, ERHIBufferUsage::CopyDestination,
        ERHIMemoryAccess::HostVisible};
    const auto Source = Device.CreateBuffer(SourceDesc);
    const auto Destination = Device.CreateBuffer(DestinationDesc);
    constexpr std::array<Stoner::Core::uint8, 16> FirstBytes = {
        0x10, 0x11, 0x12, 0x13, 0x20, 0x21, 0x22, 0x23,
        0x30, 0x31, 0x32, 0x33, 0x40, 0x41, 0x42, 0x43};
    constexpr std::array<Stoner::Core::uint8, 16> SecondBytes = {
        0xa0, 0xa1, 0xa2, 0xa3, 0xb0, 0xb1, 0xb2, 0xb3,
        0xc0, 0xc1, 0xc2, 0xc3, 0xd0, 0xd1, 0xd2, 0xd3};
    const bool bBuffersCreated = Source.Succeeded() && Destination.Succeeded() &&
        Device.UploadBuffer(Source.Object, {0, FirstBytes.data(), FirstBytes.size()}) ==
            ERHIResult::Success;
    Record(bBuffersCreated,
        "Vulkan deferred native copy fixture creates and uploads persistent buffers", Failed);

    // A buffer update is rejected while a retained submission still owns the
    // native generation. After the real fence completes, the same update is
    // accepted and is observed by the next GPU copy.
    const auto InFlightCommand = Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    const auto InFlightFence = Device.CreateFence(false);
    const bool bRecordedInFlight = bBuffersCreated && InFlightCommand.Succeeded() &&
        InFlightFence.Succeeded() &&
        InFlightCommand.Object->Begin() == ERHIResult::Success &&
        InFlightCommand.Object->RecordBufferCopy(Source.Object, Destination.Object,
            {0, 0, FirstBytes.size()}) == ERHIResult::Success &&
        InFlightCommand.Object->End() == ERHIResult::Success;
    Context->ConfigureDeferredCompletionInjection(true);
    const ERHIResult InFlightSubmit = bRecordedInFlight
        ? Queue->SubmitDeferred(InFlightCommand.Object, {}, {}, InFlightFence.Object)
        : ERHIResult::InvalidState;
    const Stoner::Core::uint64 InFlightSubmissionId =
        Queue->GetLastDeferredSubmissionId();
    const ERHIResult RejectedInFlightUpdate = bBuffersCreated
        ? Device.UploadBuffer(Source.Object,
            {0, SecondBytes.data(), SecondBytes.size()})
        : ERHIResult::InvalidState;
    Record(InFlightSubmit == ERHIResult::Success && InFlightSubmissionId != 0 &&
            RejectedInFlightUpdate == ERHIResult::NotReady,
        "Vulkan deferred native submission protects in-flight buffer contents", Failed);
    const ERHIResult InFlightSignal = InFlightSubmit == ERHIResult::Success
        ? Context->SignalDeferredCompletionForTesting(InFlightSubmissionId)
        : ERHIResult::InvalidState;
    const ERHIResult InFlightWait = InFlightSubmit == ERHIResult::Success
        ? InFlightFence.Object->Wait(1000000)
        : ERHIResult::InvalidState;
    Stoner::Core::TArray<Stoner::Core::uint8> InFlightReadback;
    const ERHIResult InFlightReadbackResult = InFlightWait == ERHIResult::Success
        ? Device.ReadbackBufferForTesting(Destination.Object, 0,
            FirstBytes.size(), InFlightReadback)
        : ERHIResult::InvalidState;
    Record(InFlightSubmit != ERHIResult::Success ||
            (InFlightSignal == ERHIResult::Success &&
             InFlightWait == ERHIResult::Success &&
             InFlightReadbackResult == ERHIResult::Success &&
             InFlightReadback.size() == FirstBytes.size() &&
             std::equal(InFlightReadback.begin(), InFlightReadback.end(),
                 FirstBytes.begin())),
        "Vulkan deferred native copy preserves bytes until fence retirement", Failed);
    if (InFlightCommand.Succeeded() &&
        InFlightCommand.Object->GetState() == ERHICommandBufferState::Resettable)
        (void)InFlightCommand.Object->Reset();

    const auto CopyCommand = Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    auto SubmitCopy = [&](const auto& Expected, const char* Label) -> bool
    {
        if (!bBuffersCreated || !CopyCommand.Succeeded())
            return false;
        if (CopyCommand.Object->Begin() != ERHIResult::Success ||
            CopyCommand.Object->RecordBufferCopy(Source.Object, Destination.Object,
                {0, 0, Expected.size()}) != ERHIResult::Success ||
            CopyCommand.Object->End() != ERHIResult::Success)
            return false;
        const auto Fence = Device.CreateFence(false);
        if (!Fence.Succeeded() ||
            Queue->SubmitDeferred(CopyCommand.Object, {}, {}, Fence.Object) !=
                ERHIResult::Success ||
            Fence.Object->Wait(1000000) != ERHIResult::Success)
            return false;
        Stoner::Core::TArray<Stoner::Core::uint8> Readback;
        if (Device.ReadbackBufferForTesting(Destination.Object, 0,
                Expected.size(), Readback) != ERHIResult::Success ||
            Readback.size() != Expected.size())
            return false;
        for (std::size_t Index = 0; Index < Expected.size(); ++Index)
            if (Readback[Index] != Expected[Index])
                return false;
        Record(true, Label, Failed);
        if (CopyCommand.Object->Reset() != ERHIResult::Success)
            return false;
        return true;
    };

    Context->ConfigureDeferredCompletionInjection(false);
    const bool bChangedUploadAfterRetire = bBuffersCreated &&
        Device.UploadBuffer(Source.Object, {0, SecondBytes.data(), SecondBytes.size()}) ==
            ERHIResult::Success;
    const bool bFirstCopy = bChangedUploadAfterRetire && SubmitCopy(SecondBytes,
        "Vulkan deferred native copy completes with retained resources");
    const auto RealizationCountAfterFirst =
        Context->GetPersistentNativeBufferRealizationCount();
    const auto SourceRevisionAfterFirst =
        Context->GetPersistentNativeBufferUploadedRevisionForTesting(Source.Object);
    const bool bSecondCopy = SubmitCopy(SecondBytes,
        "Vulkan deferred native copy reuses unchanged persistent buffers");
    const auto RealizationCountAfterSecond =
        Context->GetPersistentNativeBufferRealizationCount();
    const auto SourceRevisionAfterSecond =
        Context->GetPersistentNativeBufferUploadedRevisionForTesting(Source.Object);
    Record(bFirstCopy && bSecondCopy && RealizationCountAfterFirst == 2 &&
            RealizationCountAfterSecond == RealizationCountAfterFirst &&
            SourceRevisionAfterSecond == SourceRevisionAfterFirst,
        "Vulkan deferred unchanged upload revision preserves native realization", Failed);

    const bool bChangedUpload = bBuffersCreated &&
        Device.UploadBuffer(Source.Object, {0, FirstBytes.data(), FirstBytes.size()}) ==
            ERHIResult::Success;
    const bool bThirdCopy = SubmitCopy(FirstBytes,
        "Vulkan deferred native copy observes changed upload bytes");
    Record(bChangedUpload && bThirdCopy &&
            Context->GetPersistentNativeBufferRealizationCount() ==
                RealizationCountAfterFirst &&
            Context->GetPersistentNativeBufferUploadedRevisionForTesting(Source.Object) !=
                SourceRevisionAfterFirst,
        "Vulkan deferred changed upload revision updates native bytes without reallocating", Failed);

    // Cache entries use weak RHI owners and are reclaimed after their last
    // retained submission releases. Repeated create/use/drop cycles must not
    // grow the native realization set beyond the live persistent buffers.
    const auto PersistentBaseline =
        Context->GetPersistentNativeBufferRealizationCount();
    bool bEphemeralBounded = true;
    for (int Iteration = 0; Iteration < 4; ++Iteration)
    {
        bool bIteration = false;
        {
            const auto EphemeralSource = Device.CreateBuffer(SourceDesc);
            const auto EphemeralDestination = Device.CreateBuffer(DestinationDesc);
            const auto EphemeralCommand =
                Device.CreateCommandBuffer(ERHIQueueType::Graphics);
            const auto EphemeralFence = Device.CreateFence(false);
            bIteration = EphemeralSource.Succeeded() &&
                EphemeralDestination.Succeeded() && EphemeralCommand.Succeeded() &&
                EphemeralFence.Succeeded() &&
                Device.UploadBuffer(EphemeralSource.Object,
                    {0, FirstBytes.data(), FirstBytes.size()}) ==
                    ERHIResult::Success &&
                EphemeralCommand.Object->Begin() == ERHIResult::Success &&
                EphemeralCommand.Object->RecordBufferCopy(
                    EphemeralSource.Object, EphemeralDestination.Object,
                    {0, 0, FirstBytes.size()}) == ERHIResult::Success &&
                EphemeralCommand.Object->End() == ERHIResult::Success;
            if (bIteration)
            {
                bIteration = Queue->SubmitDeferred(
                        EphemeralCommand.Object, {}, {}, EphemeralFence.Object) ==
                    ERHIResult::Success &&
                    EphemeralFence.Object->Wait(1000000) == ERHIResult::Success;
                Stoner::Core::TArray<Stoner::Core::uint8> Readback;
                bIteration = bIteration &&
                    Device.ReadbackBufferForTesting(
                        EphemeralDestination.Object, 0, FirstBytes.size(),
                        Readback) == ERHIResult::Success &&
                    Readback.size() == FirstBytes.size() &&
                    std::equal(Readback.begin(), Readback.end(), FirstBytes.begin());
                if (EphemeralCommand.Object->GetState() ==
                    ERHICommandBufferState::Resettable)
                    bIteration = EphemeralCommand.Object->Reset() ==
                        ERHIResult::Success && bIteration;
            }
        }
        const auto LiveRealizations =
            Context->GetPersistentNativeBufferRealizationCount();
        bEphemeralBounded = bEphemeralBounded && bIteration &&
            LiveRealizations <= PersistentBaseline;
    }
    Record(bEphemeralBounded,
        "Vulkan deferred persistent native buffer cache remains bounded after drop", Failed);

    // Two real native copies share one stable sampled texture. The first
    // submission leaves it in transfer-source layout; the second returns it
    // to shader-read layout. They must both be admitted, and observing the
    // second fence first must not restore the first submission's stale layout.
    FRHITextureDesc SharedTextureDesc;
    SharedTextureDesc.Width = 2;
    SharedTextureDesc.Height = 2;
    SharedTextureDesc.Format = ERHIFormat::R8G8B8A8_UNorm;
    SharedTextureDesc.Usage = ERHITextureUsage::Sampled |
        ERHITextureUsage::CopySource | ERHITextureUsage::CopyDestination;
    const auto SharedTexture = Device.CreateTexture(SharedTextureDesc);
    constexpr std::array<Stoner::Core::uint8, 16> SharedPixels = {
        7, 8, 9, 255, 17, 18, 19, 255,
        27, 28, 29, 255, 37, 38, 39, 255};
    FRHITextureUploadDesc SharedUpload;
    SharedUpload.Width = 2;
    SharedUpload.Height = 2;
    SharedUpload.RowPitchBytes = 8;
    SharedUpload.Data = SharedPixels.data();
    SharedUpload.DataSizeBytes = SharedPixels.size();
    const bool bSharedTextureReady = SharedTexture.Succeeded() &&
        Device.UploadTexture(SharedTexture.Object, SharedUpload) ==
            ERHIResult::Success;
    const FRHIBufferDesc TextureReadbackDesc{
        SharedPixels.size(), ERHIBufferUsage::CopyDestination,
        ERHIMemoryAccess::HostVisible};
    const auto TextureReadbackA = Device.CreateBuffer(TextureReadbackDesc);
    const auto TextureReadbackB = Device.CreateBuffer(TextureReadbackDesc);
    struct FTextureCopyRecord
    {
        Stoner::Core::TSharedPtr<IRHICommandBuffer> Command;
        Stoner::Core::TSharedPtr<IRHIFence> Fence;
        ERHIResult BeginResult = ERHIResult::InvalidState;
        ERHIResult ToCopyResult = ERHIResult::InvalidState;
        ERHIResult CopyResult = ERHIResult::InvalidState;
        ERHIResult ToShaderReadResult = ERHIResult::InvalidState;
        ERHIResult EndResult = ERHIResult::InvalidState;
    };
    const auto PrepareTextureCopy = [&](
        const Stoner::Core::TSharedPtr<IRHIBuffer>& Destination,
        ERHIResourceLayout BeforeLayout,
        bool bReturnToShaderRead) -> FTextureCopyRecord
    {
        FTextureCopyRecord Result;
        const auto CommandResult = Device.CreateCommandBuffer(
            ERHIQueueType::Graphics);
        const auto FenceResult = Device.CreateFence(false);
        if (!CommandResult.Succeeded() || !FenceResult.Succeeded() ||
            !Destination)
            return Result;
        FRHIResourceBarrierDesc ToCopy;
        ToCopy.Texture = SharedTexture.Object;
        ToCopy.RequiredTextureUsage = ERHITextureUsage::CopySource;
        ToCopy.Before = BeforeLayout;
        ToCopy.After = ERHIResourceLayout::CopySource;
        Result.BeginResult = CommandResult.Object->Begin();
        if (Result.BeginResult != ERHIResult::Success)
            return Result;
        Result.ToCopyResult = BeforeLayout == ERHIResourceLayout::CopySource
            ? ERHIResult::Success
            : CommandResult.Object->RecordLayoutTransition(ToCopy);
        if (Result.ToCopyResult != ERHIResult::Success)
            return Result;
        Result.CopyResult = CommandResult.Object->RecordTextureToBufferCopy(
            SharedTexture.Object, Destination,
            {0, 0, 0, 0, 0, 2, 2, 1, 0, 0, 0});
        if (Result.CopyResult != ERHIResult::Success)
            return Result;
        if (bReturnToShaderRead)
        {
            ToCopy.Before = ERHIResourceLayout::CopySource;
            ToCopy.After = ERHIResourceLayout::ShaderReadOnly;
            Result.ToShaderReadResult =
                CommandResult.Object->RecordLayoutTransition(ToCopy);
            if (Result.ToShaderReadResult != ERHIResult::Success)
                return Result;
        }
        else
        {
            Result.ToShaderReadResult = ERHIResult::Success;
        }
        Result.EndResult = CommandResult.Object->End();
        const bool bRecorded = Result.EndResult == ERHIResult::Success;
        if (!bRecorded)
            return Result;
        Result.Command = CommandResult.Object;
        Result.Fence = FenceResult.Object;
        return Result;
    };

    Context->ConfigureDeferredCompletionInjection(true);
    const auto FirstTextureCopy = bSharedTextureReady &&
        TextureReadbackA.Succeeded()
        ? PrepareTextureCopy(TextureReadbackA.Object,
            ERHIResourceLayout::ShaderReadOnly, false)
        : FTextureCopyRecord{};
    const ERHIResult FirstTextureSubmit = FirstTextureCopy.Command
        ? Queue->SubmitDeferred(FirstTextureCopy.Command, {}, {},
            FirstTextureCopy.Fence)
        : ERHIResult::InvalidState;
    const Stoner::Core::uint64 FirstTextureId =
        FirstTextureSubmit == ERHIResult::Success
        ? Queue->GetLastDeferredSubmissionId() : 0;
    const auto SecondTextureCopy = FirstTextureSubmit == ERHIResult::Success &&
        TextureReadbackB.Succeeded()
        ? PrepareTextureCopy(TextureReadbackB.Object,
            ERHIResourceLayout::CopySource, true)
        : FTextureCopyRecord{};
    const ERHIResult SecondTextureSubmit = SecondTextureCopy.Command
        ? Queue->SubmitDeferred(SecondTextureCopy.Command, {}, {},
            SecondTextureCopy.Fence)
        : ERHIResult::InvalidState;
    const Stoner::Core::uint64 SecondTextureId =
        SecondTextureSubmit == ERHIResult::Success
        ? Queue->GetLastDeferredSubmissionId() : 0;
    const ERHIResult UploadWhileTextureLeased = bSharedTextureReady
        ? Device.UploadTexture(SharedTexture.Object, SharedUpload)
        : ERHIResult::InvalidState;
    if (FirstTextureSubmit != ERHIResult::Success ||
        SecondTextureSubmit != ERHIResult::Success ||
        Context->GetPendingDeferredSubmissionCount() != 2)
    {
        std::cout << "[DETAIL] shared texture admission: ready="
            << bSharedTextureReady
            << " first-record=(" << static_cast<int>(FirstTextureCopy.BeginResult)
            << ',' << static_cast<int>(FirstTextureCopy.ToCopyResult)
            << ',' << static_cast<int>(FirstTextureCopy.CopyResult)
            << ',' << static_cast<int>(FirstTextureCopy.ToShaderReadResult)
            << ',' << static_cast<int>(FirstTextureCopy.EndResult)
            << ") first-submit=" << static_cast<int>(FirstTextureSubmit)
            << " second-record=(" << static_cast<int>(SecondTextureCopy.BeginResult)
            << ',' << static_cast<int>(SecondTextureCopy.ToCopyResult)
            << ',' << static_cast<int>(SecondTextureCopy.CopyResult)
            << ',' << static_cast<int>(SecondTextureCopy.ToShaderReadResult)
            << ',' << static_cast<int>(SecondTextureCopy.EndResult)
            << ") second-submit=" << static_cast<int>(SecondTextureSubmit)
            << " pending=" << Context->GetPendingDeferredSubmissionCount()
            << " upload=" << static_cast<int>(UploadWhileTextureLeased) << '\n';
    }
    Record(FirstTextureSubmit == ERHIResult::Success &&
            SecondTextureSubmit == ERHIResult::Success &&
            FirstTextureId != 0 && SecondTextureId != 0 &&
            Context->GetPendingDeferredSubmissionCount() == 2 &&
            UploadWhileTextureLeased == ERHIResult::NotReady,
        "Vulkan deferred accepts two pending copies sharing a sampled texture lease", Failed);

    const ERHIResult SecondTextureSignal = SecondTextureSubmit == ERHIResult::Success
        ? Context->SignalDeferredCompletionForTesting(SecondTextureId)
        : ERHIResult::InvalidState;
    const ERHIResult SecondTextureWait = SecondTextureSubmit == ERHIResult::Success
        ? SecondTextureCopy.Fence->Wait(1000000)
        : ERHIResult::InvalidState;
    Stoner::Core::TArray<Stoner::Core::uint8> ReadbackB;
    const ERHIResult ReadbackBResult = SecondTextureWait == ERHIResult::Success
        ? Device.ReadbackBufferForTesting(TextureReadbackB.Object, 0,
            SharedPixels.size(), ReadbackB)
        : ERHIResult::InvalidState;
    Stoner::Core::TArray<Stoner::Core::uint8> TextureWhileFirstPending;
    const ERHIResult TextureWhileFirstPendingResult =
        SecondTextureWait == ERHIResult::Success
        ? Device.ReadbackTextureForTesting(SharedTexture.Object, 0,
            TextureWhileFirstPending)
        : ERHIResult::InvalidState;
    if (SecondTextureSubmit == ERHIResult::Success &&
        (SecondTextureSignal != ERHIResult::Success ||
         SecondTextureWait != ERHIResult::Success ||
         ReadbackBResult != ERHIResult::Success ||
         TextureWhileFirstPendingResult != ERHIResult::NotReady))
    {
        std::cout << "[DETAIL] shared texture reverse observation: signal="
            << static_cast<int>(SecondTextureSignal)
            << " wait=" << static_cast<int>(SecondTextureWait)
            << " readback=" << static_cast<int>(ReadbackBResult)
            << " texture-readback="
            << static_cast<int>(TextureWhileFirstPendingResult)
            << " pending=" << Context->GetPendingDeferredSubmissionCount()
            << '\n';
    }
    Record(SecondTextureSubmit == ERHIResult::Success &&
            SecondTextureSignal == ERHIResult::Success &&
            SecondTextureWait == ERHIResult::Success &&
            ReadbackBResult == ERHIResult::Success &&
            ReadbackB.size() == SharedPixels.size() &&
            std::equal(ReadbackB.begin(), ReadbackB.end(), SharedPixels.begin()) &&
            TextureWhileFirstPendingResult == ERHIResult::NotReady &&
            Context->GetPendingDeferredSubmissionCount() == 1,
        "Vulkan deferred out-of-order observation keeps first texture lease pending", Failed);

    const ERHIResult FirstTextureSignal = FirstTextureSubmit == ERHIResult::Success
        ? Context->SignalDeferredCompletionForTesting(FirstTextureId)
        : ERHIResult::InvalidState;
    const ERHIResult FirstTextureWait = FirstTextureSubmit == ERHIResult::Success
        ? FirstTextureCopy.Fence->Wait(1000000)
        : ERHIResult::InvalidState;
    Stoner::Core::TArray<Stoner::Core::uint8> ReadbackA;
    const ERHIResult ReadbackAResult = FirstTextureWait == ERHIResult::Success
        ? Device.ReadbackBufferForTesting(TextureReadbackA.Object, 0,
            SharedPixels.size(), ReadbackA)
        : ERHIResult::InvalidState;
    Stoner::Core::TArray<Stoner::Core::uint8> TextureReadback;
    const ERHIResult TextureReadbackResult = FirstTextureWait == ERHIResult::Success
        ? Device.ReadbackTextureForTesting(SharedTexture.Object, 0,
            TextureReadback)
        : ERHIResult::InvalidState;
    if (FirstTextureSubmit == ERHIResult::Success &&
        (FirstTextureSignal != ERHIResult::Success ||
         FirstTextureWait != ERHIResult::Success ||
         ReadbackAResult != ERHIResult::Success ||
         TextureReadbackResult != ERHIResult::Success))
    {
        std::cout << "[DETAIL] shared texture reverse completion: signal="
            << static_cast<int>(FirstTextureSignal)
            << " wait=" << static_cast<int>(FirstTextureWait)
            << " readback=" << static_cast<int>(ReadbackAResult)
            << " texture-readback=" << static_cast<int>(TextureReadbackResult)
            << " pending=" << Context->GetPendingDeferredSubmissionCount()
            << '\n';
    }
    Record(FirstTextureSubmit == ERHIResult::Success &&
            FirstTextureSignal == ERHIResult::Success &&
            FirstTextureWait == ERHIResult::Success &&
            ReadbackAResult == ERHIResult::Success &&
            ReadbackA.size() == SharedPixels.size() &&
            std::equal(ReadbackA.begin(), ReadbackA.end(), SharedPixels.begin()) &&
            TextureReadbackResult == ERHIResult::Success &&
            TextureReadback.size() == SharedPixels.size() &&
            std::equal(TextureReadback.begin(), TextureReadback.end(), SharedPixels.begin()) &&
            Context->GetPendingDeferredSubmissionCount() == 0,
        "Vulkan deferred out-of-order completion preserves scheduled texture layout", Failed);
    if (FirstTextureCopy.Command &&
        FirstTextureCopy.Command->GetState() == ERHICommandBufferState::Resettable)
        (void)FirstTextureCopy.Command->Reset();
    if (SecondTextureCopy.Command &&
        SecondTextureCopy.Command->GetState() == ERHICommandBufferState::Resettable)
        (void)SecondTextureCopy.Command->Reset();

    Context->ConfigureDeferredCompletionInjection(true);
    const auto InvalidationReadbackA = Device.CreateBuffer(TextureReadbackDesc);
    const auto InvalidationReadbackB = Device.CreateBuffer(TextureReadbackDesc);
    const auto InvalidationCopyA = bSharedTextureReady &&
        InvalidationReadbackA.Succeeded()
        ? PrepareTextureCopy(InvalidationReadbackA.Object,
            ERHIResourceLayout::ShaderReadOnly, false)
        : FTextureCopyRecord{};
    const ERHIResult InvalidationSubmitA = InvalidationCopyA.Command
        ? Queue->SubmitDeferred(InvalidationCopyA.Command, {}, {},
            InvalidationCopyA.Fence)
        : ERHIResult::InvalidState;
    const auto InvalidationIdA = InvalidationSubmitA == ERHIResult::Success
        ? Queue->GetLastDeferredSubmissionId() : 0;
    const auto InvalidationCopyB = InvalidationSubmitA == ERHIResult::Success &&
        InvalidationReadbackB.Succeeded()
        ? PrepareTextureCopy(InvalidationReadbackB.Object,
            ERHIResourceLayout::CopySource, false)
        : FTextureCopyRecord{};
    const ERHIResult InvalidationSubmitB = InvalidationCopyB.Command
        ? Queue->SubmitDeferred(InvalidationCopyB.Command, {}, {},
            InvalidationCopyB.Fence)
        : ERHIResult::InvalidState;
    const auto InvalidationIdB = InvalidationSubmitB == ERHIResult::Success
        ? Queue->GetLastDeferredSubmissionId() : 0;
    const auto TextureCountBeforeInvalidation = Context->GetSnapshot().LiveTextures;
    const ERHIResult InvalidateWhileLeased =
        (InvalidationSubmitA == ERHIResult::Success &&
         InvalidationSubmitB == ERHIResult::Success)
        ? SharedTexture.Object->Invalidate() : ERHIResult::InvalidState;
    const auto TextureCountAfterInvalidation = Context->GetSnapshot().LiveTextures;
    const ERHIResult InvalidationSignalB = InvalidationSubmitB == ERHIResult::Success
        ? Context->SignalDeferredCompletionForTesting(InvalidationIdB)
        : ERHIResult::InvalidState;
    const ERHIResult InvalidationWaitB = InvalidationSubmitB == ERHIResult::Success
        ? InvalidationCopyB.Fence->Wait(1000000) : ERHIResult::InvalidState;
    const auto TextureCountAfterFirstLease = Context->GetSnapshot().LiveTextures;
    const ERHIResult InvalidationSignalA = InvalidationSubmitA == ERHIResult::Success
        ? Context->SignalDeferredCompletionForTesting(InvalidationIdA)
        : ERHIResult::InvalidState;
    const ERHIResult InvalidationWaitA = InvalidationSubmitA == ERHIResult::Success
        ? InvalidationCopyA.Fence->Wait(1000000) : ERHIResult::InvalidState;
    const auto TextureCountAfterFinalLease = Context->GetSnapshot().LiveTextures;
    Record(InvalidationSubmitA == ERHIResult::Success &&
            InvalidationSubmitB == ERHIResult::Success &&
            InvalidateWhileLeased == ERHIResult::Success &&
            TextureCountAfterInvalidation == TextureCountBeforeInvalidation &&
            InvalidationSignalB == ERHIResult::Success &&
            InvalidationWaitB == ERHIResult::Success &&
            TextureCountAfterFirstLease == TextureCountBeforeInvalidation &&
            InvalidationSignalA == ERHIResult::Success &&
            InvalidationWaitA == ERHIResult::Success &&
            TextureCountAfterFinalLease + 1 == TextureCountBeforeInvalidation,
        "Vulkan deferred texture invalidation waits for its final native lease", Failed);

    // A submitted draw retains its native pipeline independently of the
    // public wrapper. Invalidation may clear the wrapper immediately, but it
    // must leave the VkPipeline and VkPipelineLayout alive until the native
    // fence has completed and the readback has been finalized.
    FRHIPipelineLayoutDesc PipelineLayoutDesc;
    PipelineLayoutDesc.Bindings = {{
        0,
        0,
        ERHIDescriptorType::UniformBuffer,
        1,
        ERHIShaderStageFlags::Vertex | ERHIShaderStageFlags::Fragment}};
    const auto DrawPipelineLayout =
        Device.CreatePipelineLayout(PipelineLayoutDesc);
    const auto DrawVertexShader = Device.CreateShaderModule(MakeShaderDesc(
        ERHIShaderStage::Vertex,
        "deferred-pipeline-invalidation-vs",
        ReadShaderWords("Content/Shaders/Triangle/Triangle.vert.spv")));
    const auto DrawFragmentShader = Device.CreateShaderModule(MakeShaderDesc(
        ERHIShaderStage::Fragment,
        "deferred-pipeline-invalidation-fs",
        ReadShaderWords("Content/Shaders/Triangle/Triangle.frag.spv")));
    FRHIGraphicsPipelineDesc DrawPipelineDesc;
    DrawPipelineDesc.PipelineLayout = DrawPipelineLayout.Object;
    DrawPipelineDesc.ShaderModules = {
        DrawVertexShader.Object, DrawFragmentShader.Object};
    DrawPipelineDesc.VertexInput.Stride = sizeof(float) * 5;
    DrawPipelineDesc.VertexInput.Attributes = {
        {0, ERHIFormat::R32G32_Float, 0},
        {1, ERHIFormat::R32G32B32_Float, sizeof(float) * 2}};
    DrawPipelineDesc.Rasterizer.CullMode = ERHICullMode::None;
    DrawPipelineDesc.RenderTargets.ColorFormats = {
        ERHIFormat::R8G8B8A8_UNorm};
    const auto DrawPipeline = Device.CreateGraphicsPipeline(DrawPipelineDesc);
    FRHIRenderPassDesc DrawPassDesc;
    DrawPassDesc.Attachments = {{
        ERHIAttachmentRole::Color,
        ERHIFormat::R8G8B8A8_UNorm,
        ERHISampleCount::One,
        ERHIAttachmentLoadOp::Clear,
        ERHIAttachmentStoreOp::Store}};
    const auto DrawPass = Device.CreateRenderPass(DrawPassDesc);
    FRHITextureDesc DrawTargetDesc;
    DrawTargetDesc.Width = 8;
    DrawTargetDesc.Height = 8;
    DrawTargetDesc.Format = ERHIFormat::R8G8B8A8_UNorm;
    DrawTargetDesc.Usage = ERHITextureUsage::ColorAttachment |
        ERHITextureUsage::CopySource;
    const auto DrawTarget = Device.CreateTexture(DrawTargetDesc);
    FRHIFramebufferDesc DrawFramebufferDesc;
    DrawFramebufferDesc.RenderPass = DrawPass.Object;
    DrawFramebufferDesc.Attachments = {{DrawTarget.Object}};
    DrawFramebufferDesc.Width = DrawTargetDesc.Width;
    DrawFramebufferDesc.Height = DrawTargetDesc.Height;
    const auto DrawFramebuffer = Device.CreateFramebuffer(DrawFramebufferDesc);
    const auto DrawVertexBuffer = Device.CreateBuffer({
        sizeof(float) * 15,
        ERHIBufferUsage::Vertex,
        ERHIMemoryAccess::HostVisible});
    const auto DrawReadback = Device.CreateBuffer({
        DrawTargetDesc.Width * DrawTargetDesc.Height * 4,
        ERHIBufferUsage::CopyDestination,
        ERHIMemoryAccess::HostVisible});
    constexpr std::array<float, 15> TriangleVertices = {
        -0.5f, -0.5f, 1.0f, 0.0f, 0.0f,
         0.5f, -0.5f, 0.0f, 1.0f, 0.0f,
         0.0f,  0.5f, 0.0f, 0.0f, 1.0f};
    const ERHIResult DrawUploadResult = DrawVertexBuffer.Succeeded()
        ? Device.UploadBuffer(DrawVertexBuffer.Object,
            {0, TriangleVertices.data(), sizeof(TriangleVertices)})
        : ERHIResult::InvalidState;
    const auto DrawCommand = Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    const auto DrawFence = Device.CreateFence(false);
    const bool bDrawResources = DrawPipelineLayout.Succeeded() &&
        DrawVertexShader.Succeeded() && DrawFragmentShader.Succeeded() &&
        DrawPipeline.Succeeded() && DrawPass.Succeeded() &&
        DrawTarget.Succeeded() && DrawFramebuffer.Succeeded() &&
        DrawVertexBuffer.Succeeded() && DrawReadback.Succeeded() &&
        DrawUploadResult == ERHIResult::Success && DrawCommand.Succeeded() &&
        DrawFence.Succeeded();
    bool bDrawRecorded = false;
    if (bDrawResources)
    {
        FRHIResourceBarrierDesc ToCopy;
        ToCopy.Texture = DrawTarget.Object;
        ToCopy.RequiredTextureUsage = ERHITextureUsage::CopySource;
        ToCopy.Before = ERHIResourceLayout::ColorAttachment;
        ToCopy.After = ERHIResourceLayout::CopySource;
        FRHITextureBufferCopyRegion ReadbackRegion;
        ReadbackRegion.Width = DrawTargetDesc.Width;
        ReadbackRegion.Height = DrawTargetDesc.Height;
        ReadbackRegion.Depth = 1;
        bDrawRecorded =
            DrawCommand.Object->Begin() == ERHIResult::Success &&
            DrawCommand.Object->BeginRenderPass(
                DrawPass.Object, DrawFramebuffer.Object) == ERHIResult::Success &&
            DrawCommand.Object->BindGraphicsPipeline(DrawPipeline.Object) ==
                ERHIResult::Success &&
            DrawCommand.Object->BindVertexBuffer(DrawVertexBuffer.Object) ==
                ERHIResult::Success &&
            DrawCommand.Object->SetViewport({
                0.0f, 0.0f,
                static_cast<float>(DrawTargetDesc.Width),
                static_cast<float>(DrawTargetDesc.Height), 0.0f, 1.0f}) ==
                ERHIResult::Success &&
            DrawCommand.Object->SetScissor({
                0, 0, DrawTargetDesc.Width, DrawTargetDesc.Height}) ==
                ERHIResult::Success &&
            DrawCommand.Object->RecordDraw(3, 1) == ERHIResult::Success &&
            DrawCommand.Object->EndRenderPass() == ERHIResult::Success &&
            DrawCommand.Object->RecordLayoutTransition(ToCopy) ==
                ERHIResult::Success &&
            DrawCommand.Object->RecordTextureToBufferCopy(
                DrawTarget.Object, DrawReadback.Object, ReadbackRegion) ==
                ERHIResult::Success &&
            DrawCommand.Object->End() == ERHIResult::Success;
    }
    Record(bDrawResources && bDrawRecorded,
        "Vulkan deferred pending draw records a real pipeline invalidation fixture", Failed);

    Context->ConfigureDeferredCompletionInjection(true);
    const ERHIResult DrawSubmitResult = bDrawRecorded
        ? Queue->SubmitDeferred(DrawCommand.Object, {}, {}, DrawFence.Object)
        : ERHIResult::InvalidState;
    const auto DrawSubmissionId = DrawSubmitResult == ERHIResult::Success
        ? Queue->GetLastDeferredSubmissionId() : 0;
    const auto PipelineCountBeforeInvalidation =
        Context->GetSnapshot().LivePipelines;
    const ERHIResult DrawInvalidateResult = DrawSubmitResult == ERHIResult::Success
        ? DrawPipeline.Object->Invalidate() : ERHIResult::InvalidState;
    const auto PipelineCountWhilePending = Context->GetSnapshot().LivePipelines;
    Record(DrawSubmitResult == ERHIResult::Success && DrawSubmissionId != 0 &&
            DrawInvalidateResult == ERHIResult::Success &&
            PipelineCountWhilePending == PipelineCountBeforeInvalidation &&
            Context->GetPendingDeferredSubmissionCount() == 1,
        "Vulkan deferred invalidated pipeline remains native while draw is pending", Failed);

    const ERHIResult DrawSignalResult = DrawSubmitResult == ERHIResult::Success
        ? Context->SignalDeferredCompletionForTesting(DrawSubmissionId)
        : ERHIResult::InvalidState;
    const ERHIResult DrawWaitResult = DrawSubmitResult == ERHIResult::Success
        ? DrawFence.Object->Wait(1000000) : ERHIResult::InvalidState;
    Stoner::Core::TArray<Stoner::Core::uint8> DrawReadbackBytes;
    const ERHIResult DrawReadbackResult = DrawWaitResult == ERHIResult::Success
        ? Device.ReadbackBufferForTesting(DrawReadback.Object, 0,
            DrawTargetDesc.Width * DrawTargetDesc.Height * 4,
            DrawReadbackBytes)
        : ERHIResult::InvalidState;
    bool bDrawProducedColor = false;
    for (std::size_t Index = 0; Index + 3 < DrawReadbackBytes.size();
         Index += 4)
    {
        bDrawProducedColor = bDrawProducedColor ||
            DrawReadbackBytes[Index] != 0 ||
            DrawReadbackBytes[Index + 1] != 0 ||
            DrawReadbackBytes[Index + 2] != 0;
    }
    Record(DrawSubmitResult == ERHIResult::Success &&
            DrawSignalResult == ERHIResult::Success &&
            DrawWaitResult == ERHIResult::Success &&
            DrawReadbackResult == ERHIResult::Success && bDrawProducedColor &&
            Context->GetPendingDeferredSubmissionCount() == 0 &&
            PipelineCountWhilePending == PipelineCountBeforeInvalidation &&
            Context->GetSnapshot().LivePipelines + 1 ==
                PipelineCountBeforeInvalidation,
        "Vulkan deferred draw readback retires invalidated pipeline after native completion", Failed);
    if (DrawCommand.Object &&
        DrawCommand.Object->GetState() == ERHICommandBufferState::Resettable)
        (void)DrawCommand.Object->Reset();
    Context->ConfigureDeferredCompletionInjection(false);

    // Exercise the formal synchronous facade after a real vkQueueSubmit. The
    // injected failure represents an observation error after submission, so
    // the first native record remains retained for teardown and subsequent
    // work is rejected with the same terminal result instead of growing an
    // unbounded failed-record list.
    const auto SyncFailureCommand =
        Device.CreateCommandBuffer(ERHIQueueType::Graphics);
    const bool bSyncFailureRecorded = bBuffersCreated &&
        SyncFailureCommand.Succeeded() &&
        SyncFailureCommand.Object->Begin() == ERHIResult::Success &&
        SyncFailureCommand.Object->RecordBufferCopy(
            Source.Object, Destination.Object, {0, 0, FirstBytes.size()}) ==
            ERHIResult::Success &&
        SyncFailureCommand.Object->End() == ERHIResult::Success;
    Context->ConfigureSynchronousObservationFailureForTesting(true);
    const ERHIResult FirstSyncFailure = bSyncFailureRecorded
        ? Queue->Submit(SyncFailureCommand.Object, {}, {}, {})
        : ERHIResult::InvalidState;
    const ERHIResult SecondSyncFailure = bSyncFailureRecorded
        ? Queue->Submit(SyncFailureCommand.Object, {}, {}, {})
        : ERHIResult::InvalidState;
    const ERHIResult ThirdSyncFailure = bSyncFailureRecorded
        ? Queue->Submit(SyncFailureCommand.Object, {}, {}, {})
        : ERHIResult::InvalidState;
    const auto RejectedDeferredFence = Device.CreateFence(false);
    const ERHIResult DeferredAfterSyncFailure =
        bSyncFailureRecorded && RejectedDeferredFence.Succeeded()
        ? Queue->SubmitDeferred(SyncFailureCommand.Object, {}, {},
            RejectedDeferredFence.Object)
        : ERHIResult::InvalidState;
    const ERHIResult UploadAfterSyncFailure = bBuffersCreated
        ? Device.UploadBuffer(Source.Object,
            {0, FirstBytes.data(), FirstBytes.size()})
        : ERHIResult::InvalidState;
    Record(bSyncFailureRecorded && FirstSyncFailure == ERHIResult::Failed &&
            SecondSyncFailure == FirstSyncFailure &&
            ThirdSyncFailure == FirstSyncFailure &&
            DeferredAfterSyncFailure == FirstSyncFailure &&
            RejectedDeferredFence.Succeeded() &&
            !RejectedDeferredFence.Object->IsSignaled() &&
            UploadAfterSyncFailure == ERHIResult::NotReady &&
            SyncFailureCommand.Object->GetState() ==
                ERHICommandBufferState::Completed &&
            Context->GetPendingDeferredSubmissionCount() == 0,
        "Vulkan synchronous post-submit failure is terminal and bounded", Failed);
    Context->ConfigureSynchronousObservationFailureForTesting(false);

    const ERHIResult ExpectedShutdownResult =
        bSyncFailureRecorded && FirstSyncFailure == ERHIResult::Failed
        ? ERHIResult::Failed
        : ERHIResult::Success;
    Record(Device.Shutdown() == ExpectedShutdownResult,
        "Vulkan deferred native test preserves synchronous failure through teardown", Failed);
    return Failed;
}
