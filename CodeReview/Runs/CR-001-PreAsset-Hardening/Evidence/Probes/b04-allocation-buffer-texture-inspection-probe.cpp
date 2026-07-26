#include "RHI/FRHIBufferDesc.h"
#include "RHI/FRHITextureDesc.h"
#include "VulkanRHI/FVulkanBuffer.h"
#include "VulkanRHI/FVulkanDevice.h"
#include "VulkanRHI/FVulkanMemoryAllocator.h"
#include "VulkanRHI/FVulkanTexture.h"

#include <exception>
#include <iostream>
#include <limits>
#include <memory>

using namespace Stoner::Backend::Vulkan;
using namespace Stoner::Core;
using namespace Stoner::RHI;

namespace
{

bool InitializeFallback(FVulkanDevice& Device)
{
    FVulkanInstanceDesc Desc;
    Desc.RuntimeMode = EVulkanInstanceRuntimeMode::DeterministicFallback;
    return Device.Initialize(Desc) == ERHIResult::Success;
}

FRHIBufferDesc MakeBufferDesc(uint64 Size, ERHIMemoryAccess Access)
{
    FRHIBufferDesc Desc;
    Desc.SizeInBytes = Size;
    Desc.Usage = ERHIBufferUsage::CopyDestination;
    Desc.MemoryAccess = Access;
    return Desc;
}

FRHITextureDesc MakeTextureDesc(
    uint32 Width,
    uint32 Height,
    uint32 Mips,
    ERHISampleCount Samples,
    ERHIFormat Format)
{
    FRHITextureDesc Desc;
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.MipLevels = Mips;
    Desc.SampleCount = Samples;
    Desc.Format = Format;
    Desc.Usage = ERHITextureUsage::Sampled;
    return Desc;
}

} // namespace

int main()
{
    FVulkanDevice Device;
    const bool bDeviceReady = InitializeFallback(Device);

    const FRHIBufferDesc HugeDesc = MakeBufferDesc(
        std::numeric_limits<uint64>::max(), ERHIMemoryAccess::HostVisible);
    const auto Huge = Device.CreateBuffer(HugeDesc);
    const auto Tiny = Device.CreateBuffer(
        MakeBufferDesc(2, ERHIMemoryAccess::DeviceLocal));
    const FVulkanAllocationSnapshot Wrapped = Device.GetAllocationSnapshot();
    const bool bAllocationCounterOverflow =
        bDeviceReady && Huge.Succeeded() && Tiny.Succeeded() &&
        Wrapped.AllocatedBytes == 1 && Wrapped.LiveAllocationCount == 2;

    Device.ConfigureAllocationBudget(2);
    const auto BudgetBypass = Device.CreateBuffer(
        MakeBufferDesc(1, ERHIMemoryAccess::DeviceLocal));
    const bool bOverflowBudgetBypass =
        bAllocationCounterOverflow && BudgetBypass.Succeeded() &&
        Device.GetAllocationSnapshot().AllocatedBytes == 2 &&
        Device.GetAllocationSnapshot().LiveAllocationCount == 3;

    bool bOversizedUploadThrows = false;
    const uint8 Byte = 0x5a;
    try
    {
        (void)Huge.Object->Upload(&Byte, 1, 0);
    }
    catch (const std::exception&)
    {
        bOversizedUploadThrows = true;
    }

    auto SharedAllocator = std::make_shared<FVulkanMemoryAllocator>();
    SharedAllocator->ConfigureBudgetLimit(64);
    FVulkanResourceAllocation First = SharedAllocator->AllocateBuffer(
        MakeBufferDesc(32, ERHIMemoryAccess::DeviceLocal), true);
    FVulkanResourceAllocation Second = SharedAllocator->AllocateBuffer(
        MakeBufferDesc(32, ERHIMemoryAccess::DeviceLocal), true);
    FVulkanResourceAllocation FirstCopy = First;
    const bool bDuplicateReleaseAccepted =
        First.IsSuccessful() && Second.IsSuccessful() &&
        SharedAllocator->Release(First) == ERHIResult::Success &&
        SharedAllocator->Release(FirstCopy) == ERHIResult::Success &&
        SharedAllocator->GetSnapshot().AllocatedBytes == 0 &&
        SharedAllocator->GetSnapshot().LiveAllocationCount == 0 &&
        Second.IsSuccessful();
    FVulkanResourceAllocation Third = SharedAllocator->AllocateBuffer(
        MakeBufferDesc(64, ERHIMemoryAccess::DeviceLocal), true);
    const bool bDuplicateReleaseBudgetBypass =
        bDuplicateReleaseAccepted && Third.IsSuccessful() &&
        SharedAllocator->GetSnapshot().LiveAllocationCount == 1;

    FVulkanMemoryAllocator MultisampleAllocator;
    MultisampleAllocator.ConfigureBudgetLimit(64);
    const FRHITextureDesc MultisampleDesc = MakeTextureDesc(
        4, 4, 1, ERHISampleCount::Four, ERHIFormat::R8G8B8A8_UNorm);
    const uint64 MultisampleEstimate =
        FVulkanMemoryAllocator::EstimateTextureBytes(MultisampleDesc);
    FVulkanResourceAllocation Multisample =
        MultisampleAllocator.AllocateTexture(MultisampleDesc, true);
    const bool bMultisampleFootprintUndercounted =
        IsValidRHITextureDesc(MultisampleDesc) &&
        MultisampleEstimate == 64 && Multisample.IsSuccessful();

    FVulkanMemoryAllocator WideFormatAllocator;
    WideFormatAllocator.ConfigureBudgetLimit(64);
    const FRHITextureDesc WideFormatDesc = MakeTextureDesc(
        4, 4, 1, ERHISampleCount::One, ERHIFormat::R32G32B32_Float);
    const uint64 WideFormatEstimate =
        FVulkanMemoryAllocator::EstimateTextureBytes(WideFormatDesc);
    FVulkanResourceAllocation WideFormat =
        WideFormatAllocator.AllocateTexture(WideFormatDesc, true);
    const bool bWideFormatFootprintUndercounted =
        IsValidRHITextureDesc(WideFormatDesc) &&
        WideFormatEstimate == 64 && WideFormat.IsSuccessful();

    FVulkanMemoryAllocator MipAllocator;
    MipAllocator.ConfigureBudgetLimit(340);
    const FRHITextureDesc MipDesc = MakeTextureDesc(
        8, 8, 4, ERHISampleCount::One, ERHIFormat::R8G8B8A8_UNorm);
    const uint64 MipEstimate =
        FVulkanMemoryAllocator::EstimateTextureBytes(MipDesc);
    FVulkanResourceAllocation Mipped =
        MipAllocator.AllocateTexture(MipDesc, true);
    const bool bMipChainFootprintOvercounted =
        IsValidRHITextureDesc(MipDesc) && MipEstimate == 1024 &&
        Mipped.Failure == EVulkanAllocationFailure::BudgetExceeded;

    auto PartialAllocator = std::make_shared<FVulkanMemoryAllocator>();
    const FRHIBufferDesc PartialBufferDesc = MakeBufferDesc(
        16, ERHIMemoryAccess::HostVisible);
    const FVulkanResourceAllocation FailedBufferAllocation =
        FVulkanResourceAllocation::MakeFailure(
            EVulkanResourceKind::Buffer,
            EVulkanAllocationFailure::BudgetExceeded,
            16,
            "injected failure");
    FVulkanBuffer PartialBuffer(
        PartialBufferDesc, FailedBufferAllocation, PartialAllocator);
    const bool bFailedAllocationBufferUsable =
        PartialBuffer.GetLifecycleState() ==
            ERHIResourceLifecycleState::Valid &&
        !PartialBuffer.GetAllocation().IsSuccessful() &&
        PartialBuffer.Upload(&Byte, 1, 0) == ERHIResult::Success;

    const FRHITextureDesc PartialTextureDesc = MakeTextureDesc(
        1, 1, 1, ERHISampleCount::One, ERHIFormat::R8G8B8A8_UNorm);
    const FVulkanResourceAllocation FailedTextureAllocation =
        FVulkanResourceAllocation::MakeFailure(
            EVulkanResourceKind::Texture,
            EVulkanAllocationFailure::BudgetExceeded,
            4,
            "injected failure");
    FVulkanTexture PartialTexture(
        PartialTextureDesc, FailedTextureAllocation, PartialAllocator);
    const bool bFailedAllocationTextureValid =
        PartialTexture.GetLifecycleState() ==
            ERHIResourceLifecycleState::Valid &&
        !PartialTexture.GetAllocation().IsSuccessful();

    if (Device.IsActive())
    {
        (void)Device.Shutdown();
    }

    std::cout
        << "allocation_counter_overflow=" << bAllocationCounterOverflow << '\n'
        << "overflow_budget_bypass=" << bOverflowBudgetBypass << '\n'
        << "oversized_upload_throws=" << bOversizedUploadThrows << '\n'
        << "duplicate_release_accepted=" << bDuplicateReleaseAccepted << '\n'
        << "duplicate_release_budget_bypass="
        << bDuplicateReleaseBudgetBypass << '\n'
        << "multisample_footprint_undercounted="
        << bMultisampleFootprintUndercounted << '\n'
        << "wide_format_footprint_undercounted="
        << bWideFormatFootprintUndercounted << '\n'
        << "mip_chain_footprint_overcounted="
        << bMipChainFootprintOvercounted << '\n'
        << "failed_allocation_buffer_usable="
        << bFailedAllocationBufferUsable << '\n'
        << "failed_allocation_texture_valid="
        << bFailedAllocationTextureValid << '\n';

    const bool bDefectsReproduced = bAllocationCounterOverflow &&
        bOverflowBudgetBypass && bOversizedUploadThrows &&
        bDuplicateReleaseAccepted && bDuplicateReleaseBudgetBypass &&
        bMultisampleFootprintUndercounted &&
        bWideFormatFootprintUndercounted &&
        bMipChainFootprintOvercounted && bFailedAllocationBufferUsable &&
        bFailedAllocationTextureValid;
    std::cout << "classification="
              << (bDefectsReproduced
                      ? "allocation-buffer-texture-contract-defects"
                      : "unexpected")
              << '\n';
    return bDefectsReproduced ? 0 : 3;
}
