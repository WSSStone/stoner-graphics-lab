#include "VulkanRHI/FVulkanUploadStaging.h"

#include <cstring>

namespace Stoner::Backend::Vulkan
{

namespace
{

[[nodiscard]] bool ValidData(const void* Data, Stoner::Core::uint64 SizeBytes) noexcept
{
    return Data != nullptr && SizeBytes > 0;
}

} // namespace

Stoner::RHI::TRHIObjectResult<FVulkanUploadRequest> FVulkanUploadRequest::CreateBufferUpload(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& InBuffer, const void* Data, Stoner::Core::uint64 SizeBytes, FVulkanBufferUploadRange Range)
{
    if (!ValidData(Data, SizeBytes) || !InBuffer || InBuffer->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid || Range.SizeBytes == 0 || Range.OffsetBytes > InBuffer->GetSizeInBytes() || Range.SizeBytes > InBuffer->GetSizeInBytes() - Range.OffsetBytes || SizeBytes < Range.SizeBytes)
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    auto Request = Stoner::Core::MakeShared<FVulkanUploadRequest>();
    Request->Kind = EVulkanUploadKind::Buffer;
    Request->Buffer = InBuffer;
    Request->BufferRange = Range;
    Request->StagingData.resize(static_cast<size_t>(SizeBytes));
    std::memcpy(Request->StagingData.data(), Data, static_cast<size_t>(SizeBytes));
    return {Stoner::RHI::ERHIResult::Success, Request};
}

Stoner::RHI::TRHIObjectResult<FVulkanUploadRequest> FVulkanUploadRequest::CreateTextureUpload(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& InTexture, const void* Data, Stoner::Core::uint64 SizeBytes, FVulkanTextureUploadRegion Region)
{
    if (!ValidData(Data, SizeBytes) || !InTexture || InTexture->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid)
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    const Stoner::RHI::FRHITextureDesc& Desc = InTexture->GetDesc();
    if (Region.Width == 0 || Region.Height == 0 || Region.Depth == 0 || Region.MipLevel >= Desc.MipLevels || Region.ArrayLayer >= Desc.ArrayLayers ||
        Region.X > Desc.Width || Region.Width > Desc.Width - Region.X ||
        Region.Y > Desc.Height || Region.Height > Desc.Height - Region.Y ||
        Region.Z > Desc.Depth || Region.Depth > Desc.Depth - Region.Z)
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    auto Request = Stoner::Core::MakeShared<FVulkanUploadRequest>();
    Request->Kind = EVulkanUploadKind::Texture;
    Request->Texture = InTexture;
    Request->TextureRegion = Region;
    Request->StagingData.resize(static_cast<size_t>(SizeBytes));
    std::memcpy(Request->StagingData.data(), Data, static_cast<size_t>(SizeBytes));
    return {Stoner::RHI::ERHIResult::Success, Request};
}

EVulkanUploadKind FVulkanUploadRequest::GetKind() const noexcept { return Kind; }
EVulkanUploadLifecycle FVulkanUploadRequest::GetLifecycle() const noexcept { return Lifecycle; }
const Stoner::Core::TArray<unsigned char>& FVulkanUploadRequest::GetStagingData() const noexcept { return StagingData; }
FVulkanBufferUploadRange FVulkanUploadRequest::GetBufferRange() const noexcept { return BufferRange; }
FVulkanTextureUploadRegion FVulkanUploadRequest::GetTextureRegion() const noexcept { return TextureRegion; }
bool FVulkanUploadRequest::ClaimsExecution() const noexcept { return false; }
Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer> FVulkanUploadRequest::GetBuffer() const noexcept { return Buffer.lock(); }
Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture> FVulkanUploadRequest::GetTexture() const noexcept { return Texture.lock(); }

Stoner::RHI::ERHIResult FVulkanUploadRequest::MarkScheduled() noexcept
{
    if (Lifecycle != EVulkanUploadLifecycle::Pending)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    Lifecycle = EVulkanUploadLifecycle::Scheduled;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanUploadRequest::Invalidate() noexcept
{
    if (Lifecycle == EVulkanUploadLifecycle::Invalidated)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    Lifecycle = EVulkanUploadLifecycle::Invalidated;
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
