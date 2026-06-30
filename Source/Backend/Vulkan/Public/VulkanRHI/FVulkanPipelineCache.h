#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/FRHIComputePipelineDesc.h"
#include "RHI/FRHIGraphicsPipelineDesc.h"

#include <utility>

namespace Stoner::Backend::Vulkan
{

class FVulkanComputePipeline;
class FVulkanGraphicsPipeline;

class FVulkanPipelineCache final
{
public:
    [[nodiscard]] Stoner::Core::FString BuildGraphicsKey(const Stoner::RHI::FRHIGraphicsPipelineDesc& Desc) const;
    [[nodiscard]] Stoner::Core::FString BuildComputeKey(const Stoner::RHI::FRHIComputePipelineDesc& Desc) const;

    [[nodiscard]] Stoner::Core::TSharedPtr<FVulkanGraphicsPipeline> FindGraphics(const Stoner::Core::FString& Key) const noexcept;
    [[nodiscard]] Stoner::Core::TSharedPtr<FVulkanComputePipeline> FindCompute(const Stoner::Core::FString& Key) const noexcept;

    void InsertGraphics(const Stoner::Core::FString& Key, const Stoner::Core::TSharedPtr<FVulkanGraphicsPipeline>& Pipeline);
    void InsertCompute(const Stoner::Core::FString& Key, const Stoner::Core::TSharedPtr<FVulkanComputePipeline>& Pipeline);
    void Invalidate() noexcept;
    [[nodiscard]] Stoner::Core::uint32 GetGeneration() const noexcept;

private:
    Stoner::Core::TArray<std::pair<Stoner::Core::FString, Stoner::Core::TWeakPtr<FVulkanGraphicsPipeline>>> GraphicsEntries;
    Stoner::Core::TArray<std::pair<Stoner::Core::FString, Stoner::Core::TWeakPtr<FVulkanComputePipeline>>> ComputeEntries;
    Stoner::Core::uint32 Generation = 0;
};

} // namespace Stoner::Backend::Vulkan
