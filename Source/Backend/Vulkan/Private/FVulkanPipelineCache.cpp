#include "VulkanRHI/FVulkanPipelineCache.h"

#include "VulkanRHI/FVulkanComputePipeline.h"
#include "VulkanRHI/FVulkanGraphicsPipeline.h"

#include "RHI/IRHIPipelineLayout.h"
#include "RHI/IRHIShaderModule.h"

#include <sstream>

namespace Stoner::Backend::Vulkan
{

namespace
{

void AppendShaderKey(std::ostringstream& Stream, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIShaderModule>& Shader)
{
    if (!Shader)
    {
        Stream << "null";
        return;
    }
    Stream << static_cast<int>(Shader->GetStage()) << ':' << Shader->GetDesc().PayloadIdentity.CStr() << ':' << Shader->GetDesc().EntryPoint.CStr();
}

void AppendLayoutKey(std::ostringstream& Stream, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout>& Layout)
{
    if (!Layout)
    {
        Stream << "|layout:null";
        return;
    }
    Stream << "|layout:";
    for (const Stoner::RHI::FRHIDescriptorBinding& Binding : Layout->GetDesc().Bindings)
    {
        Stream << Binding.SetIndex << ',' << Binding.BindingSlot << ',' << static_cast<int>(Binding.DescriptorType) << ',' << Binding.ArrayCount << ',' << static_cast<unsigned int>(Binding.Visibility) << ';';
    }
    Stream << "|ranges:";
    for (const Stoner::RHI::FRHIShaderConstantRange& Range : Layout->GetDesc().ConstantRanges)
    {
        Stream << Range.OffsetBytes << ',' << Range.SizeBytes << ',' << static_cast<unsigned int>(Range.Visibility) << ';';
    }
}

} // namespace

Stoner::Core::FString FVulkanPipelineCache::BuildGraphicsKey(const Stoner::RHI::FRHIGraphicsPipelineDesc& Desc) const
{
    std::ostringstream Stream;
    Stream << "graphics|";
    for (const auto& Shader : Desc.ShaderModules)
    {
        AppendShaderKey(Stream, Shader);
        Stream << '|';
    }
    AppendLayoutKey(Stream, Desc.PipelineLayout);
    Stream << "|stride:" << Desc.VertexInput.Stride << "|attrs:";
    for (const Stoner::RHI::FRHIVertexAttributeDesc& Attribute : Desc.VertexInput.Attributes)
    {
        Stream << Attribute.Location << ',' << static_cast<int>(Attribute.Format) << ',' << Attribute.Offset << ';';
    }
    Stream << "|topology:" << static_cast<int>(Desc.Topology)
           << "|colors:";
    for (Stoner::RHI::ERHIFormat Format : Desc.RenderTargets.ColorFormats)
    {
        Stream << static_cast<int>(Format) << ',';
    }
    Stream << "|depth:" << static_cast<int>(Desc.RenderTargets.DepthStencilFormat)
           << "|samples:" << static_cast<int>(Desc.RenderTargets.SampleCount);
    return Stoner::Core::FString(Stream.str());
}

Stoner::Core::FString FVulkanPipelineCache::BuildComputeKey(const Stoner::RHI::FRHIComputePipelineDesc& Desc) const
{
    std::ostringstream Stream;
    Stream << "compute|";
    for (const auto& Shader : Desc.ShaderModules)
    {
        AppendShaderKey(Stream, Shader);
        Stream << '|';
    }
    AppendLayoutKey(Stream, Desc.PipelineLayout);
    return Stoner::Core::FString(Stream.str());
}

Stoner::Core::TSharedPtr<FVulkanGraphicsPipeline> FVulkanPipelineCache::FindGraphics(const Stoner::Core::FString& Key) const noexcept
{
    for (const auto& Entry : GraphicsEntries)
    {
        if (Entry.first == Key)
        {
            auto Pipeline = Entry.second.lock();
            if (Pipeline && Pipeline->GetLifecycleState() == Stoner::RHI::ERHIResourceLifecycleState::Valid)
            {
                return Pipeline;
            }
        }
    }
    return nullptr;
}

Stoner::Core::TSharedPtr<FVulkanComputePipeline> FVulkanPipelineCache::FindCompute(const Stoner::Core::FString& Key) const noexcept
{
    for (const auto& Entry : ComputeEntries)
    {
        if (Entry.first == Key)
        {
            auto Pipeline = Entry.second.lock();
            if (Pipeline && Pipeline->GetLifecycleState() == Stoner::RHI::ERHIResourceLifecycleState::Valid)
            {
                return Pipeline;
            }
        }
    }
    return nullptr;
}

void FVulkanPipelineCache::InsertGraphics(const Stoner::Core::FString& Key, const Stoner::Core::TSharedPtr<FVulkanGraphicsPipeline>& Pipeline)
{
    GraphicsEntries.push_back({Key, Pipeline});
}

void FVulkanPipelineCache::InsertCompute(const Stoner::Core::FString& Key, const Stoner::Core::TSharedPtr<FVulkanComputePipeline>& Pipeline)
{
    ComputeEntries.push_back({Key, Pipeline});
}

void FVulkanPipelineCache::Invalidate() noexcept
{
    GraphicsEntries.clear();
    ComputeEntries.clear();
    ++Generation;
}

Stoner::Core::uint32 FVulkanPipelineCache::GetGeneration() const noexcept
{
    return Generation;
}

} // namespace Stoner::Backend::Vulkan
