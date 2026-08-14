#include "FVulkanNativeOffscreenSession.h"
#include "FVulkanNativeDeviceAccess.h"
#include "FVulkanStruct.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace Stoner::Backend::Vulkan
{

namespace
{

constexpr Stoner::Core::uint32 ValidationWidth = 32;
constexpr Stoner::Core::uint32 ValidationHeight = 32;

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE

constexpr Stoner::Core::uint64 CompletionTimeoutNanoseconds = 5'000'000'000ull;

float MaxAbsoluteError(const Stoner::Core::FVector4& Expected,
    const Stoner::Core::FVector4& Observed) noexcept
{
    return std::max({
        Stoner::Core::FMath::Abs(Expected.X - Observed.X),
        Stoner::Core::FMath::Abs(Expected.Y - Observed.Y),
        Stoner::Core::FMath::Abs(Expected.Z - Observed.Z),
        Stoner::Core::FMath::Abs(Expected.W - Observed.W)});
}

float NormalDot(const Stoner::Core::FVector4& Expected,
    const Stoner::Core::FVector4& Observed) noexcept
{
    return Stoner::Core::FVector3(Expected.X, Expected.Y, Expected.Z).GetSafeNormal().Dot(
        Stoner::Core::FVector3(Observed.X, Observed.Y, Observed.Z).GetSafeNormal());
}

bool IsFinite(const Stoner::Core::FVector4& Value) noexcept
{
    return Stoner::Core::FMath::IsFinite(Value.X) &&
        Stoner::Core::FMath::IsFinite(Value.Y) &&
        Stoner::Core::FMath::IsFinite(Value.Z) &&
        Stoner::Core::FMath::IsFinite(Value.W);
}

struct alignas(16) FNativeFrameUniform
{
    std::array<float, 16> View{};
    std::array<float, 16> Projection{};
    std::array<float, 16> InverseViewProjection{};
    std::array<float, 16> ViewProjection{};
    std::array<float, 4> CameraPosition{};
    std::array<float, 4> OutputExtent{};
    std::array<float, 4> DepthConvention{};
};

struct alignas(16) FNativeDrawUniform
{
    std::array<float, 16> Model{};
    std::array<float, 16> WorldNormalFromModel{};
    std::array<float, 4> BaseColorAO{};
    std::array<float, 4> EmissiveMetallic{};
    std::array<float, 4> RoughnessAlphaCutoffFlags{};
};

struct alignas(16) FNativeLightUniform
{
    std::array<float, 4> PositionRange{};
    std::array<float, 4> DirectionOuterCos{};
    std::array<float, 4> ColorIntensity{};
    std::array<float, 4> InnerCosTypeVolumeMode{};
};

static_assert(sizeof(FNativeFrameUniform) == 304);
static_assert(sizeof(FNativeDrawUniform) == 176);
static_assert(sizeof(FNativeLightUniform) == 64);

void SetIdentity(std::array<float, 16>& Matrix)
{
    Matrix = {};
    Matrix[0] = Matrix[5] = Matrix[10] = Matrix[15] = 1.0f;
}

float DecodeHalf(Stoner::Core::uint16 Value) noexcept
{
    const Stoner::Core::uint32 Sign = (Value & 0x8000u) << 16u;
    Stoner::Core::uint32 Exponent = (Value >> 10u) & 0x1fu;
    Stoner::Core::uint32 Mantissa = Value & 0x03ffu;
    Stoner::Core::uint32 Bits = 0;
    if (Exponent == 0)
    {
        if (Mantissa == 0)
        {
            Bits = Sign;
        }
        else
        {
            int Shift = 0;
            while ((Mantissa & 0x0400u) == 0)
            {
                Mantissa <<= 1u;
                ++Shift;
            }
            Mantissa &= 0x03ffu;
            Bits = Sign | static_cast<Stoner::Core::uint32>(127 - 15 - Shift) << 23u |
                Mantissa << 13u;
        }
    }
    else if (Exponent == 31)
    {
        Bits = Sign | 0x7f800000u | Mantissa << 13u;
    }
    else
    {
        Exponent += 127 - 15;
        Bits = Sign | Exponent << 23u | Mantissa << 13u;
    }
    float Result = 0.0f;
    std::memcpy(&Result, &Bits, sizeof(Result));
    return Result;
}

#endif

} // namespace

struct FVulkanNativeOffscreenSession::FImpl
{
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    struct FImage
    {
        VkImage Image = VK_NULL_HANDLE;
        VkDeviceMemory Memory = VK_NULL_HANDLE;
        VkImageView View = VK_NULL_HANDLE;
        VkFormat Format = VK_FORMAT_UNDEFINED;
        VkImageAspectFlags Aspect = 0;
        VkImageLayout ReadLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Stoner::Core::uint32 BytesPerPixel = 0;
    };

    struct FBuffer
    {
        VkBuffer Buffer = VK_NULL_HANDLE;
        VkDeviceMemory Memory = VK_NULL_HANDLE;
        VkDeviceSize Size = 0;
        void* Mapped = nullptr;
    };

    FVulkanNativeDeviceAccess Access;
    std::array<FImage, 6> Images;
    std::array<FBuffer, 6> Readbacks;
    FBuffer SurfaceVertices;
    FBuffer FullscreenVertices;
    FBuffer SphereVertices;
    FBuffer SphereIndices;
    FBuffer ConeVertices;
    FBuffer ConeIndices;
    FBuffer FrameUniform;
    FBuffer DrawUniform;
    FBuffer MaskedDrawUniform;
    FBuffer LightUniform;
    VkSampler Sampler = VK_NULL_HANDLE;
    std::array<VkDescriptorSetLayout, 4> SetLayouts{};
    VkDescriptorPool DescriptorPool = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, 4> DescriptorSets{};
    VkDescriptorSet MaskedDrawSet = VK_NULL_HANDLE;
    std::array<VkShaderModule, 9> Shaders{};
    VkPipelineLayout PipelineLayout = VK_NULL_HANDLE;
    VkRenderPass SurfacePass = VK_NULL_HANDLE;
    VkRenderPass LightingPass = VK_NULL_HANDLE;
    VkRenderPass CompositionPass = VK_NULL_HANDLE;
    VkFramebuffer SurfaceFramebuffer = VK_NULL_HANDLE;
    VkFramebuffer LightingFramebuffer = VK_NULL_HANDLE;
    VkFramebuffer CompositionFramebuffer = VK_NULL_HANDLE;
    std::array<VkPipeline, 9> Pipelines{};
    VkCommandPool CommandPool = VK_NULL_HANDLE;
    VkCommandBuffer CommandBuffer = VK_NULL_HANDLE;
    VkFence Fence = VK_NULL_HANDLE;
    Stoner::Core::uint32 LiveObjects = 0;
    Stoner::Core::uint32 PeakLiveObjects = 0;
    bool bSubmitted = false;

    void TrackCreate(Stoner::Core::uint32 Count = 1) noexcept
    {
        LiveObjects += Count;
        PeakLiveObjects = std::max(PeakLiveObjects, LiveObjects);
    }

    void TrackRelease(Stoner::Core::uint32 Count = 1) noexcept
    {
        LiveObjects = Count > LiveObjects ? 0 : LiveObjects - Count;
    }

    Stoner::Core::uint32 FindMemoryType(Stoner::Core::uint32 TypeBits,
        VkMemoryPropertyFlags Required) const noexcept
    {
        VkPhysicalDeviceMemoryProperties Properties{};
        vkGetPhysicalDeviceMemoryProperties(Access.PhysicalDevice, &Properties);
        for (Stoner::Core::uint32 Index = 0; Index < Properties.memoryTypeCount; ++Index)
        {
            if ((TypeBits & (1u << Index)) &&
                (Properties.memoryTypes[Index].propertyFlags & Required) == Required)
            {
                return Index;
            }
        }
        return UINT32_MAX;
    }

    bool CreateBuffer(VkDeviceSize Size, VkBufferUsageFlags Usage,
        VkMemoryPropertyFlags Properties, const void* InitialData, FBuffer& Out)
    {
        VkBufferCreateInfo Info = MakeVulkanStruct<VkBufferCreateInfo>(VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO);
        Info.size = Size;
        Info.usage = Usage;
        Info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(Access.Device, &Info, nullptr, &Out.Buffer) != VK_SUCCESS)
        {
            return false;
        }
        TrackCreate();
        VkMemoryRequirements Requirements{};
        vkGetBufferMemoryRequirements(Access.Device, Out.Buffer, &Requirements);
        const Stoner::Core::uint32 MemoryType =
            FindMemoryType(Requirements.memoryTypeBits, Properties);
        if (MemoryType == UINT32_MAX)
        {
            return false;
        }
        VkMemoryAllocateInfo Allocation = MakeVulkanStruct<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        Allocation.allocationSize = Requirements.size;
        Allocation.memoryTypeIndex = MemoryType;
        if (vkAllocateMemory(Access.Device, &Allocation, nullptr, &Out.Memory) != VK_SUCCESS)
        {
            return false;
        }
        TrackCreate();
        if (vkBindBufferMemory(Access.Device, Out.Buffer, Out.Memory, 0) != VK_SUCCESS)
        {
            return false;
        }
        Out.Size = Size;
        if (InitialData != nullptr)
        {
            void* Mapped = nullptr;
            if (vkMapMemory(Access.Device, Out.Memory, 0, Size, 0, &Mapped) != VK_SUCCESS)
            {
                return false;
            }
            std::memcpy(Mapped, InitialData, static_cast<std::size_t>(Size));
            vkUnmapMemory(Access.Device, Out.Memory);
        }
        return true;
    }

    bool CreateImage(VkFormat Format, VkImageUsageFlags Usage,
        VkImageAspectFlags Aspect, VkImageLayout ReadLayout,
        Stoner::Core::uint32 BytesPerPixel, FImage& Out)
    {
        VkImageCreateInfo Info = MakeVulkanStruct<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
        Info.imageType = VK_IMAGE_TYPE_2D;
        Info.format = Format;
        Info.extent = {ValidationWidth, ValidationHeight, 1};
        Info.mipLevels = 1;
        Info.arrayLayers = 1;
        Info.samples = VK_SAMPLE_COUNT_1_BIT;
        Info.tiling = VK_IMAGE_TILING_OPTIMAL;
        Info.usage = Usage | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        Info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        Info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        if (vkCreateImage(Access.Device, &Info, nullptr, &Out.Image) != VK_SUCCESS)
        {
            return false;
        }
        TrackCreate();
        VkMemoryRequirements Requirements{};
        vkGetImageMemoryRequirements(Access.Device, Out.Image, &Requirements);
        const Stoner::Core::uint32 MemoryType =
            FindMemoryType(Requirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        if (MemoryType == UINT32_MAX)
        {
            return false;
        }
        VkMemoryAllocateInfo Allocation = MakeVulkanStruct<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
        Allocation.allocationSize = Requirements.size;
        Allocation.memoryTypeIndex = MemoryType;
        if (vkAllocateMemory(Access.Device, &Allocation, nullptr, &Out.Memory) != VK_SUCCESS ||
            vkBindImageMemory(Access.Device, Out.Image, Out.Memory, 0) != VK_SUCCESS)
        {
            return false;
        }
        TrackCreate();
        VkImageViewCreateInfo ViewInfo = MakeVulkanStruct<VkImageViewCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO);
        ViewInfo.image = Out.Image;
        ViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        ViewInfo.format = Format;
        ViewInfo.subresourceRange.aspectMask = Aspect;
        ViewInfo.subresourceRange.levelCount = 1;
        ViewInfo.subresourceRange.layerCount = 1;
        if (vkCreateImageView(Access.Device, &ViewInfo, nullptr, &Out.View) != VK_SUCCESS)
        {
            return false;
        }
        TrackCreate();
        Out.Format = Format;
        Out.Aspect = Aspect;
        Out.ReadLayout = ReadLayout;
        Out.BytesPerPixel = BytesPerPixel;
        return true;
    }

    bool CreateShader(
        const Stoner::RHI::FRHIShaderModuleDesc& Desc,
        VkShaderModule& Out)
    {
        if (!Stoner::RHI::IsValidRHIShaderModuleDesc(Desc))
        {
            return false;
        }
        VkShaderModuleCreateInfo Info = MakeVulkanStruct<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
        Info.codeSize = Desc.Bytecode.Words.size() * sizeof(Stoner::Core::uint32);
        Info.pCode = Desc.Bytecode.Words.data();
        if (vkCreateShaderModule(Access.Device, &Info, nullptr, &Out) != VK_SUCCESS)
        {
            return false;
        }
        TrackCreate();
        return true;
    }

    void DestroyBuffer(FBuffer& Buffer)
    {
        if (Buffer.Mapped)
        {
            vkUnmapMemory(Access.Device, Buffer.Memory);
            Buffer.Mapped = nullptr;
        }
        if (Buffer.Buffer)
        {
            vkDestroyBuffer(Access.Device, Buffer.Buffer, nullptr);
            Buffer.Buffer = VK_NULL_HANDLE;
            TrackRelease();
        }
        if (Buffer.Memory)
        {
            vkFreeMemory(Access.Device, Buffer.Memory, nullptr);
            Buffer.Memory = VK_NULL_HANDLE;
            TrackRelease();
        }
        Buffer.Size = 0;
    }

    void DestroyImage(FImage& Image)
    {
        if (Image.View)
        {
            vkDestroyImageView(Access.Device, Image.View, nullptr);
            Image.View = VK_NULL_HANDLE;
            TrackRelease();
        }
        if (Image.Image)
        {
            vkDestroyImage(Access.Device, Image.Image, nullptr);
            Image.Image = VK_NULL_HANDLE;
            TrackRelease();
        }
        if (Image.Memory)
        {
            vkFreeMemory(Access.Device, Image.Memory, nullptr);
            Image.Memory = VK_NULL_HANDLE;
            TrackRelease();
        }
    }

    void ReleaseAll() noexcept
    {
        if (Access.Device == VK_NULL_HANDLE)
        {
            LiveObjects = 0;
            return;
        }
        if (bSubmitted)
        {
            if (Fence)
            {
                (void)vkWaitForFences(Access.Device, 1, &Fence, VK_TRUE,
                    CompletionTimeoutNanoseconds);
            }
            bSubmitted = false;
        }
        if (Fence)
        {
            vkDestroyFence(Access.Device, Fence, nullptr);
            Fence = VK_NULL_HANDLE;
            TrackRelease();
        }
        if (CommandPool)
        {
            vkDestroyCommandPool(Access.Device, CommandPool, nullptr);
            CommandPool = VK_NULL_HANDLE;
            CommandBuffer = VK_NULL_HANDLE;
            TrackRelease(2);
        }
        for (VkPipeline& Pipeline : Pipelines)
        {
            if (Pipeline)
            {
                vkDestroyPipeline(Access.Device, Pipeline, nullptr);
                Pipeline = VK_NULL_HANDLE;
                TrackRelease();
            }
        }
        if (CompositionFramebuffer)
        {
            vkDestroyFramebuffer(Access.Device, CompositionFramebuffer, nullptr);
            CompositionFramebuffer = VK_NULL_HANDLE;
            TrackRelease();
        }
        if (LightingFramebuffer)
        {
            vkDestroyFramebuffer(Access.Device, LightingFramebuffer, nullptr);
            LightingFramebuffer = VK_NULL_HANDLE;
            TrackRelease();
        }
        if (SurfaceFramebuffer)
        {
            vkDestroyFramebuffer(Access.Device, SurfaceFramebuffer, nullptr);
            SurfaceFramebuffer = VK_NULL_HANDLE;
            TrackRelease();
        }
        for (VkRenderPass* Pass : {&CompositionPass, &LightingPass, &SurfacePass})
        {
            if (*Pass)
            {
                vkDestroyRenderPass(Access.Device, *Pass, nullptr);
                *Pass = VK_NULL_HANDLE;
                TrackRelease();
            }
        }
        if (PipelineLayout)
        {
            vkDestroyPipelineLayout(Access.Device, PipelineLayout, nullptr);
            PipelineLayout = VK_NULL_HANDLE;
            TrackRelease();
        }
        for (VkShaderModule& Shader : Shaders)
        {
            if (Shader)
            {
                vkDestroyShaderModule(Access.Device, Shader, nullptr);
                Shader = VK_NULL_HANDLE;
                TrackRelease();
            }
        }
        if (DescriptorPool)
        {
            vkDestroyDescriptorPool(Access.Device, DescriptorPool, nullptr);
            DescriptorPool = VK_NULL_HANDLE;
            DescriptorSets = {};
            MaskedDrawSet = VK_NULL_HANDLE;
            TrackRelease(6);
        }
        for (VkDescriptorSetLayout& Layout : SetLayouts)
        {
            if (Layout)
            {
                vkDestroyDescriptorSetLayout(Access.Device, Layout, nullptr);
                Layout = VK_NULL_HANDLE;
                TrackRelease();
            }
        }
        if (Sampler)
        {
            vkDestroySampler(Access.Device, Sampler, nullptr);
            Sampler = VK_NULL_HANDLE;
            TrackRelease();
        }
        DestroyBuffer(LightUniform);
        DestroyBuffer(MaskedDrawUniform);
        DestroyBuffer(DrawUniform);
        DestroyBuffer(FrameUniform);
        DestroyBuffer(ConeIndices);
        DestroyBuffer(ConeVertices);
        DestroyBuffer(SphereIndices);
        DestroyBuffer(SphereVertices);
        DestroyBuffer(FullscreenVertices);
        DestroyBuffer(SurfaceVertices);
        for (FBuffer& Buffer : Readbacks)
        {
            DestroyBuffer(Buffer);
        }
        for (FImage& Image : Images)
        {
            DestroyImage(Image);
        }
    }
#endif
};

FVulkanNativeOffscreenSession::FVulkanNativeOffscreenSession(
    FVulkanNativeContext& InContext) noexcept
    : Context(InContext), Impl(std::make_unique<FImpl>())
{
}

FVulkanNativeOffscreenSession::~FVulkanNativeOffscreenSession()
{
    (void)Shutdown();
}

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE

namespace
{

bool CreateDescriptorState(FVulkanNativeOffscreenSession::FImpl& State)
{
    const VkShaderStageFlags Both =
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    const std::array<VkDescriptorSetLayoutBinding, 1> Set0 = {{
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, Both, nullptr}}};
    const std::array<VkDescriptorSetLayoutBinding, 1> Set1 = {{
        {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, Both, nullptr}}};
    const std::array<VkDescriptorSetLayoutBinding, 5> Set2 = {{
        {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr},
        {4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr}}};
    const std::array<VkDescriptorSetLayoutBinding, 1> Set3 = {{
        {0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, Both, nullptr}}};
    const auto CreateLayout = [&State](const auto& Bindings, VkDescriptorSetLayout& Out) {
        VkDescriptorSetLayoutCreateInfo Info = MakeVulkanStruct<VkDescriptorSetLayoutCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO);
        Info.bindingCount = static_cast<Stoner::Core::uint32>(Bindings.size());
        Info.pBindings = Bindings.data();
        if (vkCreateDescriptorSetLayout(State.Access.Device, &Info, nullptr, &Out) != VK_SUCCESS)
        {
            return false;
        }
        State.TrackCreate();
        return true;
    };
    if (!CreateLayout(Set0, State.SetLayouts[0]) ||
        !CreateLayout(Set1, State.SetLayouts[1]) ||
        !CreateLayout(Set2, State.SetLayouts[2]) ||
        !CreateLayout(Set3, State.SetLayouts[3]))
    {
        return false;
    }

    const std::array<VkDescriptorPoolSize, 3> PoolSizes = {{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}}};
    VkDescriptorPoolCreateInfo PoolInfo = MakeVulkanStruct<VkDescriptorPoolCreateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO);
    PoolInfo.maxSets = 5;
    PoolInfo.poolSizeCount = static_cast<Stoner::Core::uint32>(PoolSizes.size());
    PoolInfo.pPoolSizes = PoolSizes.data();
    if (vkCreateDescriptorPool(State.Access.Device, &PoolInfo, nullptr,
            &State.DescriptorPool) != VK_SUCCESS)
    {
        return false;
    }
    State.TrackCreate();
    VkDescriptorSetAllocateInfo Allocate = MakeVulkanStruct<VkDescriptorSetAllocateInfo>(VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO);
    Allocate.descriptorPool = State.DescriptorPool;
    Allocate.descriptorSetCount = 4;
    Allocate.pSetLayouts = State.SetLayouts.data();
    if (vkAllocateDescriptorSets(State.Access.Device, &Allocate,
            State.DescriptorSets.data()) != VK_SUCCESS)
    {
        return false;
    }
    State.TrackCreate(4);
    Allocate.descriptorSetCount = 1;
    Allocate.pSetLayouts = &State.SetLayouts[1];
    if (vkAllocateDescriptorSets(State.Access.Device, &Allocate,
            &State.MaskedDrawSet) != VK_SUCCESS)
    {
        return false;
    }
    State.TrackCreate();

    VkPipelineLayoutCreateInfo PipelineInfo = MakeVulkanStruct<VkPipelineLayoutCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO);
    PipelineInfo.setLayoutCount = 4;
    PipelineInfo.pSetLayouts = State.SetLayouts.data();
    if (vkCreatePipelineLayout(State.Access.Device, &PipelineInfo, nullptr,
            &State.PipelineLayout) != VK_SUCCESS)
    {
        return false;
    }
    State.TrackCreate();
    return true;
}

bool CreateRenderPasses(FVulkanNativeOffscreenSession::FImpl& State, bool bReversed)
{
    std::array<VkAttachmentDescription, 4> SurfaceAttachments{};
    const std::array<VkFormat, 4> SurfaceFormats = {
        VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_D32_SFLOAT};
    for (std::size_t Index = 0; Index < SurfaceAttachments.size(); ++Index)
    {
        auto& Attachment = SurfaceAttachments[Index];
        Attachment.format = SurfaceFormats[Index];
        Attachment.samples = VK_SAMPLE_COUNT_1_BIT;
        Attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        Attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        Attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        Attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        Attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        Attachment.finalLayout = Index == 3
            ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    const std::array<VkAttachmentReference, 3> Colors = {{
        {0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL},
        {2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}}};
    const VkAttachmentReference Depth{3, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription SurfaceSubpass{};
    SurfaceSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    SurfaceSubpass.colorAttachmentCount = 3;
    SurfaceSubpass.pColorAttachments = Colors.data();
    SurfaceSubpass.pDepthStencilAttachment = &Depth;
    std::array<VkSubpassDependency, 2> SurfaceDependencies{};
    SurfaceDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    SurfaceDependencies[0].dstSubpass = 0;
    SurfaceDependencies[0].srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    SurfaceDependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    SurfaceDependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    SurfaceDependencies[1].srcSubpass = 0;
    SurfaceDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    SurfaceDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    SurfaceDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    SurfaceDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    SurfaceDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    VkRenderPassCreateInfo SurfaceInfo = MakeVulkanStruct<VkRenderPassCreateInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
    SurfaceInfo.attachmentCount = 4;
    SurfaceInfo.pAttachments = SurfaceAttachments.data();
    SurfaceInfo.subpassCount = 1;
    SurfaceInfo.pSubpasses = &SurfaceSubpass;
    SurfaceInfo.dependencyCount =
        static_cast<Stoner::Core::uint32>(SurfaceDependencies.size());
    SurfaceInfo.pDependencies = SurfaceDependencies.data();
    if (vkCreateRenderPass(State.Access.Device, &SurfaceInfo, nullptr,
            &State.SurfacePass) != VK_SUCCESS)
    {
        return false;
    }
    State.TrackCreate();

    VkAttachmentDescription LightingAttachment{};
    LightingAttachment.format = VK_FORMAT_R16G16B16A16_SFLOAT;
    LightingAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    LightingAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    LightingAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    LightingAttachment.initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    LightingAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    const VkAttachmentReference LightingColor{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription LightingSubpass{};
    LightingSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    LightingSubpass.colorAttachmentCount = 1;
    LightingSubpass.pColorAttachments = &LightingColor;
    std::array<VkSubpassDependency, 2> LightingDependencies{};
    LightingDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    LightingDependencies[0].dstSubpass = 0;
    LightingDependencies[0].srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    LightingDependencies[0].dstStageMask =
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    LightingDependencies[0].srcAccessMask =
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    LightingDependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    LightingDependencies[1].srcSubpass = 0;
    LightingDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    LightingDependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    LightingDependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    LightingDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    LightingDependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    VkRenderPassCreateInfo LightingInfo = MakeVulkanStruct<VkRenderPassCreateInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
    LightingInfo.attachmentCount = 1;
    LightingInfo.pAttachments = &LightingAttachment;
    LightingInfo.subpassCount = 1;
    LightingInfo.pSubpasses = &LightingSubpass;
    LightingInfo.dependencyCount =
        static_cast<Stoner::Core::uint32>(LightingDependencies.size());
    LightingInfo.pDependencies = LightingDependencies.data();
    if (vkCreateRenderPass(State.Access.Device, &LightingInfo, nullptr,
            &State.LightingPass) != VK_SUCCESS)
    {
        return false;
    }
    State.TrackCreate();

    VkAttachmentDescription CompositionAttachment{};
    CompositionAttachment.format = VK_FORMAT_R8G8B8A8_UNORM;
    CompositionAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    CompositionAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    CompositionAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    CompositionAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    CompositionAttachment.finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    const VkAttachmentReference CompositionColor{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription CompositionSubpass{};
    CompositionSubpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    CompositionSubpass.colorAttachmentCount = 1;
    CompositionSubpass.pColorAttachments = &CompositionColor;
    std::array<VkSubpassDependency, 2> CompositionDependencies{};
    CompositionDependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
    CompositionDependencies[0].dstSubpass = 0;
    CompositionDependencies[0].srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    CompositionDependencies[0].dstStageMask =
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT |
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    CompositionDependencies[0].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    CompositionDependencies[0].dstAccessMask = VK_ACCESS_SHADER_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    CompositionDependencies[1].srcSubpass = 0;
    CompositionDependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
    CompositionDependencies[1].srcStageMask =
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    CompositionDependencies[1].dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    CompositionDependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    CompositionDependencies[1].dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    VkRenderPassCreateInfo CompositionInfo = MakeVulkanStruct<VkRenderPassCreateInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO);
    CompositionInfo.attachmentCount = 1;
    CompositionInfo.pAttachments = &CompositionAttachment;
    CompositionInfo.subpassCount = 1;
    CompositionInfo.pSubpasses = &CompositionSubpass;
    CompositionInfo.dependencyCount =
        static_cast<Stoner::Core::uint32>(CompositionDependencies.size());
    CompositionInfo.pDependencies = CompositionDependencies.data();
    if (vkCreateRenderPass(State.Access.Device, &CompositionInfo, nullptr,
            &State.CompositionPass) != VK_SUCCESS)
    {
        return false;
    }
    State.TrackCreate();
    (void)bReversed;
    return true;
}

bool CreateFramebuffers(FVulkanNativeOffscreenSession::FImpl& State)
{
    const std::array<VkImageView, 4> SurfaceViews = {
        State.Images[0].View, State.Images[1].View, State.Images[2].View,
        State.Images[3].View};
    VkFramebufferCreateInfo Info = MakeVulkanStruct<VkFramebufferCreateInfo>(VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO);
    Info.renderPass = State.SurfacePass;
    Info.attachmentCount = 4;
    Info.pAttachments = SurfaceViews.data();
    Info.width = ValidationWidth;
    Info.height = ValidationHeight;
    Info.layers = 1;
    if (vkCreateFramebuffer(State.Access.Device, &Info, nullptr,
            &State.SurfaceFramebuffer) != VK_SUCCESS)
    {
        return false;
    }
    State.TrackCreate();
    Info.renderPass = State.LightingPass;
    Info.attachmentCount = 1;
    Info.pAttachments = &State.Images[4].View;
    if (vkCreateFramebuffer(State.Access.Device, &Info, nullptr,
            &State.LightingFramebuffer) != VK_SUCCESS)
    {
        return false;
    }
    State.TrackCreate();
    Info.renderPass = State.CompositionPass;
    Info.pAttachments = &State.Images[5].View;
    if (vkCreateFramebuffer(State.Access.Device, &Info, nullptr,
            &State.CompositionFramebuffer) != VK_SUCCESS)
    {
        return false;
    }
    State.TrackCreate();
    return true;
}

bool CreateGraphicsPipeline(FVulkanNativeOffscreenSession::FImpl& State,
    VkShaderModule Vertex, VkShaderModule Fragment, VkRenderPass RenderPass,
    const VkVertexInputBindingDescription* Binding,
    const VkVertexInputAttributeDescription* Attributes,
    Stoner::Core::uint32 AttributeCount, bool bDepth, bool bReversed,
    bool bAdditive, VkCullModeFlags CullMode, VkPipeline& Out)
{
    const std::array<VkPipelineShaderStageCreateInfo, 2> Stages = {{
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
            VK_SHADER_STAGE_VERTEX_BIT, Vertex, "main", nullptr},
        {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0,
            VK_SHADER_STAGE_FRAGMENT_BIT, Fragment, "main", nullptr}}};
    VkPipelineVertexInputStateCreateInfo VertexInput = MakeVulkanStruct<VkPipelineVertexInputStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO);
    if (Binding)
    {
        VertexInput.vertexBindingDescriptionCount = 1;
        VertexInput.pVertexBindingDescriptions = Binding;
    }
    VertexInput.vertexAttributeDescriptionCount = AttributeCount;
    VertexInput.pVertexAttributeDescriptions = Attributes;
    VkPipelineInputAssemblyStateCreateInfo Assembly = MakeVulkanStruct<VkPipelineInputAssemblyStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO);
    Assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo Viewport = MakeVulkanStruct<VkPipelineViewportStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO);
    Viewport.viewportCount = 1;
    Viewport.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo Rasterizer = MakeVulkanStruct<VkPipelineRasterizationStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO);
    Rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    Rasterizer.cullMode = CullMode;
    Rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    Rasterizer.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo Multisample = MakeVulkanStruct<VkPipelineMultisampleStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO);
    Multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo Depth = MakeVulkanStruct<VkPipelineDepthStencilStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO);
    Depth.depthTestEnable = bDepth ? VK_TRUE : VK_FALSE;
    Depth.depthWriteEnable = bDepth ? VK_TRUE : VK_FALSE;
    Depth.depthCompareOp = bReversed ? VK_COMPARE_OP_GREATER_OR_EQUAL
                                    : VK_COMPARE_OP_LESS_OR_EQUAL;
    std::array<VkPipelineColorBlendAttachmentState, 3> BlendAttachments{};
    const Stoner::Core::uint32 ColorCount = RenderPass == State.SurfacePass ? 3 : 1;
    for (Stoner::Core::uint32 Index = 0; Index < ColorCount; ++Index)
    {
        BlendAttachments[Index].colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        BlendAttachments[Index].blendEnable = bAdditive ? VK_TRUE : VK_FALSE;
        BlendAttachments[Index].srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        BlendAttachments[Index].dstColorBlendFactor = VK_BLEND_FACTOR_ONE;
        BlendAttachments[Index].colorBlendOp = VK_BLEND_OP_ADD;
        BlendAttachments[Index].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        BlendAttachments[Index].dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        BlendAttachments[Index].alphaBlendOp = VK_BLEND_OP_ADD;
    }
    VkPipelineColorBlendStateCreateInfo Blend = MakeVulkanStruct<VkPipelineColorBlendStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO);
    Blend.attachmentCount = ColorCount;
    Blend.pAttachments = BlendAttachments.data();
    const std::array<VkDynamicState, 2> Dynamics = {
        VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo Dynamic = MakeVulkanStruct<VkPipelineDynamicStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO);
    Dynamic.dynamicStateCount = 2;
    Dynamic.pDynamicStates = Dynamics.data();
    VkGraphicsPipelineCreateInfo Info = MakeVulkanStruct<VkGraphicsPipelineCreateInfo>(VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO);
    Info.stageCount = 2;
    Info.pStages = Stages.data();
    Info.pVertexInputState = &VertexInput;
    Info.pInputAssemblyState = &Assembly;
    Info.pViewportState = &Viewport;
    Info.pRasterizationState = &Rasterizer;
    Info.pMultisampleState = &Multisample;
    Info.pDepthStencilState = &Depth;
    Info.pColorBlendState = &Blend;
    Info.pDynamicState = &Dynamic;
    Info.layout = State.PipelineLayout;
    Info.renderPass = RenderPass;
    if (vkCreateGraphicsPipelines(State.Access.Device, VK_NULL_HANDLE, 1, &Info,
            nullptr, &Out) != VK_SUCCESS)
    {
        return false;
    }
    State.TrackCreate();
    return true;
}

void UpdateDescriptors(FVulkanNativeOffscreenSession::FImpl& State)
{
    const VkDescriptorBufferInfo FrameInfo{
        State.FrameUniform.Buffer, 0, State.FrameUniform.Size};
    const VkDescriptorBufferInfo DrawInfo{
        State.DrawUniform.Buffer, 0, State.DrawUniform.Size};
    const VkDescriptorBufferInfo MaskedDrawInfo{
        State.MaskedDrawUniform.Buffer, 0, State.MaskedDrawUniform.Size};
    const VkDescriptorBufferInfo LightInfo{
        State.LightUniform.Buffer, 0, State.LightUniform.Size};
    std::array<VkDescriptorImageInfo, 5> ImageInfos{};
    for (std::size_t Index = 0; Index < ImageInfos.size(); ++Index)
    {
        ImageInfos[Index].sampler = State.Sampler;
        ImageInfos[Index].imageView = State.Images[Index].View;
        ImageInfos[Index].imageLayout = Index == 3
            ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
            : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    }
    std::array<VkWriteDescriptorSet, 9> Writes{};
    Writes[0] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        State.DescriptorSets[0], 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        nullptr, &FrameInfo, nullptr};
    Writes[1] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        State.DescriptorSets[1], 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        nullptr, &DrawInfo, nullptr};
    Writes[2] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        State.MaskedDrawSet, 0, 0, 1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        nullptr, &MaskedDrawInfo, nullptr};
    for (Stoner::Core::uint32 Index = 0; Index < 5; ++Index)
    {
        Writes[3 + Index] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
            State.DescriptorSets[2], Index, 0, 1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, &ImageInfos[Index],
            nullptr, nullptr};
    }
    Writes[8] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
        State.DescriptorSets[3], 0, 0, 1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
        nullptr, &LightInfo, nullptr};
    vkUpdateDescriptorSets(State.Access.Device,
        static_cast<Stoner::Core::uint32>(Writes.size()), Writes.data(), 0, nullptr);
}

void BindSets(VkCommandBuffer Commands,
    const FVulkanNativeOffscreenSession::FImpl& State,
    std::initializer_list<Stoner::Core::uint32> Sets)
{
    for (Stoner::Core::uint32 Set : Sets)
    {
        vkCmdBindDescriptorSets(Commands, VK_PIPELINE_BIND_POINT_GRAPHICS,
            State.PipelineLayout, Set, 1, &State.DescriptorSets[Set], 0, nullptr);
    }
}

void TransitionForCopy(VkCommandBuffer Commands,
    const FVulkanNativeOffscreenSession::FImpl::FImage& Image,
    VkImageLayout OldLayout)
{
    VkImageMemoryBarrier Barrier = MakeVulkanStruct<VkImageMemoryBarrier>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
    Barrier.srcAccessMask = OldLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL
        ? VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT
        : VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;
    Barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    Barrier.oldLayout = OldLayout;
    Barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.image = Image.Image;
    Barrier.subresourceRange.aspectMask = Image.Aspect;
    Barrier.subresourceRange.levelCount = 1;
    Barrier.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(Commands,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
            VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &Barrier);
}

void TransitionToColorAttachment(VkCommandBuffer Commands,
    const FVulkanNativeOffscreenSession::FImpl::FImage& Image,
    VkImageLayout OldLayout)
{
    VkImageMemoryBarrier Barrier = MakeVulkanStruct<VkImageMemoryBarrier>(VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER);
    Barrier.srcAccessMask = OldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
        ? VK_ACCESS_TRANSFER_READ_BIT
        : 0;
    Barrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
        VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    Barrier.oldLayout = OldLayout;
    Barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    Barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    Barrier.image = Image.Image;
    Barrier.subresourceRange.aspectMask = Image.Aspect;
    Barrier.subresourceRange.levelCount = 1;
    Barrier.subresourceRange.layerCount = 1;
    const VkPipelineStageFlags SrcStage =
        OldLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL
            ? VK_PIPELINE_STAGE_TRANSFER_BIT
            : VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    vkCmdPipelineBarrier(Commands, SrcStage,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, 0, 0, nullptr, 0,
        nullptr, 1, &Barrier);
}

Stoner::Core::FVector4 ReadPixel(
    const FVulkanNativeOffscreenSession::FImpl::FBuffer& Readback,
    VkFormat Format, Stoner::Core::uint32 X, Stoner::Core::uint32 Y)
{
    if (Readback.Mapped == nullptr || X >= ValidationWidth || Y >= ValidationHeight)
    {
        return {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0.0f};
    }
    const std::size_t PixelIndex =
        static_cast<std::size_t>(Y) * ValidationWidth + X;
    const auto* Bytes = static_cast<const Stoner::Core::uint8*>(Readback.Mapped);
    if (Format == VK_FORMAT_R8G8B8A8_UNORM)
    {
        const auto* Pixel = Bytes + PixelIndex * 4;
        return {Pixel[0] / 255.0f, Pixel[1] / 255.0f,
            Pixel[2] / 255.0f, Pixel[3] / 255.0f};
    }
    if (Format == VK_FORMAT_R16G16B16A16_SFLOAT)
    {
        const auto* Pixel = reinterpret_cast<const Stoner::Core::uint16*>(
            Bytes + PixelIndex * 8);
        return {DecodeHalf(Pixel[0]), DecodeHalf(Pixel[1]),
            DecodeHalf(Pixel[2]), DecodeHalf(Pixel[3])};
    }
    if (Format == VK_FORMAT_D32_SFLOAT)
    {
        float Depth = 0.0f;
        std::memcpy(&Depth, Bytes + PixelIndex * 4, sizeof(Depth));
        return {Depth, 0.0f, 0.0f, 0.0f};
    }
    return {std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0.0f};
}

} // namespace

#endif

Stoner::RHI::ERHIResult FVulkanNativeOffscreenSession::Execute(
    const FVulkanDeferredShaderSet& Shaders,
    FVulkanDeferredValidationReport& OutReport,
    EVulkanDeferredFailurePoint FailurePoint,
    const FVulkanDeferredUniformPayload* UniformPayload)
{
    OutReport = {};
    OutReport.InjectedFailure = FailurePoint;
    if (bShutdown || !Context.IsAvailable())
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
#if !defined(STONER_VULKAN_NATIVE_AVAILABLE) || !STONER_VULKAN_NATIVE_AVAILABLE
    (void)Shaders;
    (void)UniformPayload;
    return Stoner::RHI::ERHIResult::Unsupported;
#else
    if (!Context.GetNativeDeviceAccess(Impl->Access))
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    const auto& Snapshot = Context.GetSnapshot();
    OutReport.RuntimeMode = "RealRuntime";
    OutReport.AdapterIdentity = Snapshot.AdapterName;
    OutReport.bSoftwareDevice = Snapshot.bSoftwareDevice;
    OutReport.ReferencePath = "NativeDeferredReadback";
    const auto FailInjected = [&](const char* Stage) {
        OutReport.PrimaryFailureStage = Stage;
        OutReport.PeakLiveObjects = Impl->PeakLiveObjects;
        Impl->ReleaseAll();
        OutReport.FinalLiveObjects = Impl->LiveObjects;
        OutReport.bPassed = false;
        return Stoner::RHI::ERHIResult::Failed;
    };

    const std::array<const Stoner::RHI::FRHIShaderModuleDesc*, 9>
        ShaderDescriptions = {
            &Shaders.SurfaceVertex,
            &Shaders.SurfaceFragment,
            &Shaders.FullscreenVertex,
            &Shaders.DirectionalFragment,
            &Shaders.PointVertex,
            &Shaders.PointFragment,
            &Shaders.SpotVertex,
            &Shaders.SpotFragment,
            &Shaders.CompositionFragment};
    for (std::size_t Index = 0;
         Index < ShaderDescriptions.size();
         ++Index)
    {
        if (!Impl->CreateShader(*ShaderDescriptions[Index],
                Impl->Shaders[Index]))
        {
            Impl->ReleaseAll();
            return Stoner::RHI::ERHIResult::Failed;
        }
    }
    if (FailurePoint == EVulkanDeferredFailurePoint::PartialInitialization)
    {
        return FailInjected("PartialInitialization");
    }

    const VkImageUsageFlags SampledColor =
        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    if (!Impl->CreateImage(VK_FORMAT_R8G8B8A8_UNORM, SampledColor,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 4,
            Impl->Images[0]) ||
        !Impl->CreateImage(VK_FORMAT_R16G16B16A16_SFLOAT, SampledColor,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 8,
            Impl->Images[1]) ||
        !Impl->CreateImage(VK_FORMAT_R16G16B16A16_SFLOAT, SampledColor,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 8,
            Impl->Images[2]) ||
        !Impl->CreateImage(VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, 4, Impl->Images[3]) ||
        !Impl->CreateImage(VK_FORMAT_R16G16B16A16_SFLOAT, SampledColor,
            VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, 8,
            Impl->Images[4]) ||
        !Impl->CreateImage(VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, 4, Impl->Images[5]))
    {
        Impl->ReleaseAll();
        return Stoner::RHI::ERHIResult::Failed;
    }
    for (std::size_t Index = 0; Index < Impl->Readbacks.size(); ++Index)
    {
        const VkDeviceSize Size = ValidationWidth * ValidationHeight *
            Impl->Images[Index].BytesPerPixel;
        if (!Impl->CreateBuffer(Size, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                nullptr, Impl->Readbacks[Index]))
        {
            Impl->ReleaseAll();
            return Stoner::RHI::ERHIResult::Failed;
        }
    }

    const std::array<float, 18> SurfaceVertices = {
        -0.8f, -0.8f, 0.5f, 0.0f, 0.0f, 1.0f,
         0.8f, -0.8f, 0.5f, 0.0f, 0.0f, 1.0f,
         0.0f,  0.8f, 0.5f, 0.0f, 0.0f, 1.0f};
    const std::array<float, 6> FullscreenVertices = {
        -1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
    const std::array<float, 18> SphereVertices = {
         1.0f,  0.0f,  0.0f, -1.0f,  0.0f,  0.0f,
         0.0f,  1.0f,  0.0f,  0.0f, -1.0f,  0.0f,
         0.0f,  0.0f,  1.0f,  0.0f,  0.0f, -1.0f};
    const std::array<Stoner::Core::uint16, 24> SphereIndices = {
        4, 2, 0, 4, 1, 2, 4, 3, 1, 4, 0, 3,
        5, 0, 2, 5, 2, 1, 5, 1, 3, 5, 3, 0};
    constexpr float ConeRadius = 0.5463025f;
    const std::array<float, 15> ConeVertices = {
        0.0f, 0.0f, 0.0f,
        -ConeRadius, -ConeRadius, -1.0f,
         ConeRadius, -ConeRadius, -1.0f,
         ConeRadius,  ConeRadius, -1.0f,
        -ConeRadius,  ConeRadius, -1.0f};
    const std::array<Stoner::Core::uint16, 18> ConeIndices = {
        0, 2, 1, 0, 3, 2, 0, 4, 3, 0, 1, 4,
        1, 3, 4, 1, 2, 3};
    if (!Impl->CreateBuffer(sizeof(SurfaceVertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            SurfaceVertices.data(), Impl->SurfaceVertices) ||
        !Impl->CreateBuffer(sizeof(FullscreenVertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            FullscreenVertices.data(), Impl->FullscreenVertices) ||
        !Impl->CreateBuffer(sizeof(SphereVertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            SphereVertices.data(), Impl->SphereVertices) ||
        !Impl->CreateBuffer(sizeof(SphereIndices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            SphereIndices.data(), Impl->SphereIndices) ||
        !Impl->CreateBuffer(sizeof(ConeVertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            ConeVertices.data(), Impl->ConeVertices) ||
        !Impl->CreateBuffer(sizeof(ConeIndices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            ConeIndices.data(), Impl->ConeIndices))
    {
        Impl->ReleaseAll();
        return Stoner::RHI::ERHIResult::Failed;
    }

    FNativeFrameUniform Frame{};
    SetIdentity(Frame.View);
    SetIdentity(Frame.Projection);
    SetIdentity(Frame.InverseViewProjection);
    SetIdentity(Frame.ViewProjection);
    Frame.CameraPosition = {0.0f, 0.0f, 0.75f, 0.0f};
    Frame.OutputExtent = {static_cast<float>(ValidationWidth),
        static_cast<float>(ValidationHeight), 1.0f / ValidationWidth,
        1.0f / ValidationHeight};
    FNativeDrawUniform Draw{};
    SetIdentity(Draw.Model);
    SetIdentity(Draw.WorldNormalFromModel);
    Draw.BaseColorAO = {0.8f, 0.2f, 0.1f, 0.75f};
    Draw.EmissiveMetallic = {0.3f, 0.05f, 0.0f, 0.65f};
    Draw.RoughnessAlphaCutoffFlags = {0.42f, 0.5f, 0.0f, 0.0f};
    if (UniformPayload != nullptr)
    {
        std::memcpy(&Frame, UniformPayload->FrameBytes.data(), sizeof(Frame));
        std::memcpy(&Draw, UniformPayload->DrawBytes.data(), sizeof(Draw));
    }
    FNativeDrawUniform MaskedDraw = Draw;
    MaskedDraw.Model[0] = 0.25f;
    MaskedDraw.Model[5] = 0.25f;
    MaskedDraw.Model[12] = 0.65f;
    MaskedDraw.Model[13] = 0.65f;
    MaskedDraw.RoughnessAlphaCutoffFlags = {0.42f, 0.25f, 0.5f, 1.0f};
    std::array<FNativeLightUniform, 7> Lights{};
    Lights[0].DirectionOuterCos = {0.0f, 0.0f, -1.0f, 0.0f};
    Lights[0].ColorIntensity = {1.0f, 1.0f, 1.0f, 0.25f};
    Lights[1].PositionRange = {0.0f, 0.0f, 0.75f, 0.75f};
    Lights[1].ColorIntensity = {1.0f, 0.0f, 0.0f, 0.4f};
    Lights[2].PositionRange = {0.0f, 0.0f, 0.75f, 0.75f};
    Lights[2].DirectionOuterCos = {0.0f, 0.0f, -1.0f, std::cos(0.5f)};
    Lights[2].ColorIntensity = {0.0f, 1.0f, 0.0f, 0.4f};
    Lights[2].InnerCosTypeVolumeMode = {std::cos(0.35f), 2.0f, 0.0f, 0.0f};
    Lights[3].PositionRange = {4.0f, 0.0f, 0.75f, 0.5f};
    Lights[3].ColorIntensity = {1.0f, 1.0f, 0.0f, 0.3f};
    Lights[4].PositionRange = {0.0f, 0.0f, 0.75f, 0.75f};
    Lights[4].ColorIntensity = {0.0f, 0.0f, 1.0f, 0.15f};
    Lights[5].PositionRange = {0.0f, 0.0f, 0.75f, 0.75f};
    Lights[5].DirectionOuterCos = {0.0f, 0.0f, 1.0f, std::cos(0.5f)};
    Lights[5].ColorIntensity = {1.0f, 1.0f, 0.0f, 0.4f};
    Lights[5].InnerCosTypeVolumeMode = {std::cos(0.35f), 2.0f, 0.0f, 0.0f};
    Lights[6].PositionRange = {0.0f, 0.0f, 0.75f, 3.0f};
    Lights[6].DirectionOuterCos = {0.0f, 0.0f, -1.0f, std::cos(0.5f)};
    Lights[6].ColorIntensity = {1.0f, 0.0f, 1.0f, 0.12f};
    Lights[6].InnerCosTypeVolumeMode = {std::cos(0.35f), 2.0f, 0.0f, 0.0f};
    if (!Impl->CreateBuffer(sizeof(Frame), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &Frame, Impl->FrameUniform) ||
        !Impl->CreateBuffer(sizeof(Draw), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &Draw, Impl->DrawUniform) ||
        !Impl->CreateBuffer(sizeof(MaskedDraw), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &MaskedDraw, Impl->MaskedDrawUniform) ||
        !Impl->CreateBuffer(sizeof(Lights), VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            Lights.data(), Impl->LightUniform))
    {
        Impl->ReleaseAll();
        return Stoner::RHI::ERHIResult::Failed;
    }

    VkSamplerCreateInfo SamplerInfo = MakeVulkanStruct<VkSamplerCreateInfo>(VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO);
    SamplerInfo.magFilter = VK_FILTER_NEAREST;
    SamplerInfo.minFilter = VK_FILTER_NEAREST;
    SamplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    SamplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    SamplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    if (vkCreateSampler(Impl->Access.Device, &SamplerInfo, nullptr,
            &Impl->Sampler) != VK_SUCCESS)
    {
        Impl->ReleaseAll();
        return Stoner::RHI::ERHIResult::Failed;
    }
    Impl->TrackCreate();
    if (!CreateDescriptorState(*Impl) || !CreateRenderPasses(*Impl, false) ||
        !CreateFramebuffers(*Impl))
    {
        Impl->ReleaseAll();
        return Stoner::RHI::ERHIResult::Failed;
    }
    UpdateDescriptors(*Impl);

    const VkVertexInputBindingDescription SurfaceBinding{0, 24,
        VK_VERTEX_INPUT_RATE_VERTEX};
    const std::array<VkVertexInputAttributeDescription, 2> SurfaceAttributes = {{
        {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32_SFLOAT, 12}}};
    const VkVertexInputBindingDescription FullscreenBinding{0, 8,
        VK_VERTEX_INPUT_RATE_VERTEX};
    const VkVertexInputAttributeDescription FullscreenAttribute{
        0, 0, VK_FORMAT_R32G32_SFLOAT, 0};
    const VkVertexInputBindingDescription VolumeBinding{0, 12,
        VK_VERTEX_INPUT_RATE_VERTEX};
    const VkVertexInputAttributeDescription VolumeAttribute{
        0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0};
    if (!CreateGraphicsPipeline(*Impl, Impl->Shaders[0], Impl->Shaders[1],
            Impl->SurfacePass, &SurfaceBinding, SurfaceAttributes.data(), 2,
            true, false, false, VK_CULL_MODE_NONE, Impl->Pipelines[0]) ||
        !CreateGraphicsPipeline(*Impl, Impl->Shaders[2], Impl->Shaders[3],
            Impl->LightingPass, &FullscreenBinding, &FullscreenAttribute, 1,
            false, false, true, VK_CULL_MODE_NONE, Impl->Pipelines[1]) ||
        !CreateGraphicsPipeline(*Impl, Impl->Shaders[4], Impl->Shaders[5],
            Impl->LightingPass, &VolumeBinding, &VolumeAttribute, 1,
            false, false, true, VK_CULL_MODE_BACK_BIT, Impl->Pipelines[2]) ||
        !CreateGraphicsPipeline(*Impl, Impl->Shaders[4], Impl->Shaders[5],
            Impl->LightingPass, &VolumeBinding, &VolumeAttribute, 1,
            false, false, true, VK_CULL_MODE_FRONT_BIT, Impl->Pipelines[3]) ||
        !CreateGraphicsPipeline(*Impl, Impl->Shaders[4], Impl->Shaders[5],
            Impl->LightingPass, &VolumeBinding, &VolumeAttribute, 1,
            false, false, true, VK_CULL_MODE_NONE, Impl->Pipelines[8]) ||
        !CreateGraphicsPipeline(*Impl, Impl->Shaders[6], Impl->Shaders[7],
            Impl->LightingPass, &VolumeBinding, &VolumeAttribute, 1,
            false, false, true, VK_CULL_MODE_BACK_BIT, Impl->Pipelines[4]) ||
        !CreateGraphicsPipeline(*Impl, Impl->Shaders[6], Impl->Shaders[7],
            Impl->LightingPass, &VolumeBinding, &VolumeAttribute, 1,
            false, false, true, VK_CULL_MODE_FRONT_BIT, Impl->Pipelines[5]) ||
        !CreateGraphicsPipeline(*Impl, Impl->Shaders[2], Impl->Shaders[8],
            Impl->CompositionPass, &FullscreenBinding, &FullscreenAttribute, 1,
            false, false, false, VK_CULL_MODE_NONE, Impl->Pipelines[6]) ||
        !CreateGraphicsPipeline(*Impl, Impl->Shaders[0], Impl->Shaders[1],
            Impl->SurfacePass, &SurfaceBinding, SurfaceAttributes.data(), 2,
            true, true, false, VK_CULL_MODE_NONE, Impl->Pipelines[7]))
    {
        Impl->ReleaseAll();
        return Stoner::RHI::ERHIResult::Failed;
    }

    VkCommandPoolCreateInfo PoolInfo = MakeVulkanStruct<VkCommandPoolCreateInfo>(VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO);
    PoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    PoolInfo.queueFamilyIndex = Impl->Access.GraphicsQueueFamily;
    if (vkCreateCommandPool(Impl->Access.Device, &PoolInfo, nullptr,
            &Impl->CommandPool) != VK_SUCCESS)
    {
        Impl->ReleaseAll();
        return Stoner::RHI::ERHIResult::Failed;
    }
    Impl->TrackCreate();
    VkCommandBufferAllocateInfo Allocate = MakeVulkanStruct<VkCommandBufferAllocateInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO);
    Allocate.commandPool = Impl->CommandPool;
    Allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    Allocate.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(Impl->Access.Device, &Allocate,
            &Impl->CommandBuffer) != VK_SUCCESS)
    {
        Impl->ReleaseAll();
        return Stoner::RHI::ERHIResult::Failed;
    }
    Impl->TrackCreate();
    VkFenceCreateInfo FenceInfo = MakeVulkanStruct<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
    if (vkCreateFence(Impl->Access.Device, &FenceInfo, nullptr, &Impl->Fence) != VK_SUCCESS)
    {
        Impl->ReleaseAll();
        return Stoner::RHI::ERHIResult::Failed;
    }
    Impl->TrackCreate();

    VkImageLayout LightingImageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    auto ExecuteConvention = [&](const char* Convention, bool bReversed) {
        if (FailurePoint == EVulkanDeferredFailurePoint::Record)
        {
            OutReport.PrimaryFailureStage = "Record";
            return false;
        }
        Frame.DepthConvention = {0.1f, 100.0f, bReversed ? 1.0f : 0.0f, 0.0f};
        void* FrameMapped = nullptr;
        if (vkMapMemory(Impl->Access.Device, Impl->FrameUniform.Memory, 0,
                sizeof(Frame), 0, &FrameMapped) != VK_SUCCESS)
        {
            return false;
        }
        std::memcpy(FrameMapped, &Frame, sizeof(Frame));
        vkUnmapMemory(Impl->Access.Device, Impl->FrameUniform.Memory);
        if (vkResetFences(Impl->Access.Device, 1, &Impl->Fence) != VK_SUCCESS ||
            vkResetCommandBuffer(Impl->CommandBuffer, 0) != VK_SUCCESS)
        {
            return false;
        }
        VkCommandBufferBeginInfo Begin = MakeVulkanStruct<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
        if (vkBeginCommandBuffer(Impl->CommandBuffer, &Begin) != VK_SUCCESS)
        {
            return false;
        }
        const VkViewport Viewport{0.0f, 0.0f,
            static_cast<float>(ValidationWidth), static_cast<float>(ValidationHeight),
            0.0f, 1.0f};
        const VkRect2D Scissor{{0, 0}, {ValidationWidth, ValidationHeight}};
        std::array<VkClearValue, 4> SurfaceClears{};
        SurfaceClears[3].depthStencil.depth = bReversed ? 0.0f : 1.0f;
        VkRenderPassBeginInfo PassBegin = MakeVulkanStruct<VkRenderPassBeginInfo>(VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO);
        PassBegin.renderPass = Impl->SurfacePass;
        PassBegin.framebuffer = Impl->SurfaceFramebuffer;
        PassBegin.renderArea.extent = {ValidationWidth, ValidationHeight};
        PassBegin.clearValueCount = 4;
        PassBegin.pClearValues = SurfaceClears.data();
        vkCmdBeginRenderPass(Impl->CommandBuffer, &PassBegin,
            VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(Impl->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            bReversed ? Impl->Pipelines[7] : Impl->Pipelines[0]);
        BindSets(Impl->CommandBuffer, *Impl, {0, 1});
        VkDeviceSize Offset = 0;
        vkCmdBindVertexBuffers(Impl->CommandBuffer, 0, 1,
            &Impl->SurfaceVertices.Buffer, &Offset);
        vkCmdSetViewport(Impl->CommandBuffer, 0, 1, &Viewport);
        vkCmdSetScissor(Impl->CommandBuffer, 0, 1, &Scissor);
        vkCmdDraw(Impl->CommandBuffer, 3, 1, 0, 0);
        vkCmdBindDescriptorSets(Impl->CommandBuffer,
            VK_PIPELINE_BIND_POINT_GRAPHICS, Impl->PipelineLayout, 1, 1,
            &Impl->MaskedDrawSet, 0, nullptr);
        vkCmdDraw(Impl->CommandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(Impl->CommandBuffer);

        VkClearValue LightingClear{};
        TransitionToColorAttachment(Impl->CommandBuffer, Impl->Images[4],
            LightingImageLayout);
        PassBegin.renderPass = Impl->LightingPass;
        PassBegin.framebuffer = Impl->LightingFramebuffer;
        PassBegin.clearValueCount = 1;
        PassBegin.pClearValues = &LightingClear;
        vkCmdBeginRenderPass(Impl->CommandBuffer, &PassBegin,
            VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(Impl->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            Impl->Pipelines[1]);
        BindSets(Impl->CommandBuffer, *Impl, {2, 3});
        vkCmdBindVertexBuffers(Impl->CommandBuffer, 0, 1,
            &Impl->FullscreenVertices.Buffer, &Offset);
        vkCmdSetViewport(Impl->CommandBuffer, 0, 1, &Viewport);
        vkCmdSetScissor(Impl->CommandBuffer, 0, 1, &Scissor);
        vkCmdDraw(Impl->CommandBuffer, 3, 1, 0, 0);
        vkCmdBindPipeline(Impl->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            Impl->Pipelines[2]);
        BindSets(Impl->CommandBuffer, *Impl, {0, 2, 3});
        vkCmdBindVertexBuffers(Impl->CommandBuffer, 0, 1,
            &Impl->SphereVertices.Buffer, &Offset);
        vkCmdBindIndexBuffer(Impl->CommandBuffer, Impl->SphereIndices.Buffer, 0,
            VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(Impl->CommandBuffer,
            static_cast<Stoner::Core::uint32>(SphereIndices.size()), 1, 0, 0, 1);
        vkCmdDrawIndexed(Impl->CommandBuffer,
            static_cast<Stoner::Core::uint32>(SphereIndices.size()), 1, 0, 0, 3);
        vkCmdBindPipeline(Impl->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            Impl->Pipelines[8]);
        vkCmdDrawIndexed(Impl->CommandBuffer,
            static_cast<Stoner::Core::uint32>(SphereIndices.size()), 1, 0, 0, 4);
        vkCmdBindPipeline(Impl->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            Impl->Pipelines[4]);
        vkCmdBindVertexBuffers(Impl->CommandBuffer, 0, 1,
            &Impl->ConeVertices.Buffer, &Offset);
        vkCmdBindIndexBuffer(Impl->CommandBuffer, Impl->ConeIndices.Buffer, 0,
            VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(Impl->CommandBuffer,
            static_cast<Stoner::Core::uint32>(ConeIndices.size()), 1, 0, 0, 2);
        vkCmdDrawIndexed(Impl->CommandBuffer,
            static_cast<Stoner::Core::uint32>(ConeIndices.size()), 1, 0, 0, 5);
        vkCmdBindPipeline(Impl->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            Impl->Pipelines[5]);
        vkCmdDrawIndexed(Impl->CommandBuffer,
            static_cast<Stoner::Core::uint32>(ConeIndices.size()), 1, 0, 0, 6);
        vkCmdEndRenderPass(Impl->CommandBuffer);

        PassBegin.renderPass = Impl->CompositionPass;
        PassBegin.framebuffer = Impl->CompositionFramebuffer;
        PassBegin.pClearValues = &LightingClear;
        vkCmdBeginRenderPass(Impl->CommandBuffer, &PassBegin,
            VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(Impl->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
            Impl->Pipelines[6]);
        BindSets(Impl->CommandBuffer, *Impl, {2});
        vkCmdBindVertexBuffers(Impl->CommandBuffer, 0, 1,
            &Impl->FullscreenVertices.Buffer, &Offset);
        vkCmdSetViewport(Impl->CommandBuffer, 0, 1, &Viewport);
        vkCmdSetScissor(Impl->CommandBuffer, 0, 1, &Scissor);
        vkCmdDraw(Impl->CommandBuffer, 3, 1, 0, 0);
        vkCmdEndRenderPass(Impl->CommandBuffer);

        if (FailurePoint == EVulkanDeferredFailurePoint::Copy)
        {
            OutReport.PrimaryFailureStage = "Copy";
            return false;
        }
        for (std::size_t Index = 0; Index < Impl->Images.size(); ++Index)
        {
            if (Index != 5)
            {
                TransitionForCopy(Impl->CommandBuffer, Impl->Images[Index],
                    Impl->Images[Index].ReadLayout);
            }
            VkBufferImageCopy Copy{};
            Copy.imageSubresource.aspectMask = Impl->Images[Index].Aspect;
            Copy.imageSubresource.layerCount = 1;
            Copy.imageExtent = {ValidationWidth, ValidationHeight, 1};
            vkCmdCopyImageToBuffer(Impl->CommandBuffer, Impl->Images[Index].Image,
                VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                Impl->Readbacks[Index].Buffer, 1, &Copy);
        }
        LightingImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        if (vkEndCommandBuffer(Impl->CommandBuffer) != VK_SUCCESS)
        {
            return false;
        }
        VkSubmitInfo Submit = MakeVulkanStruct<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
        Submit.commandBufferCount = 1;
        Submit.pCommandBuffers = &Impl->CommandBuffer;
        if (FailurePoint == EVulkanDeferredFailurePoint::Submit)
        {
            OutReport.PrimaryFailureStage = "Submit";
            return false;
        }
        if (vkQueueSubmit(Impl->Access.GraphicsQueue, 1, &Submit, Impl->Fence) != VK_SUCCESS)
        {
            return false;
        }
        Impl->bSubmitted = true;
        if (FailurePoint == EVulkanDeferredFailurePoint::Fence)
        {
            OutReport.PrimaryFailureStage = "Fence";
            return false;
        }
        if (vkWaitForFences(Impl->Access.Device, 1, &Impl->Fence, VK_TRUE,
                CompletionTimeoutNanoseconds) != VK_SUCCESS)
        {
            return false;
        }
        Impl->bSubmitted = false;
        if (FailurePoint == EVulkanDeferredFailurePoint::Map)
        {
            OutReport.PrimaryFailureStage = "Map";
            return false;
        }
        for (std::size_t Index = 0; Index < Impl->Readbacks.size(); ++Index)
        {
            if (vkMapMemory(Impl->Access.Device, Impl->Readbacks[Index].Memory, 0,
                    Impl->Readbacks[Index].Size, 0,
                    &Impl->Readbacks[Index].Mapped) != VK_SUCCESS)
            {
                return false;
            }
        }

        if (FailurePoint == EVulkanDeferredFailurePoint::Decode)
        {
            OutReport.PrimaryFailureStage = "Decode";
            return false;
        }
        const Stoner::Core::uint32 CenterX = ValidationWidth / 2;
        const Stoner::Core::uint32 CenterY = ValidationHeight / 2;
        const auto AddProbe = [&](const char* Name, const char* Semantic,
            std::size_t ImageIndex, Stoner::Core::uint32 X, Stoner::Core::uint32 Y,
            Stoner::Core::FVector4 Expected, float Threshold,
            EVulkanDeferredProbeMetric Metric) {
            FVulkanDeferredProbe Probe;
            Probe.Convention = Convention;
            Probe.Name = Name;
            Probe.Semantic = Semantic;
            Probe.X = X;
            Probe.Y = Y;
            Probe.Expected = Expected;
            Probe.Observed = ReadPixel(Impl->Readbacks[ImageIndex],
                Impl->Images[ImageIndex].Format, X, Y);
            Probe.Threshold = Threshold;
            Probe.Metric = Metric;
            Probe.ErrorMeasure = Metric == EVulkanDeferredProbeMetric::NormalDot
                ? NormalDot(Probe.Expected, Probe.Observed)
                : MaxAbsoluteError(Probe.Expected, Probe.Observed);
            Probe.bPassed = IsFinite(Probe.Expected) && IsFinite(Probe.Observed) &&
                (Metric == EVulkanDeferredProbeMetric::NormalDot
                    ? Probe.ErrorMeasure >= Threshold
                    : Probe.ErrorMeasure <= Threshold);
            OutReport.Probes.push_back(Probe);
        };
        AddProbe("masked-boundary", "MaskedCoverage", 0, 26, 26,
            {0, 0, 0, 0}, 2.0f / 255.0f, EVulkanDeferredProbeMetric::Absolute);
        AddProbe("background-depth", "Depth", 3, 1, 1,
            {bReversed ? 0.0f : 1.0f, 0, 0, 0}, 1.0e-4f,
            EVulkanDeferredProbeMetric::Absolute);
        AddProbe("background-final", "FinalLDR", 5, 1, 1,
            {0, 0, 0, 1}, 2.0f / 255.0f, EVulkanDeferredProbeMetric::Absolute);
        AddProbe("opaque-base", "BaseColor", 0, CenterX, CenterY,
            {0.8f, 0.2f, 0.1f, 0.75f}, 2.0f / 255.0f,
            EVulkanDeferredProbeMetric::Absolute);
        AddProbe("ambient-occlusion", "AmbientOcclusion", 0, CenterX, CenterY,
            {0.8f, 0.2f, 0.1f, 0.75f}, 2.0e-3f,
            EVulkanDeferredProbeMetric::Absolute);
        AddProbe("world-normal", "WorldNormal", 1, CenterX, CenterY,
            {0, 0, 1, 0.42f}, 0.999f, EVulkanDeferredProbeMetric::NormalDot);
        AddProbe("roughness", "Roughness", 1, CenterX, CenterY,
            {0, 0, 1, 0.42f}, 1.0e-3f, EVulkanDeferredProbeMetric::Absolute);
        AddProbe("emissive", "Emissive", 2, CenterX, CenterY,
            {0.3f, 0.05f, 0, 0.65f}, 1.0e-3f,
            EVulkanDeferredProbeMetric::Absolute);
        AddProbe("metallic", "Metallic", 2, CenterX, CenterY,
            {0.3f, 0.05f, 0, 0.65f}, 1.0e-3f,
            EVulkanDeferredProbeMetric::Absolute);
        AddProbe("surface-depth", "Depth", 3, CenterX, CenterY,
            {0.5f, 0, 0, 0}, 1.0e-4f, EVulkanDeferredProbeMetric::Absolute);
        constexpr float CenterNdc = 1.0f / ValidationWidth;
        const float LightDistance = std::sqrt(
            CenterNdc * CenterNdc * 2.0f + 0.25f * 0.25f);
        const float Attenuation =
            std::pow(std::max(1.0f - LightDistance / 0.75f, 0.0f), 2.0f);
        const float Diffuse = 0.25f / LightDistance;
        const float LocalContribution = 0.4f * Attenuation * Diffuse;
        const float WideRangeAttenuation =
            std::pow(std::max(1.0f - LightDistance / 3.0f, 0.0f), 2.0f);
        const float CameraInsidePointContribution =
            LocalContribution * (0.15f / 0.4f);
        const float NearPlaneSpotContribution =
            0.12f * WideRangeAttenuation * Diffuse;
        const Stoner::Core::FVector4 Lighting{
            0.25f + LocalContribution + NearPlaneSpotContribution,
            0.25f + LocalContribution,
            0.25f + CameraInsidePointContribution + NearPlaneSpotContribution,
            0.0f};
        AddProbe("lighting-accumulation", "Lighting", 4, CenterX, CenterY,
            Lighting, 1.0e-3f, EVulkanDeferredProbeMetric::Absolute);
        AddProbe("point-visible", "LocalLightCase", 4, CenterX, CenterY,
            Lighting, 1.0e-3f, EVulkanDeferredProbeMetric::Absolute);
        AddProbe("point-outside-view", "LocalLightCase", 4, 1, 1,
            {0, 0, 0, 0}, 1.0e-3f, EVulkanDeferredProbeMetric::Absolute);
        AddProbe("point-camera-inside", "LocalLightCase", 4, CenterX, CenterY,
            Lighting, 1.0e-3f, EVulkanDeferredProbeMetric::Absolute);
        AddProbe("spot-visible", "LocalLightCase", 4, CenterX, CenterY,
            Lighting, 1.0e-3f, EVulkanDeferredProbeMetric::Absolute);
        AddProbe("spot-outside-cone", "LocalLightCase", 4, CenterX, CenterY,
            Lighting, 1.0e-3f, EVulkanDeferredProbeMetric::Absolute);
        AddProbe("spot-near-plane", "LocalLightCase", 4, CenterX, CenterY,
            Lighting, 1.0e-3f, EVulkanDeferredProbeMetric::Absolute);
        const Stoner::Core::FVector4 ExpectedFinal{
            0.8f * Lighting.X + 0.3f,
            0.2f * Lighting.Y + 0.05f,
            0.1f * Lighting.Z, 1.0f};
        AddProbe("composed-final", "FinalLDR", 5, CenterX, CenterY,
            ExpectedFinal, 2.0f / 255.0f, EVulkanDeferredProbeMetric::Absolute);
        if (FailurePoint == EVulkanDeferredFailurePoint::Probe)
        {
            OutReport.PrimaryFailureStage = "Probe";
            OutReport.Probes.clear();
            return false;
        }

        for (FImpl::FBuffer& Buffer : Impl->Readbacks)
        {
            vkUnmapMemory(Impl->Access.Device, Buffer.Memory);
            Buffer.Mapped = nullptr;
        }
        return true;
    };

    if (!ExecuteConvention("StandardZ", false) ||
        !ExecuteConvention("ReversedZ", true))
    {
        Impl->ReleaseAll();
        OutReport.PeakLiveObjects = Impl->PeakLiveObjects;
        OutReport.FinalLiveObjects = Impl->LiveObjects;
        return Stoner::RHI::ERHIResult::Failed;
    }
    OutReport.bNativeSubmissionCompleted = true;
    OutReport.CompletedStageCount = 6;
    OutReport.PeakLiveObjects = Impl->PeakLiveObjects;
    Impl->ReleaseAll();
    OutReport.FinalLiveObjects = Impl->LiveObjects;
    OutReport.bPassed = OutReport.Probes.size() == 36 &&
        OutReport.GetProbeCount("StandardZ") == 18 &&
        OutReport.GetProbeCount("ReversedZ") == 18 &&
        std::all_of(OutReport.Probes.begin(), OutReport.Probes.end(),
            [](const FVulkanDeferredProbe& Probe) { return Probe.bPassed; }) &&
        OutReport.FinalLiveObjects == 0 && OutReport.bNativeSubmissionCompleted;
    return OutReport.bPassed ? Stoner::RHI::ERHIResult::Success
                             : Stoner::RHI::ERHIResult::Failed;
#endif
}

Stoner::RHI::ERHIResult FVulkanNativeOffscreenSession::Shutdown() noexcept
{
    if (bShutdown)
    {
        return Stoner::RHI::ERHIResult::Success;
    }
    if (Impl)
    {
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
        Impl->ReleaseAll();
#endif
    }
    bShutdown = true;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::Core::uint32 FVulkanDeferredValidationReport::GetProbeCount(
    const Stoner::Core::FString& Convention) const noexcept
{
    return static_cast<Stoner::Core::uint32>(std::count_if(Probes.begin(), Probes.end(),
        [&Convention](const FVulkanDeferredProbe& Probe) {
            return Probe.Convention == Convention;
        }));
}

Stoner::Core::FString FVulkanDeferredValidationReport::Dump() const
{
    std::ostringstream Stream;
    Stream << "runtime=" << RuntimeMode.CStr() << '\n'
        << "adapter=" << AdapterIdentity.CStr() << '\n'
        << "reference_path=" << ReferencePath.CStr() << '\n'
        << "software_device=" << (bSoftwareDevice ? "true" : "false") << '\n'
        << "native_submission=" << (bNativeSubmissionCompleted ? "true" : "false") << '\n'
        << "injected_failure=" << ToString(InjectedFailure) << '\n'
        << "primary_failure_stage=" << PrimaryFailureStage.CStr() << '\n'
        << "completed_stage_count=" << CompletedStageCount << '\n'
        << "surface_extent=" << ValidationWidth << 'x' << ValidationHeight << '\n'
        << "surface_layout=BaseColorAO,NormalRoughness,EmissiveMetallic,Depth,"
            "LightingAccumulation,FinalLDR\n"
        << "depth_convention=StandardZ far_clear=1 compare=LessEqual\n"
        << "depth_convention=ReversedZ far_clear=0 compare=GreaterEqual\n"
        << "pass_count=6\n"
        << "draw_count=20\n"
        << "light_count=7\n"
        << "peak_live_objects=" << PeakLiveObjects << '\n'
        << "final_live_objects=" << FinalLiveObjects << '\n';
    for (const FVulkanDeferredProbe& Probe : Probes)
    {
        Stream << "probe convention=" << Probe.Convention.CStr()
            << " name=" << Probe.Name.CStr()
            << " semantic=" << Probe.Semantic.CStr()
            << " x=" << Probe.X << " y=" << Probe.Y
            << " expected=" << Probe.Expected.X << ',' << Probe.Expected.Y << ','
            << Probe.Expected.Z << ',' << Probe.Expected.W
            << " observed=" << Probe.Observed.X << ',' << Probe.Observed.Y << ','
            << Probe.Observed.Z << ',' << Probe.Observed.W
            << " threshold=" << Probe.Threshold
            << " error=" << Probe.ErrorMeasure
            << " passed=" << (Probe.bPassed ? "true" : "false") << '\n';
    }
    Stream << "result=" << (bPassed ? "PASS" : "FAIL") << '\n';
    return Stoner::Core::FString(Stream.str());
}

const char* ToString(EVulkanDeferredFailurePoint FailurePoint) noexcept
{
    switch (FailurePoint)
    {
    case EVulkanDeferredFailurePoint::None: return "None";
    case EVulkanDeferredFailurePoint::PartialInitialization: return "PartialInitialization";
    case EVulkanDeferredFailurePoint::Record: return "Record";
    case EVulkanDeferredFailurePoint::Submit: return "Submit";
    case EVulkanDeferredFailurePoint::Fence: return "Fence";
    case EVulkanDeferredFailurePoint::Copy: return "Copy";
    case EVulkanDeferredFailurePoint::Map: return "Map";
    case EVulkanDeferredFailurePoint::Decode: return "Decode";
    case EVulkanDeferredFailurePoint::Probe: return "Probe";
    }
    return "Unknown";
}

} // namespace Stoner::Backend::Vulkan
