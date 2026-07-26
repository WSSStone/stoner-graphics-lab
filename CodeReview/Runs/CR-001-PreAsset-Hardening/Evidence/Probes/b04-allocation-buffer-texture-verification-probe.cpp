#include "VulkanRHI/VulkanDevice.h"

#include <array>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace
{

std::atomic<long long> GFailAfter{-1};

[[nodiscard]] bool ShouldFailAllocation() noexcept
{
    long long Remaining = GFailAfter.load(std::memory_order_relaxed);
    while (Remaining >= 0)
    {
        if (Remaining == 0)
        {
            if (GFailAfter.compare_exchange_weak(Remaining, -1,
                    std::memory_order_relaxed))
            {
                return true;
            }
        }
        else if (GFailAfter.compare_exchange_weak(Remaining, Remaining - 1,
                     std::memory_order_relaxed))
        {
            return false;
        }
    }
    return false;
}

void FailAllocationAfter(long long SuccessfulAllocations) noexcept
{
    GFailAfter.store(SuccessfulAllocations, std::memory_order_relaxed);
}

void DisableAllocationFailure() noexcept
{
    GFailAfter.store(-1, std::memory_order_relaxed);
}

using namespace Stoner::Backend::Vulkan;
using namespace Stoner::Core;
using namespace Stoner::RHI;

struct FProbe
{
    void Check(const char* Name, bool bValue)
    {
        std::cout << Name << '=' << (bValue ? 1 : 0) << '\n';
        bPassed = bPassed && bValue;
    }

    bool bPassed = true;
};

[[nodiscard]] FRHIBufferDesc BufferDesc(
    uint64 Size,
    ERHIMemoryAccess MemoryAccess = ERHIMemoryAccess::DeviceLocal)
{
    FRHIBufferDesc Desc;
    Desc.SizeInBytes = Size;
    Desc.Usage = ERHIBufferUsage::Storage;
    Desc.MemoryAccess = MemoryAccess;
    return Desc;
}

[[nodiscard]] FRHITextureDesc Texture2D(
    uint32 Width,
    uint32 Height,
    ERHIFormat Format = ERHIFormat::R8G8B8A8_UNorm)
{
    FRHITextureDesc Desc;
    Desc.Width = Width;
    Desc.Height = Height;
    Desc.Format = Format;
    Desc.Usage = IsDepthStencilFormat(Format)
        ? ERHITextureUsage::DepthStencilAttachment
        : ERHITextureUsage::Sampled;
    return Desc;
}

[[nodiscard]] bool Initialize(FVulkanDevice& Device)
{
    FVulkanInstanceDesc Desc;
    Desc.RuntimeMode = EVulkanInstanceRuntimeMode::DeterministicFallback;
    return Device.Initialize(Desc) == ERHIResult::Success;
}

[[nodiscard]] bool BufferFactoryRollback(long long FailureIndex)
{
    FVulkanDevice Device;
    if (!Initialize(Device))
    {
        return false;
    }
    FailAllocationAfter(FailureIndex);
    const auto Result = Device.CreateBuffer(BufferDesc(64));
    DisableAllocationFailure();
    const auto Snapshot = Device.GetAllocationSnapshot();
    const bool bRolledBack = Result.Result == ERHIResult::Unavailable &&
        !Result.Object && Snapshot.AllocatedBytes == 0 &&
        Snapshot.LiveAllocationCount == 0;
    (void)Device.Shutdown();
    return bRolledBack;
}

[[nodiscard]] bool TextureFactoryRollback(long long FailureIndex)
{
    FVulkanDevice Device;
    if (!Initialize(Device))
    {
        return false;
    }
    FailAllocationAfter(FailureIndex);
    const auto Result = Device.CreateTexture(Texture2D(4, 4));
    DisableAllocationFailure();
    const auto Snapshot = Device.GetAllocationSnapshot();
    const bool bRolledBack = Result.Result == ERHIResult::Unavailable &&
        !Result.Object && Snapshot.AllocatedBytes == 0 &&
        Snapshot.LiveAllocationCount == 0;
    (void)Device.Shutdown();
    return bRolledBack;
}

} // namespace

void* operator new(std::size_t Size)
{
    if (ShouldFailAllocation())
    {
        throw std::bad_alloc();
    }
    if (void* Memory = std::malloc(Size == 0 ? 1 : Size))
    {
        return Memory;
    }
    throw std::bad_alloc();
}

void* operator new[](std::size_t Size)
{
    return ::operator new(Size);
}

void* operator new(std::size_t Size, const std::nothrow_t&) noexcept
{
    try
    {
        return ::operator new(Size);
    }
    catch (...)
    {
        return nullptr;
    }
}

void* operator new[](std::size_t Size, const std::nothrow_t&) noexcept
{
    return ::operator new(Size, std::nothrow);
}

void operator delete(void* Memory) noexcept
{
    std::free(Memory);
}

void operator delete[](void* Memory) noexcept
{
    std::free(Memory);
}

void operator delete(void* Memory, std::size_t) noexcept
{
    std::free(Memory);
}

void operator delete[](void* Memory, std::size_t) noexcept
{
    std::free(Memory);
}

int main()
{
    static_assert(!std::is_copy_constructible_v<FVulkanResourceAllocation>);
    static_assert(!std::is_copy_assignable_v<FVulkanResourceAllocation>);
    static_assert(std::is_nothrow_move_constructible_v<
        FVulkanResourceAllocation>);
    static_assert(!std::is_move_assignable_v<FVulkanResourceAllocation>);
    static_assert(!std::is_copy_constructible_v<FVulkanMemoryAllocator>);
    static_assert(!std::is_move_constructible_v<FVulkanMemoryAllocator>);
    static_assert(!std::is_constructible_v<FVulkanBuffer,
        const FRHIBufferDesc&, FVulkanResourceAllocation&&,
        std::shared_ptr<FVulkanMemoryAllocator>>);
    static_assert(!std::is_constructible_v<FVulkanTexture,
        const FRHITextureDesc&, FVulkanResourceAllocation&&,
        std::shared_ptr<FVulkanMemoryAllocator>>);

    FProbe Probe;
    Probe.Check("ownership_api_closed", true);

    FVulkanMemoryAllocator MoveOwner;
    auto MoveSource = MoveOwner.AllocateBuffer(BufferDesc(8), true);
    auto MoveTarget = std::move(MoveSource);
    const auto BeforeRelease = MoveOwner.GetSnapshot();
    const bool bMoveTransfer = !MoveSource.IsSuccessful() &&
        MoveSource.IsReleased() && MoveTarget.IsSuccessful() &&
        MoveOwner.Release(MoveSource) == ERHIResult::InvalidState &&
        BeforeRelease.AllocatedBytes == 8 &&
        BeforeRelease.LiveAllocationCount == 1 &&
        MoveOwner.Release(MoveTarget) == ERHIResult::Success &&
        MoveOwner.GetSnapshot().AllocatedBytes == 0 &&
        MoveOwner.GetSnapshot().LiveAllocationCount == 0;
    Probe.Check("move_transfers_release_authority", bMoveTransfer);

    alignas(FVulkanMemoryAllocator)
        std::array<std::byte, sizeof(FVulkanMemoryAllocator)> Storage{};
    auto* FirstOwner = std::construct_at(
        reinterpret_cast<FVulkanMemoryAllocator*>(Storage.data()));
    auto ReusedAddressTicket =
        FirstOwner->AllocateBuffer(BufferDesc(16), true);
    std::destroy_at(FirstOwner);
    auto* SecondOwner = std::construct_at(
        reinterpret_cast<FVulkanMemoryAllocator*>(Storage.data()));
    const bool bAddressReuseRejected =
        SecondOwner->Release(ReusedAddressTicket) ==
            ERHIResult::InvalidState &&
        SecondOwner->GetSnapshot().AllocatedBytes == 0 &&
        SecondOwner->GetSnapshot().LiveAllocationCount == 0;
    std::destroy_at(SecondOwner);
    Probe.Check("allocator_address_reuse_rejected", bAddressReuseRejected);

    FVulkanMemoryAllocator Accounting;
    auto Maximum = Accounting.AllocateBuffer(
        BufferDesc(std::numeric_limits<uint64>::max()), true);
    auto Overflow = Accounting.AllocateBuffer(BufferDesc(1), true);
    const auto OverflowSnapshot = Accounting.GetSnapshot();
    const bool bOverflowAtomic = Maximum.IsSuccessful() &&
        Overflow.GetFailure() == EVulkanAllocationFailure::ArithmeticOverflow &&
        OverflowSnapshot.AllocatedBytes ==
            std::numeric_limits<uint64>::max() &&
        OverflowSnapshot.LiveAllocationCount == 1;
    Accounting.ConfigureBudgetLimit(2);
    auto BudgetBypass = Accounting.AllocateBuffer(BufferDesc(1), true);
    const bool bBudgetClosed =
        BudgetBypass.GetFailure() == EVulkanAllocationFailure::BudgetExceeded &&
        Accounting.GetSnapshot().AllocatedBytes ==
            std::numeric_limits<uint64>::max() &&
        Accounting.GetSnapshot().LiveAllocationCount == 1;
    (void)Accounting.Release(Maximum);
    Probe.Check("counter_overflow_atomic", bOverflowAtomic);
    Probe.Check("post_overflow_budget_closed", bBudgetClosed);

    FVulkanMemoryAllocator CountLimited;
    CountLimited.ConfigureAllocationCountLimit(1);
    auto FirstCounted = CountLimited.AllocateBuffer(BufferDesc(4), true);
    auto CountRejected = CountLimited.AllocateBuffer(BufferDesc(4), true);
    const bool bCountStable = FirstCounted.IsSuccessful() &&
        CountRejected.GetFailure() ==
            EVulkanAllocationFailure::AllocationCountExceeded &&
        CountLimited.GetSnapshot().LiveAllocationCount == 1 &&
        CountLimited.Release(FirstCounted) == ERHIResult::Success;
    Probe.Check("allocation_count_failure_atomic", bCountStable);

    const std::array<std::pair<ERHIFormat, uint64>, 10> Formats = {{
        {ERHIFormat::R8_UNorm, 1},
        {ERHIFormat::R8G8B8A8_UNorm, 4},
        {ERHIFormat::B8G8R8A8_UNorm, 4},
        {ERHIFormat::R16G16B16A16_Float, 8},
        {ERHIFormat::R32_Float, 4},
        {ERHIFormat::R32G32_Float, 8},
        {ERHIFormat::R32G32B32_Float, 12},
        {ERHIFormat::D24_UNorm_S8_UInt, 4},
        {ERHIFormat::D32_Float, 4},
        {ERHIFormat::S8_UInt, 1},
    }};
    bool bFormatsExact = true;
    for (const auto& [Format, BytesPerTexel] : Formats)
    {
        uint64 Bytes = 0;
        bFormatsExact = bFormatsExact &&
            FVulkanMemoryAllocator::TryEstimateTextureBytes(
                Texture2D(3, 2, Format), Bytes) &&
            Bytes == 6 * BytesPerTexel;
    }
    Probe.Check("all_rhi_format_widths_exact", bFormatsExact);

    bool bDimensionsExact = true;
    uint64 Bytes = 0;
    FRHITextureDesc Desc = Texture2D(8, 1);
    Desc.Dimension = ERHITextureDimension::Texture1D;
    Desc.MipLevels = 4;
    bDimensionsExact = bDimensionsExact &&
        FVulkanMemoryAllocator::TryEstimateTextureBytes(Desc, Bytes) &&
        Bytes == 60;
    Desc = Texture2D(8, 4);
    Desc.MipLevels = 4;
    bDimensionsExact = bDimensionsExact &&
        FVulkanMemoryAllocator::TryEstimateTextureBytes(Desc, Bytes) &&
        Bytes == 172;
    Desc = Texture2D(8, 4);
    Desc.Dimension = ERHITextureDimension::Texture3D;
    Desc.Depth = 2;
    Desc.MipLevels = 4;
    bDimensionsExact = bDimensionsExact &&
        FVulkanMemoryAllocator::TryEstimateTextureBytes(Desc, Bytes) &&
        Bytes == 300;
    Desc = Texture2D(4, 4);
    Desc.Dimension = ERHITextureDimension::TextureCube;
    Desc.ArrayLayers = 6;
    bDimensionsExact = bDimensionsExact &&
        FVulkanMemoryAllocator::TryEstimateTextureBytes(Desc, Bytes) &&
        Bytes == 384;
    Desc = Texture2D(4, 1);
    Desc.Dimension = ERHITextureDimension::Texture1DArray;
    Desc.ArrayLayers = 3;
    bDimensionsExact = bDimensionsExact &&
        FVulkanMemoryAllocator::TryEstimateTextureBytes(Desc, Bytes) &&
        Bytes == 48;
    Desc = Texture2D(4, 2);
    Desc.Dimension = ERHITextureDimension::Texture2DArray;
    Desc.ArrayLayers = 3;
    bDimensionsExact = bDimensionsExact &&
        FVulkanMemoryAllocator::TryEstimateTextureBytes(Desc, Bytes) &&
        Bytes == 96;
    Desc = Texture2D(4, 4);
    Desc.Dimension = ERHITextureDimension::TextureCubeArray;
    Desc.ArrayLayers = 12;
    bDimensionsExact = bDimensionsExact &&
        FVulkanMemoryAllocator::TryEstimateTextureBytes(Desc, Bytes) &&
        Bytes == 768;
    Desc = Texture2D(4, 4);
    Desc.SampleCount = ERHISampleCount::Eight;
    bDimensionsExact = bDimensionsExact &&
        FVulkanMemoryAllocator::TryEstimateTextureBytes(Desc, Bytes) &&
        Bytes == 512;
    Probe.Check("dimension_mip_layer_sample_matrix_exact", bDimensionsExact);

    Desc = Texture2D(
        std::numeric_limits<uint32>::max(),
        std::numeric_limits<uint32>::max(), ERHIFormat::R8_UNorm);
    Desc.MipLevels = 2;
    Bytes = 1;
    const bool bMipSumOverflow =
        !FVulkanMemoryAllocator::TryEstimateTextureBytes(Desc, Bytes) &&
        Bytes == 0;
    Probe.Check("mip_sum_overflow_rejected", bMipSumOverflow);

    Probe.Check("buffer_wrapper_allocation_rollback",
        BufferFactoryRollback(0));
    Probe.Check("buffer_control_block_rollback",
        BufferFactoryRollback(1));
    Probe.Check("buffer_tracking_rollback", BufferFactoryRollback(2));
    Probe.Check("texture_wrapper_allocation_rollback",
        TextureFactoryRollback(0));
    Probe.Check("texture_control_block_rollback",
        TextureFactoryRollback(1));
    Probe.Check("texture_tracking_rollback", TextureFactoryRollback(2));

    FVulkanDevice UploadDevice;
    bool bUploadPreserved = Initialize(UploadDevice);
    const auto UploadBuffer = UploadDevice.CreateBuffer(
        BufferDesc(64, ERHIMemoryAccess::HostVisible));
    auto ConcreteBuffer =
        std::dynamic_pointer_cast<FVulkanBuffer>(UploadBuffer.Object);
    const std::array<uint8, 4> Initial = {1, 2, 3, 4};
    const std::array<uint8, 4> Extension = {5, 6, 7, 8};
    bUploadPreserved = bUploadPreserved && ConcreteBuffer &&
        ConcreteBuffer->Upload(Initial.data(), Initial.size(), 0) ==
            ERHIResult::Success;
    FailAllocationAfter(0);
    const ERHIResult FailedUpload = ConcreteBuffer
        ? ConcreteBuffer->Upload(Extension.data(), Extension.size(), 8)
        : ERHIResult::Failed;
    DisableAllocationFailure();
    bUploadPreserved = bUploadPreserved &&
        FailedUpload == ERHIResult::Unavailable &&
        ConcreteBuffer->GetUploadedBytes().size() == Initial.size() &&
        std::equal(ConcreteBuffer->GetUploadedBytes().begin(),
            ConcreteBuffer->GetUploadedBytes().end(), Initial.begin()) &&
        ConcreteBuffer->Upload(
            Extension.data(), Extension.size(), Initial.size()) ==
            ERHIResult::Success &&
        ConcreteBuffer->GetUploadedBytes().size() == 8;
    (void)UploadDevice.Shutdown();
    Probe.Check("host_upload_failure_preserves_prior_bytes", bUploadPreserved);

    FVulkanDevice ExtremeDevice;
    bool bExtremeSparse = Initialize(ExtremeDevice);
    const auto Extreme = ExtremeDevice.CreateBuffer(BufferDesc(
        std::numeric_limits<uint64>::max(),
        ERHIMemoryAccess::HostVisible));
    auto ExtremeBuffer =
        std::dynamic_pointer_cast<FVulkanBuffer>(Extreme.Object);
    const uint8 Byte = 0x7f;
    bExtremeSparse = bExtremeSparse && ExtremeBuffer &&
        ExtremeBuffer->Upload(&Byte, 1, 0) == ERHIResult::Success &&
        ExtremeBuffer->GetUploadedBytes().size() == 1 &&
        ExtremeBuffer->Upload(&Byte, 1,
            std::numeric_limits<uint64>::max() - 1) ==
            ERHIResult::Unavailable;
    (void)ExtremeDevice.Shutdown();
    bExtremeSparse = bExtremeSparse &&
        ExtremeDevice.GetAllocationSnapshot().AllocatedBytes == 0 &&
        ExtremeDevice.GetAllocationSnapshot().LiveAllocationCount == 0;
    Probe.Check("extreme_sparse_upload_and_shutdown", bExtremeSparse);

    std::cout << "classification="
              << (Probe.bPassed
                      ? "allocation-buffer-texture-fixes-independently-verified"
                      : "allocation-buffer-texture-verification-failed")
              << '\n';
    return Probe.bPassed ? 0 : 1;
}
