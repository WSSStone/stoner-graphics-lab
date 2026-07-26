#include "VulkanRHI/VulkanDevice.h"

#include <atomic>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <type_traits>

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
            if (GFailAfter.compare_exchange_weak(
                    Remaining, -1, std::memory_order_relaxed))
            {
                return true;
            }
        }
        else if (GFailAfter.compare_exchange_weak(
                     Remaining, Remaining - 1,
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

[[nodiscard]] bool Initialize(FVulkanDevice& Device)
{
    FVulkanInstanceDesc Desc;
    Desc.RuntimeMode = EVulkanInstanceRuntimeMode::DeterministicFallback;
    return Device.Initialize(Desc) == ERHIResult::Success;
}

[[nodiscard]] FRHIPipelineLayoutDesc LayoutDesc()
{
    FRHIPipelineLayoutDesc Desc;
    Desc.Bindings = {{0, 0, ERHIDescriptorType::UniformBuffer, 1,
        ERHIShaderStageFlags::Vertex}};
    return Desc;
}

[[nodiscard]] FRHIBufferDesc BufferDesc()
{
    return {64, ERHIBufferUsage::Uniform |
        ERHIBufferUsage::CopyDestination};
}

[[nodiscard]] bool PoolCreationRollback(long long FailureIndex)
{
    FVulkanDevice Device;
    if (!Initialize(Device))
    {
        return false;
    }
    Device.ConfigureDescriptorPoolCapacity(1);
    const auto Layout = Device.CreatePipelineLayout(LayoutDesc());
    FailAllocationAfter(FailureIndex);
    const auto Result = Device.CreateDescriptorSet(Layout.Object, 0);
    DisableAllocationFailure();
    const bool bPassed = Result.Result == ERHIResult::Unavailable &&
        !Result.Object && Device.GetDescriptorPoolAllocatedCount() == 0;
    (void)Device.Shutdown();
    return bPassed;
}

[[nodiscard]] bool DescriptorCreationRollback(long long FailureIndex)
{
    FVulkanDevice Device;
    if (!Initialize(Device))
    {
        return false;
    }
    Device.ConfigureDescriptorPoolCapacity(1);
    const auto Layout = Device.CreatePipelineLayout(LayoutDesc());
    const auto Seed = Device.CreateDescriptorSet(Layout.Object, 0);
    if (!Seed.Succeeded() ||
        Seed.Object->Invalidate() != ERHIResult::Success ||
        Device.GetDescriptorPoolAllocatedCount() != 0)
    {
        return false;
    }

    FailAllocationAfter(FailureIndex);
    const auto Result = Device.CreateDescriptorSet(Layout.Object, 0);
    DisableAllocationFailure();
    const bool bPassed = Result.Result == ERHIResult::Unavailable &&
        !Result.Object && Device.GetDescriptorPoolAllocatedCount() == 0;
    (void)Device.Shutdown();
    return bPassed;
}

[[nodiscard]] bool SamplerCreationFailure(long long FailureIndex)
{
    FVulkanDevice Device;
    if (!Initialize(Device))
    {
        return false;
    }
    FailAllocationAfter(FailureIndex);
    const auto Result = Device.CreateSampler({});
    DisableAllocationFailure();
    const bool bPassed = Result.Result == ERHIResult::Unavailable &&
        !Result.Object;
    (void)Device.Shutdown();
    return bPassed;
}

[[nodiscard]] bool UploadCreationFailure(long long FailureIndex)
{
    FVulkanDevice Device;
    if (!Initialize(Device))
    {
        return false;
    }
    const auto Buffer = Device.CreateBuffer(BufferDesc());
    if (!Buffer.Succeeded())
    {
        return false;
    }
    const unsigned char Data[16] = {};
    FailAllocationAfter(FailureIndex);
    const auto Result = Device.StageBufferUpload(
        Buffer.Object, Data, sizeof(Data), {0, sizeof(Data)});
    DisableAllocationFailure();
    const bool bPassed = Result.Result == ERHIResult::Unavailable &&
        !Result.Object;
    (void)Device.Shutdown();
    return bPassed;
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
    static_assert(!std::is_constructible_v<FVulkanDescriptorPool, uint32>);
    static_assert(!std::is_constructible_v<FVulkanDescriptorSet,
        const TSharedPtr<IRHIPipelineLayout>&, uint32,
        FVulkanDescriptorReservation&&>);
    static_assert(!std::is_constructible_v<FVulkanSampler,
        const FRHISamplerDesc&>);
    static_assert(!std::is_default_constructible_v<FVulkanUploadRequest>);
    static_assert(!std::is_copy_constructible_v<
        FVulkanDescriptorReservation>);
    static_assert(std::is_nothrow_move_constructible_v<
        FVulkanDescriptorReservation>);

    const bool bPoolObjectRollback = PoolCreationRollback(0);
    const bool bPoolControlRollback = PoolCreationRollback(1);
    const bool bDescriptorWrapperRollback = DescriptorCreationRollback(0);
    const bool bDescriptorControlRollback = DescriptorCreationRollback(1);
    const bool bDescriptorTrackingRollback = DescriptorCreationRollback(2);
    const bool bSamplerWrapperFailure = SamplerCreationFailure(0);
    const bool bSamplerControlFailure = SamplerCreationFailure(1);
    const bool bSamplerTrackingFailure = SamplerCreationFailure(2);
    const bool bUploadWrapperFailure = UploadCreationFailure(0);
    const bool bUploadControlFailure = UploadCreationFailure(1);
    const bool bUploadStagingFailure = UploadCreationFailure(2);
    const bool bUploadTrackingFailure = UploadCreationFailure(3);

    std::cout << "pool_object_rollback=" << bPoolObjectRollback << '\n';
    std::cout << "pool_control_rollback=" << bPoolControlRollback << '\n';
    std::cout << "descriptor_wrapper_rollback="
              << bDescriptorWrapperRollback << '\n';
    std::cout << "descriptor_control_rollback="
              << bDescriptorControlRollback << '\n';
    std::cout << "descriptor_tracking_rollback="
              << bDescriptorTrackingRollback << '\n';
    std::cout << "sampler_wrapper_failure=" << bSamplerWrapperFailure << '\n';
    std::cout << "sampler_control_failure=" << bSamplerControlFailure << '\n';
    std::cout << "sampler_tracking_failure=" << bSamplerTrackingFailure << '\n';
    std::cout << "upload_wrapper_failure=" << bUploadWrapperFailure << '\n';
    std::cout << "upload_control_failure=" << bUploadControlFailure << '\n';
    std::cout << "upload_staging_failure=" << bUploadStagingFailure << '\n';
    std::cout << "upload_tracking_failure=" << bUploadTrackingFailure << '\n';
    std::cout << "classification=descriptor-sampler-upload-fixes-active\n";

    return bPoolObjectRollback && bPoolControlRollback &&
            bDescriptorWrapperRollback && bDescriptorControlRollback &&
            bDescriptorTrackingRollback && bSamplerWrapperFailure &&
            bSamplerControlFailure && bSamplerTrackingFailure &&
            bUploadWrapperFailure && bUploadControlFailure &&
            bUploadStagingFailure && bUploadTrackingFailure
        ? 0
        : 1;
}
