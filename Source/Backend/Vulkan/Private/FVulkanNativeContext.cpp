#include "VulkanRHI/FVulkanNativeContext.h"

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
#include <fstream>
#include <string>
#include <vector>
#endif

namespace Stoner::Backend::Vulkan
{

struct FVulkanNativeContext::FImpl
{
    Stoner::RHI::FRHIRuntimeSnapshot Snapshot;
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
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
    std::string VisibleVertexShaderPath;
    std::string VisibleFragmentShaderPath;

    VkBuffer VertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory VertexMemory = VK_NULL_HANDLE;
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

    static bool HasName(const std::vector<VkExtensionProperties>& Properties, const char* Name)
    {
        return std::any_of(Properties.begin(), Properties.end(), [Name](const VkExtensionProperties& Item)
        {
            return std::strcmp(Item.extensionName, Name) == 0;
        });
    }

    static std::vector<Stoner::Core::uint32> ReadSpirv(const Stoner::Core::FString& Path)
    {
        std::ifstream Input(Path.CStr(), std::ios::binary | std::ios::ate);
        if (!Input) return {};
        const std::streamsize Size = Input.tellg();
        if (Size < 20 || Size % 4 != 0) return {};
        std::vector<Stoner::Core::uint32> Words(static_cast<std::size_t>(Size) / 4);
        Input.seekg(0);
        Input.read(reinterpret_cast<char*>(Words.data()), Size);
        if (!Input.good() || Words[0] != 0x07230203u) return {};
        return Words;
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
        if (RenderFinished) vkDestroySemaphore(Device, RenderFinished, nullptr);
        if (ImageAvailable) vkDestroySemaphore(Device, ImageAvailable, nullptr);
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
        Fence = VK_NULL_HANDLE; ImageAvailable = VK_NULL_HANDLE; RenderFinished = VK_NULL_HANDLE;
        CommandPool = VK_NULL_HANDLE; CommandBuffer = VK_NULL_HANDLE;
        Pipeline = VK_NULL_HANDLE; PipelineLayout = VK_NULL_HANDLE;
        VertexShader = VK_NULL_HANDLE; FragmentShader = VK_NULL_HANDLE;
        Framebuffer = VK_NULL_HANDLE; RenderPass = VK_NULL_HANDLE;
        ColorView = VK_NULL_HANDLE; ColorImage = VK_NULL_HANDLE; ColorMemory = VK_NULL_HANDLE;
        VertexBuffer = VK_NULL_HANDLE; VertexMemory = VK_NULL_HANDLE;
        Swapchain = VK_NULL_HANDLE; SwapchainFormat = VK_FORMAT_UNDEFINED; SwapchainExtent = {};
        SwapchainImages.clear(); SwapchainViews.clear(); SwapchainFramebuffers.clear();
        Snapshot.LiveBuffers = 0;
        Snapshot.LiveTextures = 0;
        Snapshot.LiveShaderModules = 0;
        Snapshot.LivePipelines = 0;
        Snapshot.LiveCommandBuffers = 0;
        Snapshot.LiveSynchronizationObjects = 0;
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
    VkApplicationInfo AppInfo{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    AppInfo.pApplicationName = "StonerDemo";
    AppInfo.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo InstanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
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
    VkDeviceQueueCreateInfo QueueInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    QueueInfo.queueFamilyIndex = Impl->GraphicsQueueFamily;
    QueueInfo.queueCount = 1;
    QueueInfo.pQueuePriorities = &Priority;
    VkDeviceCreateInfo DeviceInfo{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
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
    return Stoner::RHI::ERHIResult::Unsupported;
#endif
}

Stoner::RHI::ERHIResult FVulkanNativeContext::ExecuteOffscreenTriangle(
    const Stoner::Core::FString& VertexShaderPath, const Stoner::Core::FString& FragmentShaderPath)
{
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    if (!Impl || Impl->Device == VK_NULL_HANDLE) return Stoner::RHI::ERHIResult::InvalidState;
    Impl->DestroyFrameResources();
    const auto Fail = [this]() { Impl->DestroyFrameResources(); return Stoner::RHI::ERHIResult::Failed; };
    const std::vector<Stoner::Core::uint32> VertexWords = FImpl::ReadSpirv(VertexShaderPath);
    const std::vector<Stoner::Core::uint32> FragmentWords = FImpl::ReadSpirv(FragmentShaderPath);
    if (VertexWords.empty() || FragmentWords.empty()) return Fail();

    constexpr std::array<float, 15> Vertices = {
         0.0f, -0.6f, 1.0f, 0.0f, 0.0f,
         0.6f,  0.6f, 0.0f, 1.0f, 0.0f,
        -0.6f,  0.6f, 0.0f, 0.0f, 1.0f,
    };
    VkBufferCreateInfo BufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    BufferInfo.size = sizeof(Vertices);
    BufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(Impl->Device, &BufferInfo, nullptr, &Impl->VertexBuffer) != VK_SUCCESS) return Fail();
    VkMemoryRequirements BufferRequirements{};
    vkGetBufferMemoryRequirements(Impl->Device, Impl->VertexBuffer, &BufferRequirements);
    const auto BufferMemoryType = Impl->FindMemoryType(BufferRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (BufferMemoryType == UINT32_MAX) return Fail();
    VkMemoryAllocateInfo BufferAllocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    BufferAllocation.allocationSize = BufferRequirements.size;
    BufferAllocation.memoryTypeIndex = BufferMemoryType;
    if (vkAllocateMemory(Impl->Device, &BufferAllocation, nullptr, &Impl->VertexMemory) != VK_SUCCESS ||
        vkBindBufferMemory(Impl->Device, Impl->VertexBuffer, Impl->VertexMemory, 0) != VK_SUCCESS) return Fail();
    void* Mapped = nullptr;
    if (vkMapMemory(Impl->Device, Impl->VertexMemory, 0, sizeof(Vertices), 0, &Mapped) != VK_SUCCESS) return Fail();
    std::memcpy(Mapped, Vertices.data(), sizeof(Vertices));
    vkUnmapMemory(Impl->Device, Impl->VertexMemory);
    Impl->Snapshot.LiveBuffers = 1;

    VkImageCreateInfo ImageInfo{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    ImageInfo.imageType = VK_IMAGE_TYPE_2D;
    ImageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    ImageInfo.extent = {64, 64, 1};
    ImageInfo.mipLevels = 1;
    ImageInfo.arrayLayers = 1;
    ImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    ImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    ImageInfo.usage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(Impl->Device, &ImageInfo, nullptr, &Impl->ColorImage) != VK_SUCCESS) return Fail();
    VkMemoryRequirements ImageRequirements{};
    vkGetImageMemoryRequirements(Impl->Device, Impl->ColorImage, &ImageRequirements);
    const auto ImageMemoryType = Impl->FindMemoryType(ImageRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (ImageMemoryType == UINT32_MAX) return Fail();
    VkMemoryAllocateInfo ImageAllocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ImageAllocation.allocationSize = ImageRequirements.size;
    ImageAllocation.memoryTypeIndex = ImageMemoryType;
    if (vkAllocateMemory(Impl->Device, &ImageAllocation, nullptr, &Impl->ColorMemory) != VK_SUCCESS ||
        vkBindImageMemory(Impl->Device, Impl->ColorImage, Impl->ColorMemory, 0) != VK_SUCCESS) return Fail();
    Impl->Snapshot.LiveTextures = 1;

    VkImageViewCreateInfo ViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
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
    Attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference AttachmentReference{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkSubpassDescription Subpass{};
    Subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    Subpass.colorAttachmentCount = 1;
    Subpass.pColorAttachments = &AttachmentReference;
    VkRenderPassCreateInfo RenderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    RenderPassInfo.attachmentCount = 1;
    RenderPassInfo.pAttachments = &Attachment;
    RenderPassInfo.subpassCount = 1;
    RenderPassInfo.pSubpasses = &Subpass;
    if (vkCreateRenderPass(Impl->Device, &RenderPassInfo, nullptr, &Impl->RenderPass) != VK_SUCCESS) return Fail();
    VkFramebufferCreateInfo FramebufferInfo{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    FramebufferInfo.renderPass = Impl->RenderPass;
    FramebufferInfo.attachmentCount = 1;
    FramebufferInfo.pAttachments = &Impl->ColorView;
    FramebufferInfo.width = 64;
    FramebufferInfo.height = 64;
    FramebufferInfo.layers = 1;
    if (vkCreateFramebuffer(Impl->Device, &FramebufferInfo, nullptr, &Impl->Framebuffer) != VK_SUCCESS) return Fail();

    const auto CreateShader = [this](const std::vector<Stoner::Core::uint32>& Words, VkShaderModule& Out)
    {
        VkShaderModuleCreateInfo Info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        Info.codeSize = Words.size() * sizeof(Stoner::Core::uint32);
        Info.pCode = Words.data();
        return vkCreateShaderModule(Impl->Device, &Info, nullptr, &Out) == VK_SUCCESS;
    };
    if (!CreateShader(VertexWords, Impl->VertexShader) || !CreateShader(FragmentWords, Impl->FragmentShader)) return Fail();
    Impl->Snapshot.LiveShaderModules = 2;
    VkPipelineLayoutCreateInfo LayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
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
    VkPipelineVertexInputStateCreateInfo VertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VertexInput.vertexBindingDescriptionCount = 1; VertexInput.pVertexBindingDescriptions = &Binding;
    VertexInput.vertexAttributeDescriptionCount = 2; VertexInput.pVertexAttributeDescriptions = Attributes;
    VkPipelineInputAssemblyStateCreateInfo Assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    Assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo ViewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    ViewportState.viewportCount = 1; ViewportState.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo Rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    Rasterizer.polygonMode = VK_POLYGON_MODE_FILL; Rasterizer.cullMode = VK_CULL_MODE_NONE;
    Rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; Rasterizer.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo Multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    Multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState BlendAttachment{};
    BlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo Blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    Blend.attachmentCount = 1; Blend.pAttachments = &BlendAttachment;
    const VkDynamicState DynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo Dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    Dynamic.dynamicStateCount = 2; Dynamic.pDynamicStates = DynamicStates;
    VkGraphicsPipelineCreateInfo PipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    PipelineInfo.stageCount = 2; PipelineInfo.pStages = ShaderStages;
    PipelineInfo.pVertexInputState = &VertexInput; PipelineInfo.pInputAssemblyState = &Assembly;
    PipelineInfo.pViewportState = &ViewportState; PipelineInfo.pRasterizationState = &Rasterizer;
    PipelineInfo.pMultisampleState = &Multisample; PipelineInfo.pColorBlendState = &Blend;
    PipelineInfo.pDynamicState = &Dynamic; PipelineInfo.layout = Impl->PipelineLayout;
    PipelineInfo.renderPass = Impl->RenderPass;
    if (vkCreateGraphicsPipelines(Impl->Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &Impl->Pipeline) != VK_SUCCESS) return Fail();
    Impl->Snapshot.LivePipelines = 1;

    VkCommandPoolCreateInfo PoolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    PoolInfo.queueFamilyIndex = Impl->GraphicsQueueFamily;
    if (vkCreateCommandPool(Impl->Device, &PoolInfo, nullptr, &Impl->CommandPool) != VK_SUCCESS) return Fail();
    VkCommandBufferAllocateInfo CommandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    CommandInfo.commandPool = Impl->CommandPool; CommandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; CommandInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(Impl->Device, &CommandInfo, &Impl->CommandBuffer) != VK_SUCCESS) return Fail();
    Impl->Snapshot.LiveCommandBuffers = 1;
    VkCommandBufferBeginInfo BeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (vkBeginCommandBuffer(Impl->CommandBuffer, &BeginInfo) != VK_SUCCESS) return Fail();
    VkClearValue Clear{}; Clear.color = {{0.02f, 0.03f, 0.05f, 1.0f}};
    VkRenderPassBeginInfo BeginPass{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    BeginPass.renderPass = Impl->RenderPass; BeginPass.framebuffer = Impl->Framebuffer;
    BeginPass.renderArea.extent = {64, 64}; BeginPass.clearValueCount = 1; BeginPass.pClearValues = &Clear;
    vkCmdBeginRenderPass(Impl->CommandBuffer, &BeginPass, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(Impl->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Impl->Pipeline);
    const VkDeviceSize Offset = 0;
    vkCmdBindVertexBuffers(Impl->CommandBuffer, 0, 1, &Impl->VertexBuffer, &Offset);
    VkViewport Viewport{0.0f, 0.0f, 64.0f, 64.0f, 0.0f, 1.0f};
    VkRect2D Scissor{{0, 0}, {64, 64}};
    vkCmdSetViewport(Impl->CommandBuffer, 0, 1, &Viewport);
    vkCmdSetScissor(Impl->CommandBuffer, 0, 1, &Scissor);
    vkCmdDraw(Impl->CommandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(Impl->CommandBuffer);
    if (vkEndCommandBuffer(Impl->CommandBuffer) != VK_SUCCESS) return Fail();
    VkFenceCreateInfo FenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    if (vkCreateFence(Impl->Device, &FenceInfo, nullptr, &Impl->Fence) != VK_SUCCESS) return Fail();
    Impl->Snapshot.LiveSynchronizationObjects = 1;
    VkSubmitInfo Submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    Submit.commandBufferCount = 1; Submit.pCommandBuffers = &Impl->CommandBuffer;
    if (vkQueueSubmit(Impl->GraphicsQueue, 1, &Submit, Impl->Fence) != VK_SUCCESS ||
        vkWaitForFences(Impl->Device, 1, &Impl->Fence, VK_TRUE, 30ull * 1000ull * 1000ull * 1000ull) != VK_SUCCESS) return Fail();
    Impl->DestroyFrameResources();
    return Stoner::RHI::ERHIResult::Success;
#else
    (void)VertexShaderPath; (void)FragmentShaderPath;
    return Stoner::RHI::ERHIResult::Unsupported;
#endif
}

Stoner::RHI::ERHIResult FVulkanNativeContext::PrepareVisibleTriangle(
    const Stoner::Core::FString& VertexShaderPath,
    const Stoner::Core::FString& FragmentShaderPath,
    Stoner::Core::uint32 Width,
    Stoner::Core::uint32 Height)
{
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE && defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    if (!Impl || Impl->Device == VK_NULL_HANDLE || Impl->Surface == VK_NULL_HANDLE)
        return Stoner::RHI::ERHIResult::InvalidState;
    if (Width == 0 || Height == 0) return Stoner::RHI::ERHIResult::Unavailable;
    const std::vector<Stoner::Core::uint32> VertexWords = FImpl::ReadSpirv(VertexShaderPath);
    const std::vector<Stoner::Core::uint32> FragmentWords = FImpl::ReadSpirv(FragmentShaderPath);
    if (VertexWords.empty() || FragmentWords.empty()) return Stoner::RHI::ERHIResult::Failed;
    Impl->VisibleVertexShaderPath = VertexShaderPath.CStr();
    Impl->VisibleFragmentShaderPath = FragmentShaderPath.CStr();
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
    VkSwapchainCreateInfoKHR SwapchainInfo{VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
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
        VkImageViewCreateInfo ViewInfo{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
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
    VkBufferCreateInfo BufferInfo{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    BufferInfo.size = sizeof(Vertices);
    BufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    BufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(Impl->Device, &BufferInfo, nullptr, &Impl->VertexBuffer) != VK_SUCCESS) return Fail();
    VkMemoryRequirements Requirements{};
    vkGetBufferMemoryRequirements(Impl->Device, Impl->VertexBuffer, &Requirements);
    const auto MemoryType = Impl->FindMemoryType(Requirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (MemoryType == UINT32_MAX) return Fail();
    VkMemoryAllocateInfo Allocation{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    Allocation.allocationSize = Requirements.size;
    Allocation.memoryTypeIndex = MemoryType;
    if (vkAllocateMemory(Impl->Device, &Allocation, nullptr, &Impl->VertexMemory) != VK_SUCCESS ||
        vkBindBufferMemory(Impl->Device, Impl->VertexBuffer, Impl->VertexMemory, 0) != VK_SUCCESS) return Fail();
    void* Mapped = nullptr;
    if (vkMapMemory(Impl->Device, Impl->VertexMemory, 0, sizeof(Vertices), 0, &Mapped) != VK_SUCCESS) return Fail();
    std::memcpy(Mapped, Vertices.data(), sizeof(Vertices));
    vkUnmapMemory(Impl->Device, Impl->VertexMemory);

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
    VkRenderPassCreateInfo RenderPassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO};
    RenderPassInfo.attachmentCount = 1; RenderPassInfo.pAttachments = &Attachment;
    RenderPassInfo.subpassCount = 1; RenderPassInfo.pSubpasses = &Subpass;
    RenderPassInfo.dependencyCount = 1; RenderPassInfo.pDependencies = &Dependency;
    if (vkCreateRenderPass(Impl->Device, &RenderPassInfo, nullptr, &Impl->RenderPass) != VK_SUCCESS) return Fail();
    for (VkImageView View : Impl->SwapchainViews)
    {
        VkFramebufferCreateInfo Info{VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
        Info.renderPass = Impl->RenderPass; Info.attachmentCount = 1; Info.pAttachments = &View;
        Info.width = Extent.width; Info.height = Extent.height; Info.layers = 1;
        VkFramebuffer Target = VK_NULL_HANDLE;
        if (vkCreateFramebuffer(Impl->Device, &Info, nullptr, &Target) != VK_SUCCESS) return Fail();
        Impl->SwapchainFramebuffers.push_back(Target);
    }

    const auto CreateShader = [this](const std::vector<Stoner::Core::uint32>& Words, VkShaderModule& Out)
    {
        VkShaderModuleCreateInfo Info{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        Info.codeSize = Words.size() * sizeof(Stoner::Core::uint32); Info.pCode = Words.data();
        return vkCreateShaderModule(Impl->Device, &Info, nullptr, &Out) == VK_SUCCESS;
    };
    if (!CreateShader(VertexWords, Impl->VertexShader) || !CreateShader(FragmentWords, Impl->FragmentShader)) return Fail();
    VkPipelineLayoutCreateInfo LayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    if (vkCreatePipelineLayout(Impl->Device, &LayoutInfo, nullptr, &Impl->PipelineLayout) != VK_SUCCESS) return Fail();
    VkPipelineShaderStageCreateInfo Stages[2]{};
    Stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    Stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT; Stages[0].module = Impl->VertexShader; Stages[0].pName = "main";
    Stages[1] = Stages[0]; Stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; Stages[1].module = Impl->FragmentShader;
    VkVertexInputBindingDescription Binding{0, sizeof(float) * 5, VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription Attributes[2] = {{0,0,VK_FORMAT_R32G32_SFLOAT,0},{1,0,VK_FORMAT_R32G32B32_SFLOAT,sizeof(float)*2}};
    VkPipelineVertexInputStateCreateInfo VertexInput{VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    VertexInput.vertexBindingDescriptionCount = 1; VertexInput.pVertexBindingDescriptions = &Binding;
    VertexInput.vertexAttributeDescriptionCount = 2; VertexInput.pVertexAttributeDescriptions = Attributes;
    VkPipelineInputAssemblyStateCreateInfo Assembly{VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO}; Assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo ViewportState{VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO}; ViewportState.viewportCount = 1; ViewportState.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo Rasterizer{VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO}; Rasterizer.polygonMode = VK_POLYGON_MODE_FILL; Rasterizer.cullMode = VK_CULL_MODE_NONE; Rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; Rasterizer.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo Multisample{VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO}; Multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState BlendAttachment{}; BlendAttachment.colorWriteMask = 0xf;
    VkPipelineColorBlendStateCreateInfo Blend{VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO}; Blend.attachmentCount = 1; Blend.pAttachments = &BlendAttachment;
    const VkDynamicState DynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo Dynamic{VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO}; Dynamic.dynamicStateCount = 2; Dynamic.pDynamicStates = DynamicStates;
    VkGraphicsPipelineCreateInfo PipelineInfo{VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    PipelineInfo.stageCount = 2; PipelineInfo.pStages = Stages; PipelineInfo.pVertexInputState = &VertexInput;
    PipelineInfo.pInputAssemblyState = &Assembly; PipelineInfo.pViewportState = &ViewportState; PipelineInfo.pRasterizationState = &Rasterizer;
    PipelineInfo.pMultisampleState = &Multisample; PipelineInfo.pColorBlendState = &Blend; PipelineInfo.pDynamicState = &Dynamic;
    PipelineInfo.layout = Impl->PipelineLayout; PipelineInfo.renderPass = Impl->RenderPass;
    if (vkCreateGraphicsPipelines(Impl->Device, VK_NULL_HANDLE, 1, &PipelineInfo, nullptr, &Impl->Pipeline) != VK_SUCCESS) return Fail();
    VkCommandPoolCreateInfo PoolInfo{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    PoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; PoolInfo.queueFamilyIndex = Impl->GraphicsQueueFamily;
    if (vkCreateCommandPool(Impl->Device, &PoolInfo, nullptr, &Impl->CommandPool) != VK_SUCCESS) return Fail();
    VkCommandBufferAllocateInfo CommandInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    CommandInfo.commandPool = Impl->CommandPool; CommandInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; CommandInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(Impl->Device, &CommandInfo, &Impl->CommandBuffer) != VK_SUCCESS) return Fail();
    VkSemaphoreCreateInfo SemaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    if (vkCreateSemaphore(Impl->Device, &SemaphoreInfo, nullptr, &Impl->ImageAvailable) != VK_SUCCESS ||
        vkCreateSemaphore(Impl->Device, &SemaphoreInfo, nullptr, &Impl->RenderFinished) != VK_SUCCESS) return Fail();
    VkFenceCreateInfo FenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; FenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateFence(Impl->Device, &FenceInfo, nullptr, &Impl->Fence) != VK_SUCCESS) return Fail();
    Impl->Snapshot.LiveBuffers = 1;
    Impl->Snapshot.LiveTextures = static_cast<Stoner::Core::uint32>(Impl->SwapchainImages.size());
    Impl->Snapshot.LiveShaderModules = 2; Impl->Snapshot.LivePipelines = 1; Impl->Snapshot.LiveCommandBuffers = 1;
    Impl->Snapshot.LiveSynchronizationObjects = 3;
    return Stoner::RHI::ERHIResult::Success;
#else
    (void)VertexShaderPath; (void)FragmentShaderPath; (void)Width; (void)Height;
    return Stoner::RHI::ERHIResult::Unsupported;
#endif
}

Stoner::RHI::ERHIResult FVulkanNativeContext::DrawVisibleFrame()
{
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE && defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    if (!Impl || Impl->Swapchain == VK_NULL_HANDLE || Impl->Fence == VK_NULL_HANDLE) return Stoner::RHI::ERHIResult::InvalidState;
    const VkResult WaitResult = vkWaitForFences(Impl->Device, 1, &Impl->Fence, VK_TRUE, 30ull * 1000ull * 1000ull * 1000ull);
    if (WaitResult == VK_TIMEOUT) return Stoner::RHI::ERHIResult::Timeout;
    if (WaitResult != VK_SUCCESS) return Stoner::RHI::ERHIResult::Failed;
    Stoner::Core::uint32 ImageIndex = 0;
    const VkResult AcquireResult = vkAcquireNextImageKHR(Impl->Device, Impl->Swapchain, UINT64_MAX, Impl->ImageAvailable, VK_NULL_HANDLE, &ImageIndex);
    if (AcquireResult == VK_ERROR_OUT_OF_DATE_KHR) return Stoner::RHI::ERHIResult::ResizeRequired;
    if (AcquireResult != VK_SUCCESS && AcquireResult != VK_SUBOPTIMAL_KHR) return Stoner::RHI::ERHIResult::Failed;
    vkResetFences(Impl->Device, 1, &Impl->Fence);
    vkResetCommandBuffer(Impl->CommandBuffer, 0);
    VkCommandBufferBeginInfo BeginInfo{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    if (vkBeginCommandBuffer(Impl->CommandBuffer, &BeginInfo) != VK_SUCCESS) return Stoner::RHI::ERHIResult::Failed;
    VkClearValue Clear{}; Clear.color = {{0.02f, 0.03f, 0.05f, 1.0f}};
    VkRenderPassBeginInfo PassInfo{VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
    PassInfo.renderPass = Impl->RenderPass; PassInfo.framebuffer = Impl->SwapchainFramebuffers[ImageIndex];
    PassInfo.renderArea.extent = Impl->SwapchainExtent; PassInfo.clearValueCount = 1; PassInfo.pClearValues = &Clear;
    vkCmdBeginRenderPass(Impl->CommandBuffer, &PassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(Impl->CommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, Impl->Pipeline);
    const VkDeviceSize Offset = 0; vkCmdBindVertexBuffers(Impl->CommandBuffer, 0, 1, &Impl->VertexBuffer, &Offset);
    VkViewport Viewport{0.0f, 0.0f, static_cast<float>(Impl->SwapchainExtent.width), static_cast<float>(Impl->SwapchainExtent.height), 0.0f, 1.0f};
    VkRect2D Scissor{{0,0}, Impl->SwapchainExtent};
    vkCmdSetViewport(Impl->CommandBuffer, 0, 1, &Viewport); vkCmdSetScissor(Impl->CommandBuffer, 0, 1, &Scissor);
    vkCmdDraw(Impl->CommandBuffer, 3, 1, 0, 0); vkCmdEndRenderPass(Impl->CommandBuffer);
    if (vkEndCommandBuffer(Impl->CommandBuffer) != VK_SUCCESS) return Stoner::RHI::ERHIResult::Failed;
    const VkPipelineStageFlags WaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo Submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    Submit.waitSemaphoreCount = 1; Submit.pWaitSemaphores = &Impl->ImageAvailable; Submit.pWaitDstStageMask = &WaitStage;
    Submit.commandBufferCount = 1; Submit.pCommandBuffers = &Impl->CommandBuffer;
    Submit.signalSemaphoreCount = 1; Submit.pSignalSemaphores = &Impl->RenderFinished;
    if (vkQueueSubmit(Impl->GraphicsQueue, 1, &Submit, Impl->Fence) != VK_SUCCESS) return Stoner::RHI::ERHIResult::Failed;
    VkPresentInfoKHR Present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    Present.waitSemaphoreCount = 1; Present.pWaitSemaphores = &Impl->RenderFinished;
    Present.swapchainCount = 1; Present.pSwapchains = &Impl->Swapchain; Present.pImageIndices = &ImageIndex;
    const VkResult PresentResult = vkQueuePresentKHR(Impl->GraphicsQueue, &Present);
    if (PresentResult == VK_ERROR_OUT_OF_DATE_KHR || PresentResult == VK_SUBOPTIMAL_KHR || AcquireResult == VK_SUBOPTIMAL_KHR)
        return Stoner::RHI::ERHIResult::ResizeRequired;
    return PresentResult == VK_SUCCESS ? Stoner::RHI::ERHIResult::Success : Stoner::RHI::ERHIResult::Failed;
#else
    return Stoner::RHI::ERHIResult::Unsupported;
#endif
}

Stoner::RHI::ERHIResult FVulkanNativeContext::RecreateVisiblePresentation(
    Stoner::Core::uint32 Width, Stoner::Core::uint32 Height)
{
    if (!Impl || Impl->VisibleVertexShaderPath.empty() || Impl->VisibleFragmentShaderPath.empty())
        return Stoner::RHI::ERHIResult::InvalidState;
    const Stoner::Core::FString VertexPath = Impl->VisibleVertexShaderPath.c_str();
    const Stoner::Core::FString FragmentPath = Impl->VisibleFragmentShaderPath.c_str();
    return PrepareVisibleTriangle(VertexPath, FragmentPath, Width, Height);
}

Stoner::RHI::ERHIResult FVulkanNativeContext::Shutdown()
{
    if (!Impl) return Stoner::RHI::ERHIResult::InvalidState;
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    Impl->DestroyFrameResources();
    if (Impl->Device)
    {
        vkDeviceWaitIdle(Impl->Device);
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

} // namespace Stoner::Backend::Vulkan
