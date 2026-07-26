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
#include <fstream>
#include <string>
#include <vector>
#endif

namespace Stoner::Backend::Vulkan
{

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
namespace
{

using namespace Stoner::RHI;

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
    FNativeBufferBinding()
    {
        Desc.SizeInBytes = sizeof(float) * 15;
        Desc.Usage = ERHIBufferUsage::Vertex;
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
        VkPipeline InPipeline, VkBuffer InVertexBuffer, VkExtent2D InExtent)
        : Commands(InCommands), RenderPass(InRenderPass), Framebuffer(InFramebuffer), Pipeline(InPipeline),
          VertexBuffer(InVertexBuffer), Extent(InExtent) {}
    ERHICommandBufferState GetState() const noexcept override { return State; }
    ERHIQueueType GetCompatibleQueueType() const noexcept override { return ERHIQueueType::Graphics; }
    Stoner::Core::uint32 GetRecordedCommandCount() const noexcept override { return CommandCount; }
    ERHIResult Begin() override
    {
        if (State != ERHICommandBufferState::Idle && State != ERHICommandBufferState::Resettable) return ERHIResult::InvalidState;
        if (vkResetCommandBuffer(Commands, 0) != VK_SUCCESS) return ERHIResult::Failed;
        VkCommandBufferBeginInfo Info = MakeVulkanStruct<VkCommandBufferBeginInfo>(VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO);
        if (vkBeginCommandBuffer(Commands, &Info) != VK_SUCCESS) return ERHIResult::Failed;
        State = ERHICommandBufferState::Recording; CommandCount = 0; return ERHIResult::Success;
    }
    ERHIResult End() override
    {
        if (State != ERHICommandBufferState::Recording || bInsideRenderPass) return ERHIResult::InvalidState;
        if (vkEndCommandBuffer(Commands) != VK_SUCCESS) return ERHIResult::Failed;
        State = ERHICommandBufferState::Completed; return ERHIResult::Success;
    }
    ERHIResult Reset() override { State = ERHICommandBufferState::Resettable; CommandCount = 0; bInsideRenderPass = false; return ERHIResult::Success; }
    ERHIResult RecordDraw(Stoner::Core::uint32 Vertices, Stoner::Core::uint32 Instances) override
    { if (!bInsideRenderPass || Vertices != 3 || Instances != 1) return ERHIResult::InvalidState; vkCmdDraw(Commands, Vertices, Instances, 0, 0); ++CommandCount; return ERHIResult::Success; }
    ERHIResult RecordDrawIndexed(Stoner::Core::uint32, Stoner::Core::uint32,
        Stoner::Core::uint32) override { return ERHIResult::Unsupported; }
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
    VkExtent2D Extent{};
    ERHICommandBufferState State = ERHICommandBufferState::Idle;
    Stoner::Core::uint32 CommandCount = 0;
    bool bInsideRenderPass = false;
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
    std::vector<VkCommandBuffer> VisibleCommandBuffers;
    std::vector<VkFence> VisibleFences;
    std::vector<VkSemaphore> VisibleImageAvailable;
    std::vector<VkSemaphore> VisibleRenderFinished;
    Stoner::Core::uint32 CurrentFrameSlot = 0;
    Stoner::Core::uint32 AcquiredImageIndex = 0;
    Stoner::Core::uint32 AcquiredFrameSlot = 0;
    bool bFrameAcquired = false;

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
        Fence = VK_NULL_HANDLE; ImageAvailable = VK_NULL_HANDLE; RenderFinished = VK_NULL_HANDLE;
        VisibleCommandBuffers.clear(); VisibleFences.clear(); VisibleImageAvailable.clear(); VisibleRenderFinished.clear();
        CommandPool = VK_NULL_HANDLE; CommandBuffer = VK_NULL_HANDLE;
        Pipeline = VK_NULL_HANDLE; PipelineLayout = VK_NULL_HANDLE;
        VertexShader = VK_NULL_HANDLE; FragmentShader = VK_NULL_HANDLE;
        Framebuffer = VK_NULL_HANDLE; RenderPass = VK_NULL_HANDLE;
        ColorView = VK_NULL_HANDLE; ColorImage = VK_NULL_HANDLE; ColorMemory = VK_NULL_HANDLE;
        VertexBuffer = VK_NULL_HANDLE; VertexMemory = VK_NULL_HANDLE;
        Swapchain = VK_NULL_HANDLE; SwapchainFormat = VK_FORMAT_UNDEFINED; SwapchainExtent = {};
        SwapchainImages.clear(); SwapchainViews.clear(); SwapchainFramebuffers.clear();
        CurrentFrameSlot = 0; AcquiredImageIndex = 0; AcquiredFrameSlot = 0; bFrameAcquired = false;
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
    Impl->Snapshot.LiveBuffers = 1;

    VkImageCreateInfo ImageInfo = MakeVulkanStruct<VkImageCreateInfo>(VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO);
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
    VkMemoryAllocateInfo ImageAllocation = MakeVulkanStruct<VkMemoryAllocateInfo>(VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO);
    ImageAllocation.allocationSize = ImageRequirements.size;
    ImageAllocation.memoryTypeIndex = ImageMemoryType;
    if (vkAllocateMemory(Impl->Device, &ImageAllocation, nullptr, &Impl->ColorMemory) != VK_SUCCESS ||
        vkBindImageMemory(Impl->Device, Impl->ColorImage, Impl->ColorMemory, 0) != VK_SUCCESS) return Fail();
    Impl->Snapshot.LiveTextures = 1;

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
    Attachment.finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
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

    const auto CreateShader = [this](const std::vector<Stoner::Core::uint32>& Words, VkShaderModule& Out)
    {
        VkShaderModuleCreateInfo Info = MakeVulkanStruct<VkShaderModuleCreateInfo>(VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO);
        Info.codeSize = Words.size() * sizeof(Stoner::Core::uint32);
        Info.pCode = Words.data();
        return vkCreateShaderModule(Impl->Device, &Info, nullptr, &Out) == VK_SUCCESS;
    };
    if (!CreateShader(VertexWords, Impl->VertexShader) || !CreateShader(FragmentWords, Impl->FragmentShader)) return Fail();
    Impl->Snapshot.LiveShaderModules = 2;
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
    Rasterizer.polygonMode = VK_POLYGON_MODE_FILL; Rasterizer.cullMode = VK_CULL_MODE_NONE;
    Rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; Rasterizer.lineWidth = 1.0f;
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
    Impl->Snapshot.LivePipelines = 1;

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
    VkViewport Viewport{0.0f, 0.0f, 64.0f, 64.0f, 0.0f, 1.0f};
    VkRect2D Scissor{{0, 0}, {64, 64}};
    vkCmdSetViewport(Impl->CommandBuffer, 0, 1, &Viewport);
    vkCmdSetScissor(Impl->CommandBuffer, 0, 1, &Scissor);
    vkCmdDraw(Impl->CommandBuffer, 3, 1, 0, 0);
    vkCmdEndRenderPass(Impl->CommandBuffer);
    if (vkEndCommandBuffer(Impl->CommandBuffer) != VK_SUCCESS) return Fail();
    VkFenceCreateInfo FenceInfo = MakeVulkanStruct<VkFenceCreateInfo>(VK_STRUCTURE_TYPE_FENCE_CREATE_INFO);
    if (vkCreateFence(Impl->Device, &FenceInfo, nullptr, &Impl->Fence) != VK_SUCCESS) return Fail();
    Impl->Snapshot.LiveSynchronizationObjects = 1;
    VkSubmitInfo Submit = MakeVulkanStruct<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
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

Stoner::RHI::ERHIResult FVulkanNativeContext::ExecuteDeferredOffscreenValidation(
    const Stoner::Core::FString& ShaderDirectory,
    FVulkanDeferredValidationReport& OutReport,
    EVulkanDeferredFailurePoint FailurePoint)
{
    FVulkanNativeOffscreenSession Session(*this);
    const Stoner::RHI::ERHIResult Result =
        Session.Execute(ShaderDirectory, OutReport, FailurePoint);
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

    const auto CreateShader = [this](const std::vector<Stoner::Core::uint32>& Words, VkShaderModule& Out)
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
    VkPipelineRasterizationStateCreateInfo Rasterizer = MakeVulkanStruct<VkPipelineRasterizationStateCreateInfo>(VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO); Rasterizer.polygonMode = VK_POLYGON_MODE_FILL; Rasterizer.cullMode = VK_CULL_MODE_NONE; Rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; Rasterizer.lineWidth = 1.0f;
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
    Impl->Snapshot.LiveBuffers = 1;
    Impl->Snapshot.LiveTextures = static_cast<Stoner::Core::uint32>(Impl->SwapchainImages.size());
    Impl->Snapshot.LiveShaderModules = 2; Impl->Snapshot.LivePipelines = 1;
    Impl->Snapshot.LiveCommandBuffers = VisibleFrameSlotCount;
    Impl->Snapshot.LiveSynchronizationObjects = static_cast<Stoner::Core::uint32>(
        Impl->VisibleFences.size() + Impl->VisibleImageAvailable.size() + Impl->VisibleRenderFinished.size());
    return Stoner::RHI::ERHIResult::Success;
#else
    (void)VertexShaderPath; (void)FragmentShaderPath; (void)Width; (void)Height;
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

    const Stoner::RHI::ERHIFormat Format = ToRHIFormat(Impl->SwapchainFormat);
    OutBindings.ImageIndex = ImageIndex;
    OutBindings.FrameSlot = FrameSlot;
    OutBindings.OutputTexture = Stoner::Core::MakeShared<FNativeTextureBinding>(Impl->SwapchainExtent.width, Impl->SwapchainExtent.height, Format);
    OutBindings.VertexBuffer = Stoner::Core::MakeShared<FNativeBufferBinding>();
    OutBindings.GraphicsPipeline = Stoner::Core::MakeShared<FNativePipelineBinding>(Format);
    OutBindings.RenderPass = Stoner::Core::MakeShared<FNativeRenderPassBinding>(Format);
    OutBindings.Framebuffer = Stoner::Core::MakeShared<FNativeFramebufferBinding>(
        OutBindings.RenderPass, OutBindings.OutputTexture, Impl->SwapchainExtent.width, Impl->SwapchainExtent.height);
    OutBindings.CommandBuffer = Stoner::Core::MakeShared<FNativeCommandBufferBinding>(Impl->VisibleCommandBuffers[FrameSlot],
        Impl->RenderPass, Impl->SwapchainFramebuffers[ImageIndex], Impl->Pipeline, Impl->VertexBuffer, Impl->SwapchainExtent);
    return AcquireResult == VK_SUBOPTIMAL_KHR ? Stoner::RHI::ERHIResult::ResizeRequired : Stoner::RHI::ERHIResult::Success;
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
        Impl->bFrameAcquired = false;
        return Stoner::RHI::ERHIResult::Failed;
    }
    const VkPipelineStageFlags WaitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo Submit = MakeVulkanStruct<VkSubmitInfo>(VK_STRUCTURE_TYPE_SUBMIT_INFO);
    Submit.waitSemaphoreCount = 1; Submit.pWaitSemaphores = &Impl->VisibleImageAvailable[Bindings.FrameSlot]; Submit.pWaitDstStageMask = &WaitStage;
    Submit.commandBufferCount = 1; Submit.pCommandBuffers = &Impl->VisibleCommandBuffers[Bindings.FrameSlot];
    Submit.signalSemaphoreCount = 1; Submit.pSignalSemaphores = &Impl->VisibleRenderFinished[Bindings.ImageIndex];
    if (vkQueueSubmit(Impl->GraphicsQueue, 1, &Submit, SubmitFence) != VK_SUCCESS)
    {
        Impl->bFrameAcquired = false;
        return Stoner::RHI::ERHIResult::Failed;
    }
    VkPresentInfoKHR Present = MakeVulkanStruct<VkPresentInfoKHR>(VK_STRUCTURE_TYPE_PRESENT_INFO_KHR);
    Present.waitSemaphoreCount = 1; Present.pWaitSemaphores = &Impl->VisibleRenderFinished[Bindings.ImageIndex];
    Present.swapchainCount = 1; Present.pSwapchains = &Impl->Swapchain; Present.pImageIndices = &Impl->AcquiredImageIndex;
    const VkResult PresentResult = vkQueuePresentKHR(Impl->GraphicsQueue, &Present);
    Impl->bFrameAcquired = false;
    Impl->CurrentFrameSlot = (Bindings.FrameSlot + 1) % static_cast<Stoner::Core::uint32>(Impl->VisibleFences.size());
    if (PresentResult == VK_ERROR_OUT_OF_DATE_KHR || PresentResult == VK_SUBOPTIMAL_KHR)
        return Stoner::RHI::ERHIResult::ResizeRequired;
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
        Commands.SetViewport({0, 0, static_cast<float>(Bindings.Framebuffer->GetWidth()), static_cast<float>(Bindings.Framebuffer->GetHeight()), 0, 1}) != Stoner::RHI::ERHIResult::Success ||
        Commands.SetScissor({0, 0, Bindings.Framebuffer->GetWidth(), Bindings.Framebuffer->GetHeight()}) != Stoner::RHI::ERHIResult::Success ||
        Commands.RecordDraw(3, 1) != Stoner::RHI::ERHIResult::Success ||
        Commands.EndRenderPass() != Stoner::RHI::ERHIResult::Success ||
        Commands.RecordLayoutTransition(Transition) != Stoner::RHI::ERHIResult::Success ||
        Commands.End() != Stoner::RHI::ERHIResult::Success)
    {
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
        if (Impl) Impl->bFrameAcquired = false;
#endif
        return Stoner::RHI::ERHIResult::Failed;
    }
    return SubmitAndPresentVisibleFrame(Bindings);
}

Stoner::RHI::ERHIResult FVulkanNativeContext::RecreateVisiblePresentation(
    Stoner::Core::uint32 Width, Stoner::Core::uint32 Height)
{
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE && defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    if (!Impl || Impl->VisibleVertexShaderPath.empty() || Impl->VisibleFragmentShaderPath.empty())
        return Stoner::RHI::ERHIResult::InvalidState;
    const Stoner::Core::FString VertexPath = Impl->VisibleVertexShaderPath.c_str();
    const Stoner::Core::FString FragmentPath = Impl->VisibleFragmentShaderPath.c_str();
    return PrepareVisibleTriangle(VertexPath, FragmentPath, Width, Height);
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
