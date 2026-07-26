#include "VulkanRHI/FVulkanDescriptorSet.h"
#include "VulkanRHI/FVulkanSampler.h"
#include "VulkanRHI/FVulkanUploadStaging.h"

#include "RHI/FRHISamplerDesc.h"
#include "RHI/FRHITextureDesc.h"

#include <array>
#include <iostream>
#include <memory>

namespace
{

class FProbeTexture final : public Stoner::RHI::IRHITexture
{
public:
    FProbeTexture()
    {
        Desc.Dimension = Stoner::RHI::ERHITextureDimension::Texture2D;
        Desc.Width = 8;
        Desc.Height = 8;
        Desc.Depth = 1;
        Desc.MipLevels = 2;
        Desc.ArrayLayers = 1;
        Desc.SampleCount = Stoner::RHI::ERHISampleCount::One;
        Desc.Format = Stoner::RHI::ERHIFormat::R8G8B8A8_UNorm;
        Desc.Usage = Stoner::RHI::ERHITextureUsage::CopyDestination;
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

} // namespace

int main()
{
    using namespace Stoner::Backend::Vulkan;
    using namespace Stoner::RHI;

    auto Pool = std::make_shared<FVulkanDescriptorPool>(1);
    const bool bInitialReservation = Pool->Allocate() == ERHIResult::Success;
    {
        FVulkanDescriptorSet UnallocatedSet(nullptr, 0, Pool);
    }
    const bool bUnallocatedSetReleasedAnotherReservation =
        bInitialReservation && Pool->GetAllocatedCount() == 0;
    const bool bCapacityCanNowBeOvercommitted =
        Pool->Allocate() == ERHIResult::Success;

    FRHISamplerDesc InvalidSamplerDesc;
    InvalidSamplerDesc.MipFilter = ERHISamplerMipFilter::None;
    InvalidSamplerDesc.CompareMode = ERHISamplerCompareMode::Less;
    const FVulkanSampler InvalidSampler(InvalidSamplerDesc);
    const bool bInvalidSamplerConstructedValid =
        !IsValidRHISamplerDesc(InvalidSamplerDesc) &&
        InvalidSampler.GetLifecycleState() == ERHIResourceLifecycleState::Valid;

    auto Texture = std::make_shared<FProbeTexture>();
    std::array<unsigned char, 256> FullMipData{};
    const auto OversizedMipUpload = FVulkanUploadRequest::CreateTextureUpload(
        Texture,
        FullMipData.data(),
        FullMipData.size(),
        {1, 0, 0, 0, 0, 8, 8, 1});
    const bool bOversizedMipRegionAccepted = OversizedMipUpload.Succeeded();

    const unsigned char OneByte = 0;
    const auto UnderfilledUpload = FVulkanUploadRequest::CreateTextureUpload(
        Texture,
        &OneByte,
        1,
        {0, 0, 0, 0, 0, 4, 4, 1});
    const bool bUnderfilledRgbaRegionAccepted = UnderfilledUpload.Succeeded();

    std::cout << "unallocated_set_released_another_reservation="
              << bUnallocatedSetReleasedAnotherReservation << '\n';
    std::cout << "descriptor_capacity_overcommit_enabled="
              << bCapacityCanNowBeOvercommitted << '\n';
    std::cout << "invalid_sampler_constructed_valid="
              << bInvalidSamplerConstructedValid << '\n';
    std::cout << "oversized_mip_region_accepted="
              << bOversizedMipRegionAccepted << '\n';
    std::cout << "underfilled_rgba_region_accepted="
              << bUnderfilledRgbaRegionAccepted << '\n';
    std::cout << "classification=descriptor-sampler-upload-contract-defects\n";

    return bUnallocatedSetReleasedAnotherReservation &&
            bCapacityCanNowBeOvercommitted &&
            bInvalidSamplerConstructedValid &&
            bOversizedMipRegionAccepted &&
            bUnderfilledRgbaRegionAccepted
        ? 0
        : 1;
}
