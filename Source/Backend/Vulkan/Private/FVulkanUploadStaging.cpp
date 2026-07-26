#include "VulkanRHI/FVulkanUploadStaging.h"

#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace Stoner::Backend::Vulkan
{

namespace
{

[[nodiscard]] bool ValidData(const void* Data, Stoner::Core::uint64 SizeBytes) noexcept
{
    return Data != nullptr && SizeBytes > 0;
}

[[nodiscard]] bool TryMultiply(
    Stoner::Core::uint64 Left,
    Stoner::Core::uint64 Right,
    Stoner::Core::uint64& OutValue) noexcept
{
    if (Left != 0 &&
        Right > std::numeric_limits<Stoner::Core::uint64>::max() / Left)
    {
        return false;
    }
    OutValue = Left * Right;
    return true;
}

[[nodiscard]] bool TextureRegionFits(
    const Stoner::RHI::FRHITextureDesc& Desc,
    const FVulkanTextureUploadRegion& Region) noexcept
{
    if (Region.Width == 0 || Region.Height == 0 || Region.Depth == 0 ||
        Region.MipLevel >= Desc.MipLevels ||
        Region.ArrayLayer >= Desc.ArrayLayers)
    {
        return false;
    }

    const Stoner::Core::uint32 MipWidth =
        Stoner::RHI::GetRHIMipExtent(Desc.Width, Region.MipLevel);
    const Stoner::Core::uint32 MipHeight =
        Stoner::RHI::GetRHIMipExtent(Desc.Height, Region.MipLevel);
    const Stoner::Core::uint32 MipDepth =
        Stoner::RHI::GetRHIMipExtent(Desc.Depth, Region.MipLevel);
    return Region.X <= MipWidth && Region.Width <= MipWidth - Region.X &&
        Region.Y <= MipHeight && Region.Height <= MipHeight - Region.Y &&
        Region.Z <= MipDepth && Region.Depth <= MipDepth - Region.Z;
}

[[nodiscard]] bool TryGetTextureRegionBytes(
    const Stoner::RHI::FRHITextureDesc& Desc,
    const FVulkanTextureUploadRegion& Region,
    Stoner::Core::uint64& OutByteSize) noexcept
{
    OutByteSize = Region.Width;
    const Stoner::Core::uint64 FormatBytes =
        Stoner::RHI::GetRHIFormatByteSize(Desc.Format);
    return FormatBytes > 0 &&
        TryMultiply(OutByteSize, Region.Height, OutByteSize) &&
        TryMultiply(OutByteSize, Region.Depth, OutByteSize) &&
        TryMultiply(OutByteSize, FormatBytes, OutByteSize);
}

[[nodiscard]] bool IsRepresentableStagingSize(
    Stoner::Core::uint64 SizeBytes) noexcept
{
    return SizeBytes <=
        static_cast<Stoner::Core::uint64>(
            std::numeric_limits<Stoner::Core::usize>::max());
}

} // namespace

Stoner::RHI::TRHIObjectResult<FVulkanUploadRequest> FVulkanUploadRequest::CreateBufferUpload(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIBuffer>& InBuffer, const void* Data, Stoner::Core::uint64 SizeBytes, FVulkanBufferUploadRange Range)
{
    if (!ValidData(Data, SizeBytes) || !InBuffer ||
        InBuffer->GetLifecycleState() !=
            Stoner::RHI::ERHIResourceLifecycleState::Valid ||
        Range.SizeBytes == 0 || SizeBytes != Range.SizeBytes ||
        Range.OffsetBytes > InBuffer->GetSizeInBytes() ||
        Range.SizeBytes > InBuffer->GetSizeInBytes() - Range.OffsetBytes)
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }
    if (!Stoner::RHI::HasRHIFlag(
            InBuffer->GetUsage(),
            Stoner::RHI::ERHIBufferUsage::CopyDestination))
    {
        return {Stoner::RHI::ERHIResult::Unsupported, nullptr};
    }
    if (!IsRepresentableStagingSize(SizeBytes))
    {
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }

    try
    {
        Stoner::Core::TSharedPtr<FVulkanUploadRequest> Request(
            new FVulkanUploadRequest());
        Request->Kind = EVulkanUploadKind::Buffer;
        Request->Buffer = InBuffer;
        Request->BufferRange = Range;
        Request->StagingData.resize(
            static_cast<Stoner::Core::usize>(SizeBytes));
        std::memcpy(Request->StagingData.data(), Data,
            static_cast<Stoner::Core::usize>(SizeBytes));
        return {Stoner::RHI::ERHIResult::Success, Request};
    }
    catch (const std::bad_alloc&)
    {
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
}

Stoner::RHI::TRHIObjectResult<FVulkanUploadRequest> FVulkanUploadRequest::CreateTextureUpload(const Stoner::Core::TSharedPtr<Stoner::RHI::IRHITexture>& InTexture, const void* Data, Stoner::Core::uint64 SizeBytes, FVulkanTextureUploadRegion Region)
{
    if (!ValidData(Data, SizeBytes) || !InTexture || InTexture->GetLifecycleState() != Stoner::RHI::ERHIResourceLifecycleState::Valid)
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    const Stoner::RHI::FRHITextureDesc& Desc = InTexture->GetDesc();
    if (!Stoner::RHI::IsValidRHITextureDesc(Desc) ||
        !Stoner::RHI::HasRHIFlag(
            Desc.Usage,
            Stoner::RHI::ERHITextureUsage::CopyDestination) ||
        Desc.SampleCount != Stoner::RHI::ERHISampleCount::One)
    {
        return {Stoner::RHI::ERHIResult::Unsupported, nullptr};
    }
    if (!TextureRegionFits(Desc, Region))
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    Stoner::Core::uint64 RequiredBytes = 0;
    if (!TryGetTextureRegionBytes(Desc, Region, RequiredBytes) ||
        !IsRepresentableStagingSize(RequiredBytes))
    {
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    if (SizeBytes != RequiredBytes)
    {
        return {Stoner::RHI::ERHIResult::InvalidState, nullptr};
    }

    try
    {
        Stoner::Core::TSharedPtr<FVulkanUploadRequest> Request(
            new FVulkanUploadRequest());
        Request->Kind = EVulkanUploadKind::Texture;
        Request->Texture = InTexture;
        Request->TextureRegion = Region;
        Request->StagingData.resize(
            static_cast<Stoner::Core::usize>(SizeBytes));
        std::memcpy(Request->StagingData.data(), Data,
            static_cast<Stoner::Core::usize>(SizeBytes));
        return {Stoner::RHI::ERHIResult::Success, Request};
    }
    catch (const std::bad_alloc&)
    {
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
    catch (const std::length_error&)
    {
        return {Stoner::RHI::ERHIResult::Unavailable, nullptr};
    }
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
