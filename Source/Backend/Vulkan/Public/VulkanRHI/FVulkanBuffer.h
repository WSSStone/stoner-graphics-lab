#pragma once

#include "VulkanRHI/FVulkanMemoryAllocator.h"
#include "RHI/IRHIBuffer.h"

#include <memory>

namespace Stoner::Backend::Vulkan
{

class FVulkanDevice;
class FVulkanNativeContext;
class FDeferredNativeSubmission;

class FVulkanBuffer final : public Stoner::RHI::IRHIBuffer
{
public:
    ~FVulkanBuffer() override;
    FVulkanBuffer(const FVulkanBuffer&) = delete;
    FVulkanBuffer& operator=(const FVulkanBuffer&) = delete;

    [[nodiscard]] const Stoner::RHI::FRHIBufferDesc& GetDesc() const noexcept override;
    [[nodiscard]] Stoner::Core::uint64 GetSizeInBytes() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIBufferUsage GetUsage() const noexcept override;
    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept override;
    [[nodiscard]] const FVulkanResourceAllocation& GetAllocation() const noexcept;

    Stoner::RHI::ERHIResult Invalidate() override;
    Stoner::RHI::ERHIResult Upload(const void* Data, Stoner::Core::uint64 SizeBytes, Stoner::Core::uint64 OffsetBytes = 0) override;
    [[nodiscard]] const Stoner::Core::TArray<Stoner::Core::uint8>& GetUploadedBytes() const noexcept { return UploadedBytes; }
    [[nodiscard]] Stoner::Core::uint64 GetUploadRevision() const noexcept { return UploadRevision; }
    [[nodiscard]] bool HasPendingNativeUse() const noexcept { return NativeUseCount != 0; }

private:
    friend class FVulkanDevice;
    friend class FVulkanNativeContext;
    friend class FDeferredNativeSubmission;

    FVulkanBuffer(
        const Stoner::RHI::FRHIBufferDesc& InDesc,
        FVulkanResourceAllocation&& InAllocation,
        std::shared_ptr<FVulkanMemoryAllocator> InAllocator);
    [[nodiscard]] Stoner::RHI::ERHIResult RecordNativeUpload(
        const void* Data,
        Stoner::Core::uint64 SizeBytes,
        Stoner::Core::uint64 OffsetBytes);

    Stoner::RHI::FRHIBufferDesc Desc;
    FVulkanResourceAllocation Allocation;
    std::shared_ptr<FVulkanMemoryAllocator> Allocator;
    Stoner::RHI::ERHIResourceLifecycleState LifecycleState = Stoner::RHI::ERHIResourceLifecycleState::Valid;
    Stoner::Core::TArray<Stoner::Core::uint8> UploadedBytes;
    Stoner::Core::uint64 UploadRevision = 1;
    Stoner::Core::uint32 NativeUseCount = 0;
    bool bInvalidationPending = false;

    [[nodiscard]] bool AcquireNativeUse() noexcept;
    void ReleaseNativeUse() noexcept;
};

} // namespace Stoner::Backend::Vulkan
