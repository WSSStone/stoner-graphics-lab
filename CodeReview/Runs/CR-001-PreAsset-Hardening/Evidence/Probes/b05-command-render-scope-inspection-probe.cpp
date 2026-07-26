#include "VulkanRHI/FVulkanCommandBuffer.h"
#include "VulkanRHI/FVulkanCommandPool.h"
#include "VulkanRHI/FVulkanDiagnostics.h"
#include "VulkanRHI/FVulkanFramebuffer.h"
#include "VulkanRHI/FVulkanRenderPass.h"

#include "RHI/FRHIBufferDesc.h"
#include "RHI/FRHIFramebufferDesc.h"
#include "RHI/FRHIRenderPassDesc.h"
#include "RHI/FRHITextureBufferCopyRegion.h"
#include "RHI/FRHITextureDesc.h"

#include <iostream>
#include <memory>

namespace
{

class FProbeTexture final : public Stoner::RHI::IRHITexture
{
public:
    explicit FProbeTexture(Stoner::RHI::FRHITextureDesc InDesc)
        : Desc(InDesc)
    {
    }

    [[nodiscard]] const Stoner::RHI::FRHITextureDesc& GetDesc() const noexcept override
    {
        return Desc;
    }

    [[nodiscard]] Stoner::RHI::ERHITextureDimension GetDimension() const noexcept override
    {
        return Desc.Dimension;
    }

    [[nodiscard]] Stoner::RHI::ERHIFormat GetFormat() const noexcept override
    {
        return Desc.Format;
    }

    [[nodiscard]] Stoner::RHI::ERHITextureUsage GetUsage() const noexcept override
    {
        return Desc.Usage;
    }

    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept override
    {
        return Lifecycle;
    }

    Stoner::RHI::ERHIResult Invalidate() override
    {
        if (Lifecycle == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
        {
            return Stoner::RHI::ERHIResult::InvalidState;
        }
        Lifecycle = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
        return Stoner::RHI::ERHIResult::Success;
    }

private:
    Stoner::RHI::FRHITextureDesc Desc;
    Stoner::RHI::ERHIResourceLifecycleState Lifecycle =
        Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

class FProbeBuffer final : public Stoner::RHI::IRHIBuffer
{
public:
    explicit FProbeBuffer(Stoner::Core::uint64 SizeInBytes)
    {
        Desc.SizeInBytes = SizeInBytes;
        Desc.Usage = Stoner::RHI::ERHIBufferUsage::CopyDestination;
        Desc.MemoryAccess = Stoner::RHI::ERHIMemoryAccess::HostVisible;
    }

    [[nodiscard]] const Stoner::RHI::FRHIBufferDesc& GetDesc() const noexcept override
    {
        return Desc;
    }

    [[nodiscard]] Stoner::Core::uint64 GetSizeInBytes() const noexcept override
    {
        return Desc.SizeInBytes;
    }

    [[nodiscard]] Stoner::RHI::ERHIBufferUsage GetUsage() const noexcept override
    {
        return Desc.Usage;
    }

    [[nodiscard]] Stoner::RHI::ERHIResourceLifecycleState GetLifecycleState() const noexcept override
    {
        return Lifecycle;
    }

    Stoner::RHI::ERHIResult Invalidate() override
    {
        if (Lifecycle == Stoner::RHI::ERHIResourceLifecycleState::Invalidated)
        {
            return Stoner::RHI::ERHIResult::InvalidState;
        }
        Lifecycle = Stoner::RHI::ERHIResourceLifecycleState::Invalidated;
        return Stoner::RHI::ERHIResult::Success;
    }

private:
    Stoner::RHI::FRHIBufferDesc Desc;
    Stoner::RHI::ERHIResourceLifecycleState Lifecycle =
        Stoner::RHI::ERHIResourceLifecycleState::Valid;
};

Stoner::RHI::FRHITextureDesc MakeTextureDesc(
    Stoner::RHI::ERHIFormat Format,
    Stoner::RHI::ERHITextureUsage Usage)
{
    Stoner::RHI::FRHITextureDesc Desc;
    Desc.Dimension = Stoner::RHI::ERHITextureDimension::Texture2D;
    Desc.Width = 8;
    Desc.Height = 8;
    Desc.Depth = 1;
    Desc.MipLevels = 2;
    Desc.ArrayLayers = 1;
    Desc.SampleCount = Stoner::RHI::ERHISampleCount::One;
    Desc.Format = Format;
    Desc.Usage = Usage;
    return Desc;
}

} // namespace

int main()
{
    using namespace Stoner::Backend::Vulkan;
    using namespace Stoner::RHI;

    FVulkanDiagnostics Diagnostics;
    FVulkanCommandBuffer CommandBuffer(ERHIQueueType::Transfer, &Diagnostics);
    const bool bBegan = CommandBuffer.Begin() == ERHIResult::Success;

    auto Source = std::make_shared<FProbeTexture>(
        MakeTextureDesc(ERHIFormat::R8G8B8A8_UNorm, ERHITextureUsage::CopySource));
    auto Destination = std::make_shared<FProbeTexture>(
        MakeTextureDesc(ERHIFormat::R8G8B8A8_UNorm, ERHITextureUsage::CopyDestination));

    FRHITextureCopyRegion OversizedMipRegion;
    OversizedMipRegion.SourceMipLevel = 1;
    OversizedMipRegion.DestinationMipLevel = 1;
    OversizedMipRegion.Width = 8;
    OversizedMipRegion.Height = 8;
    const bool bOversizedMipTextureCopyAccepted =
        CommandBuffer.RecordTextureCopy(Source, Destination, OversizedMipRegion) == ERHIResult::Success;

    auto Readback = std::make_shared<FProbeBuffer>(1024);
    FRHITextureBufferCopyRegion OversizedReadbackRegion;
    OversizedReadbackRegion.SourceMipLevel = 1;
    OversizedReadbackRegion.Width = 8;
    OversizedReadbackRegion.Height = 8;
    const bool bOversizedMipReadbackAccepted =
        CommandBuffer.RecordTextureToBufferCopy(Source, Readback, OversizedReadbackRegion) ==
        ERHIResult::Success;

    auto IncompatibleDestination = std::make_shared<FProbeTexture>(
        MakeTextureDesc(ERHIFormat::R8_UNorm, ERHITextureUsage::CopyDestination));
    FRHITextureCopyRegion IncompatibleRegion;
    IncompatibleRegion.Width = 4;
    IncompatibleRegion.Height = 4;
    const bool bIncompatibleFormatCopyAccepted =
        CommandBuffer.RecordTextureCopy(Source, IncompatibleDestination, IncompatibleRegion) ==
        ERHIResult::Success;

    const bool bEnded = CommandBuffer.End() == ERHIResult::Success;
    const bool bSubmissionStateForgeable =
        bEnded &&
        CommandBuffer.MarkSubmitted() == ERHIResult::Success &&
        CommandBuffer.MarkCompletedOrResettable() == ERHIResult::Success;

    FVulkanCommandPool UnsupportedPool(ERHIQueueType::Present, 1);
    const bool bUnsupportedDirectPoolAllocationSucceeded =
        UnsupportedPool.Allocate(Diagnostics).Succeeded();

    const FRHIRenderPassDesc InvalidRenderPassDesc;
    FVulkanRenderPass InvalidRenderPass(InvalidRenderPassDesc);
    const bool bInvalidRenderPassConstructedValid =
        !IsValidRHIRenderPassDesc(InvalidRenderPassDesc) &&
        InvalidRenderPass.GetLifecycleState() == ERHIResourceLifecycleState::Valid;

    const FRHIFramebufferDesc InvalidFramebufferDesc;
    FVulkanFramebuffer InvalidFramebuffer(InvalidFramebufferDesc);
    const bool bInvalidFramebufferConstructedValid =
        InvalidFramebuffer.GetLifecycleState() == ERHIResourceLifecycleState::Valid;

    std::cout << "oversized_mip_texture_copy_accepted="
              << bOversizedMipTextureCopyAccepted << '\n';
    std::cout << "oversized_mip_readback_accepted="
              << bOversizedMipReadbackAccepted << '\n';
    std::cout << "incompatible_format_copy_accepted="
              << bIncompatibleFormatCopyAccepted << '\n';
    std::cout << "submission_state_forgeable=" << bSubmissionStateForgeable << '\n';
    std::cout << "unsupported_direct_pool_allocation_succeeded="
              << bUnsupportedDirectPoolAllocationSucceeded << '\n';
    std::cout << "invalid_render_pass_constructed_valid="
              << bInvalidRenderPassConstructedValid << '\n';
    std::cout << "invalid_framebuffer_constructed_valid="
              << bInvalidFramebufferConstructedValid << '\n';
    std::cout << "classification=command-transfer-factory-contract-defects\n";

    return bBegan &&
            bOversizedMipTextureCopyAccepted &&
            bOversizedMipReadbackAccepted &&
            bIncompatibleFormatCopyAccepted &&
            bSubmissionStateForgeable &&
            bUnsupportedDirectPoolAllocationSucceeded &&
            bInvalidRenderPassConstructedValid &&
            bInvalidFramebufferConstructedValid
        ? 0
        : 1;
}
