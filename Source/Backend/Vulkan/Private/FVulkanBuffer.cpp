#include "VulkanRHI/FVulkanBuffer.h"

#include <cstring>
#include <new>
#include <stdexcept>

namespace Stoner::Backend::Vulkan
{

namespace
{

[[nodiscard]] bool IsCompatibleAllocation(
    const Stoner::RHI::FRHIBufferDesc& Desc,
    const FVulkanResourceAllocation& Allocation,
    const std::shared_ptr<FVulkanMemoryAllocator>& Allocator) noexcept
{
    return Allocator && Stoner::RHI::IsValidRHIBufferDesc(Desc) &&
        Allocation.IsSuccessful() &&
        Allocation.GetKind() == EVulkanResourceKind::Buffer &&
        Allocation.GetByteSize() == Desc.SizeInBytes;
}

} // namespace

FVulkanBuffer::FVulkanBuffer(const Stoner::RHI::FRHIBufferDesc& InDesc, FVulkanResourceAllocation&& InAllocation, std::shared_ptr<FVulkanMemoryAllocator> InAllocator)
    : Desc(InDesc)
    , Allocation(IsCompatibleAllocation(InDesc, InAllocation, InAllocator)
              ? std::move(InAllocation)
              : FVulkanResourceAllocation{})
    , Allocator(std::move(InAllocator))
{
    if (!Allocation.IsSuccessful())
    {
        LifecycleState =
            Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    }
}

FVulkanBuffer::~FVulkanBuffer()
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Valid)
    {
        (void)Invalidate();
    }
}

const Stoner::RHI::FRHIBufferDesc& FVulkanBuffer::GetDesc() const noexcept { return Desc; }
Stoner::Core::uint64 FVulkanBuffer::GetSizeInBytes() const noexcept { return Desc.SizeInBytes; }
Stoner::RHI::ERHIBufferUsage FVulkanBuffer::GetUsage() const noexcept { return Desc.Usage; }
Stoner::RHI::ERHIResourceLifecycleState FVulkanBuffer::GetLifecycleState() const noexcept { return LifecycleState; }
const FVulkanResourceAllocation& FVulkanBuffer::GetAllocation() const noexcept { return Allocation; }

Stoner::RHI::ERHIResult FVulkanBuffer::Invalidate()
{
    if (LifecycleState == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
    return Allocator
        ? Allocator->Release(Allocation)
        : Stoner::RHI::ERHIResult::InvalidState;
}

Stoner::RHI::ERHIResult FVulkanBuffer::Upload(const void* Data, Stoner::Core::uint64 SizeBytes, Stoner::Core::uint64 OffsetBytes)
{
    if (LifecycleState != Stoner::RHI::ERHIResourceLifecycleState::Valid || Data == nullptr || SizeBytes == 0 ||
        Desc.MemoryAccess != Stoner::RHI::ERHIMemoryAccess::HostVisible || OffsetBytes > Desc.SizeInBytes ||
        SizeBytes > Desc.SizeInBytes - OffsetBytes)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    const Stoner::Core::uint64 RequiredBytes = OffsetBytes + SizeBytes;
    if (RequiredBytes >
        static_cast<Stoner::Core::uint64>(UploadedBytes.max_size()))
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }

    try
    {
        if (UploadedBytes.size() <
            static_cast<Stoner::Core::usize>(RequiredBytes))
        {
            UploadedBytes.resize(
                static_cast<Stoner::Core::usize>(RequiredBytes));
        }
    }
    catch (const std::bad_alloc&)
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    std::memcpy(UploadedBytes.data() + static_cast<Stoner::Core::usize>(OffsetBytes), Data, static_cast<Stoner::Core::usize>(SizeBytes));
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
