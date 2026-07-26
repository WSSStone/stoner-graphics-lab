#include "VulkanRHI/VulkanDevice.h"

#include <iostream>
#include <limits>
#include <memory>
#include <type_traits>

namespace
{

using namespace Stoner::Backend::Vulkan;
using namespace Stoner::Core;
using namespace Stoner::RHI;

void Print(const char* Name, bool Value)
{
    std::cout << Name << '=' << (Value ? 1 : 0) << '\n';
}

[[nodiscard]] FRHIBufferDesc BufferDesc(uint64 Size)
{
    FRHIBufferDesc Desc;
    Desc.SizeInBytes = Size;
    Desc.Usage = ERHIBufferUsage::Storage;
    return Desc;
}

[[nodiscard]] FRHITextureDesc TextureDesc()
{
    FRHITextureDesc Desc;
    Desc.Width = 4;
    Desc.Height = 4;
    Desc.Format = ERHIFormat::R8G8B8A8_UNorm;
    Desc.Usage = ERHITextureUsage::Sampled;
    return Desc;
}

} // namespace

int main()
{
    static_assert(!std::is_copy_constructible_v<FVulkanResourceAllocation>);
    static_assert(!std::is_copy_constructible_v<FVulkanMemoryAllocator>);
    static_assert(!std::is_move_constructible_v<FVulkanMemoryAllocator>);
    static_assert(!std::is_constructible_v<FVulkanBuffer,
        const FRHIBufferDesc&, FVulkanResourceAllocation&&,
        std::shared_ptr<FVulkanMemoryAllocator>>);
    static_assert(!std::is_constructible_v<FVulkanTexture,
        const FRHITextureDesc&, FVulkanResourceAllocation&&,
        std::shared_ptr<FVulkanMemoryAllocator>>);

    bool bPassed = true;

    FVulkanMemoryAllocator Accounting;
    auto Maximum = Accounting.AllocateBuffer(
        BufferDesc(std::numeric_limits<uint64>::max()), true);
    auto Overflow = Accounting.AllocateBuffer(BufferDesc(2), true);
    const auto OverflowSnapshot = Accounting.GetSnapshot();
    const bool bOverflowRejected = Maximum.IsSuccessful() &&
        !Overflow.IsSuccessful() &&
        Overflow.GetFailure() == EVulkanAllocationFailure::ArithmeticOverflow &&
        OverflowSnapshot.AllocatedBytes ==
            std::numeric_limits<uint64>::max() &&
        OverflowSnapshot.LiveAllocationCount == 1;
    Accounting.ConfigureBudgetLimit(2);
    auto BudgetAttempt = Accounting.AllocateBuffer(BufferDesc(1), true);
    const bool bBudgetPreserved = !BudgetAttempt.IsSuccessful() &&
        BudgetAttempt.GetFailure() == EVulkanAllocationFailure::BudgetExceeded &&
        Accounting.GetSnapshot().AllocatedBytes ==
            std::numeric_limits<uint64>::max();
    (void)Accounting.Release(Maximum);

    FVulkanMemoryAllocator Owner;
    FVulkanMemoryAllocator Foreign;
    auto Ticket = Owner.AllocateBuffer(BufferDesc(32), true);
    const bool bUniqueRelease =
        Foreign.Release(Ticket) == ERHIResult::InvalidState &&
        Owner.Release(Ticket) == ERHIResult::Success &&
        Owner.Release(Ticket) == ERHIResult::InvalidState &&
        Owner.GetSnapshot().AllocatedBytes == 0 &&
        Owner.GetSnapshot().LiveAllocationCount == 0;

    FRHITextureDesc Multisampled = TextureDesc();
    Multisampled.SampleCount = ERHISampleCount::Four;
    FRHITextureDesc Wide = TextureDesc();
    Wide.Format = ERHIFormat::R32G32B32_Float;
    FRHITextureDesc Mipped = TextureDesc();
    Mipped.Width = 8;
    Mipped.Height = 8;
    Mipped.MipLevels = 4;
    uint64 MultisampledBytes = 0;
    uint64 WideBytes = 0;
    uint64 MippedBytes = 0;
    const bool bFootprintsExact =
        FVulkanMemoryAllocator::TryEstimateTextureBytes(
            Multisampled, MultisampledBytes) &&
        FVulkanMemoryAllocator::TryEstimateTextureBytes(Wide, WideBytes) &&
        FVulkanMemoryAllocator::TryEstimateTextureBytes(Mipped, MippedBytes) &&
        MultisampledBytes == 256 && WideBytes == 192 && MippedBytes == 340;

    FVulkanDevice Device;
    FVulkanInstanceDesc InstanceDesc;
    InstanceDesc.RuntimeMode =
        EVulkanInstanceRuntimeMode::DeterministicFallback;
    const bool bInitialized = Device.Initialize(InstanceDesc) == ERHIResult::Success;
    FRHIBufferDesc HugeDesc =
        BufferDesc(std::numeric_limits<uint64>::max());
    HugeDesc.MemoryAccess = ERHIMemoryAccess::HostVisible;
    const auto Huge = Device.CreateBuffer(HugeDesc);
    auto VulkanBuffer = std::dynamic_pointer_cast<FVulkanBuffer>(Huge.Object);
    const uint8 Byte = 0x5a;
    const bool bSparseUpload = bInitialized && VulkanBuffer &&
        VulkanBuffer->Upload(&Byte, 1, 0) == ERHIResult::Success &&
        VulkanBuffer->GetUploadedBytes().size() == 1 &&
        VulkanBuffer->Upload(&Byte, 1,
            std::numeric_limits<uint64>::max() - 1) ==
            ERHIResult::Unavailable;
    const bool bShutdown = Device.Shutdown() == ERHIResult::Success &&
        Device.GetAllocationSnapshot().AllocatedBytes == 0 &&
        Device.GetAllocationSnapshot().LiveAllocationCount == 0;

    Print("allocation_overflow_rejected", bOverflowRejected);
    Print("post_overflow_budget_preserved", bBudgetPreserved);
    Print("unique_release_enforced", bUniqueRelease);
    Print("texture_footprints_exact", bFootprintsExact);
    Print("device_only_construction", true);
    Print("sparse_upload_explicit_failure", bSparseUpload);
    Print("shutdown_accounting_zero", bShutdown);

    bPassed = bOverflowRejected && bBudgetPreserved && bUniqueRelease &&
        bFootprintsExact && bSparseUpload && bShutdown;
    std::cout << "classification="
              << (bPassed ? "allocation-resource-contract-hardened"
                          : "allocation-resource-regression")
              << '\n';
    return bPassed ? 0 : 1;
}
