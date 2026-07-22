#include "VulkanRHI/FVulkanBuffer.h"

#include <cstring>

namespace Stoner::Backend::Vulkan
{

FVulkanBuffer::FVulkanBuffer(const Stoner::RHI::FRHIBufferDesc& InDesc, const FVulkanResourceAllocation& InAllocation, std::shared_ptr<FVulkanMemoryAllocator> InAllocator)
    : Desc(InDesc)
    , Allocation(InAllocation)
    , Allocator(std::move(InAllocator))
{
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
    return Allocator ? Allocator->Release(Allocation) : Allocation.Release();
}

Stoner::RHI::ERHIResult FVulkanBuffer::Upload(const void* Data, Stoner::Core::uint64 SizeBytes, Stoner::Core::uint64 OffsetBytes)
{
    if (LifecycleState != Stoner::RHI::ERHIResourceLifecycleState::Valid || Data == nullptr || SizeBytes == 0 ||
        Desc.MemoryAccess != Stoner::RHI::ERHIMemoryAccess::HostVisible || OffsetBytes > Desc.SizeInBytes ||
        SizeBytes > Desc.SizeInBytes - OffsetBytes)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (UploadedBytes.size() != static_cast<Stoner::Core::usize>(Desc.SizeInBytes))
        UploadedBytes.resize(static_cast<Stoner::Core::usize>(Desc.SizeInBytes));
    std::memcpy(UploadedBytes.data() + static_cast<Stoner::Core::usize>(OffsetBytes), Data, static_cast<Stoner::Core::usize>(SizeBytes));
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
