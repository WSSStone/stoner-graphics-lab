#include "VulkanRHI/FVulkanPipelineCache.h"

#include "VulkanRHI/FVulkanComputePipeline.h"
#include "VulkanRHI/FVulkanGraphicsPipeline.h"

#include "RHI/IRHIPipelineLayout.h"
#include "RHI/IRHIShaderModule.h"

#include <algorithm>
#include <sstream>
#include <tuple>
#include <vector>

namespace Stoner::Backend::Vulkan
{

namespace
{

void AppendText(std::ostringstream& Stream, const Stoner::Core::FString& Value)
{
    const std::string_view View = Value.View();
    Stream << View.size() << ':';
    Stream.write(View.data(), static_cast<std::streamsize>(View.size()));
}

template <typename TBinding>
[[nodiscard]] auto BindingOrderKey(const TBinding& Binding) noexcept
{
    return std::tuple{
        Binding.SetIndex,
        Binding.BindingSlot,
        static_cast<int>(Binding.DescriptorType),
        Binding.ArrayCount,
        static_cast<unsigned int>(Binding.Visibility)};
}

[[nodiscard]] auto RangeOrderKey(
    const Stoner::RHI::FRHIShaderConstantRange& Range) noexcept
{
    return std::tuple{
        Range.OffsetBytes,
        Range.SizeBytes,
        static_cast<unsigned int>(Range.Visibility)};
}

template <typename TBinding>
void AppendBindings(
    std::ostringstream& Stream,
    const Stoner::Core::TArray<TBinding>& Bindings)
{
    std::vector<TBinding> Sorted(Bindings.begin(), Bindings.end());
    std::sort(
        Sorted.begin(), Sorted.end(),
        [](const TBinding& Left, const TBinding& Right)
        {
            return BindingOrderKey(Left) < BindingOrderKey(Right);
        });
    Stream << Sorted.size() << ':';
    for (const TBinding& Binding : Sorted)
    {
        Stream << Binding.SetIndex << ','
               << Binding.BindingSlot << ','
               << static_cast<int>(Binding.DescriptorType) << ','
               << Binding.ArrayCount << ','
               << static_cast<unsigned int>(Binding.Visibility) << ';';
    }
}

void AppendRanges(
    std::ostringstream& Stream,
    const Stoner::Core::TArray<Stoner::RHI::FRHIShaderConstantRange>& Ranges)
{
    auto Sorted = Ranges;
    std::sort(
        Sorted.begin(), Sorted.end(),
        [](const Stoner::RHI::FRHIShaderConstantRange& Left,
           const Stoner::RHI::FRHIShaderConstantRange& Right)
        {
            return RangeOrderKey(Left) < RangeOrderKey(Right);
        });
    Stream << Sorted.size() << ':';
    for (const Stoner::RHI::FRHIShaderConstantRange& Range : Sorted)
    {
        Stream << Range.OffsetBytes << ','
               << Range.SizeBytes << ','
               << static_cast<unsigned int>(Range.Visibility) << ';';
    }
}

void AppendShaderKey(
    std::ostringstream& Stream,
    const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIShaderModule>& Shader)
{
    if (!Shader)
    {
        Stream << "shader:null;";
        return;
    }
    const Stoner::RHI::FRHIShaderModuleDesc& Desc = Shader->GetDesc();
    Stream << "shader:" << static_cast<int>(Shader->GetStage()) << ','
           << static_cast<int>(Desc.ValidationMode) << ','
           << static_cast<int>(Desc.RuntimeMode) << ',';
    AppendText(Stream, Desc.Payload.PayloadIdentity);
    AppendText(Stream, Desc.EntryPoint);
    Stream << static_cast<int>(Desc.Payload.Format) << ',';
    AppendText(Stream, Desc.Payload.TargetProfile);
    Stream << Desc.Payload.Bytes.size() << ':';
    for (Stoner::Core::uint8 Byte : Desc.Payload.Bytes)
    {
        Stream << static_cast<unsigned int>(Byte) << ',';
    }
    Stream << "interface:";
    AppendBindings(Stream, Desc.InterfaceMetadata.Bindings);
    AppendRanges(Stream, Desc.InterfaceMetadata.ConstantRanges);
    Stream << ';';
}

void AppendLayoutKey(std::ostringstream& Stream, const Stoner::Core::TSharedPtr<Stoner::RHI::IRHIPipelineLayout>& Layout)
{
    if (!Layout)
    {
        Stream << "|layout:null";
        return;
    }
    Stream << "|layout:";
    AppendBindings(Stream, Layout->GetDesc().Bindings);
    Stream << "|ranges:";
    AppendRanges(Stream, Layout->GetDesc().ConstantRanges);
}

template <typename TPipelineDesc>
void AppendShaderList(
    std::ostringstream& Stream,
    const TPipelineDesc& Desc)
{
    auto Shaders = Desc.ShaderModules;
    std::sort(
        Shaders.begin(), Shaders.end(),
        [](const auto& Left, const auto& Right)
        {
            const int LeftStage = Left
                ? static_cast<int>(Left->GetStage())
                : -1;
            const int RightStage = Right
                ? static_cast<int>(Right->GetStage())
                : -1;
            return LeftStage < RightStage;
        });
    Stream << "mode:" << static_cast<int>(Desc.RuntimeMode)
           << "|shaders:" << Shaders.size() << ':';
    for (const auto& Shader : Shaders)
    {
        AppendShaderKey(Stream, Shader);
    }
}

} // namespace

Stoner::Core::FString FVulkanPipelineCache::BuildGraphicsKey(const Stoner::RHI::FRHIGraphicsPipelineDesc& Desc) const
{
    std::ostringstream Stream;
    Stream << "graphics|";
    AppendShaderList(Stream, Desc);
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
    Stream << "|raster:" << static_cast<int>(Desc.Rasterizer.CullMode) << ',' << static_cast<int>(Desc.Rasterizer.FrontFace) << ',' << (Desc.Rasterizer.bDepthClampEnabled ? 1 : 0);
    Stream << "|blend:" << (Desc.Blend.bEnabled ? 1 : 0) << ',' << static_cast<int>(Desc.Blend.SourceColor) << ',' << static_cast<int>(Desc.Blend.DestinationColor) << ',' << static_cast<int>(Desc.Blend.ColorOp);
    Stream << "|ds:" << (Desc.DepthStencil.bDepthTestEnabled ? 1 : 0) << ',' << (Desc.DepthStencil.bDepthWriteEnabled ? 1 : 0) << ',' << static_cast<int>(Desc.DepthStencil.DepthCompare);
    Stream << "|ms:" << static_cast<int>(Desc.Multisample.SampleCount) << ',' << (Desc.Multisample.bSampleShadingEnabled ? 1 : 0);
    Stream << "|dyn:" << (Desc.DynamicState.bViewportDynamic ? 1 : 0) << ',' << (Desc.DynamicState.bScissorDynamic ? 1 : 0);
    return Stoner::Core::FString(Stream.str());
}

Stoner::Core::FString FVulkanPipelineCache::BuildComputeKey(const Stoner::RHI::FRHIComputePipelineDesc& Desc) const
{
    std::ostringstream Stream;
    Stream << "compute|";
    AppendShaderList(Stream, Desc);
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
            if (Pipeline && Pipeline->HasValidDependencies())
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
            if (Pipeline && Pipeline->HasValidDependencies())
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
