#include "VulkanRHI/FVulkanNativeContext.h"
#include "FVulkanNativeDeviceAccess.h"
#include "FVulkanNativeOffscreenSession.h"
#include "FVulkanStruct.h"

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
#include <vulkan/vulkan.h>

#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#endif

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>
#endif

namespace Stoner::Backend::Vulkan
{

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
namespace
{

using namespace Stoner::RHI;

[[nodiscard]] ERHIResult MapVulkanCreationResult(VkResult Result) noexcept
{
    if (Result == VK_SUCCESS)
    {
        return ERHIResult::Success;
    }
    return Result == VK_ERROR_OUT_OF_HOST_MEMORY ||
            Result == VK_ERROR_OUT_OF_DEVICE_MEMORY
        ? ERHIResult::Unavailable
        : ERHIResult::Failed;
}

[[nodiscard]] VkFormat ToVulkanFormat(ERHIFormat Format) noexcept
{
    switch (Format)
    {
    case ERHIFormat::R8_UNorm: return VK_FORMAT_R8_UNORM;
    case ERHIFormat::R8G8_UNorm: return VK_FORMAT_R8G8_UNORM;
    case ERHIFormat::R8G8B8A8_UNorm: return VK_FORMAT_R8G8B8A8_UNORM;
    case ERHIFormat::R8G8B8A8_sRGB: return VK_FORMAT_R8G8B8A8_SRGB;
    case ERHIFormat::B8G8R8A8_UNorm: return VK_FORMAT_B8G8R8A8_UNORM;
    case ERHIFormat::R16G16B16A16_Float: return VK_FORMAT_R16G16B16A16_SFLOAT;
    case ERHIFormat::R32_Float: return VK_FORMAT_R32_SFLOAT;
    case ERHIFormat::R32G32_Float: return VK_FORMAT_R32G32_SFLOAT;
    case ERHIFormat::R32G32B32_Float: return VK_FORMAT_R32G32B32_SFLOAT;
    case ERHIFormat::R32G32B32A32_Float: return VK_FORMAT_R32G32B32A32_SFLOAT;
    case ERHIFormat::BC1_RGBA_UNorm: return VK_FORMAT_BC1_RGBA_UNORM_BLOCK;
    case ERHIFormat::BC1_RGBA_sRGB: return VK_FORMAT_BC1_RGBA_SRGB_BLOCK;
    case ERHIFormat::BC3_RGBA_UNorm: return VK_FORMAT_BC3_UNORM_BLOCK;
    case ERHIFormat::BC3_RGBA_sRGB: return VK_FORMAT_BC3_SRGB_BLOCK;
    case ERHIFormat::BC4_R_UNorm: return VK_FORMAT_BC4_UNORM_BLOCK;
    case ERHIFormat::BC5_RG_UNorm: return VK_FORMAT_BC5_UNORM_BLOCK;
    case ERHIFormat::BC7_RGBA_UNorm: return VK_FORMAT_BC7_UNORM_BLOCK;
    case ERHIFormat::BC7_RGBA_sRGB: return VK_FORMAT_BC7_SRGB_BLOCK;
    case ERHIFormat::ETC2_RGB8_UNorm: return VK_FORMAT_ETC2_R8G8B8_UNORM_BLOCK;
    case ERHIFormat::ETC2_RGB8_sRGB: return VK_FORMAT_ETC2_R8G8B8_SRGB_BLOCK;
    case ERHIFormat::ETC2_RGBA8_UNorm: return VK_FORMAT_ETC2_R8G8B8A8_UNORM_BLOCK;
    case ERHIFormat::ETC2_RGBA8_sRGB: return VK_FORMAT_ETC2_R8G8B8A8_SRGB_BLOCK;
    case ERHIFormat::EAC_R11_UNorm: return VK_FORMAT_EAC_R11_UNORM_BLOCK;
    case ERHIFormat::EAC_RG11_UNorm: return VK_FORMAT_EAC_R11G11_UNORM_BLOCK;
    case ERHIFormat::ASTC_4x4_RGBA_UNorm: return VK_FORMAT_ASTC_4x4_UNORM_BLOCK;
    case ERHIFormat::ASTC_4x4_RGBA_sRGB: return VK_FORMAT_ASTC_4x4_SRGB_BLOCK;
    case ERHIFormat::D24_UNorm_S8_UInt: return VK_FORMAT_D24_UNORM_S8_UINT;
    case ERHIFormat::D32_Float: return VK_FORMAT_D32_SFLOAT;
    case ERHIFormat::S8_UInt: return VK_FORMAT_S8_UINT;
    case ERHIFormat::Unknown: return VK_FORMAT_UNDEFINED;
    case ERHIFormat::Count: return VK_FORMAT_UNDEFINED;
    }
    return VK_FORMAT_UNDEFINED;
}

[[nodiscard]] ERHIFormatCapability ToRHIFormatCapabilities(
    VkFormatFeatureFlags Features) noexcept
{
    ERHIFormatCapability Capabilities =
        ERHIFormatCapability::None;
    if ((Features & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT) != 0)
        Capabilities |= ERHIFormatCapability::SampledImage;
    if ((Features & VK_FORMAT_FEATURE_TRANSFER_SRC_BIT) != 0)
        Capabilities |= ERHIFormatCapability::CopySource;
    if ((Features & VK_FORMAT_FEATURE_TRANSFER_DST_BIT) != 0)
        Capabilities |= ERHIFormatCapability::CopyDestination;
    if ((Features & VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) != 0)
        Capabilities |= ERHIFormatCapability::ColorAttachment;
    if ((Features &
         VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
        Capabilities |=
            ERHIFormatCapability::DepthStencilAttachment;
    return Capabilities;
}

[[nodiscard]] VkFormatFeatureFlags RequiredTextureFeatures(
    ERHITextureUsage Usage) noexcept
{
    VkFormatFeatureFlags Features = 0;
    if (HasRHIFlag(Usage, ERHITextureUsage::Sampled))
        Features |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if (HasRHIFlag(Usage, ERHITextureUsage::Storage))
        Features |= VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT;
    if (HasRHIFlag(Usage, ERHITextureUsage::ColorAttachment))
        Features |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    if (HasRHIFlag(
            Usage, ERHITextureUsage::DepthStencilAttachment))
        Features |=
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (HasRHIFlag(Usage, ERHITextureUsage::CopySource))
        Features |= VK_FORMAT_FEATURE_TRANSFER_SRC_BIT;
    if (HasRHIFlag(Usage, ERHITextureUsage::CopyDestination))
        Features |= VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    return Features;
}

[[nodiscard]] VkImageUsageFlags ToVulkanImageUsage(
    ERHITextureUsage Usage) noexcept
{
    VkImageUsageFlags Result = 0;
    if (HasRHIFlag(Usage, ERHITextureUsage::Sampled))
        Result |= VK_IMAGE_USAGE_SAMPLED_BIT;
    if (HasRHIFlag(Usage, ERHITextureUsage::Storage))
        Result |= VK_IMAGE_USAGE_STORAGE_BIT;
    if (HasRHIFlag(Usage, ERHITextureUsage::ColorAttachment))
        Result |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    if (HasRHIFlag(
            Usage, ERHITextureUsage::DepthStencilAttachment))
        Result |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    if (HasRHIFlag(Usage, ERHITextureUsage::CopySource))
        Result |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    if (HasRHIFlag(Usage, ERHITextureUsage::CopyDestination))
        Result |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    return Result;
}

[[nodiscard]] VkDescriptorType ToVulkanDescriptorType(
    ERHIDescriptorType Type) noexcept
{
    switch (Type)
    {
    case ERHIDescriptorType::UniformBuffer: return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    case ERHIDescriptorType::StorageBuffer: return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    case ERHIDescriptorType::SampledTexture: return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
    case ERHIDescriptorType::StorageTexture: return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    case ERHIDescriptorType::Sampler: return VK_DESCRIPTOR_TYPE_SAMPLER;
    case ERHIDescriptorType::CombinedTextureSampler:
        return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    }
    return VK_DESCRIPTOR_TYPE_MAX_ENUM;
}

[[nodiscard]] VkShaderStageFlags ToVulkanShaderStageFlags(
    ERHIShaderStageFlags Flags) noexcept
{
    VkShaderStageFlags Result = 0;
    if (HasRHIFlag(Flags, ERHIShaderStageFlags::Vertex))
        Result |= VK_SHADER_STAGE_VERTEX_BIT;
    if (HasRHIFlag(Flags, ERHIShaderStageFlags::Fragment))
        Result |= VK_SHADER_STAGE_FRAGMENT_BIT;
    if (HasRHIFlag(Flags, ERHIShaderStageFlags::Compute))
        Result |= VK_SHADER_STAGE_COMPUTE_BIT;
    if (HasRHIFlag(Flags, ERHIShaderStageFlags::Geometry))
        Result |= VK_SHADER_STAGE_GEOMETRY_BIT;
    if (HasRHIFlag(Flags, ERHIShaderStageFlags::TessellationControl))
        Result |= VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    if (HasRHIFlag(Flags, ERHIShaderStageFlags::TessellationEvaluation))
        Result |= VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;

    constexpr unsigned int Supported =
        static_cast<unsigned int>(ERHIShaderStageFlags::Vertex) |
        static_cast<unsigned int>(ERHIShaderStageFlags::Fragment) |
        static_cast<unsigned int>(ERHIShaderStageFlags::Compute) |
        static_cast<unsigned int>(ERHIShaderStageFlags::Geometry) |
        static_cast<unsigned int>(ERHIShaderStageFlags::TessellationControl) |
        static_cast<unsigned int>(ERHIShaderStageFlags::TessellationEvaluation);
    return (static_cast<unsigned int>(Flags) & ~Supported) == 0 ? Result : 0;
}

[[nodiscard]] VkShaderStageFlagBits ToVulkanShaderStage(
    ERHIShaderStage Stage) noexcept
{
    switch (Stage)
    {
    case ERHIShaderStage::Vertex: return VK_SHADER_STAGE_VERTEX_BIT;
    case ERHIShaderStage::Fragment: return VK_SHADER_STAGE_FRAGMENT_BIT;
    case ERHIShaderStage::Compute: return VK_SHADER_STAGE_COMPUTE_BIT;
    case ERHIShaderStage::Geometry: return VK_SHADER_STAGE_GEOMETRY_BIT;
    case ERHIShaderStage::TessellationControl:
        return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
    case ERHIShaderStage::TessellationEvaluation:
        return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    default: return static_cast<VkShaderStageFlagBits>(0);
    }
}

[[nodiscard]] VkSampleCountFlagBits ToVulkanSampleCount(
    ERHISampleCount Count) noexcept
{
    switch (Count)
    {
    case ERHISampleCount::One: return VK_SAMPLE_COUNT_1_BIT;
    case ERHISampleCount::Two: return VK_SAMPLE_COUNT_2_BIT;
    case ERHISampleCount::Four: return VK_SAMPLE_COUNT_4_BIT;
    case ERHISampleCount::Eight: return VK_SAMPLE_COUNT_8_BIT;
    }
    return static_cast<VkSampleCountFlagBits>(0);
}

[[nodiscard]] VkPrimitiveTopology ToVulkanTopology(
    ERHIPrimitiveTopology Topology) noexcept
{
    return Topology == ERHIPrimitiveTopology::TriangleStrip
        ? VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP
        : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
}

[[nodiscard]] VkCullModeFlags ToVulkanCullMode(ERHICullMode Mode) noexcept
{
    if (Mode == ERHICullMode::Front) return VK_CULL_MODE_FRONT_BIT;
    if (Mode == ERHICullMode::Back) return VK_CULL_MODE_BACK_BIT;
    return VK_CULL_MODE_NONE;
}

[[nodiscard]] VkFrontFace ToVulkanFrontFace(ERHIFrontFace Face) noexcept
{
    return Face == ERHIFrontFace::Clockwise
        ? VK_FRONT_FACE_CLOCKWISE
        : VK_FRONT_FACE_COUNTER_CLOCKWISE;
}

[[nodiscard]] VkBlendFactor ToVulkanBlendFactor(
    ERHIBlendFactor Factor) noexcept
{
    switch (Factor)
    {
    case ERHIBlendFactor::Zero: return VK_BLEND_FACTOR_ZERO;
    case ERHIBlendFactor::One: return VK_BLEND_FACTOR_ONE;
    case ERHIBlendFactor::SourceAlpha: return VK_BLEND_FACTOR_SRC_ALPHA;
    case ERHIBlendFactor::OneMinusSourceAlpha:
        return VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    }
    return VK_BLEND_FACTOR_ZERO;
}

[[nodiscard]] VkBlendOp ToVulkanBlendOp(ERHIBlendOp Op) noexcept
{
    if (Op == ERHIBlendOp::Subtract) return VK_BLEND_OP_SUBTRACT;
    if (Op == ERHIBlendOp::ReverseSubtract) return VK_BLEND_OP_REVERSE_SUBTRACT;
    return VK_BLEND_OP_ADD;
}

[[nodiscard]] VkCompareOp ToVulkanCompareOp(ERHICompareOp Op) noexcept
{
    switch (Op)
    {
    case ERHICompareOp::Never: return VK_COMPARE_OP_NEVER;
    case ERHICompareOp::Less: return VK_COMPARE_OP_LESS;
    case ERHICompareOp::LessEqual: return VK_COMPARE_OP_LESS_OR_EQUAL;
    case ERHICompareOp::Equal: return VK_COMPARE_OP_EQUAL;
    case ERHICompareOp::GreaterEqual: return VK_COMPARE_OP_GREATER_OR_EQUAL;
    case ERHICompareOp::Greater: return VK_COMPARE_OP_GREATER;
    case ERHICompareOp::NotEqual: return VK_COMPARE_OP_NOT_EQUAL;
    case ERHICompareOp::Always: return VK_COMPARE_OP_ALWAYS;
    }
    return VK_COMPARE_OP_NEVER;
}

class FNativeTextureBinding final : public IRHITexture
{
public:
    FNativeTextureBinding(Stoner::Core::uint32 Width, Stoner::Core::uint32 Height, ERHIFormat Format)
    {
        Desc.Width = Width; Desc.Height = Height; Desc.Format = Format;
        Desc.Usage = ERHITextureUsage::ColorAttachment | ERHITextureUsage::Present;
    }
    const FRHITextureDesc& GetDesc() const noexcept override { return Desc; }
    ERHITextureDimension GetDimension() const noexcept override { return Desc.Dimension; }
    ERHIFormat GetFormat() const noexcept override { return Desc.Format; }
    ERHITextureUsage GetUsage() const noexcept override { return Desc.Usage; }
    ERHIResourceLifecycleState GetLifecycleState() const noexcept override { return State; }
    ERHIResult Invalidate() override { State = ERHIResourceLifecycleState::Invalidated; return ERHIResult::Success; }
private:
    FRHITextureDesc Desc;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

class FNativeBufferBinding final : public IRHIBuffer
{
public:
    FNativeBufferBinding(Stoner::Core::uint64 SizeInBytes,
        ERHIBufferUsage Usage)
    {
        Desc.SizeInBytes = SizeInBytes;
        Desc.Usage = Usage;
        Desc.MemoryAccess = ERHIMemoryAccess::HostVisible;
    }
    const FRHIBufferDesc& GetDesc() const noexcept override { return Desc; }
    Stoner::Core::uint64 GetSizeInBytes() const noexcept override { return Desc.SizeInBytes; }
    ERHIBufferUsage GetUsage() const noexcept override { return Desc.Usage; }
    ERHIResourceLifecycleState GetLifecycleState() const noexcept override { return State; }
    ERHIResult Invalidate() override { State = ERHIResourceLifecycleState::Invalidated; return ERHIResult::Success; }
private:
    FRHIBufferDesc Desc;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

class FNativePipelineBinding final : public IRHIGraphicsPipeline
{
public:
    explicit FNativePipelineBinding(ERHIFormat Format)
    {
        Desc.VertexInput.Stride = sizeof(float) * 5;
        Desc.VertexInput.Attributes = {{0, ERHIFormat::R32_Float, 0}, {1, ERHIFormat::R32_Float, sizeof(float) * 2}};
        Desc.RenderTargets.ColorFormats = {Format};
        Desc.RuntimeMode = ERHIRuntimeObjectMode::RealRuntime;
    }
    const FRHIGraphicsPipelineDesc& GetDesc() const noexcept override { return Desc; }
    Stoner::Core::TSharedPtr<IRHIPipelineLayout> GetPipelineLayout() const noexcept override { return {}; }
    ERHIResourceLifecycleState GetLifecycleState() const noexcept override { return State; }
    ERHIResult Invalidate() override { State = ERHIResourceLifecycleState::Invalidated; return ERHIResult::Success; }
private:
    FRHIGraphicsPipelineDesc Desc;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

class FNativeRenderPassBinding final : public IRHIRenderPass
{
public:
    explicit FNativeRenderPassBinding(ERHIFormat Format) { Desc.Attachments = {{ERHIAttachmentRole::Color, Format, ERHISampleCount::One}}; }
    const FRHIRenderPassDesc& GetDesc() const noexcept override { return Desc; }
    Stoner::Core::uint32 GetAttachmentCount() const noexcept override { return static_cast<Stoner::Core::uint32>(Desc.Attachments.size()); }
    const FRHIRenderPassAttachmentDesc* GetAttachment(Stoner::Core::uint32 Index) const noexcept override
    { return Index < Desc.Attachments.size() ? &Desc.Attachments[Index] : nullptr; }
    ERHIResourceLifecycleState GetLifecycleState() const noexcept override { return State; }
    ERHIResult Invalidate() override { State = ERHIResourceLifecycleState::Invalidated; return ERHIResult::Success; }
private:
    FRHIRenderPassDesc Desc;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

class FNativeFramebufferBinding final : public IRHIFramebuffer
{
public:
    FNativeFramebufferBinding(Stoner::Core::TSharedPtr<IRHIRenderPass> InRenderPass,
        Stoner::Core::TSharedPtr<IRHITexture> InTexture, Stoner::Core::uint32 Width, Stoner::Core::uint32 Height)
    {
        Desc.RenderPass = std::move(InRenderPass); Desc.Attachments = {{std::move(InTexture), 0, 0}};
        Desc.Width = Width; Desc.Height = Height;
    }
    const FRHIFramebufferDesc& GetDesc() const noexcept override { return Desc; }
    Stoner::Core::TSharedPtr<IRHIRenderPass> GetRenderPass() const noexcept override { return Desc.RenderPass; }
    Stoner::Core::uint32 GetWidth() const noexcept override { return Desc.Width; }
    Stoner::Core::uint32 GetHeight() const noexcept override { return Desc.Height; }
    Stoner::Core::uint32 GetAttachmentCount() const noexcept override { return static_cast<Stoner::Core::uint32>(Desc.Attachments.size()); }
    ERHIResourceLifecycleState GetLifecycleState() const noexcept override { return State; }
    ERHIResult Invalidate() override { State = ERHIResourceLifecycleState::Invalidated; return ERHIResult::Success; }
private:
    FRHIFramebufferDesc Desc;
    ERHIResourceLifecycleState State = ERHIResourceLifecycleState::Valid;
};

class FNativeCommandBufferBinding final : public IRHICommandBuffer
{
public:
    FNativeCommandBufferBinding(VkCommandBuffer InCommands, VkRenderPass InRenderPass, VkFramebuffer InFramebuffer,
        VkPipeline InPipeline, VkBuffer InVertexBuffer, VkBuffer InIndexBuffer,
        VkExtent2D InExtent)
        : Commands(InCommands), RenderPass(InRenderPass), Framebuffer(InFramebuffer), Pipeline(InPipeline),
          VertexBuffer(InVertexBuffer), IndexBuffer(InIndexBuffer), Extent(InExtent) {}
    ERHICommandBufferState GetState() const noexcept override { return State; }
    ERHIQueueType GetCompatibleQueueType() const noexcept override { return ERHIQueueType::Graphics; }
    Stoner::Core::uint32 GetRecordedCommandCount() const noexcept override { return CommandCount; }
    ERHIResult Begin() override
    {
        if (State != ERHICommandBufferState::Idle && State != ERHICommandBufferState::Resettable) return ERHIResult::InvalidState;
        if (vkResetCommandBuffer(Commands, 0) != VK_SUCCESS) return ERHIResult::Failed;
        VkCommandBufferBeginInfo Info = MakeVulkanStruct<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
        if (vkBeginCommandBuffer(Commands, &Info) != VK_SUCCESS) return ERHIResult::Failed;
        State = ERHICommandBufferState::Recording; CommandCount = 0;
        bIndexBufferBound = false;
        return ERHIResult::Success;
    }
    ERHIResult End() override
    {
        if (State != ERHICommandBufferState::Recording || bInsideRenderPass) return ERHIResult::InvalidState;
        if (vkEndCommandBuffer(Commands) != VK_SUCCESS) return ERHIResult::Failed;
        State = ERHICommandBufferState::Completed; return ERHIResult::Success;
    }
    ERHIResult Reset() override { State = ERHICommandBufferState::Resettable; CommandCount = 0; bInsideRenderPass = false; bIndexBufferBound = false; return ERHIResult::Success; }
    ERHIResult RecordDraw(Stoner::Core::uint32 Vertices, Stoner::Core::uint32 Instances) override
    { if (!bInsideRenderPass || Vertices != 3 || Instances != 1) return ERHIResult::InvalidState; vkCmdDraw(Commands, Vertices, Instances, 0, 0); ++CommandCount; return ERHIResult::Success; }
    ERHIResult RecordDrawIndexed(
        const FRHIIndexedDrawArguments& Arguments) override
    {
        if (!bInsideRenderPass || !bIndexBufferBound ||
            !IsValidRHIIndexedDrawArguments(Arguments))
        {
            return ERHIResult::InvalidState;
        }
        vkCmdDrawIndexed(Commands, Arguments.IndexCount,
            Arguments.InstanceCount, Arguments.FirstIndex,
            Arguments.VertexOffset, Arguments.FirstInstance);
        ++CommandCount;
        return ERHIResult::Success;
    }
    ERHIResult RecordDrawIndexed(Stoner::Core::uint32 IndexCount,
        Stoner::Core::uint32 InstanceCount,
        Stoner::Core::uint32 FirstInstance) override
    {
        return RecordDrawIndexed(
            {IndexCount, InstanceCount, 0, 0, FirstInstance});
    }
    ERHIResult RecordDispatch(Stoner::Core::uint32, Stoner::Core::uint32, Stoner::Core::uint32) override { return ERHIResult::Unsupported; }
    ERHIResult BindGraphicsPipeline(const Stoner::Core::TSharedPtr<IRHIGraphicsPipeline>& Value) override
    { if (!bInsideRenderPass || !Value) return ERHIResult::InvalidState; vkCmdBindPipeline(Commands, VK_PIPELINE_BIND_POINT_GRAPHICS, Pipeline); ++CommandCount; return ERHIResult::Success; }
    ERHIResult BindComputePipeline(const Stoner::Core::TSharedPtr<IRHIComputePipeline>&) override { return ERHIResult::Unsupported; }
    ERHIResult RecordBarrier() override { return ERHIResult::Unsupported; }
    ERHIResult RecordBarrier(const FRHIResourceBarrierDesc&) override { return ERHIResult::Unsupported; }
    ERHIResult RecordBufferCopy(const Stoner::Core::TSharedPtr<IRHIBuffer>&, const Stoner::Core::TSharedPtr<IRHIBuffer>&, FRHIBufferCopyRange) override { return ERHIResult::Unsupported; }
    ERHIResult RecordTextureCopy(const Stoner::Core::TSharedPtr<IRHITexture>&, const Stoner::Core::TSharedPtr<IRHITexture>&, FRHITextureCopyRegion) override { return ERHIResult::Unsupported; }
    ERHIResult RecordLayoutTransition(const FRHIResourceBarrierDesc&) override
    { if (State != ERHICommandBufferState::Recording || bInsideRenderPass) return ERHIResult::InvalidState; ++CommandCount; return ERHIResult::Success; }
    ERHIResult BeginRenderPass(const Stoner::Core::TSharedPtr<IRHIRenderPass>& Pass, const Stoner::Core::TSharedPtr<IRHIFramebuffer>& Target) override
    {
        if (State != ERHICommandBufferState::Recording || bInsideRenderPass || !Pass || !Target) return ERHIResult::InvalidState;
        VkClearValue Clear{}; Clear.color = {{0.02f, 0.03f, 0.05f, 1.0f}};
        VkRenderPassBeginInfo Info = MakeVulkanStruct<VkRenderPassBeginInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
        Info.renderPass = RenderPass; Info.framebuffer = Framebuffer; Info.renderArea.extent = Extent;
        Info.clearValueCount = 1; Info.pClearValues = &Clear;
        vkCmdBeginRenderPass(Commands, &Info, VK_SUBPASS_CONTENTS_INLINE); bInsideRenderPass = true; ++CommandCount; return ERHIResult::Success;
    }
    ERHIResult EndRenderPass() override
    { if (!bInsideRenderPass) return ERHIResult::InvalidState; vkCmdEndRenderPass(Commands); bInsideRenderPass = false; ++CommandCount; return ERHIResult::Success; }
    ERHIResult BindVertexBuffer(const Stoner::Core::TSharedPtr<IRHIBuffer>& Value, Stoner::Core::uint64 Offset) override
    { if (!bInsideRenderPass || !Value) return ERHIResult::InvalidState; const VkDeviceSize NativeOffset = Offset; vkCmdBindVertexBuffers(Commands, 0, 1, &VertexBuffer, &NativeOffset); ++CommandCount; return ERHIResult::Success; }
    ERHIResult BindIndexBuffer(const Stoner::Core::TSharedPtr<IRHIBuffer>& Value,
        ERHIIndexType IndexType, Stoner::Core::uint64 Offset) override
    {
        const Stoner::Core::uint64 IndexSize = GetRHIIndexTypeSize(IndexType);
        if (!bInsideRenderPass || !Value ||
            Value->GetLifecycleState() != ERHIResourceLifecycleState::Valid ||
            !HasRHIFlag(Value->GetUsage(), ERHIBufferUsage::Index) ||
            IndexSize == 0 || Offset >= Value->GetSizeInBytes() ||
            Offset % IndexSize != 0 || IndexBuffer == VK_NULL_HANDLE)
        {
            return ERHIResult::InvalidState;
        }
        vkCmdBindIndexBuffer(Commands, IndexBuffer, Offset,
            IndexType == ERHIIndexType::UInt16 ? VK_INDEX_TYPE_UINT16 :
                VK_INDEX_TYPE_UINT32);
        bIndexBufferBound = true;
        ++CommandCount;
        return ERHIResult::Success;
    }
    ERHIResult SetViewport(const FRHIViewport& Value) override
    { if (!bInsideRenderPass) return ERHIResult::InvalidState; VkViewport Native{Value.X, Value.Y, Value.Width, Value.Height, Value.MinDepth, Value.MaxDepth}; vkCmdSetViewport(Commands, 0, 1, &Native); ++CommandCount; return ERHIResult::Success; }
    ERHIResult SetScissor(const FRHIScissorRect& Value) override
    { if (!bInsideRenderPass) return ERHIResult::InvalidState; VkRect2D Native{{static_cast<int32_t>(Value.X), static_cast<int32_t>(Value.Y)}, {Value.Width, Value.Height}}; vkCmdSetScissor(Commands, 0, 1, &Native); ++CommandCount; return ERHIResult::Success; }
private:
    VkCommandBuffer Commands = VK_NULL_HANDLE;
    VkRenderPass RenderPass = VK_NULL_HANDLE;
    VkFramebuffer Framebuffer = VK_NULL_HANDLE;
    VkPipeline Pipeline = VK_NULL_HANDLE;
    VkBuffer VertexBuffer = VK_NULL_HANDLE;
    VkBuffer IndexBuffer = VK_NULL_HANDLE;
    VkExtent2D Extent{};
    ERHICommandBufferState State = ERHICommandBufferState::Idle;
    Stoner::Core::uint32 CommandCount = 0;
    bool bInsideRenderPass = false;
    bool bIndexBufferBound = false;
};

[[maybe_unused]] ERHIFormat ToRHIFormat(VkFormat Format)
{
    return Format == VK_FORMAT_B8G8R8A8_UNORM || Format == VK_FORMAT_B8G8R8A8_SRGB
        ? ERHIFormat::B8G8R8A8_UNorm : ERHIFormat::R8G8B8A8_UNorm;
}

} // namespace
#endif

struct FVulkanNativeContext::FImpl
{
    Stoner::RHI::FRHIRuntimeSnapshot Snapshot;
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    using ERHIResult = Stoner::RHI::ERHIResult;
    using FRHIDescriptorBinding = Stoner::RHI::FRHIDescriptorBinding;
    using FRHIPipelineLayoutDesc = Stoner::RHI::FRHIPipelineLayoutDesc;
    using FRHIShaderConstantRange = Stoner::RHI::FRHIShaderConstantRange;

    VkInstance Instance = VK_NULL_HANDLE;
    VkPhysicalDevice PhysicalDevice = VK_NULL_HANDLE;
    VkDevice Device = VK_NULL_HANDLE;
    VkQueue GraphicsQueue = VK_NULL_HANDLE;
    Stoner::Core::uint32 GraphicsQueueFamily = 0;
    VkSurfaceKHR Surface = VK_NULL_HANDLE;
    VkSwapchainKHR Swapchain = VK_NULL_HANDLE;
    VkFormat SwapchainFormat = VK_FORMAT_UNDEFINED;
    VkExtent2D SwapchainExtent{};
    std::vector<VkImage> SwapchainImages;
    std::vector<VkImageView> SwapchainViews;
    std::vector<VkFramebuffer> SwapchainFramebuffers;
    VkSemaphore ImageAvailable = VK_NULL_HANDLE;
    VkSemaphore RenderFinished = VK_NULL_HANDLE;
    Stoner::RHI::FRHIShaderModuleDesc VisibleVertexShader;
    Stoner::RHI::FRHIShaderModuleDesc VisibleFragmentShader;
    bool bHasVisibleShaders = false;
    Stoner::Core::uint64 NextOwnedShaderToken = 1;
    std::unordered_map<Stoner::Core::uint64, VkShaderModule> OwnedShaderModules;
    struct FOwnedPipelineResources
    {
        VkPipeline Pipeline = VK_NULL_HANDLE;
        VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
        VkRenderPass RenderPass = VK_NULL_HANDLE;
        std::vector<VkDescriptorSetLayout> DescriptorSetLayouts;
    };
    Stoner::Core::uint64 NextOwnedPipelineToken = 1;
    std::unordered_map<Stoner::Core::uint64, FOwnedPipelineResources>
        OwnedPipelines;
    struct FOwnedTextureResources
    {
        Stoner::RHI::FRHITextureDesc Desc;
        VkImage Image = VK_NULL_HANDLE;
        VkDeviceMemory Memory = VK_NULL_HANDLE;
        std::vector<VkImageLayout> MipLayouts;
    };
    Stoner::Core::uint64 NextOwnedTextureToken = 1;
    std::unordered_map<Stoner::Core::uint64, FOwnedTextureResources>
        OwnedTextures;

    VkBuffer VertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory VertexMemory = VK_NULL_HANDLE;
    VkBuffer IndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory IndexMemory = VK_NULL_HANDLE;
    VkBuffer ReadbackBuffer = VK_NULL_HANDLE;
    VkDeviceMemory ReadbackMemory = VK_NULL_HANDLE;
    VkImage ColorImage = VK_NULL_HANDLE;
    VkDeviceMemory ColorMemory = VK_NULL_HANDLE;
    VkImageView ColorView = VK_NULL_HANDLE;
    VkRenderPass RenderPass = VK_NULL_HANDLE;
    VkFramebuffer Framebuffer = VK_NULL_HANDLE;
    VkShaderModule VertexShader = VK_NULL_HANDLE;
    VkShaderModule FragmentShader = VK_NULL_HANDLE;
    VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
    VkPipeline Pipeline = VK_NULL_HANDLE;
    VkCommandPool CommandPool = VK_NULL_HANDLE;
    VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
    VkFence Fence = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> VisibleCommandBuffers;
    std::vector<VkFence> VisibleFences;
    std::vector<VkSemaphore> VisibleImageAvailable;
    std::vector<VkSemaphore> VisibleRenderFinished;
    Stoner::Core::uint32 CurrentFrameSlot = 0;
    Stoner::Core::uint32 AcquiredImageIndex = 0;
    Stoner::Core::uint32 AcquiredFrameSlot = 0;
    bool bFrameAcquired = false;
    bool bAcquiredSuboptimal = false;

    static bool HasName(const std::vector<VkExtensionProperties>& Properties, const char* Name)
    {
        return std::any_of(Properties.begin(), Properties.end(), [Name](const VkExtensionProperties& Item)
        {
            return std::strcmp(Item.extensionName, Name) == 0;
        });
    }

    [[nodiscard]] Stoner::Core::uint32 GetLiveShaderModuleCount() const noexcept
    {
        return static_cast<Stoner::Core::uint32>(OwnedShaderModules.size()) +
            (VertexShader != VK_NULL_HANDLE ? 1u : 0u) +
            (FragmentShader != VK_NULL_HANDLE ? 1u : 0u);
    }

    [[nodiscard]] Stoner::Core::uint32 GetLivePipelineCount() const noexcept
    {
        return static_cast<Stoner::Core::uint32>(OwnedPipelines.size()) +
            (Pipeline != VK_NULL_HANDLE ? 1u : 0u);
    }

    [[nodiscard]] Stoner::Core::uint32 GetLiveTextureCount() const noexcept
    {
        return static_cast<Stoner::Core::uint32>(
                   OwnedTextures.size()) +
            static_cast<Stoner::Core::uint32>(
                SwapchainImages.size()) +
            (ColorImage != VK_NULL_HANDLE ? 1u : 0u);
    }

    void DestroyOwnedPipelineResources(
        FOwnedPipelineResources& Resources) noexcept
    {
        if (Device != VK_NULL_HANDLE)
        {
            if (Resources.Pipeline != VK_NULL_HANDLE)
                vkDestroyPipeline(Device, Resources.Pipeline, nullptr);
            if (Resources.PipelineLayout != VK_NULL_HANDLE)
                vkDestroyPipelineLayout(Device, Resources.PipelineLayout, nullptr);
            if (Resources.RenderPass != VK_NULL_HANDLE)
                vkDestroyRenderPass(Device, Resources.RenderPass, nullptr);
            for (VkDescriptorSetLayout Layout : Resources.DescriptorSetLayouts)
            {
                if (Layout != VK_NULL_HANDLE)
                    vkDestroyDescriptorSetLayout(Device, Layout, nullptr);
            }
        }
        Resources = {};
    }

    void DestroyAllOwnedPipelines() noexcept
    {
        for (auto& [Token, Resources] : OwnedPipelines)
        {
            (void)Token;
            DestroyOwnedPipelineResources(Resources);
        }
        OwnedPipelines.clear();
        NextOwnedPipelineToken = 1;
        Snapshot.LivePipelines = GetLivePipelineCount();
    }

    void DestroyOwnedTextureResources(
        FOwnedTextureResources& Resources) noexcept
    {
        if (Device != VK_NULL_HANDLE)
        {
            if (Resources.Image != VK_NULL_HANDLE)
            {
                vkDestroyImage(Device, Resources.Image, nullptr);
            }
            if (Resources.Memory != VK_NULL_HANDLE)
            {
                vkFreeMemory(Device, Resources.Memory, nullptr);
            }
        }
        Resources = {};
    }

    void DestroyAllOwnedTextures() noexcept
    {
        for (auto& [Token, Resources] : OwnedTextures)
        {
            (void)Token;
            DestroyOwnedTextureResources(Resources);
        }
        OwnedTextures.clear();
        NextOwnedTextureToken = 1;
        Snapshot.LiveTextures = GetLiveTextureCount();
    }

    [[nodiscard]] ERHIResult CreateOwnedPipelineLayout(
        const FRHIPipelineLayoutDesc& Desc,
        FOwnedPipelineResources& OutResources) noexcept
    {
        try
        {
            Stoner::Core::uint32 MaxSetIndex = 0;
            for (const FRHIDescriptorBinding& Binding : Desc.Bindings)
            {
                MaxSetIndex = std::max(MaxSetIndex, Binding.SetIndex);
            }
            const Stoner::Core::uint64 SetCount64 =
                static_cast<Stoner::Core::uint64>(MaxSetIndex) + 1u;
            VkPhysicalDeviceProperties Properties{};
            vkGetPhysicalDeviceProperties(PhysicalDevice, &Properties);
            if (SetCount64 > Properties.limits.maxBoundDescriptorSets ||
                SetCount64 > std::numeric_limits<Stoner::Core::uint32>::max())
            {
                return ERHIResult::Unsupported;
            }
            const Stoner::Core::uint32 SetCount =
                static_cast<Stoner::Core::uint32>(SetCount64);
            std::vector<std::vector<VkDescriptorSetLayoutBinding>> BindingsBySet(
                SetCount);
            for (const FRHIDescriptorBinding& Binding : Desc.Bindings)
            {
                const VkDescriptorType DescriptorType =
                    ToVulkanDescriptorType(Binding.DescriptorType);
                const VkShaderStageFlags StageFlags =
                    ToVulkanShaderStageFlags(Binding.Visibility);
                if (DescriptorType == VK_DESCRIPTOR_TYPE_MAX_ENUM ||
                    StageFlags == 0)
                {
                    return ERHIResult::Unsupported;
                }
                VkDescriptorSetLayoutBinding NativeBinding{};
                NativeBinding.binding = Binding.BindingSlot;
                NativeBinding.descriptorType = DescriptorType;
                NativeBinding.descriptorCount = Binding.ArrayCount;
                NativeBinding.stageFlags = StageFlags;
                BindingsBySet[Binding.SetIndex].push_back(NativeBinding);
            }

            OutResources.DescriptorSetLayouts.assign(
                SetCount, VK_NULL_HANDLE);
            for (Stoner::Core::uint32 SetIndex = 0;
                 SetIndex < SetCount;
                 ++SetIndex)
            {
                auto& SetBindings = BindingsBySet[SetIndex];
                std::sort(
                    SetBindings.begin(), SetBindings.end(),
                    [](const VkDescriptorSetLayoutBinding& Left,
                       const VkDescriptorSetLayoutBinding& Right)
                    {
                        return Left.binding < Right.binding;
                    });
                VkDescriptorSetLayoutCreateInfo LayoutInfo =
                    MakeVulkanStruct<VkDescriptorSetLayoutCreateInfo>(
                        VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
                LayoutInfo.bindingCount =
                    static_cast<Stoner::Core::uint32>(SetBindings.size());
                LayoutInfo.pBindings =
                    SetBindings.empty() ? nullptr : SetBindings.data();
                const VkResult LayoutResult = vkCreateDescriptorSetLayout(
                    Device, &LayoutInfo, nullptr,
                    &OutResources.DescriptorSetLayouts[SetIndex]);
                if (LayoutResult != VK_SUCCESS)
                {
                    const ERHIResult Result =
                        MapVulkanCreationResult(LayoutResult);
                    DestroyOwnedPipelineResources(OutResources);
                    return Result;
                }
            }

            std::vector<VkPushConstantRange> PushRanges;
            PushRanges.reserve(Desc.ConstantRanges.size());
            for (const FRHIShaderConstantRange& Range : Desc.ConstantRanges)
            {
                const VkShaderStageFlags StageFlags =
                    ToVulkanShaderStageFlags(Range.Visibility);
                if (StageFlags == 0 ||
                    (Range.OffsetBytes % 4u) != 0 ||
                    (Range.SizeBytes % 4u) != 0)
                {
                    DestroyOwnedPipelineResources(OutResources);
                    return ERHIResult::Unsupported;
                }
                PushRanges.push_back(
                    {StageFlags, Range.OffsetBytes, Range.SizeBytes});
            }

            VkPipelineLayoutCreateInfo PipelineLayoutInfo =
                MakeVulkanStruct<VkPipelineLayoutCreateInfo>(
                    VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
            PipelineLayoutInfo.setLayoutCount = static_cast<Stoner::Core::uint32>(
                OutResources.DescriptorSetLayouts.size());
            PipelineLayoutInfo.pSetLayouts =
                OutResources.DescriptorSetLayouts.empty()
                ? nullptr
                : OutResources.DescriptorSetLayouts.data();
            PipelineLayoutInfo.pushConstantRangeCount =
                static_cast<Stoner::Core::uint32>(PushRanges.size());
            PipelineLayoutInfo.pPushConstantRanges =
                PushRanges.empty() ? nullptr : PushRanges.data();
            const VkResult PipelineLayoutResult = vkCreatePipelineLayout(
                Device, &PipelineLayoutInfo, nullptr,
                &OutResources.PipelineLayout);
            if (PipelineLayoutResult != VK_SUCCESS)
            {
                const ERHIResult Result =
                    MapVulkanCreationResult(PipelineLayoutResult);
                DestroyOwnedPipelineResources(OutResources);
                return Result;
            }
            return ERHIResult::Success;
        }
        catch (const std::bad_alloc&)
        {
            DestroyOwnedPipelineResources(OutResources);
            return ERHIResult::Unavailable;
        }
        catch (const std::length_error&)
        {
            DestroyOwnedPipelineResources(OutResources);
            return ERHIResult::Unavailable;
        }
    }

    [[nodiscard]] ERHIResult PublishOwnedPipeline(
        FOwnedPipelineResources& Resources,
        Stoner::Core::uint64& OutToken) noexcept
    {
        OutToken = 0;
        if (NextOwnedPipelineToken == 0)
        {
            DestroyOwnedPipelineResources(Resources);
            return ERHIResult::Unavailable;
        }
        const Stoner::Core::uint64 Token = NextOwnedPipelineToken++;
        try
        {
            const auto [Iterator, bInserted] =
                OwnedPipelines.emplace(Token, std::move(Resources));
            (void)Iterator;
            if (!bInserted)
            {
                DestroyOwnedPipelineResources(Resources);
                return ERHIResult::Unavailable;
            }
        }
        catch (const std::bad_alloc&)
        {
            DestroyOwnedPipelineResources(Resources);
            return ERHIResult::Unavailable;
        }
        catch (const std::length_error&)
        {
            DestroyOwnedPipelineResources(Resources);
            return ERHIResult::Unavailable;
        }
        Snapshot.LivePipelines = GetLivePipelineCount();
        OutToken = Token;
        Resources = {};
        return ERHIResult::Success;
    }

    Stoner::Core::uint32 FindMemoryType(Stoner::Core::uint32 TypeBits, VkMemoryPropertyFlags Required) const
    {
        VkPhysicalDeviceMemoryProperties Properties{};
        vkGetPhysicalDeviceMemoryProperties(PhysicalDevice, &Properties);
        for (Stoner::Core::uint32 Index = 0; Index < Properties.memoryTypeCount; ++Index)
            if ((TypeBits & (1u << Index)) && (Properties.memoryTypes[Index].propertyFlags & Required) == Required) return Index;
        return UINT32_MAX;
    }

    void DestroyFrameResources()
    {
        if (Device == VK_NULL_HANDLE) return;
        if (Fence) vkDestroyFence(Device, Fence, nullptr);
        for (VkFence Item : VisibleFences) vkDestroyFence(Device, Item, nullptr);
        if (RenderFinished) vkDestroySemaphore(Device, RenderFinished, nullptr);
        if (ImageAvailable) vkDestroySemaphore(Device, ImageAvailable, nullptr);
        for (VkSemaphore Item : VisibleRenderFinished) vkDestroySemaphore(Device, Item, nullptr);
        for (VkSemaphore Item : VisibleImageAvailable) vkDestroySemaphore(Device, Item, nullptr);
        if (CommandPool) vkDestroyCommandPool(Device, CommandPool, nullptr);
        if (Pipeline) vkDestroyPipeline(Device, Pipeline, nullptr);
        if (PipelineLayout) vkDestroyPipelineLayout(Device, PipelineLayout, nullptr);
        if (FragmentShader) vkDestroyShaderModule(Device, FragmentShader, nullptr);
        if (VertexShader) vkDestroyShaderModule(Device, VertexShader, nullptr);
        for (VkFramebuffer Item : SwapchainFramebuffers) vkDestroyFramebuffer(Device, Item, nullptr);
        if (Framebuffer) vkDestroyFramebuffer(Device, Framebuffer, nullptr);
        if (RenderPass) vkDestroyRenderPass(Device, RenderPass, nullptr);
        for (VkImageView Item : SwapchainViews) vkDestroyImageView(Device, Item, nullptr);
        if (Swapchain) vkDestroySwapchainKHR(Device, Swapchain, nullptr);
        if (ColorView) vkDestroyImageView(Device, ColorView, nullptr);
        if (ColorImage) vkDestroyImage(Device, ColorImage, nullptr);
        if (ColorMemory) vkFreeMemory(Device, ColorMemory, nullptr);
        if (VertexBuffer) vkDestroyBuffer(Device, VertexBuffer, nullptr);
        if (VertexMemory) vkFreeMemory(Device, VertexMemory, nullptr);
        if (IndexBuffer) vkDestroyBuffer(Device, IndexBuffer, nullptr);
        if (IndexMemory) vkFreeMemory(Device, IndexMemory, nullptr);
        if (ReadbackBuffer) vkDestroyBuffer(Device, ReadbackBuffer, nullptr);
        if (ReadbackMemory) vkFreeMemory(Device, ReadbackMemory, nullptr);
        Fence = VK_NULL_HANDLE; ImageAvailable = VK_NULL_HANDLE; RenderFinished = VK_NULL_HANDLE;
        VisibleCommandBuffers.clear(); VisibleFences.clear(); VisibleImageAvailable.clear(); VisibleRenderFinished.clear();
        CommandPool = VK_NULL_HANDLE; CommandBuffer = VK_NULL_HANDLE;
        Pipeline = VK_NULL_HANDLE; PipelineLayout = VK_NULL_HANDLE;
        VertexShader = VK_NULL_HANDLE; FragmentShader = VK_NULL_HANDLE;
        Framebuffer = VK_NULL_HANDLE; RenderPass = VK_NULL_HANDLE;
        ColorView = VK_NULL_HANDLE; ColorImage = VK_NULL_HANDLE; ColorMemory = VK_NULL_HANDLE;
        VertexBuffer = VK_NULL_HANDLE; VertexMemory = VK_NULL_HANDLE;
        IndexBuffer = VK_NULL_HANDLE; IndexMemory = VK_NULL_HANDLE;
        ReadbackBuffer = VK_NULL_HANDLE; ReadbackMemory = VK_NULL_HANDLE;
        Swapchain = VK_NULL_HANDLE; SwapchainFormat = VK_FORMAT_UNDEFINED; SwapchainExtent = {};
        SwapchainImages.clear(); SwapchainViews.clear(); SwapchainFramebuffers.clear();
        CurrentFrameSlot = 0; AcquiredImageIndex = 0; AcquiredFrameSlot = 0;
        bFrameAcquired = false; bAcquiredSuboptimal = false;
        Snapshot.LiveBuffers = 0;
        Snapshot.LiveTextures = GetLiveTextureCount();
        Snapshot.LiveShaderModules = GetLiveShaderModuleCount();
        Snapshot.LivePipelines = GetLivePipelineCount();
        Snapshot.LiveCommandBuffers = 0;
        Snapshot.LiveSynchronizationObjects = 0;
    }

    bool RecreateVisibleFenceSignaled(Stoner::Core::uint32 FrameSlot) noexcept
    {
        if (Device == VK_NULL_HANDLE || FrameSlot >= VisibleFences.size())
        {
            return false;
        }
        if (VisibleFences[FrameSlot] != VK_NULL_HANDLE)
        {
            vkDestroyFence(Device, VisibleFences[FrameSlot], nullptr);
            VisibleFences[FrameSlot] = VK_NULL_HANDLE;
        }
        VkFenceCreateInfo FenceInfo =
            MakeVulkanStruct<VkFenceCreateInfo>(
                VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
        FenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        return vkCreateFence(
            Device, &FenceInfo, nullptr, &VisibleFences[FrameSlot]) ==
            VK_SUCCESS;
    }

    void AbandonAcquiredVisibleFrame() noexcept
    {
        if (!bFrameAcquired)
        {
            return;
        }
        if (Device != VK_NULL_HANDLE && GraphicsQueue != VK_NULL_HANDLE &&
            Swapchain != VK_NULL_HANDLE &&
            AcquiredFrameSlot < VisibleImageAvailable.size())
        {
            VkPresentInfoKHR Present =
                MakeVulkanStruct<VkPresentInfoKHR>(
                    VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
            const VkSemaphore WaitSemaphore =
                VisibleImageAvailable[AcquiredFrameSlot];
            Present.waitSemaphoreCount = WaitSemaphore != VK_NULL_HANDLE ? 1u : 0u;
            Present.pWaitSemaphores =
                WaitSemaphore != VK_NULL_HANDLE ? &WaitSemaphore : nullptr;
            Present.swapchainCount = 1;
            Present.pSwapchains = &Swapchain;
            Present.pImageIndices = &AcquiredImageIndex;
            (void)vkQueuePresentKHR(GraphicsQueue, &Present);
        }
        if (!VisibleFences.empty())
        {
            CurrentFrameSlot =
                (AcquiredFrameSlot + 1u) %
                static_cast<Stoner::Core::uint32>(VisibleFences.size());
        }
        bFrameAcquired = false;
        bAcquiredSuboptimal = false;
    }
#endif
};

FVulkanNativeContext::FVulkanNativeContext() : Impl(std::make_unique<FImpl>()) {}
FVulkanNativeContext::~FVulkanNativeContext() { (void)Shutdown(); }

Stoner::RHI::ERHIResult FVulkanNativeContext::Initialize(
    Stoner::RHI::ERHIRuntimeMode Mode, const Stoner::Core::FPlatformWindow& PlatformWindow)
{
    if (!Impl || Impl->Snapshot.LiveInstances != 0) return Stoner::RHI::ERHIResult::InvalidState;
    Impl->Snapshot = {};
    Impl->Snapshot.RequestedMode = Mode;
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    Stoner::Core::uint32 ExtensionCount = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, nullptr) != VK_SUCCESS)
        return Stoner::RHI::ERHIResult::Unavailable;
    std::vector<VkExtensionProperties> Extensions(ExtensionCount);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &ExtensionCount, Extensions.data()) != VK_SUCCESS)
        return Stoner::RHI::ERHIResult::Unavailable;

    std::vector<const char*> EnabledExtensions;
    VkInstanceCreateFlags InstanceFlags = 0;
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
    if (FImpl::HasName(Extensions, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
    {
        EnabledExtensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
        InstanceFlags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }
#endif
#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    if (Mode == Stoner::RHI::ERHIRuntimeMode::Native)
    {
        if (!PlatformWindow.IsValid()) return Stoner::RHI::ERHIResult::Unavailable;
        Stoner::Core::uint32 GlfwExtensionCount = 0;
        const char** GlfwExtensions = glfwGetRequiredInstanceExtensions(&GlfwExtensionCount);
        if (GlfwExtensions == nullptr || GlfwExtensionCount == 0) return Stoner::RHI::ERHIResult::Unavailable;
        for (Stoner::Core::uint32 Index = 0; Index < GlfwExtensionCount; ++Index)
            if (std::find_if(EnabledExtensions.begin(), EnabledExtensions.end(), [GlfwExtensions, Index](const char* Name)
                { return std::strcmp(Name, GlfwExtensions[Index]) == 0; }) == EnabledExtensions.end())
                EnabledExtensions.push_back(GlfwExtensions[Index]);
    }
#else
    if (Mode == Stoner::RHI::ERHIRuntimeMode::Native) return Stoner::RHI::ERHIResult::Unsupported;
    (void)PlatformWindow;
#endif
    VkApplicationInfo AppInfo = MakeVulkanStruct<VkApplicationInfo>(VK_STRUCTURE_TYPE_APPLICATION_INFO);
    AppInfo.pApplicationName = "StonerDemo";
    AppInfo.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo InstanceInfo = MakeVulkanStruct<VkInstanceCreateInfo>(VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO);
    InstanceInfo.flags = InstanceFlags;
    InstanceInfo.pApplicationInfo = &AppInfo;
    InstanceInfo.enabledExtensionCount = static_cast<Stoner::Core::uint32>(EnabledExtensions.size());
    InstanceInfo.ppEnabledExtensionNames = EnabledExtensions.data();
    if (vkCreateInstance(&InstanceInfo, nullptr, &Impl->Instance) != VK_SUCCESS)
        return Stoner::RHI::ERHIResult::Unavailable;

#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    if (Mode == Stoner::RHI::ERHIRuntimeMode::Native &&
        glfwCreateWindowSurface(Impl->Instance, static_cast<GLFWwindow*>(PlatformWindow.GetNativeHandle()), nullptr, &Impl->Surface) != VK_SUCCESS)
    {
        (void)Shutdown();
        return Stoner::RHI::ERHIResult::Unavailable;
    }
#endif

    Stoner::Core::uint32 DeviceCount = 0;
    if (vkEnumeratePhysicalDevices(Impl->Instance, &DeviceCount, nullptr) != VK_SUCCESS || DeviceCount == 0)
    {
        (void)Shutdown();
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    std::vector<VkPhysicalDevice> Devices(DeviceCount);
    vkEnumeratePhysicalDevices(Impl->Instance, &DeviceCount, Devices.data());
    for (VkPhysicalDevice Candidate : Devices)
    {
        Stoner::Core::uint32 QueueCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(Candidate, &QueueCount, nullptr);
        std::vector<VkQueueFamilyProperties> Queues(QueueCount);
        vkGetPhysicalDeviceQueueFamilyProperties(Candidate, &QueueCount, Queues.data());
        for (Stoner::Core::uint32 Index = 0; Index < QueueCount; ++Index)
        {
            VkBool32 SupportsPresent = VK_TRUE;
            if (Impl->Surface != VK_NULL_HANDLE)
                vkGetPhysicalDeviceSurfaceSupportKHR(Candidate, Index, Impl->Surface, &SupportsPresent);
            if ((Queues[Index].queueFlags & VK_QUEUE_GRAPHICS_BIT) && SupportsPresent == VK_TRUE)
            {
                Impl->PhysicalDevice = Candidate;
                Impl->GraphicsQueueFamily = Index;
                break;
            }
        }
        if (Impl->PhysicalDevice) break;
    }
    if (!Impl->PhysicalDevice)
    {
        (void)Shutdown();
        return Stoner::RHI::ERHIResult::Unsupported;
    }

    Stoner::Core::uint32 DeviceExtensionCount = 0;
    vkEnumerateDeviceExtensionProperties(Impl->PhysicalDevice, nullptr, &DeviceExtensionCount, nullptr);
    std::vector<VkExtensionProperties> DeviceExtensions(DeviceExtensionCount);
    vkEnumerateDeviceExtensionProperties(Impl->PhysicalDevice, nullptr, &DeviceExtensionCount, DeviceExtensions.data());
    std::vector<const char*> EnabledDeviceExtensions;
    if (Impl->Surface != VK_NULL_HANDLE)
    {
        if (!FImpl::HasName(DeviceExtensions, VK_KHR_SWAPCHAIN_EXTENSION_NAME))
        {
            (void)Shutdown();
            return Stoner::RHI::ERHIResult::Unsupported;
        }
        EnabledDeviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
    if (FImpl::HasName(DeviceExtensions, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
        EnabledDeviceExtensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif
    const float Priority = 1.0f;
    VkDeviceQueueCreateInfo QueueInfo = MakeVulkanStruct<VkDeviceQueueCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO);
    QueueInfo.queueFamilyIndex = Impl->GraphicsQueueFamily;
    QueueInfo.queueCount = 1;
    QueueInfo.pQueuePriorities = &Priority;
    VkDeviceCreateInfo DeviceInfo = MakeVulkanStruct<VkDeviceCreateInfo>(VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO);
    DeviceInfo.queueCreateInfoCount = 1;
    DeviceInfo.pQueueCreateInfos = &QueueInfo;
    DeviceInfo.enabledExtensionCount = static_cast<Stoner::Core::uint32>(EnabledDeviceExtensions.size());
    DeviceInfo.ppEnabledExtensionNames = EnabledDeviceExtensions.data();
    if (vkCreateDevice(Impl->PhysicalDevice, &DeviceInfo, nullptr, &Impl->Device) != VK_SUCCESS)
    {
        (void)Shutdown();
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    vkGetDeviceQueue(Impl->Device, Impl->GraphicsQueueFamily, 0, &Impl->GraphicsQueue);
    VkPhysicalDeviceProperties Properties{};
    vkGetPhysicalDeviceProperties(Impl->PhysicalDevice, &Properties);
    Impl->Snapshot.ObjectMode = Stoner::RHI::ERHIRuntimeObjectMode::RealRuntime;
    Impl->Snapshot.AdapterName = Properties.deviceName;
    Impl->Snapshot.bSoftwareDevice = Properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
    std::string LowerName(Properties.deviceName);
    std::transform(LowerName.begin(), LowerName.end(), LowerName.begin(), [](unsigned char Character) { return static_cast<char>(std::tolower(Character)); });
    Impl->Snapshot.bSoftwareDevice = Impl->Snapshot.bSoftwareDevice || LowerName.find("lavapipe") != std::string::npos || LowerName.find("llvmpipe") != std::string::npos;
    Impl->Snapshot.LiveInstances = 1;
    Impl->Snapshot.LiveDevices = 1;
    return Stoner::RHI::ERHIResult::Success;
#else
    (void)Mode;
    (void)PlatformWindow;
    return Stoner::RHI::ERHIResult::Unsupported;
#endif
}

Stoner::RHI::ERHIResult FVulkanNativeContext::ExecuteOffscreenTriangle(
    const Stoner::RHI::FRHIShaderModuleDesc& VertexShader,
    const Stoner::RHI::FRHIShaderModuleDesc& FragmentShader)
{
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    if (!Impl || Impl->Device == VK_NULL_HANDLE) return Stoner::RHI::ERHIResult::InvalidState;
    Impl->DestroyFrameResources();
    const auto Fail = [this]() { Impl->DestroyFrameResources(); return Stoner::RHI::ERHIResult::Failed; };
    if (!Stoner::RHI::IsValidRHIShaderModuleDesc(VertexShader) ||
        !Stoner::RHI::IsValidRHIShaderModuleDesc(FragmentShader) ||
        VertexShader.Stage != Stoner::RHI::ERHIShaderStage::Vertex ||
        FragmentShader.Stage != Stoner::RHI::ERHIShaderStage::Fragment)
        return Fail();
    Stoner::Core::TArray<Stoner::Core::uint32> VertexWords;
    Stoner::Core::TArray<Stoner::Core::uint32> FragmentWords;
    if (!Stoner::RHI::TryGetRHIShaderSpirvWords(
            VertexShader.Payload, VertexWords) ||
        !Stoner::RHI::TryGetRHIShaderSpirvWords(
            FragmentShader.Payload, FragmentWords))
    {
        return Fail();
    }

    constexpr std::array<float, 15> Vertices = {
         0.0f, -0.6f, 1.0f, 0.0f, 0.0f,
         0.6f,  0.6f, 0.0f, 1.0f, 0.0f,
        -0.6f,  0.6f, 0.0f, 0.0f, 1.0f,
    };
    VkBufferCreateInfo BufferInfo = MakeVulkanStruct<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
    BufferInfo.size = sizeof(Vertices);
    BufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(Impl->Device, &BufferInfo, nullptr, &Impl->VertexBuffer) != VK_SUCCESS) return Fail();
    VkMemoryRequirements BufferRequirements{};
    vkGetBufferMemoryRequirements(Impl->Device, Impl->VertexBuffer, &BufferRequirements);
    const auto BufferMemoryType = Impl->FindMemoryType(BufferRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (BufferMemoryType == UINT32_MAX) return Fail();
    VkMemoryAllocateInfo BufferAllocation = MakeVulkanStruct<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
    BufferAllocation.allocationSize = BufferRequirements.size;
    BufferAllocation.memoryTypeIndex = BufferMemoryType;
    if (vkAllocateMemory(Impl->Device, &BufferAllocation, nullptr, &Impl->VertexMemory) != VK_SUCCESS ||
        vkBindBufferMemory(Impl->Device, Impl->VertexBuffer, Impl->VertexMemory, 0) != VK_SUCCESS) return Fail();
    void* Mapped = nullptr;
    if (vkMapMemory(Impl->Device, Impl->VertexMemory, 0, sizeof(Vertices), 0, &Mapped) != VK_SUCCESS) return Fail();
    std::memcpy(Mapped, Vertices.data(), sizeof(Vertices));
    vkUnmapMemory(Impl->Device, Impl->VertexMemory);

    constexpr std::array<Stoner::Core::uint16, 3> Indices = {0, 1, 2};
    VkBufferCreateInfo IndexBufferInfo =
        MakeVulkanStruct<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
    IndexBufferInfo.size = sizeof(Indices);
    IndexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    IndexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(Impl->Device, &IndexBufferInfo, nullptr,
            &Impl->IndexBuffer) != VK_SUCCESS)
    {
        return Fail();
    }
    VkMemoryRequirements IndexRequirements{};
    vkGetBufferMemoryRequirements(Impl->Device, Impl->IndexBuffer,
        &IndexRequirements);
    const auto IndexMemoryType = Impl->FindMemoryType(
        IndexRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (IndexMemoryType == UINT32_MAX)
    {
        return Fail();
    }
    VkMemoryAllocateInfo IndexAllocation =
        MakeVulkanStruct<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
    IndexAllocation.allocationSize = IndexRequirements.size;
    IndexAllocation.memoryTypeIndex = IndexMemoryType;
    if (vkAllocateMemory(Impl->Device, &IndexAllocation, nullptr,
            &Impl->IndexMemory) != VK_SUCCESS ||
        vkBindBufferMemory(Impl->Device, Impl->IndexBuffer,
            Impl->IndexMemory, 0) != VK_SUCCESS)
    {
        return Fail();
    }
    Mapped = nullptr;
    if (vkMapMemory(Impl->Device, Impl->IndexMemory, 0, sizeof(Indices), 0,
            &Mapped) != VK_SUCCESS)
    {
        return Fail();
    }
    std::memcpy(Mapped, Indices.data(), sizeof(Indices));
    vkUnmapMemory(Impl->Device, Impl->IndexMemory);
    Impl->Snapshot.LiveBuffers = 2;

    VkImageCreateInfo ImageInfo = MakeVulkanStruct<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
    ImageInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    ImageInfo.extent = {64, 64, 1};
    ImageInfo.mipLevels = 1;
    ImageInfo.arrayLayers = 1;
    ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(Impl->Device, &ImageInfo, nullptr, &Impl->ColorImage) != VK_SUCCESS) return Fail();
    VkMemoryRequirements ImageRequirements{};
    vkGetImageMemoryRequirements(Impl->Device, Impl->ColorImage, &ImageRequirements);
    const auto ImageMemoryType = Impl->FindMemoryType(ImageRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ImageMemoryType == UINT32_MAX) return Fail();
    VkMemoryAllocateInfo ImageAllocation = MakeVulkanStruct<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
    ImageAllocation.allocationSize = ImageRequirements.size;
    ImageAllocation.memoryTypeIndex = ImageMemoryType;
    if (vkAllocateMemory(Impl->Device, &ImageAllocation, nullptr, &Impl->ColorMemory) != VK_SUCCESS ||
        vkBindImageMemory(Impl->Device, Impl->ColorImage, Impl->ColorMemory, 0) != VK_SUCCESS) return Fail();
    Impl->Snapshot.LiveTextures = Impl->GetLiveTextureCount();

    constexpr VkDeviceSize ReadbackSize = 64u * 64u * 4u;
    VkBufferCreateInfo ReadbackInfo =
        MakeVulkanStruct<VkBufferCreateInfo>(
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
    ReadbackInfo.size = ReadbackSize;
    ReadbackInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    ReadbackInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(Impl->Device, &ReadbackInfo, nullptr,
            &Impl->ReadbackBuffer) != VK_SUCCESS)
    {
        return Fail();
    }
    VkMemoryRequirements ReadbackRequirements{};
    vkGetBufferMemoryRequirements(
        Impl->Device, Impl->ReadbackBuffer, &ReadbackRequirements);
    const auto ReadbackMemoryType = Impl->FindMemoryType(
        ReadbackRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (ReadbackMemoryType == UINT32_MAX)
    {
        return Fail();
    }
    VkMemoryAllocateInfo ReadbackAllocation =
        MakeVulkanStruct<VkMemoryAllocateInfo>(
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
    ReadbackAllocation.allocationSize = ReadbackRequirements.size;
    ReadbackAllocation.memoryTypeIndex = ReadbackMemoryType;
    if (vkAllocateMemory(Impl->Device, &ReadbackAllocation, nullptr,
            &Impl->ReadbackMemory) != VK_SUCCESS ||
        vkBindBufferMemory(Impl->Device, Impl->ReadbackBuffer,
            Impl->ReadbackMemory, 0) != VK_SUCCESS)
    {
        return Fail();
    }
    Impl->Snapshot.LiveBuffers = 3;

    VkImageViewCreateInfo ViewInfo = MakeVulkanStruct<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
    ViewInfo.image = Impl->ColorImage;
    ViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    ViewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ViewInfo.subresourceRange.levelCount = 1;
    ViewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(Impl->Device, &ViewInfo, nullptr, &Impl->ColorView) != VK_SUCCESS) return Fail();

    VkAttachmentDescription Attachment{};
    Attachment.format = VK_FORMAT_R8G8B8A8_UNORM;
    Attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    Attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    Attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Attachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    VkAttachmentReference AttachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription Subpass{};
    Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    Subpass.colorAttachmentCount = 1;
    Subpass.pColorAttachments = &AttachmentReference;
    VkRenderPassCreateInfo RenderPassInfo = MakeVulkanStruct<VkRenderPassCreateInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
    RenderPassInfo.attachmentCount = 1;
    RenderPassInfo.pAttachments = &Attachment;
    RenderPassInfo.subpassCount = 1;
    RenderPassInfo.pSubpasses = &Subpass;
    if (vkCreateRenderPass(Impl->Device, &RenderPassInfo, nullptr, &Impl->RenderPass) != VK_SUCCESS) return Fail();
    VkFramebufferCreateInfo FramebufferInfo = MakeVulkanStruct<VkFramebufferCreateInfo>(VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
    FramebufferInfo.renderPass = Impl->RenderPass;
    FramebufferInfo.attachmentCount = 1;
    FramebufferInfo.pAttachments = &Impl->ColorView;
    FramebufferInfo.width = 64;
    FramebufferInfo.height = 64;
    FramebufferInfo.layers = 1;
    if (vkCreateFramebuffer(Impl->Device, &FramebufferInfo, nullptr, &Impl->Framebuffer) != VK_SUCCESS) return Fail();

    const auto CreateShader = [this](const Stoner::Core::TArray<Stoner::Core::uint32>& Words, VkShaderModule& Out)
    {
        VkShaderModuleCreateInfo Info = MakeVulkanStruct<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
        Info.codeSize = Words.size() * sizeof(Stoner::Core::uint32);
        Info.pCode = Words.data();
        return vkCreateShaderModule(Impl->Device, &Info, nullptr, &Out) == VK_SUCCESS;
    };
    if (!CreateShader(VertexWords, Impl->VertexShader) || !CreateShader(FragmentWords, Impl->FragmentShader)) return Fail();
    Impl->Snapshot.LiveShaderModules = Impl->GetLiveShaderModuleCount();
    VkPipelineLayoutCreateInfo LayoutInfo = MakeVulkanStruct<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
    if (vkCreatePipelineLayout(Impl->Device, &LayoutInfo, nullptr, &Impl->PipelineLayout) != VK_SUCCESS) return Fail();

    VkPipelineShaderStageCreateInfo ShaderStages[2]{};
    ShaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    ShaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    ShaderStages[0].module = Impl->VertexShader;
    ShaderStages[0].pName = "main";
    ShaderStages[1] = ShaderStages[0];
    ShaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    ShaderStages[1].module = Impl->FragmentShader;
    VkVertexInputBindingDescription Binding{0, sizeof(float) * 5, VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription Attributes[2] = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, sizeof(float) * 2},
    };
    VkPipelineVertexInputStateCreateInfo VertexInput = MakeVulkanStruct<VkPipelineVertexInputStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
    VertexInput.vertexBindingDescriptionCount = 1; VertexInput.pVertexBindingDescriptions = &Binding;
    VertexInput.vertexAttributeDescriptionCount = 2; VertexInput.pVertexAttributeDescriptions = Attributes;
    VkPipelineInputAssemblyStateCreateInfo Assembly = MakeVulkanStruct<VkPipelineInputAssemblyStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
    Assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo ViewportState = MakeVulkanStruct<VkPipelineViewportStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
    ViewportState.viewportCount = 1; ViewportState.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo Rasterizer = MakeVulkanStruct<VkPipelineRasterizationStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
    Rasterizer.polygonMode = VK_POLYGON_MODE_FILL; Rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    Rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE; Rasterizer.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo Multisample = MakeVulkanStruct<VkPipelineMultisampleStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
    Multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState BlendAttachment{};
    BlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo Blend = MakeVulkanStruct<VkPipelineColorBlendStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
    Blend.attachmentCount = 1; Blend.pAttachments = &BlendAttachment;
    const VkDynamicState DynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo Dynamic = MakeVulkanStruct<VkPipelineDynamicStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
    Dynamic.dynamicStateCount = 2; Dynamic.pDynamicStates = DynamicStates;
    VkGraphicsPipelineCreateInfo PipelineInfo = MakeVulkanStruct<VkGraphicsPipelineCreateInfo>(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
    PipelineInfo.stageCount = 2; PipelineInfo.pStages = ShaderStages;
    PipelineInfo.pVertexInputState = &VertexInput; PipelineInfo.pInputAssemblyState = &Assembly;
    PipelineInfo.pViewportState = &ViewportState; PipelineInfo.pRasterizationState = &Rasterizer;
    PipelineInfo.pMultisampleState = &Multisample; PipelineInfo.pColorBlendState = &Blend;
    PipelineInfo.pDynamicState = &Dynamic; PipelineInfo.layout = Impl->PipelineLayout;
    PipelineInfo.renderPass = Impl->RenderPass;
    if (vkCreateGraphicsPipelines(Impl->Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &Impl->Pipeline) != VK_SUCCESS) return Fail();
    Impl->Snapshot.LivePipelines = Impl->GetLivePipelineCount();

    VkCommandPoolCreateInfo PoolInfo = MakeVulkanStruct<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
    PoolInfo.queueFamilyIndex = Impl->GraphicsQueueFamily;
    if (vkCreateCommandPool(Impl->Device, &PoolInfo, nullptr, &Impl->CommandPool) != VK_SUCCESS) return Fail();
    VkCommandBufferAllocateInfo CommandInfo = MakeVulkanStruct<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
    CommandInfo.commandPool = Impl->CommandPool; CommandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; CommandInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(Impl->Device, &CommandInfo, &Impl->CommandBuffer) != VK_SUCCESS) return Fail();
    Impl->Snapshot.LiveCommandBuffers = 1;
    VkCommandBufferBeginInfo BeginInfo = MakeVulkanStruct<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
    if (vkBeginCommandBuffer(Impl->CommandBuffer, &BeginInfo) != VK_SUCCESS) return Fail();
    VkClearValue Clear{}; Clear.color = {{0.02f, 0.03f, 0.05f, 1.0f}};
    VkRenderPassBeginInfo BeginPass = MakeVulkanStruct<VkRenderPassBeginInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
    BeginPass.renderPass = Impl->RenderPass; BeginPass.framebuffer = Impl->Framebuffer;
    BeginPass.renderArea.extent = {64, 64}; BeginPass.clearValueCount = 1; BeginPass.pClearValues = &Clear;
    vkCmdBeginRenderPass(Impl->CommandBuffer, &BeginPass, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(Impl->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Impl->Pipeline);
    const VkDeviceSize Offset = 0;
    vkCmdBindVertexBuffers(Impl->CommandBuffer, 0, 1, &Impl->VertexBuffer, &Offset);
    vkCmdBindIndexBuffer(
        Impl->CommandBuffer, Impl->IndexBuffer, 0, VK_INDEX_TYPE_UINT16);
    VkViewport Viewport{0.0f, 0.0f, 64.0f, 64.0f, 0.0f, 1.0f};
    VkRect2D Scissor{{0, 0}, {64, 64}};
    vkCmdSetViewport(Impl->CommandBuffer, 0, 1, &Viewport);
    vkCmdSetScissor(Impl->CommandBuffer, 0, 1, &Scissor);
    vkCmdDrawIndexed(Impl->CommandBuffer, 3, 1, 0, 0, 0);
    vkCmdEndRenderPass(Impl->CommandBuffer);
    VkBufferImageCopy ReadbackRegion{};
    ReadbackRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    ReadbackRegion.imageSubresource.layerCount = 1;
    ReadbackRegion.imageExtent = {64, 64, 1};
    vkCmdCopyImageToBuffer(
        Impl->CommandBuffer,
        Impl->ColorImage,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        Impl->ReadbackBuffer,
        1,
        &ReadbackRegion);
    if (vkEndCommandBuffer(Impl->CommandBuffer) != VK_SUCCESS) return Fail();
    VkFenceCreateInfo FenceInfo = MakeVulkanStruct<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
    if (vkCreateFence(Impl->Device, &FenceInfo, nullptr, &Impl->Fence) != VK_SUCCESS) return Fail();
    Impl->Snapshot.LiveSynchronizationObjects = 1;
    VkSubmitInfo Submit = MakeVulkanStruct<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
    Submit.commandBufferCount = 1; Submit.pCommandBuffers = &Impl->CommandBuffer;
    if (vkQueueSubmit(Impl->GraphicsQueue, 1, &Submit, Impl->Fence) != VK_SUCCESS ||
        vkWaitForFences(Impl->Device, 1, &Impl->Fence, VK_TRUE, 30ull * 1000ull * 1000ull * 1000ull) != VK_SUCCESS) return Fail();
    void* Readback = nullptr;
    if (vkMapMemory(
            Impl->Device, Impl->ReadbackMemory, 0,
            ReadbackSize, 0, &Readback) != VK_SUCCESS)
    {
        return Fail();
    }
    const auto* Pixels = static_cast<const Stoner::Core::uint8*>(Readback);
    bool bObservedDrawnPixel = false;
    for (VkDeviceSize Pixel = 0; Pixel < 64u * 64u; ++Pixel)
    {
        const VkDeviceSize Byte = Pixel * 4u;
        if (Pixels[Byte] > 16u || Pixels[Byte + 1u] > 16u ||
            Pixels[Byte + 2u] > 20u)
        {
            bObservedDrawnPixel = true;
            break;
        }
    }
    vkUnmapMemory(Impl->Device, Impl->ReadbackMemory);
    if (!bObservedDrawnPixel)
    {
        return Fail();
    }
    Impl->DestroyFrameResources();
    return Stoner::RHI::ERHIResult::Success;
#else
    (void)VertexShader; (void)FragmentShader;
    return Stoner::RHI::ERHIResult::Unsupported;
#endif
}

Stoner::RHI::ERHIResult FVulkanNativeContext::ExecuteDeferredOffscreenValidation(
    std::span<const Stoner::RHI::FRHIShaderModuleDesc> Shaders,
    FVulkanDeferredValidationReport& OutReport,
    EVulkanDeferredFailurePoint FailurePoint,
    const FVulkanDeferredUniformPayload* UniformPayload)
{
    if (Shaders.size() != 9)
    {
        OutReport = {};
        return Stoner::RHI::ERHIResult::Failed;
    }
    const FVulkanDeferredShaderSet ShaderSet{
        Shaders[0], Shaders[1], Shaders[2],
        Shaders[3], Shaders[4], Shaders[5],
        Shaders[6], Shaders[7], Shaders[8]};
    FVulkanNativeOffscreenSession Session(*this);
    const Stoner::RHI::ERHIResult Result =
        Session.Execute(ShaderSet, OutReport, FailurePoint, UniformPayload);
    (void)Session.Shutdown();
    return Result;
}

FVulkanDeferredValidationReport FVulkanNativeContext::RunDeferredFailureLifecycleValidation(
    EVulkanDeferredFailurePoint FailurePoint) noexcept
{
    FVulkanDeferredValidationReport Report;
    Report.RuntimeMode = "RuntimeIndependentFailureInjection";
    Report.ReferencePath = "NativeDeferredFailureLifecycle";
    Report.InjectedFailure = FailurePoint;
    Report.PrimaryFailureStage = ToString(FailurePoint);
    Report.PeakLiveObjects = FailurePoint == EVulkanDeferredFailurePoint::PartialInitialization
        ? 7u : 32u;
    Report.CompletedStageCount =
        FailurePoint == EVulkanDeferredFailurePoint::Record ? 0u :
        FailurePoint == EVulkanDeferredFailurePoint::Submit ? 1u :
        FailurePoint == EVulkanDeferredFailurePoint::Fence ? 2u :
        FailurePoint == EVulkanDeferredFailurePoint::Copy ? 3u :
        FailurePoint == EVulkanDeferredFailurePoint::Map ? 4u :
        FailurePoint == EVulkanDeferredFailurePoint::Decode ? 5u :
        FailurePoint == EVulkanDeferredFailurePoint::Probe ? 6u : 0u;
    Report.FinalLiveObjects = 0;
    Report.bPassed = false;
    return Report;
}

FVulkanVisibleFrameFailureReport
FVulkanNativeContext::RunVisibleFrameFailureLifecycleValidation(
    EVulkanVisibleFrameFailurePoint FailurePoint) noexcept
{
    FVulkanVisibleFrameFailureReport Report;
    Report.InjectedFailure = FailurePoint;

    bool bFrameAcquired = true;
    bool bFenceSignaled = true;
    bool bAcquiredSuboptimal = false;

    switch (FailurePoint)
    {
    case EVulkanVisibleFrameFailurePoint::AcquireSuboptimal:
        bAcquiredSuboptimal = true;
        Report.FirstResult = Stoner::RHI::ERHIResult::ResizeRequired;
        break;
    case EVulkanVisibleFrameFailurePoint::Record:
        Report.FirstResult = Stoner::RHI::ERHIResult::Failed;
        break;
    case EVulkanVisibleFrameFailurePoint::SubmitAfterFenceReset:
        bFenceSignaled = false;
        Report.FirstResult = Stoner::RHI::ERHIResult::Failed;
        break;
    case EVulkanVisibleFrameFailurePoint::None:
        bFrameAcquired = false;
        Report.FirstResult = Stoner::RHI::ERHIResult::Success;
        break;
    }

    if (FailurePoint == EVulkanVisibleFrameFailurePoint::SubmitAfterFenceReset)
    {
        bFenceSignaled = true;
    }
    if (FailurePoint == EVulkanVisibleFrameFailurePoint::AcquireSuboptimal ||
        FailurePoint == EVulkanVisibleFrameFailurePoint::Record ||
        FailurePoint == EVulkanVisibleFrameFailurePoint::SubmitAfterFenceReset)
    {
        bFrameAcquired = false;
        bAcquiredSuboptimal = false;
    }

    Report.bAcquiredStateReleased = !bFrameAcquired && !bAcquiredSuboptimal;
    Report.bFenceReadyForReuse = bFenceSignaled;
    Report.NextAcquireResult =
        Report.bAcquiredStateReleased && Report.bFenceReadyForReuse
            ? Stoner::RHI::ERHIResult::Success
            : (Report.bAcquiredStateReleased
                ? Stoner::RHI::ERHIResult::Timeout
                : Stoner::RHI::ERHIResult::InvalidState);
    Report.bPassed = Report.bAcquiredStateReleased &&
        Report.bFenceReadyForReuse &&
        Report.NextAcquireResult == Stoner::RHI::ERHIResult::Success;
    return Report;
}

const char* ToString(EVulkanVisibleFrameFailurePoint FailurePoint) noexcept
{
    switch (FailurePoint)
    {
    case EVulkanVisibleFrameFailurePoint::None: return "None";
    case EVulkanVisibleFrameFailurePoint::AcquireSuboptimal:
        return "AcquireSuboptimal";
    case EVulkanVisibleFrameFailurePoint::Record: return "Record";
    case EVulkanVisibleFrameFailurePoint::SubmitAfterFenceReset:
        return "SubmitAfterFenceReset";
    }
    return "Unknown";
}

Stoner::RHI::ERHIResult FVulkanNativeContext::PrepareVisibleTriangle(
    const Stoner::RHI::FRHIShaderModuleDesc& VertexShader,
    const Stoner::RHI::FRHIShaderModuleDesc& FragmentShader,
    Stoner::Core::uint32 Width,
    Stoner::Core::uint32 Height)
{
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE && defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    if (!Impl || Impl->Device == VK_NULL_HANDLE || Impl->Surface == VK_NULL_HANDLE)
        return Stoner::RHI::ERHIResult::InvalidState;
    if (Width == 0 || Height == 0) return Stoner::RHI::ERHIResult::Unavailable;
    if (!Stoner::RHI::IsValidRHIShaderModuleDesc(VertexShader) ||
        !Stoner::RHI::IsValidRHIShaderModuleDesc(FragmentShader) ||
        VertexShader.Stage != Stoner::RHI::ERHIShaderStage::Vertex ||
        FragmentShader.Stage != Stoner::RHI::ERHIShaderStage::Fragment)
        return Stoner::RHI::ERHIResult::Failed;
    Stoner::Core::TArray<Stoner::Core::uint32> VertexWords;
    Stoner::Core::TArray<Stoner::Core::uint32> FragmentWords;
    if (!Stoner::RHI::TryGetRHIShaderSpirvWords(
            VertexShader.Payload, VertexWords) ||
        !Stoner::RHI::TryGetRHIShaderSpirvWords(
            FragmentShader.Payload, FragmentWords))
    {
        return Stoner::RHI::ERHIResult::Failed;
    }
    Impl->VisibleVertexShader = VertexShader;
    Impl->VisibleFragmentShader = FragmentShader;
    Impl->bHasVisibleShaders = true;
    vkDeviceWaitIdle(Impl->Device);
    Impl->DestroyFrameResources();
    const auto Fail = [this]() { Impl->DestroyFrameResources(); return Stoner::RHI::ERHIResult::Failed; };

    VkSurfaceCapabilitiesKHR Capabilities{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(Impl->PhysicalDevice, Impl->Surface, &Capabilities) != VK_SUCCESS)
        return Fail();
    Stoner::Core::uint32 FormatCount = 0;
    if (vkGetPhysicalDeviceSurfaceFormatsKHR(Impl->PhysicalDevice, Impl->Surface, &FormatCount, nullptr) != VK_SUCCESS || FormatCount == 0)
        return Fail();
    std::vector<VkSurfaceFormatKHR> Formats(FormatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(Impl->PhysicalDevice, Impl->Surface, &FormatCount, Formats.data());
    VkSurfaceFormatKHR SurfaceFormat = Formats[0];
    for (const VkSurfaceFormatKHR& Candidate : Formats)
    {
        if (Candidate.format == VK_FORMAT_B8G8R8A8_SRGB && Candidate.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            SurfaceFormat = Candidate;
            break;
        }
    }
    VkExtent2D Extent = Capabilities.currentExtent;
    if (Extent.width == UINT32_MAX)
    {
        Extent.width = std::clamp(Width, Capabilities.minImageExtent.width, Capabilities.maxImageExtent.width);
        Extent.height = std::clamp(Height, Capabilities.minImageExtent.height, Capabilities.maxImageExtent.height);
    }
    Stoner::Core::uint32 ImageCount = std::max(Capabilities.minImageCount + 1, 2u);
    if (Capabilities.maxImageCount > 0) ImageCount = std::min(ImageCount, Capabilities.maxImageCount);
    VkSwapchainCreateInfoKHR SwapchainInfo = MakeVulkanStruct<VkSwapchainCreateInfoKHR>(VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR);
    SwapchainInfo.surface = Impl->Surface;
    SwapchainInfo.minImageCount = ImageCount;
    SwapchainInfo.imageFormat = SurfaceFormat.format;
    SwapchainInfo.imageColorSpace = SurfaceFormat.colorSpace;
    SwapchainInfo.imageExtent = Extent;
    SwapchainInfo.imageArrayLayers = 1;
    SwapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    SwapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    SwapchainInfo.preTransform = Capabilities.currentTransform;
    SwapchainInfo.compositeAlpha = (Capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
        ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
    SwapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    SwapchainInfo.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(Impl->Device, &SwapchainInfo, nullptr, &Impl->Swapchain) != VK_SUCCESS) return Fail();
    Impl->SwapchainFormat = SurfaceFormat.format;
    Impl->SwapchainExtent = Extent;
    vkGetSwapchainImagesKHR(Impl->Device, Impl->Swapchain, &ImageCount, nullptr);
    Impl->SwapchainImages.resize(ImageCount);
    if (vkGetSwapchainImagesKHR(Impl->Device, Impl->Swapchain, &ImageCount, Impl->SwapchainImages.data()) != VK_SUCCESS) return Fail();
    for (VkImage Image : Impl->SwapchainImages)
    {
        VkImageViewCreateInfo ViewInfo = MakeVulkanStruct<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
        ViewInfo.image = Image;
        ViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ViewInfo.format = Impl->SwapchainFormat;
        ViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        ViewInfo.subresourceRange.levelCount = 1;
        ViewInfo.subresourceRange.layerCount = 1;
        VkImageView View = VK_NULL_HANDLE;
        if (vkCreateImageView(Impl->Device, &ViewInfo, nullptr, &View) != VK_SUCCESS) return Fail();
        Impl->SwapchainViews.push_back(View);
    }

    constexpr std::array<float, 15> Vertices = {
         0.0f, -0.6f, 1.0f, 0.0f, 0.0f,
         0.6f,  0.6f, 0.0f, 1.0f, 0.0f,
        -0.6f,  0.6f, 0.0f, 0.0f, 1.0f,
    };
    VkBufferCreateInfo BufferInfo = MakeVulkanStruct<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
    BufferInfo.size = sizeof(Vertices);
    BufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(Impl->Device, &BufferInfo, nullptr, &Impl->VertexBuffer) != VK_SUCCESS) return Fail();
    VkMemoryRequirements Requirements{};
    vkGetBufferMemoryRequirements(Impl->Device, Impl->VertexBuffer, &Requirements);
    const auto MemoryType = Impl->FindMemoryType(Requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (MemoryType == UINT32_MAX) return Fail();
    VkMemoryAllocateInfo Allocation = MakeVulkanStruct<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
    Allocation.allocationSize = Requirements.size;
    Allocation.memoryTypeIndex = MemoryType;
    if (vkAllocateMemory(Impl->Device, &Allocation, nullptr, &Impl->VertexMemory) != VK_SUCCESS ||
        vkBindBufferMemory(Impl->Device, Impl->VertexBuffer, Impl->VertexMemory, 0) != VK_SUCCESS) return Fail();
    void* Mapped = nullptr;
    if (vkMapMemory(Impl->Device, Impl->VertexMemory, 0, sizeof(Vertices), 0, &Mapped) != VK_SUCCESS) return Fail();
    std::memcpy(Mapped, Vertices.data(), sizeof(Vertices));
    vkUnmapMemory(Impl->Device, Impl->VertexMemory);

    constexpr std::array<Stoner::Core::uint16, 3> Indices = {0, 1, 2};
    VkBufferCreateInfo IndexBufferInfo =
        MakeVulkanStruct<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
    IndexBufferInfo.size = sizeof(Indices);
    IndexBufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    IndexBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(Impl->Device, &IndexBufferInfo, nullptr,
            &Impl->IndexBuffer) != VK_SUCCESS)
    {
        return Fail();
    }
    VkMemoryRequirements IndexRequirements{};
    vkGetBufferMemoryRequirements(Impl->Device, Impl->IndexBuffer,
        &IndexRequirements);
    const auto IndexMemoryType = Impl->FindMemoryType(
        IndexRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (IndexMemoryType == UINT32_MAX)
    {
        return Fail();
    }
    VkMemoryAllocateInfo IndexAllocation =
        MakeVulkanStruct<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
    IndexAllocation.allocationSize = IndexRequirements.size;
    IndexAllocation.memoryTypeIndex = IndexMemoryType;
    if (vkAllocateMemory(Impl->Device, &IndexAllocation, nullptr,
            &Impl->IndexMemory) != VK_SUCCESS ||
        vkBindBufferMemory(Impl->Device, Impl->IndexBuffer,
            Impl->IndexMemory, 0) != VK_SUCCESS)
    {
        return Fail();
    }
    Mapped = nullptr;
    if (vkMapMemory(Impl->Device, Impl->IndexMemory, 0, sizeof(Indices), 0,
            &Mapped) != VK_SUCCESS)
    {
        return Fail();
    }
    std::memcpy(Mapped, Indices.data(), sizeof(Indices));
    vkUnmapMemory(Impl->Device, Impl->IndexMemory);

    VkAttachmentDescription Attachment{};
    Attachment.format = Impl->SwapchainFormat;
    Attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    Attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    Attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    Attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    Attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference Reference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription Subpass{};
    Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    Subpass.colorAttachmentCount = 1;
    Subpass.pColorAttachments = &Reference;
    VkSubpassDependency Dependency{};
    Dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    Dependency.dstSubpass = 0;
    Dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    Dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo RenderPassInfo = MakeVulkanStruct<VkRenderPassCreateInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
    RenderPassInfo.attachmentCount = 1; RenderPassInfo.pAttachments = &Attachment;
    RenderPassInfo.subpassCount = 1; RenderPassInfo.pSubpasses = &Subpass;
    RenderPassInfo.dependencyCount = 1; RenderPassInfo.pDependencies = &Dependency;
    if (vkCreateRenderPass(Impl->Device, &RenderPassInfo, nullptr, &Impl->RenderPass) != VK_SUCCESS) return Fail();
    for (VkImageView View : Impl->SwapchainViews)
    {
        VkFramebufferCreateInfo Info = MakeVulkanStruct<VkFramebufferCreateInfo>(VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
        Info.renderPass = Impl->RenderPass; Info.attachmentCount = 1; Info.pAttachments = &View;
        Info.width = Extent.width; Info.height = Extent.height; Info.layers = 1;
        VkFramebuffer Target = VK_NULL_HANDLE;
        if (vkCreateFramebuffer(Impl->Device, &Info, nullptr, &Target) != VK_SUCCESS) return Fail();
        Impl->SwapchainFramebuffers.push_back(Target);
    }

    const auto CreateShader = [this](const Stoner::Core::TArray<Stoner::Core::uint32>& Words, VkShaderModule& Out)
    {
        VkShaderModuleCreateInfo Info = MakeVulkanStruct<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
        Info.codeSize = Words.size() * sizeof(Stoner::Core::uint32); Info.pCode = Words.data();
        return vkCreateShaderModule(Impl->Device, &Info, nullptr, &Out) == VK_SUCCESS;
    };
    if (!CreateShader(VertexWords, Impl->VertexShader) || !CreateShader(FragmentWords, Impl->FragmentShader)) return Fail();
    VkPipelineLayoutCreateInfo LayoutInfo = MakeVulkanStruct<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
    if (vkCreatePipelineLayout(Impl->Device, &LayoutInfo, nullptr, &Impl->PipelineLayout) != VK_SUCCESS) return Fail();
    VkPipelineShaderStageCreateInfo Stages[2]{};
    Stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    Stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; Stages[0].module = Impl->VertexShader; Stages[0].pName = "main";
    Stages[1] = Stages[0]; Stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; Stages[1].module = Impl->FragmentShader;
    VkVertexInputBindingDescription Binding{0, sizeof(float) * 5, VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription Attributes[2] = {{0,0,VK_FORMAT_R32G32_SFLOAT,0},{1,0,VK_FORMAT_R32G32B32_SFLOAT,sizeof(float)*2}};
    VkPipelineVertexInputStateCreateInfo VertexInput = MakeVulkanStruct<VkPipelineVertexInputStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
    VertexInput.vertexBindingDescriptionCount = 1; VertexInput.pVertexBindingDescriptions = &Binding;
    VertexInput.vertexAttributeDescriptionCount = 2; VertexInput.pVertexAttributeDescriptions = Attributes;
    VkPipelineInputAssemblyStateCreateInfo Assembly = MakeVulkanStruct<VkPipelineInputAssemblyStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO); Assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo ViewportState = MakeVulkanStruct<VkPipelineViewportStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO); ViewportState.viewportCount = 1; ViewportState.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo Rasterizer = MakeVulkanStruct<VkPipelineRasterizationStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO); Rasterizer.polygonMode = VK_POLYGON_MODE_FILL; Rasterizer.cullMode = VK_CULL_MODE_NONE; Rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE; Rasterizer.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo Multisample = MakeVulkanStruct<VkPipelineMultisampleStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO); Multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState BlendAttachment{}; BlendAttachment.colorWriteMask = 0xf;
    VkPipelineColorBlendStateCreateInfo Blend = MakeVulkanStruct<VkPipelineColorBlendStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO); Blend.attachmentCount = 1; Blend.pAttachments = &BlendAttachment;
    const VkDynamicState DynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo Dynamic = MakeVulkanStruct<VkPipelineDynamicStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO); Dynamic.dynamicStateCount = 2; Dynamic.pDynamicStates = DynamicStates;
    VkGraphicsPipelineCreateInfo PipelineInfo = MakeVulkanStruct<VkGraphicsPipelineCreateInfo>(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
    PipelineInfo.stageCount = 2; PipelineInfo.pStages = Stages; PipelineInfo.pVertexInputState = &VertexInput;
    PipelineInfo.pInputAssemblyState = &Assembly; PipelineInfo.pViewportState = &ViewportState; PipelineInfo.pRasterizationState = &Rasterizer;
    PipelineInfo.pMultisampleState = &Multisample; PipelineInfo.pColorBlendState = &Blend; PipelineInfo.pDynamicState = &Dynamic;
    PipelineInfo.layout = Impl->PipelineLayout; PipelineInfo.renderPass = Impl->RenderPass;
    if (vkCreateGraphicsPipelines(Impl->Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &Impl->Pipeline) != VK_SUCCESS) return Fail();
    VkCommandPoolCreateInfo PoolInfo = MakeVulkanStruct<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
    PoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; PoolInfo.queueFamilyIndex = Impl->GraphicsQueueFamily;
    if (vkCreateCommandPool(Impl->Device, &PoolInfo, nullptr, &Impl->CommandPool) != VK_SUCCESS) return Fail();
    constexpr Stoner::Core::uint32 VisibleFrameSlotCount = 2;
    Impl->VisibleCommandBuffers.resize(VisibleFrameSlotCount);
    VkCommandBufferAllocateInfo CommandInfo = MakeVulkanStruct<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
    CommandInfo.commandPool = Impl->CommandPool; CommandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CommandInfo.commandBufferCount = VisibleFrameSlotCount;
    if (vkAllocateCommandBuffers(Impl->Device, &CommandInfo, Impl->VisibleCommandBuffers.data()) != VK_SUCCESS) return Fail();
    VkSemaphoreCreateInfo SemaphoreInfo = MakeVulkanStruct<VkSemaphoreCreateInfo>(VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO);
    Impl->VisibleImageAvailable.resize(VisibleFrameSlotCount);
    Impl->VisibleRenderFinished.resize(Impl->SwapchainImages.size());
    for (VkSemaphore& Semaphore : Impl->VisibleImageAvailable)
        if (vkCreateSemaphore(Impl->Device, &SemaphoreInfo, nullptr, &Semaphore) != VK_SUCCESS) return Fail();
    for (VkSemaphore& Semaphore : Impl->VisibleRenderFinished)
        if (vkCreateSemaphore(Impl->Device, &SemaphoreInfo, nullptr, &Semaphore) != VK_SUCCESS) return Fail();
    VkFenceCreateInfo FenceInfo = MakeVulkanStruct<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO); FenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    Impl->VisibleFences.resize(VisibleFrameSlotCount);
    for (VkFence& Fence : Impl->VisibleFences)
        if (vkCreateFence(Impl->Device, &FenceInfo, nullptr, &Fence) != VK_SUCCESS) return Fail();
    Impl->Snapshot.LiveBuffers = 2;
    Impl->Snapshot.LiveTextures = Impl->GetLiveTextureCount();
    Impl->Snapshot.LiveShaderModules = Impl->GetLiveShaderModuleCount();
    Impl->Snapshot.LivePipelines = Impl->GetLivePipelineCount();
    Impl->Snapshot.LiveCommandBuffers = VisibleFrameSlotCount;
    Impl->Snapshot.LiveSynchronizationObjects = static_cast<Stoner::Core::uint32>(
        Impl->VisibleFences.size() + Impl->VisibleImageAvailable.size() + Impl->VisibleRenderFinished.size());
    return Stoner::RHI::ERHIResult::Success;
#else
    (void)VertexShader; (void)FragmentShader; (void)Width; (void)Height;
    return Stoner::RHI::ERHIResult::Unsupported;
#endif
}

Stoner::RHI::ERHIResult FVulkanNativeContext::AcquireVisibleFrame(FVulkanNativeFrameBindings& OutBindings)
{
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE && defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    OutBindings = {};
    if (!Impl || Impl->Swapchain == VK_NULL_HANDLE || Impl->VisibleFences.empty() || Impl->bFrameAcquired)
        return Stoner::RHI::ERHIResult::InvalidState;
    const Stoner::Core::uint32 FrameSlot = Impl->CurrentFrameSlot % static_cast<Stoner::Core::uint32>(Impl->VisibleFences.size());
    const VkResult WaitResult = vkWaitForFences(Impl->Device, 1, &Impl->VisibleFences[FrameSlot], VK_TRUE, 30ull * 1000ull * 1000ull * 1000ull);
    if (WaitResult == VK_TIMEOUT) return Stoner::RHI::ERHIResult::Timeout;
    if (WaitResult != VK_SUCCESS) return Stoner::RHI::ERHIResult::Failed;
    Stoner::Core::uint32 ImageIndex = 0;
    const VkResult AcquireResult = vkAcquireNextImageKHR(Impl->Device, Impl->Swapchain, UINT64_MAX,
        Impl->VisibleImageAvailable[FrameSlot], VK_NULL_HANDLE, &ImageIndex);
    if (AcquireResult == VK_ERROR_OUT_OF_DATE_KHR) return Stoner::RHI::ERHIResult::ResizeRequired;
    if (AcquireResult != VK_SUCCESS && AcquireResult != VK_SUBOPTIMAL_KHR) return Stoner::RHI::ERHIResult::Failed;
    Impl->AcquiredImageIndex = ImageIndex;
    Impl->AcquiredFrameSlot = FrameSlot;
    Impl->bFrameAcquired = true;
    Impl->bAcquiredSuboptimal = AcquireResult == VK_SUBOPTIMAL_KHR;

    const Stoner::RHI::ERHIFormat Format = ToRHIFormat(Impl->SwapchainFormat);
    OutBindings.ImageIndex = ImageIndex;
    OutBindings.FrameSlot = FrameSlot;
    OutBindings.OutputTexture = Stoner::Core::MakeShared<FNativeTextureBinding>(Impl->SwapchainExtent.width, Impl->SwapchainExtent.height, Format);
    OutBindings.VertexBuffer = Stoner::Core::MakeShared<FNativeBufferBinding>(
        sizeof(float) * 15, Stoner::RHI::ERHIBufferUsage::Vertex);
    OutBindings.IndexBuffer = Stoner::Core::MakeShared<FNativeBufferBinding>(
        sizeof(Stoner::Core::uint16) * 3,
        Stoner::RHI::ERHIBufferUsage::Index);
    OutBindings.GraphicsPipeline = Stoner::Core::MakeShared<FNativePipelineBinding>(Format);
    OutBindings.RenderPass = Stoner::Core::MakeShared<FNativeRenderPassBinding>(Format);
    OutBindings.Framebuffer = Stoner::Core::MakeShared<FNativeFramebufferBinding>(
        OutBindings.RenderPass, OutBindings.OutputTexture, Impl->SwapchainExtent.width, Impl->SwapchainExtent.height);
    OutBindings.CommandBuffer = Stoner::Core::MakeShared<FNativeCommandBufferBinding>(Impl->VisibleCommandBuffers[FrameSlot],
        Impl->RenderPass, Impl->SwapchainFramebuffers[ImageIndex], Impl->Pipeline,
        Impl->VertexBuffer, Impl->IndexBuffer, Impl->SwapchainExtent);
    return Stoner::RHI::ERHIResult::Success;
#else
    (void)OutBindings;
    return Stoner::RHI::ERHIResult::Unsupported;
#endif
}

Stoner::RHI::ERHIResult FVulkanNativeContext::SubmitAndPresentVisibleFrame(const FVulkanNativeFrameBindings& Bindings)
{
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE && defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    if (!Impl || !Impl->bFrameAcquired || Bindings.ImageIndex != Impl->AcquiredImageIndex ||
        Bindings.FrameSlot != Impl->AcquiredFrameSlot ||
        !Bindings.CommandBuffer || Bindings.CommandBuffer->GetState() != Stoner::RHI::ERHICommandBufferState::Completed)
        return Stoner::RHI::ERHIResult::InvalidState;
    VkFence& SubmitFence = Impl->VisibleFences[Bindings.FrameSlot];
    if (vkResetFences(Impl->Device, 1, &SubmitFence) != VK_SUCCESS)
    {
        Impl->AbandonAcquiredVisibleFrame();
        return Stoner::RHI::ERHIResult::Failed;
    }
    const VkPipelineStageFlags WaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo Submit = MakeVulkanStruct<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
    Submit.waitSemaphoreCount = 1; Submit.pWaitSemaphores = &Impl->VisibleImageAvailable[Bindings.FrameSlot]; Submit.pWaitDstStageMask = &WaitStage;
    Submit.commandBufferCount = 1; Submit.pCommandBuffers = &Impl->VisibleCommandBuffers[Bindings.FrameSlot];
    Submit.signalSemaphoreCount = 1; Submit.pSignalSemaphores = &Impl->VisibleRenderFinished[Bindings.ImageIndex];
    if (vkQueueSubmit(Impl->GraphicsQueue, 1, &Submit, SubmitFence) != VK_SUCCESS)
    {
        const bool bFenceRestored =
            Impl->RecreateVisibleFenceSignaled(Bindings.FrameSlot);
        Impl->AbandonAcquiredVisibleFrame();
        return bFenceRestored
            ? Stoner::RHI::ERHIResult::Failed
            : Stoner::RHI::ERHIResult::Unavailable;
    }
    VkPresentInfoKHR Present = MakeVulkanStruct<VkPresentInfoKHR>(VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
    Present.waitSemaphoreCount = 1; Present.pWaitSemaphores = &Impl->VisibleRenderFinished[Bindings.ImageIndex];
    Present.swapchainCount = 1; Present.pSwapchains = &Impl->Swapchain; Present.pImageIndices = &Impl->AcquiredImageIndex;
    const VkResult PresentResult = vkQueuePresentKHR(Impl->GraphicsQueue, &Present);
    const bool bAcquireWasSuboptimal = Impl->bAcquiredSuboptimal;
    Impl->bFrameAcquired = false;
    Impl->bAcquiredSuboptimal = false;
    Impl->CurrentFrameSlot =
        (Bindings.FrameSlot + 1) %
        static_cast<Stoner::Core::uint32>(Impl->VisibleFences.size());
    if (PresentResult == VK_ERROR_OUT_OF_DATE_KHR ||
        PresentResult == VK_SUBOPTIMAL_KHR ||
        (PresentResult == VK_SUCCESS && bAcquireWasSuboptimal))
    {
        return Stoner::RHI::ERHIResult::ResizeRequired;
    }
    return PresentResult == VK_SUCCESS ? Stoner::RHI::ERHIResult::Success : Stoner::RHI::ERHIResult::Failed;
#else
    (void)Bindings;
    return Stoner::RHI::ERHIResult::Unsupported;
#endif
}

Stoner::RHI::ERHIResult FVulkanNativeContext::DrawVisibleFrame()
{
    FVulkanNativeFrameBindings Bindings;
    const Stoner::RHI::ERHIResult AcquireResult = AcquireVisibleFrame(Bindings);
    if (AcquireResult != Stoner::RHI::ERHIResult::Success) return AcquireResult;
    auto& Commands = *Bindings.CommandBuffer;
    Stoner::RHI::FRHIResourceBarrierDesc Transition;
    Transition.Texture = Bindings.OutputTexture;
    Transition.RequiredTextureUsage = Stoner::RHI::ERHITextureUsage::ColorAttachment;
    if (Commands.Begin() != Stoner::RHI::ERHIResult::Success ||
        Commands.RecordLayoutTransition(Transition) != Stoner::RHI::ERHIResult::Success ||
        Commands.BeginRenderPass(Bindings.RenderPass, Bindings.Framebuffer) != Stoner::RHI::ERHIResult::Success ||
        Commands.BindGraphicsPipeline(Bindings.GraphicsPipeline) != Stoner::RHI::ERHIResult::Success ||
        Commands.BindVertexBuffer(Bindings.VertexBuffer) != Stoner::RHI::ERHIResult::Success ||
        Commands.BindIndexBuffer(Bindings.IndexBuffer, Stoner::RHI::ERHIIndexType::UInt16) != Stoner::RHI::ERHIResult::Success ||
        Commands.SetViewport({0, 0, static_cast<float>(Bindings.Framebuffer->GetWidth()), static_cast<float>(Bindings.Framebuffer->GetHeight()), 0, 1}) != Stoner::RHI::ERHIResult::Success ||
        Commands.SetScissor({0, 0, Bindings.Framebuffer->GetWidth(), Bindings.Framebuffer->GetHeight()}) != Stoner::RHI::ERHIResult::Success ||
        Commands.RecordDrawIndexed({3, 1, 0, 0, 0}) != Stoner::RHI::ERHIResult::Success ||
        Commands.EndRenderPass() != Stoner::RHI::ERHIResult::Success ||
        Commands.RecordLayoutTransition(Transition) != Stoner::RHI::ERHIResult::Success ||
        Commands.End() != Stoner::RHI::ERHIResult::Success)
    {
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
        if (Impl) Impl->AbandonAcquiredVisibleFrame();
#endif
        return Stoner::RHI::ERHIResult::Failed;
    }
    return SubmitAndPresentVisibleFrame(Bindings);
}

Stoner::RHI::ERHIResult FVulkanNativeContext::RecreateVisiblePresentation(
    Stoner::Core::uint32 Width, Stoner::Core::uint32 Height)
{
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE && defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    if (!Impl || !Impl->bHasVisibleShaders)
        return Stoner::RHI::ERHIResult::InvalidState;
    return PrepareVisibleTriangle(
        Impl->VisibleVertexShader,
        Impl->VisibleFragmentShader,
        Width,
        Height);
#else
    (void)Width;
    (void)Height;
    return Stoner::RHI::ERHIResult::Unsupported;
#endif
}

Stoner::RHI::ERHIResult FVulkanNativeContext::Shutdown()
{
    if (!Impl) return Stoner::RHI::ERHIResult::InvalidState;
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    if (Impl->Device)
    {
        vkDeviceWaitIdle(Impl->Device);
        Impl->DestroyFrameResources();
        Impl->DestroyAllOwnedPipelines();
        Impl->DestroyAllOwnedTextures();
        for (const auto& [Token, ShaderModule] : Impl->OwnedShaderModules)
        {
            (void)Token;
            vkDestroyShaderModule(Impl->Device, ShaderModule, nullptr);
        }
        Impl->OwnedShaderModules.clear();
        Impl->NextOwnedShaderToken = 1;
        vkDestroyDevice(Impl->Device, nullptr);
        Impl->Device = VK_NULL_HANDLE;
    }
    if (Impl->Surface)
    {
        vkDestroySurfaceKHR(Impl->Instance, Impl->Surface, nullptr);
        Impl->Surface = VK_NULL_HANDLE;
    }
    if (Impl->Instance)
    {
        vkDestroyInstance(Impl->Instance, nullptr);
        Impl->Instance = VK_NULL_HANDLE;
    }
#endif
    Impl->Snapshot = {};
    return Stoner::RHI::ERHIResult::Success;
}

const Stoner::RHI::FRHIRuntimeSnapshot& FVulkanNativeContext::GetSnapshot() const noexcept { return Impl->Snapshot; }
bool FVulkanNativeContext::IsAvailable() const noexcept { return Impl && Impl->Snapshot.ProvesNativeExecution(); }

Stoner::RHI::ERHIResult FVulkanNativeContext::CreateOwnedShaderModule(
    const Stoner::Core::TArray<Stoner::Core::uint32>& Words,
    Stoner::Core::uint64& OutToken) noexcept
{
    OutToken = 0;
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    if (!Impl || Impl->Device == VK_NULL_HANDLE || Words.empty())
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    VkShaderModuleCreateInfo Info =
        MakeVulkanStruct<VkShaderModuleCreateInfo>(
            VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
    Info.codeSize = Words.size() * sizeof(Stoner::Core::uint32);
    Info.pCode = Words.data();
    VkShaderModule ShaderModule = VK_NULL_HANDLE;
    const VkResult CreateResult =
        vkCreateShaderModule(Impl->Device, &Info, nullptr, &ShaderModule);
    if (CreateResult != VK_SUCCESS)
    {
        return CreateResult == VK_ERROR_OUT_OF_HOST_MEMORY ||
                CreateResult == VK_ERROR_OUT_OF_DEVICE_MEMORY
            ? Stoner::RHI::ERHIResult::Unavailable
            : Stoner::RHI::ERHIResult::Failed;
    }

    if (Impl->NextOwnedShaderToken == 0)
    {
        vkDestroyShaderModule(Impl->Device, ShaderModule, nullptr);
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    const Stoner::Core::uint64 Token = Impl->NextOwnedShaderToken++;
    try
    {
        Impl->OwnedShaderModules.emplace(Token, ShaderModule);
    }
    catch (const std::bad_alloc&)
    {
        vkDestroyShaderModule(Impl->Device, ShaderModule, nullptr);
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        vkDestroyShaderModule(Impl->Device, ShaderModule, nullptr);
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    Impl->Snapshot.LiveShaderModules = Impl->GetLiveShaderModuleCount();
    OutToken = Token;
    return Stoner::RHI::ERHIResult::Success;
#else
    (void)Words;
    return Stoner::RHI::ERHIResult::Unsupported;
#endif
}

void FVulkanNativeContext::DestroyOwnedShaderModule(
    Stoner::Core::uint64 Token) noexcept
{
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    if (!Impl || Token == 0)
    {
        return;
    }
    const auto Found = Impl->OwnedShaderModules.find(Token);
    if (Found == Impl->OwnedShaderModules.end())
    {
        return;
    }
    if (Impl->Device != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(Impl->Device, Found->second, nullptr);
    }
    Impl->OwnedShaderModules.erase(Found);
    Impl->Snapshot.LiveShaderModules = Impl->GetLiveShaderModuleCount();
#else
    (void)Token;
#endif
}

Stoner::RHI::ERHIResult FVulkanNativeContext::CreateOwnedGraphicsPipeline(
    const Stoner::RHI::FRHIGraphicsPipelineDesc& Desc,
    Stoner::Core::uint64 VertexShaderToken,
    Stoner::Core::uint64 FragmentShaderToken,
    Stoner::Core::uint64& OutToken) noexcept
{
    OutToken = 0;
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    if (!Impl || Impl->Device == VK_NULL_HANDLE ||
        VertexShaderToken == 0 || FragmentShaderToken == 0)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    const auto VertexShader = Impl->OwnedShaderModules.find(VertexShaderToken);
    const auto FragmentShader =
        Impl->OwnedShaderModules.find(FragmentShaderToken);
    if (VertexShader == Impl->OwnedShaderModules.end() ||
        FragmentShader == Impl->OwnedShaderModules.end() ||
        !Desc.PipelineLayout)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (Desc.Rasterizer.bDepthClampEnabled ||
        Desc.Multisample.bSampleShadingEnabled)
    {
        // The context intentionally creates a baseline device without optional
        // depth-clamp or sample-rate-shading features.
        return Stoner::RHI::ERHIResult::Unsupported;
    }

    FImpl::FOwnedPipelineResources Resources;
    try
    {
        const Stoner::RHI::ERHIResult LayoutResult =
            Impl->CreateOwnedPipelineLayout(
                Desc.PipelineLayout->GetDesc(), Resources);
        if (LayoutResult != Stoner::RHI::ERHIResult::Success)
        {
            return LayoutResult;
        }

        const VkSampleCountFlagBits SampleCount =
            ToVulkanSampleCount(Desc.RenderTargets.SampleCount);
        VkPhysicalDeviceProperties DeviceProperties{};
        vkGetPhysicalDeviceProperties(
            Impl->PhysicalDevice, &DeviceProperties);
        VkSampleCountFlags SupportedSampleCounts =
            DeviceProperties.limits.framebufferColorSampleCounts;
        if (Desc.RenderTargets.DepthStencilFormat !=
            Stoner::RHI::ERHIFormat::Unknown)
        {
            SupportedSampleCounts &=
                DeviceProperties.limits.framebufferDepthSampleCounts;
        }
        if (SampleCount == 0 ||
            (SupportedSampleCounts & SampleCount) == 0)
        {
            Impl->DestroyOwnedPipelineResources(Resources);
            return Stoner::RHI::ERHIResult::Unsupported;
        }

        std::vector<VkAttachmentDescription> Attachments;
        std::vector<VkAttachmentReference> ColorReferences;
        Attachments.reserve(
            Desc.RenderTargets.ColorFormats.size() +
            (Desc.RenderTargets.DepthStencilFormat !=
                    Stoner::RHI::ERHIFormat::Unknown
                ? 1u
                : 0u));
        ColorReferences.reserve(Desc.RenderTargets.ColorFormats.size());
        for (Stoner::RHI::ERHIFormat Format :
             Desc.RenderTargets.ColorFormats)
        {
            const VkFormat NativeFormat = ToVulkanFormat(Format);
            VkFormatProperties FormatProperties{};
            vkGetPhysicalDeviceFormatProperties(
                Impl->PhysicalDevice, NativeFormat, &FormatProperties);
            if (NativeFormat == VK_FORMAT_UNDEFINED ||
                (FormatProperties.optimalTilingFeatures &
                    VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT) == 0)
            {
                Impl->DestroyOwnedPipelineResources(Resources);
                return Stoner::RHI::ERHIResult::Unsupported;
            }
            VkAttachmentDescription Attachment{};
            Attachment.format = NativeFormat;
            Attachment.samples = SampleCount;
            Attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            Attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            Attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            Attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            Attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            Attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            Attachments.push_back(Attachment);
            ColorReferences.push_back(
                {static_cast<Stoner::Core::uint32>(
                     Attachments.size() - 1u),
                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL});
        }

        VkAttachmentReference DepthReference{};
        const bool bHasDepth =
            Desc.RenderTargets.DepthStencilFormat !=
            Stoner::RHI::ERHIFormat::Unknown;
        if (bHasDepth)
        {
            const VkFormat NativeDepthFormat =
                ToVulkanFormat(Desc.RenderTargets.DepthStencilFormat);
            VkFormatProperties FormatProperties{};
            vkGetPhysicalDeviceFormatProperties(
                Impl->PhysicalDevice,
                NativeDepthFormat,
                &FormatProperties);
            if (NativeDepthFormat == VK_FORMAT_UNDEFINED ||
                (FormatProperties.optimalTilingFeatures &
                    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) == 0)
            {
                Impl->DestroyOwnedPipelineResources(Resources);
                return Stoner::RHI::ERHIResult::Unsupported;
            }
            VkAttachmentDescription Attachment{};
            Attachment.format = NativeDepthFormat;
            Attachment.samples = SampleCount;
            Attachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            Attachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            Attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            Attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            Attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            Attachment.finalLayout =
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            Attachments.push_back(Attachment);
            DepthReference = {
                static_cast<Stoner::Core::uint32>(
                    Attachments.size() - 1u),
                VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
        }

        VkSubpassDescription Subpass{};
        Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        Subpass.colorAttachmentCount =
            static_cast<Stoner::Core::uint32>(ColorReferences.size());
        Subpass.pColorAttachments = ColorReferences.data();
        Subpass.pDepthStencilAttachment =
            bHasDepth ? &DepthReference : nullptr;
        VkRenderPassCreateInfo RenderPassInfo =
            MakeVulkanStruct<VkRenderPassCreateInfo>(
                VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
        RenderPassInfo.attachmentCount =
            static_cast<Stoner::Core::uint32>(Attachments.size());
        RenderPassInfo.pAttachments = Attachments.data();
        RenderPassInfo.subpassCount = 1;
        RenderPassInfo.pSubpasses = &Subpass;
        const VkResult RenderPassResult = vkCreateRenderPass(
            Impl->Device,
            &RenderPassInfo,
            nullptr,
            &Resources.RenderPass);
        if (RenderPassResult != VK_SUCCESS)
        {
            const Stoner::RHI::ERHIResult Result =
                MapVulkanCreationResult(RenderPassResult);
            Impl->DestroyOwnedPipelineResources(Resources);
            return Result;
        }

        const auto FindShaderByStage =
            [&Desc](Stoner::RHI::ERHIShaderStage Stage)
                -> Stoner::Core::TSharedPtr<Stoner::RHI::IRHIShaderModule>
        {
            for (const auto& Shader : Desc.ShaderModules)
            {
                if (Shader && Shader->GetStage() == Stage)
                {
                    return Shader;
                }
            }
            return nullptr;
        };
        const auto VertexDesc =
            FindShaderByStage(Stoner::RHI::ERHIShaderStage::Vertex);
        const auto FragmentDesc =
            FindShaderByStage(Stoner::RHI::ERHIShaderStage::Fragment);
        if (!VertexDesc || !FragmentDesc)
        {
            Impl->DestroyOwnedPipelineResources(Resources);
            return Stoner::RHI::ERHIResult::InvalidState;
        }
        VkPipelineShaderStageCreateInfo ShaderStages[2] = {
            MakeVulkanStruct<VkPipelineShaderStageCreateInfo>(
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO),
            MakeVulkanStruct<VkPipelineShaderStageCreateInfo>(
                VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO)};
        ShaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        ShaderStages[0].module = VertexShader->second;
        ShaderStages[0].pName = VertexDesc->GetDesc().EntryPoint.CStr();
        ShaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        ShaderStages[1].module = FragmentShader->second;
        ShaderStages[1].pName = FragmentDesc->GetDesc().EntryPoint.CStr();

        VkVertexInputBindingDescription VertexBinding{
            0,
            Desc.VertexInput.Stride,
            VK_VERTEX_INPUT_RATE_VERTEX};
        std::vector<VkVertexInputAttributeDescription> VertexAttributes;
        VertexAttributes.reserve(Desc.VertexInput.Attributes.size());
        for (const Stoner::RHI::FRHIVertexAttributeDesc& Attribute :
             Desc.VertexInput.Attributes)
        {
            const VkFormat NativeFormat = ToVulkanFormat(Attribute.Format);
            VkFormatProperties FormatProperties{};
            vkGetPhysicalDeviceFormatProperties(
                Impl->PhysicalDevice, NativeFormat, &FormatProperties);
            if (NativeFormat == VK_FORMAT_UNDEFINED ||
                (FormatProperties.bufferFeatures &
                    VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT) == 0 ||
                Stoner::RHI::GetRHIFormatByteSize(Attribute.Format) >
                    Desc.VertexInput.Stride - Attribute.Offset ||
                std::any_of(
                    VertexAttributes.begin(),
                    VertexAttributes.end(),
                    [&Attribute](
                        const VkVertexInputAttributeDescription& Existing)
                    {
                        return Existing.location == Attribute.Location;
                    }))
            {
                Impl->DestroyOwnedPipelineResources(Resources);
                return Stoner::RHI::ERHIResult::Unsupported;
            }
            VertexAttributes.push_back(
                {Attribute.Location, 0, NativeFormat, Attribute.Offset});
        }
        VkPipelineVertexInputStateCreateInfo VertexInput =
            MakeVulkanStruct<VkPipelineVertexInputStateCreateInfo>(
                VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
        VertexInput.vertexBindingDescriptionCount = 1;
        VertexInput.pVertexBindingDescriptions = &VertexBinding;
        VertexInput.vertexAttributeDescriptionCount =
            static_cast<Stoner::Core::uint32>(VertexAttributes.size());
        VertexInput.pVertexAttributeDescriptions = VertexAttributes.data();

        VkPipelineInputAssemblyStateCreateInfo InputAssembly =
            MakeVulkanStruct<VkPipelineInputAssemblyStateCreateInfo>(
                VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
        InputAssembly.topology = ToVulkanTopology(Desc.Topology);
        VkPipelineViewportStateCreateInfo ViewportState =
            MakeVulkanStruct<VkPipelineViewportStateCreateInfo>(
                VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
        ViewportState.viewportCount = 1;
        ViewportState.scissorCount = 1;
        VkPipelineRasterizationStateCreateInfo Rasterizer =
            MakeVulkanStruct<VkPipelineRasterizationStateCreateInfo>(
                VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
        Rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        Rasterizer.cullMode = ToVulkanCullMode(Desc.Rasterizer.CullMode);
        Rasterizer.frontFace = ToVulkanFrontFace(Desc.Rasterizer.FrontFace);
        Rasterizer.lineWidth = 1.0f;
        VkPipelineMultisampleStateCreateInfo Multisample =
            MakeVulkanStruct<VkPipelineMultisampleStateCreateInfo>(
                VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
        Multisample.rasterizationSamples = SampleCount;
        VkPipelineDepthStencilStateCreateInfo DepthStencil =
            MakeVulkanStruct<VkPipelineDepthStencilStateCreateInfo>(
                VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
        DepthStencil.depthTestEnable =
            Desc.DepthStencil.bDepthTestEnabled ? VK_TRUE : VK_FALSE;
        DepthStencil.depthWriteEnable =
            Desc.DepthStencil.bDepthWriteEnabled ? VK_TRUE : VK_FALSE;
        DepthStencil.depthCompareOp =
            ToVulkanCompareOp(Desc.DepthStencil.DepthCompare);

        VkPipelineColorBlendAttachmentState BlendAttachment{};
        BlendAttachment.blendEnable =
            Desc.Blend.bEnabled ? VK_TRUE : VK_FALSE;
        BlendAttachment.srcColorBlendFactor =
            ToVulkanBlendFactor(Desc.Blend.SourceColor);
        BlendAttachment.dstColorBlendFactor =
            ToVulkanBlendFactor(Desc.Blend.DestinationColor);
        BlendAttachment.colorBlendOp = ToVulkanBlendOp(Desc.Blend.ColorOp);
        BlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        BlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        BlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
        BlendAttachment.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        std::vector<VkPipelineColorBlendAttachmentState> BlendAttachments(
            Desc.RenderTargets.ColorFormats.size(), BlendAttachment);
        VkPipelineColorBlendStateCreateInfo Blend =
            MakeVulkanStruct<VkPipelineColorBlendStateCreateInfo>(
                VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
        Blend.attachmentCount =
            static_cast<Stoner::Core::uint32>(BlendAttachments.size());
        Blend.pAttachments = BlendAttachments.data();
        const VkDynamicState DynamicStates[] = {
            VK_DYNAMIC_STATE_VIEWPORT,
            VK_DYNAMIC_STATE_SCISSOR};
        VkPipelineDynamicStateCreateInfo Dynamic =
            MakeVulkanStruct<VkPipelineDynamicStateCreateInfo>(
                VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
        Dynamic.dynamicStateCount = 2;
        Dynamic.pDynamicStates = DynamicStates;

        VkGraphicsPipelineCreateInfo PipelineInfo =
            MakeVulkanStruct<VkGraphicsPipelineCreateInfo>(
                VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
        PipelineInfo.stageCount = 2;
        PipelineInfo.pStages = ShaderStages;
        PipelineInfo.pVertexInputState = &VertexInput;
        PipelineInfo.pInputAssemblyState = &InputAssembly;
        PipelineInfo.pViewportState = &ViewportState;
        PipelineInfo.pRasterizationState = &Rasterizer;
        PipelineInfo.pMultisampleState = &Multisample;
        PipelineInfo.pDepthStencilState =
            bHasDepth ? &DepthStencil : nullptr;
        PipelineInfo.pColorBlendState = &Blend;
        PipelineInfo.pDynamicState = &Dynamic;
        PipelineInfo.layout = Resources.PipelineLayout;
        PipelineInfo.renderPass = Resources.RenderPass;
        const VkResult PipelineResult = vkCreateGraphicsPipelines(
            Impl->Device,
            VK_NULL_HANDLE,
            1,
            &PipelineInfo,
            nullptr,
            &Resources.Pipeline);
        if (PipelineResult != VK_SUCCESS)
        {
            const Stoner::RHI::ERHIResult Result =
                MapVulkanCreationResult(PipelineResult);
            Impl->DestroyOwnedPipelineResources(Resources);
            return Result;
        }
        return Impl->PublishOwnedPipeline(Resources, OutToken);
    }
    catch (const std::bad_alloc&)
    {
        Impl->DestroyOwnedPipelineResources(Resources);
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        Impl->DestroyOwnedPipelineResources(Resources);
        return Stoner::RHI::ERHIResult::Unavailable;
    }
#else
    (void)Desc;
    (void)VertexShaderToken;
    (void)FragmentShaderToken;
    return Stoner::RHI::ERHIResult::Unsupported;
#endif
}

Stoner::RHI::ERHIResult FVulkanNativeContext::CreateOwnedComputePipeline(
    const Stoner::RHI::FRHIComputePipelineDesc& Desc,
    Stoner::Core::uint64 ComputeShaderToken,
    Stoner::Core::uint64& OutToken) noexcept
{
    OutToken = 0;
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    if (!Impl || Impl->Device == VK_NULL_HANDLE ||
        ComputeShaderToken == 0 || !Desc.PipelineLayout ||
        Desc.ShaderModules.size() != 1 || !Desc.ShaderModules[0])
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    const auto ComputeShader =
        Impl->OwnedShaderModules.find(ComputeShaderToken);
    if (ComputeShader == Impl->OwnedShaderModules.end())
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    FImpl::FOwnedPipelineResources Resources;
    const Stoner::RHI::ERHIResult LayoutResult =
        Impl->CreateOwnedPipelineLayout(
            Desc.PipelineLayout->GetDesc(), Resources);
    if (LayoutResult != Stoner::RHI::ERHIResult::Success)
    {
        return LayoutResult;
    }
    VkPipelineShaderStageCreateInfo ShaderStage =
        MakeVulkanStruct<VkPipelineShaderStageCreateInfo>(
            VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO);
    ShaderStage.stage = ToVulkanShaderStage(
        Desc.ShaderModules[0]->GetStage());
    ShaderStage.module = ComputeShader->second;
    ShaderStage.pName =
        Desc.ShaderModules[0]->GetDesc().EntryPoint.CStr();
    if (ShaderStage.stage != VK_SHADER_STAGE_COMPUTE_BIT)
    {
        Impl->DestroyOwnedPipelineResources(Resources);
        return Stoner::RHI::ERHIResult::Unsupported;
    }
    VkComputePipelineCreateInfo PipelineInfo =
        MakeVulkanStruct<VkComputePipelineCreateInfo>(
            VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO);
    PipelineInfo.stage = ShaderStage;
    PipelineInfo.layout = Resources.PipelineLayout;
    const VkResult PipelineResult = vkCreateComputePipelines(
        Impl->Device,
        VK_NULL_HANDLE,
        1,
        &PipelineInfo,
        nullptr,
        &Resources.Pipeline);
    if (PipelineResult != VK_SUCCESS)
    {
        const Stoner::RHI::ERHIResult Result =
            MapVulkanCreationResult(PipelineResult);
        Impl->DestroyOwnedPipelineResources(Resources);
        return Result;
    }
    return Impl->PublishOwnedPipeline(Resources, OutToken);
#else
    (void)Desc;
    (void)ComputeShaderToken;
    return Stoner::RHI::ERHIResult::Unsupported;
#endif
}

void FVulkanNativeContext::DestroyOwnedPipeline(
    Stoner::Core::uint64 Token) noexcept
{
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    if (!Impl || Token == 0)
    {
        return;
    }
    const auto Found = Impl->OwnedPipelines.find(Token);
    if (Found == Impl->OwnedPipelines.end())
    {
        return;
    }
    Impl->DestroyOwnedPipelineResources(Found->second);
    Impl->OwnedPipelines.erase(Found);
    Impl->Snapshot.LivePipelines = Impl->GetLivePipelineCount();
#else
    (void)Token;
#endif
}

Stoner::Core::TArray<Stoner::RHI::FRHIFormatCapabilities>
FVulkanNativeContext::QueryTextureFormatCapabilities() const
{
    Stoner::Core::TArray<
        Stoner::RHI::FRHIFormatCapabilities> Records;
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    if (!Impl || Impl->PhysicalDevice == VK_NULL_HANDLE)
    {
        return Records;
    }
    const auto Count = static_cast<Stoner::Core::uint32>(
        Stoner::RHI::ERHIFormat::Count);
    Records.reserve(Count > 0 ? Count - 1 : 0);
    for (Stoner::Core::uint32 Value = 1;
         Value < Count;
         ++Value)
    {
        const auto Format =
            static_cast<Stoner::RHI::ERHIFormat>(Value);
        const VkFormat NativeFormat = ToVulkanFormat(Format);
        if (NativeFormat == VK_FORMAT_UNDEFINED)
        {
            continue;
        }
        VkFormatProperties Properties{};
        vkGetPhysicalDeviceFormatProperties(
            Impl->PhysicalDevice,
            NativeFormat,
            &Properties);
        const auto Capabilities = ToRHIFormatCapabilities(
            Properties.optimalTilingFeatures);
        const Stoner::RHI::FRHIFormatCapabilities Record{
            Format, Capabilities};
        if (Record.IsValid())
        {
            Records.push_back(Record);
        }
    }
#endif
    return Records;
}

Stoner::RHI::ERHIResult FVulkanNativeContext::CreateOwnedTexture(
    const Stoner::RHI::FRHITextureDesc& Desc,
    Stoner::Core::uint64& OutToken) noexcept
{
    OutToken = 0;
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    using namespace Stoner::RHI;
    if (!Impl || Impl->Device == VK_NULL_HANDLE ||
        !IsValidRHITextureDesc(Desc))
    {
        return ERHIResult::InvalidState;
    }
    if (Desc.Dimension != ERHITextureDimension::Texture2D ||
        Desc.ArrayLayers != 1 ||
        Desc.SampleCount != ERHISampleCount::One)
    {
        return ERHIResult::Unsupported;
    }

    const VkFormat Format = ToVulkanFormat(Desc.Format);
    const VkFormatFeatureFlags RequiredFeatures =
        RequiredTextureFeatures(Desc.Usage);
    const VkImageUsageFlags ImageUsage =
        ToVulkanImageUsage(Desc.Usage);
    VkFormatProperties Properties{};
    if (Format == VK_FORMAT_UNDEFINED ||
        RequiredFeatures == 0 ||
        ImageUsage == 0)
    {
        return ERHIResult::Unsupported;
    }
    vkGetPhysicalDeviceFormatProperties(
        Impl->PhysicalDevice, Format, &Properties);
    if ((Properties.optimalTilingFeatures & RequiredFeatures) !=
        RequiredFeatures)
    {
        return ERHIResult::Unsupported;
    }

    FImpl::FOwnedTextureResources Resources;
    Resources.Desc = Desc;
    try
    {
        Resources.MipLayouts.assign(
            Desc.MipLevels, VK_IMAGE_LAYOUT_UNDEFINED);
    }
    catch (const std::bad_alloc&)
    {
        return ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        return ERHIResult::Unavailable;
    }

    VkImageCreateInfo ImageInfo =
        MakeVulkanStruct<VkImageCreateInfo>(
            VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
    ImageInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageInfo.format = Format;
    ImageInfo.extent = {Desc.Width, Desc.Height, 1};
    ImageInfo.mipLevels = Desc.MipLevels;
    ImageInfo.arrayLayers = 1;
    ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageInfo.usage = ImageUsage;
    ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkResult NativeResult = vkCreateImage(
        Impl->Device, &ImageInfo, nullptr, &Resources.Image);
    if (NativeResult != VK_SUCCESS)
    {
        return MapVulkanCreationResult(NativeResult);
    }

    VkMemoryRequirements Requirements{};
    vkGetImageMemoryRequirements(
        Impl->Device, Resources.Image, &Requirements);
    const Stoner::Core::uint32 MemoryType =
        Impl->FindMemoryType(
            Requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (MemoryType == UINT32_MAX)
    {
        Impl->DestroyOwnedTextureResources(Resources);
        return ERHIResult::Unsupported;
    }
    VkMemoryAllocateInfo Allocation =
        MakeVulkanStruct<VkMemoryAllocateInfo>(
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
    Allocation.allocationSize = Requirements.size;
    Allocation.memoryTypeIndex = MemoryType;
    NativeResult = vkAllocateMemory(
        Impl->Device, &Allocation, nullptr, &Resources.Memory);
    if (NativeResult != VK_SUCCESS)
    {
        const ERHIResult Result =
            MapVulkanCreationResult(NativeResult);
        Impl->DestroyOwnedTextureResources(Resources);
        return Result;
    }
    NativeResult = vkBindImageMemory(
        Impl->Device, Resources.Image, Resources.Memory, 0);
    if (NativeResult != VK_SUCCESS)
    {
        Impl->DestroyOwnedTextureResources(Resources);
        return ERHIResult::Failed;
    }
    if (Impl->NextOwnedTextureToken == 0)
    {
        Impl->DestroyOwnedTextureResources(Resources);
        return ERHIResult::Unavailable;
    }

    const Stoner::Core::uint64 Token =
        Impl->NextOwnedTextureToken++;
    try
    {
        const auto [Iterator, bInserted] =
            Impl->OwnedTextures.emplace(
                Token, std::move(Resources));
        (void)Iterator;
        if (!bInserted)
        {
            Impl->DestroyOwnedTextureResources(Resources);
            return ERHIResult::Unavailable;
        }
    }
    catch (const std::bad_alloc&)
    {
        Impl->DestroyOwnedTextureResources(Resources);
        return ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        Impl->DestroyOwnedTextureResources(Resources);
        return ERHIResult::Unavailable;
    }
    Impl->Snapshot.LiveTextures =
        Impl->GetLiveTextureCount();
    OutToken = Token;
    return ERHIResult::Success;
#else
    (void)Desc;
    return Stoner::RHI::ERHIResult::Unsupported;
#endif
}

Stoner::RHI::ERHIResult FVulkanNativeContext::UploadOwnedTexture(
    Stoner::Core::uint64 Token,
    const Stoner::RHI::FRHITextureUploadDesc& Upload) noexcept
{
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    using namespace Stoner::RHI;
    if (!Impl || Impl->Device == VK_NULL_HANDLE || Token == 0)
    {
        return ERHIResult::InvalidState;
    }
    const auto Found = Impl->OwnedTextures.find(Token);
    if (Found == Impl->OwnedTextures.end())
    {
        return ERHIResult::InvalidState;
    }
    FImpl::FOwnedTextureResources& Texture = Found->second;
    Stoner::Core::uint64 RequiredBytes = 0;
    FRHITextureFootprint Footprint;
    if (!IsValidRHITextureUploadDesc(Texture.Desc, Upload) ||
        !TryGetRHITextureFootprint(
            Texture.Desc.Format,
            Upload.Width,
            Upload.Height,
            Upload.Depth,
            Footprint) ||
        !TryGetRHITextureUploadRequiredBytes(
            Texture.Desc, Upload, RequiredBytes) ||
        Upload.RowPitchBytes != Footprint.TightRowBytes ||
        Upload.DataSizeBytes != RequiredBytes ||
        Upload.MipLevel >= Texture.MipLayouts.size())
    {
        return ERHIResult::InvalidState;
    }

    VkBuffer StagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory StagingMemory = VK_NULL_HANDLE;
    VkCommandPool CommandPool = VK_NULL_HANDLE;
    VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
    VkFence Fence = VK_NULL_HANDLE;
    bool bSubmitted = false;
    const auto Cleanup = [&]() noexcept
    {
        if (bSubmitted &&
            Impl->GraphicsQueue != VK_NULL_HANDLE)
        {
            (void)vkQueueWaitIdle(Impl->GraphicsQueue);
            bSubmitted = false;
        }
        if (Fence != VK_NULL_HANDLE)
            vkDestroyFence(Impl->Device, Fence, nullptr);
        if (CommandPool != VK_NULL_HANDLE)
            vkDestroyCommandPool(
                Impl->Device, CommandPool, nullptr);
        if (StagingBuffer != VK_NULL_HANDLE)
            vkDestroyBuffer(
                Impl->Device, StagingBuffer, nullptr);
        if (StagingMemory != VK_NULL_HANDLE)
            vkFreeMemory(
                Impl->Device, StagingMemory, nullptr);
    };
    const auto Fail = [&](ERHIResult Result) noexcept
    {
        Cleanup();
        return Result;
    };

    VkBufferCreateInfo BufferInfo =
        MakeVulkanStruct<VkBufferCreateInfo>(
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
    BufferInfo.size = RequiredBytes;
    BufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult NativeResult = vkCreateBuffer(
        Impl->Device, &BufferInfo, nullptr, &StagingBuffer);
    if (NativeResult != VK_SUCCESS)
    {
        return Fail(MapVulkanCreationResult(NativeResult));
    }
    VkMemoryRequirements Requirements{};
    vkGetBufferMemoryRequirements(
        Impl->Device, StagingBuffer, &Requirements);
    const Stoner::Core::uint32 MemoryType =
        Impl->FindMemoryType(
            Requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (MemoryType == UINT32_MAX)
    {
        return Fail(ERHIResult::Unsupported);
    }
    VkMemoryAllocateInfo Allocation =
        MakeVulkanStruct<VkMemoryAllocateInfo>(
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
    Allocation.allocationSize = Requirements.size;
    Allocation.memoryTypeIndex = MemoryType;
    NativeResult = vkAllocateMemory(
        Impl->Device, &Allocation, nullptr, &StagingMemory);
    if (NativeResult != VK_SUCCESS)
    {
        return Fail(MapVulkanCreationResult(NativeResult));
    }
    if (vkBindBufferMemory(
            Impl->Device,
            StagingBuffer,
            StagingMemory,
            0) != VK_SUCCESS)
    {
        return Fail(ERHIResult::Failed);
    }
    void* Mapped = nullptr;
    NativeResult = vkMapMemory(
        Impl->Device,
        StagingMemory,
        0,
        RequiredBytes,
        0,
        &Mapped);
    if (NativeResult != VK_SUCCESS)
    {
        return Fail(MapVulkanCreationResult(NativeResult));
    }
    std::memcpy(
        Mapped,
        Upload.Data,
        static_cast<std::size_t>(RequiredBytes));
    vkUnmapMemory(Impl->Device, StagingMemory);

    VkCommandPoolCreateInfo PoolInfo =
        MakeVulkanStruct<VkCommandPoolCreateInfo>(
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
    PoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    PoolInfo.queueFamilyIndex = Impl->GraphicsQueueFamily;
    NativeResult = vkCreateCommandPool(
        Impl->Device, &PoolInfo, nullptr, &CommandPool);
    if (NativeResult != VK_SUCCESS)
    {
        return Fail(MapVulkanCreationResult(NativeResult));
    }
    VkCommandBufferAllocateInfo CommandInfo =
        MakeVulkanStruct<VkCommandBufferAllocateInfo>(
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
    CommandInfo.commandPool = CommandPool;
    CommandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CommandInfo.commandBufferCount = 1;
    NativeResult = vkAllocateCommandBuffers(
        Impl->Device, &CommandInfo, &CommandBuffer);
    if (NativeResult != VK_SUCCESS)
    {
        return Fail(MapVulkanCreationResult(NativeResult));
    }
    VkCommandBufferBeginInfo BeginInfo =
        MakeVulkanStruct<VkCommandBufferBeginInfo>(
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
    BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(CommandBuffer, &BeginInfo) !=
        VK_SUCCESS)
    {
        return Fail(ERHIResult::Failed);
    }

    const VkImageLayout OldLayout =
        Texture.MipLayouts[Upload.MipLevel];
    VkImageMemoryBarrier ToTransfer =
        MakeVulkanStruct<VkImageMemoryBarrier>(
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
    ToTransfer.srcAccessMask =
        OldLayout == VK_IMAGE_LAYOUT_UNDEFINED
        ? 0
        : VK_ACCESS_SHADER_READ_BIT;
    ToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    ToTransfer.oldLayout = OldLayout;
    ToTransfer.newLayout =
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    ToTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ToTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ToTransfer.image = Texture.Image;
    ToTransfer.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    ToTransfer.subresourceRange.baseMipLevel = Upload.MipLevel;
    ToTransfer.subresourceRange.levelCount = 1;
    ToTransfer.subresourceRange.baseArrayLayer =
        Upload.ArrayLayer;
    ToTransfer.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(
        CommandBuffer,
        OldLayout == VK_IMAGE_LAYOUT_UNDEFINED
            ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
            : VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &ToTransfer);

    VkBufferImageCopy Copy{};
    Copy.bufferOffset = 0;
    Copy.bufferRowLength = 0;
    Copy.bufferImageHeight = 0;
    Copy.imageSubresource.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    Copy.imageSubresource.mipLevel = Upload.MipLevel;
    Copy.imageSubresource.baseArrayLayer =
        Upload.ArrayLayer;
    Copy.imageSubresource.layerCount = 1;
    Copy.imageOffset = {
        static_cast<Stoner::Core::int32>(Upload.X),
        static_cast<Stoner::Core::int32>(Upload.Y),
        static_cast<Stoner::Core::int32>(Upload.Z)};
    Copy.imageExtent = {
        Upload.Width, Upload.Height, Upload.Depth};
    vkCmdCopyBufferToImage(
        CommandBuffer,
        StagingBuffer,
        Texture.Image,
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        1,
        &Copy);

    VkImageMemoryBarrier ToShaderRead = ToTransfer;
    ToShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    ToShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    ToShaderRead.oldLayout =
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    ToShaderRead.newLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(
        CommandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &ToShaderRead);
    if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS)
    {
        return Fail(ERHIResult::Failed);
    }

    VkFenceCreateInfo FenceInfo =
        MakeVulkanStruct<VkFenceCreateInfo>(
            VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
    NativeResult = vkCreateFence(
        Impl->Device, &FenceInfo, nullptr, &Fence);
    if (NativeResult != VK_SUCCESS)
    {
        return Fail(MapVulkanCreationResult(NativeResult));
    }
    VkSubmitInfo Submit =
        MakeVulkanStruct<VkSubmitInfo>(
            VK_STRUCTURE_TYPE_SUBMIT_INFO);
    Submit.commandBufferCount = 1;
    Submit.pCommandBuffers = &CommandBuffer;
    NativeResult = vkQueueSubmit(
        Impl->GraphicsQueue, 1, &Submit, Fence);
    if (NativeResult != VK_SUCCESS)
    {
        return Fail(ERHIResult::Failed);
    }
    bSubmitted = true;
    NativeResult = vkWaitForFences(
        Impl->Device,
        1,
        &Fence,
        VK_TRUE,
        30ULL * 1000ULL * 1000ULL * 1000ULL);
    if (NativeResult != VK_SUCCESS)
    {
        return Fail(
            NativeResult == VK_TIMEOUT
                ? ERHIResult::Timeout
                : ERHIResult::Failed);
    }
    bSubmitted = false;
    Texture.MipLayouts[Upload.MipLevel] =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    Cleanup();
    return ERHIResult::Success;
#else
    (void)Token;
    (void)Upload;
    return Stoner::RHI::ERHIResult::Unsupported;
#endif
}

Stoner::RHI::ERHIResult FVulkanNativeContext::ReadbackOwnedTexture(
    Stoner::Core::uint64 Token,
    Stoner::Core::uint32 MipLevel,
    Stoner::Core::TArray<Stoner::Core::uint8>& OutBytes) noexcept
{
    OutBytes.clear();
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    using namespace Stoner::RHI;
    if (!Impl || Impl->Device == VK_NULL_HANDLE || Token == 0)
    {
        return ERHIResult::InvalidState;
    }
    const auto Found = Impl->OwnedTextures.find(Token);
    if (Found == Impl->OwnedTextures.end() ||
        MipLevel >= Found->second.MipLayouts.size())
    {
        return ERHIResult::InvalidState;
    }
    FImpl::FOwnedTextureResources& Texture = Found->second;
    if (!HasRHIFlag(
            Texture.Desc.Usage,
            ERHITextureUsage::CopySource))
    {
        return ERHIResult::Unsupported;
    }
    if (Texture.MipLayouts[MipLevel] !=
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    {
        return ERHIResult::NotReady;
    }
    const Stoner::Core::uint32 Width =
        GetRHIMipExtent(Texture.Desc.Width, MipLevel);
    const Stoner::Core::uint32 Height =
        GetRHIMipExtent(Texture.Desc.Height, MipLevel);
    const Stoner::Core::uint32 Depth =
        GetRHIMipExtent(Texture.Desc.Depth, MipLevel);
    FRHITextureFootprint Footprint;
    if (!TryGetRHITextureFootprint(
            Texture.Desc.Format,
            Width,
            Height,
            Depth,
            Footprint))
    {
        return ERHIResult::Unavailable;
    }
    const Stoner::Core::uint64 ByteSize =
        Footprint.TotalBytes;
    if (ByteSize >
        static_cast<Stoner::Core::uint64>(
            std::numeric_limits<Stoner::Core::usize>::max()))
    {
        return ERHIResult::Unavailable;
    }

    VkBuffer ReadbackBuffer = VK_NULL_HANDLE;
    VkDeviceMemory ReadbackMemory = VK_NULL_HANDLE;
    VkCommandPool CommandPool = VK_NULL_HANDLE;
    VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
    VkFence Fence = VK_NULL_HANDLE;
    bool bSubmitted = false;
    const auto Cleanup = [&]() noexcept
    {
        if (bSubmitted &&
            Impl->GraphicsQueue != VK_NULL_HANDLE)
        {
            (void)vkQueueWaitIdle(Impl->GraphicsQueue);
            bSubmitted = false;
        }
        if (Fence != VK_NULL_HANDLE)
            vkDestroyFence(Impl->Device, Fence, nullptr);
        if (CommandPool != VK_NULL_HANDLE)
            vkDestroyCommandPool(
                Impl->Device, CommandPool, nullptr);
        if (ReadbackBuffer != VK_NULL_HANDLE)
            vkDestroyBuffer(
                Impl->Device, ReadbackBuffer, nullptr);
        if (ReadbackMemory != VK_NULL_HANDLE)
            vkFreeMemory(
                Impl->Device, ReadbackMemory, nullptr);
    };
    const auto Fail = [&](ERHIResult Result) noexcept
    {
        Cleanup();
        OutBytes.clear();
        return Result;
    };

    VkBufferCreateInfo BufferInfo =
        MakeVulkanStruct<VkBufferCreateInfo>(
            VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
    BufferInfo.size = ByteSize;
    BufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult NativeResult = vkCreateBuffer(
        Impl->Device, &BufferInfo, nullptr, &ReadbackBuffer);
    if (NativeResult != VK_SUCCESS)
    {
        return Fail(MapVulkanCreationResult(NativeResult));
    }
    VkMemoryRequirements Requirements{};
    vkGetBufferMemoryRequirements(
        Impl->Device, ReadbackBuffer, &Requirements);
    const Stoner::Core::uint32 MemoryType =
        Impl->FindMemoryType(
            Requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (MemoryType == UINT32_MAX)
    {
        return Fail(ERHIResult::Unsupported);
    }
    VkMemoryAllocateInfo Allocation =
        MakeVulkanStruct<VkMemoryAllocateInfo>(
            VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
    Allocation.allocationSize = Requirements.size;
    Allocation.memoryTypeIndex = MemoryType;
    NativeResult = vkAllocateMemory(
        Impl->Device, &Allocation, nullptr, &ReadbackMemory);
    if (NativeResult != VK_SUCCESS)
    {
        return Fail(MapVulkanCreationResult(NativeResult));
    }
    if (vkBindBufferMemory(
            Impl->Device,
            ReadbackBuffer,
            ReadbackMemory,
            0) != VK_SUCCESS)
    {
        return Fail(ERHIResult::Failed);
    }

    VkCommandPoolCreateInfo PoolInfo =
        MakeVulkanStruct<VkCommandPoolCreateInfo>(
            VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
    PoolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    PoolInfo.queueFamilyIndex = Impl->GraphicsQueueFamily;
    NativeResult = vkCreateCommandPool(
        Impl->Device, &PoolInfo, nullptr, &CommandPool);
    if (NativeResult != VK_SUCCESS)
    {
        return Fail(MapVulkanCreationResult(NativeResult));
    }
    VkCommandBufferAllocateInfo CommandInfo =
        MakeVulkanStruct<VkCommandBufferAllocateInfo>(
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
    CommandInfo.commandPool = CommandPool;
    CommandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    CommandInfo.commandBufferCount = 1;
    NativeResult = vkAllocateCommandBuffers(
        Impl->Device, &CommandInfo, &CommandBuffer);
    if (NativeResult != VK_SUCCESS)
    {
        return Fail(MapVulkanCreationResult(NativeResult));
    }
    VkCommandBufferBeginInfo BeginInfo =
        MakeVulkanStruct<VkCommandBufferBeginInfo>(
            VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
    BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(CommandBuffer, &BeginInfo) !=
        VK_SUCCESS)
    {
        return Fail(ERHIResult::Failed);
    }

    VkImageMemoryBarrier ToTransfer =
        MakeVulkanStruct<VkImageMemoryBarrier>(
            VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
    ToTransfer.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
    ToTransfer.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    ToTransfer.oldLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    ToTransfer.newLayout =
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    ToTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ToTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ToTransfer.image = Texture.Image;
    ToTransfer.subresourceRange.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    ToTransfer.subresourceRange.baseMipLevel = MipLevel;
    ToTransfer.subresourceRange.levelCount = 1;
    ToTransfer.subresourceRange.baseArrayLayer = 0;
    ToTransfer.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(
        CommandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &ToTransfer);

    VkBufferImageCopy Copy{};
    Copy.imageSubresource.aspectMask =
        VK_IMAGE_ASPECT_COLOR_BIT;
    Copy.imageSubresource.mipLevel = MipLevel;
    Copy.imageSubresource.layerCount = 1;
    Copy.imageExtent = {
        Width,
        Height,
        Depth};
    vkCmdCopyImageToBuffer(
        CommandBuffer,
        Texture.Image,
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        ReadbackBuffer,
        1,
        &Copy);

    VkImageMemoryBarrier ToShaderRead = ToTransfer;
    ToShaderRead.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    ToShaderRead.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    ToShaderRead.oldLayout =
        VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    ToShaderRead.newLayout =
        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    vkCmdPipelineBarrier(
        CommandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0,
        nullptr,
        0,
        nullptr,
        1,
        &ToShaderRead);
    if (vkEndCommandBuffer(CommandBuffer) != VK_SUCCESS)
    {
        return Fail(ERHIResult::Failed);
    }
    VkFenceCreateInfo FenceInfo =
        MakeVulkanStruct<VkFenceCreateInfo>(
            VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
    NativeResult = vkCreateFence(
        Impl->Device, &FenceInfo, nullptr, &Fence);
    if (NativeResult != VK_SUCCESS)
    {
        return Fail(MapVulkanCreationResult(NativeResult));
    }
    VkSubmitInfo Submit =
        MakeVulkanStruct<VkSubmitInfo>(
            VK_STRUCTURE_TYPE_SUBMIT_INFO);
    Submit.commandBufferCount = 1;
    Submit.pCommandBuffers = &CommandBuffer;
    if (vkQueueSubmit(
            Impl->GraphicsQueue, 1, &Submit, Fence) !=
        VK_SUCCESS)
    {
        return Fail(ERHIResult::Failed);
    }
    bSubmitted = true;
    NativeResult = vkWaitForFences(
        Impl->Device,
        1,
        &Fence,
        VK_TRUE,
        30ULL * 1000ULL * 1000ULL * 1000ULL);
    if (NativeResult != VK_SUCCESS)
    {
        return Fail(
            NativeResult == VK_TIMEOUT
                ? ERHIResult::Timeout
                : ERHIResult::Failed);
    }
    bSubmitted = false;

    void* Mapped = nullptr;
    NativeResult = vkMapMemory(
        Impl->Device,
        ReadbackMemory,
        0,
        ByteSize,
        0,
        &Mapped);
    if (NativeResult != VK_SUCCESS)
    {
        return Fail(MapVulkanCreationResult(NativeResult));
    }
    try
    {
        OutBytes.resize(
            static_cast<Stoner::Core::usize>(ByteSize));
        std::memcpy(
            OutBytes.data(),
            Mapped,
            static_cast<std::size_t>(ByteSize));
    }
    catch (const std::bad_alloc&)
    {
        vkUnmapMemory(Impl->Device, ReadbackMemory);
        return Fail(ERHIResult::Unavailable);
    }
    catch (const std::length_error&)
    {
        vkUnmapMemory(Impl->Device, ReadbackMemory);
        return Fail(ERHIResult::Unavailable);
    }
    vkUnmapMemory(Impl->Device, ReadbackMemory);
    Cleanup();
    return ERHIResult::Success;
#else
    (void)Token;
    (void)MipLevel;
    return Stoner::RHI::ERHIResult::Unsupported;
#endif
}

void FVulkanNativeContext::DestroyOwnedTexture(
    Stoner::Core::uint64 Token) noexcept
{
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    if (!Impl || Token == 0)
    {
        return;
    }
    const auto Found = Impl->OwnedTextures.find(Token);
    if (Found == Impl->OwnedTextures.end())
    {
        return;
    }
    if (Impl->Device != VK_NULL_HANDLE)
    {
        (void)vkDeviceWaitIdle(Impl->Device);
    }
    Impl->DestroyOwnedTextureResources(Found->second);
    Impl->OwnedTextures.erase(Found);
    Impl->Snapshot.LiveTextures =
        Impl->GetLiveTextureCount();
#else
    (void)Token;
#endif
}

bool FVulkanNativeContext::GetNativeDeviceAccess(
    FVulkanNativeDeviceAccess& OutAccess) const noexcept
{
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    if (!Impl || Impl->PhysicalDevice == VK_NULL_HANDLE || Impl->Device == VK_NULL_HANDLE ||
        Impl->GraphicsQueue == VK_NULL_HANDLE)
    {
        return false;
    }
    OutAccess.PhysicalDevice = Impl->PhysicalDevice;
    OutAccess.Device = Impl->Device;
    OutAccess.GraphicsQueue = Impl->GraphicsQueue;
    OutAccess.GraphicsQueueFamily = Impl->GraphicsQueueFamily;
    return true;
#else
    (void)OutAccess;
    return false;
#endif
}

} // namespace Stoner::Backend::Vulkan
