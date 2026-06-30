#pragma once

#include "RHI/IRHIBuffer.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHITexture.h"

namespace Stoner::Backend::Vulkan
{

enum class EVulkanUploadKind
{
    Buffer,
    Texture
};

enum class EVulkanUploadLifecycle
{
    Pending,
    Scheduled,
    ConsumedByFutureCommandPhase,
    Invalidated
};

struct FVulkanBufferUploadRange
{
    Stoner::Core::uint64 OffsetBytes = 0;
    Stoner::Core::uint64 SizeBytes = 0;
};

struct FVulkanTextureUploadRegion
{
    Stoner::Core::uint32 MipLevel = 0;
    Stoner::Core::uint32 ArrayLayer = 0;
    Stoner::Core::uint32 X = 0;
    Stoner::Core::uint32 Y = 0;
    Stoner::Core::uint32 Z = 0;
    Stoner::Core::uint32 Width = 1;
    Stoner::Core::uint32 Height = 1;
    Stoner::Core::uint32 Depth = 1;
};

class FVulkanUploadRequest
{
public:
    static Stoner::RHI::TRHIObjectResult<FVulkanUploadRequest> CreateBufferUpload(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& Buffer, const void* Data, Stoner::Core::uint64 SizeBytes, FVulkanBufferUploadRange Range);
    static Stoner::RHI::TRHIObjectResult<FVulkanUploadRequest> CreateTextureUpload(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& Texture, const void* Data, Stoner::Core::uint64 SizeBytes, FVulkanTextureUploadRegion Region);

    [[nodiscard]] EVulkanUploadKind GetKind() const noexcept;
    [[nodiscard]] EVulkanUploadLifecycle GetLifecycle() const noexcept;
    [[nodiscard]] const Stoner::Core::TArray<unsigned char>& GetStagingData() const noexcept;
    [[nodiscard]] FVulkanBufferUploadRange GetBufferRange() const noexcept;
    [[nodiscard]] FVulkanTextureUploadRegion GetTextureRegion() const noexcept;
    [[nodiscard]] bool ClaimsExecution() const noexcept;
    [[nodiscard]] Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> GetBuffer() const noexcept;
    [[nodiscard]] Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> GetTexture() const noexcept;

    Stoner::RHI::ERHIResult MarkScheduled() noexcept;
    Stoner::RHI::ERHIResult Invalidate() noexcept;

private:
    EVulkanUploadKind Kind = EVulkanUploadKind::Buffer;
    EVulkanUploadLifecycle Lifecycle = EVulkanUploadLifecycle::Pending;
    Stoner::Core::TArray<unsigned char> StagingData;
    Stoner::Core::TWeakPtr<Stoner::RHI::IRHIBuffer> Buffer;
    Stoner::Core::TWeakPtr<Stoner::RHI::IRHITexture> Texture;
    FVulkanBufferUploadRange BufferRange;
    FVulkanTextureUploadRegion TextureRegion;
};

} // namespace Stoner::Backend::Vulkan
