#include "VulkanOutputTransformNativeTests.h"

#include "Application/FWindow.h"
#include "Application/FWindowDesc.h"
#include "RHI/RHIMinimal.h"
#include "VulkanRHI/FVulkanDevice.h"
#include "VulkanRHI/FVulkanSwapchain.h"

#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
#include "FVulkanStruct.h"
#endif

#include <iostream>
#include <cstdlib>
#include <cstring>

namespace
{

void Record(FVulkanOutputTransformNativeTestResult& Result, bool bPassed,
    const char* Name)
{
    if (bPassed)
    {
        ++Result.Passed;
        std::cout << "[PASS] " << Name << '\n';
    }
    else
    {
        ++Result.Failed;
        std::cout << "[FAIL] " << Name << '\n';
    }
}

} // namespace

FVulkanOutputTransformNativeTestResult
RunVulkanOutputTransformNativeTests()
{
    FVulkanOutputTransformNativeTestResult Result;
#if defined(STONER_VULKAN_NATIVE_AVAILABLE) && STONER_VULKAN_NATIVE_AVAILABLE
    using namespace Stoner::Backend::Vulkan;
    using namespace Stoner::RHI;

    Record(Result,
        ToVulkanPresentationFormat(ERHIFormat::B8G8R8A8_UNorm) ==
            VK_FORMAT_B8G8R8A8_UNORM &&
        ToVulkanPresentationColorSpace(
            ERHIPresentationColorSpace::SrgbNonlinear) ==
            VK_COLOR_SPACE_SRGB_NONLINEAR_KHR,
        "Vulkan SDR presentation keeps Renderer-encoded UNorm storage explicit");
    Record(Result,
        ToVulkanPresentationFormat(ERHIFormat::R10G10B10A2_UNorm) ==
            VK_FORMAT_A2B10G10R10_UNORM_PACK32 &&
        ToVulkanPresentationColorSpace(
            ERHIPresentationColorSpace::Hdr10St2084) ==
            VK_COLOR_SPACE_HDR10_ST2084_EXT,
        "Vulkan PQ presentation requires the exact packed-10 HDR10 pair");
    Record(Result,
        ToVulkanPresentationFormat(ERHIFormat::R16G16B16A16_Float) ==
            VK_FORMAT_R16G16B16A16_SFLOAT &&
        ToVulkanPresentationColorSpace(
            ERHIPresentationColorSpace::ExtendedSrgbLinear) ==
            VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT,
        "Vulkan linear HDR presentation requires the exact FP16 extended pair");
    Record(Result,
        FromVulkanPresentationFormat(
            VK_FORMAT_A2B10G10R10_UNORM_PACK32) ==
            ERHIFormat::R10G10B10A2_UNorm &&
        FromVulkanPresentationColorSpace(
            VK_COLOR_SPACE_HDR10_ST2084_EXT) ==
            ERHIPresentationColorSpace::Hdr10St2084,
        "Vulkan exact presentation pair round-trips without inferred conversion");
    Record(Result,
        ToVulkanPresentationFormat(ERHIFormat::R8G8B8A8_sRGB) ==
            VK_FORMAT_R8G8B8A8_SRGB &&
        ToVulkanPresentationFormat(ERHIFormat::B8G8R8A8_UNorm) !=
            VK_FORMAT_B8G8R8A8_SRGB,
        "Vulkan native mapping does not silently replace UNorm with an sRGB format");

    const char* RequireVisible =
        std::getenv("STONER_REQUIRE_VULKAN_OUTPUT_PRESENTATION");
    if (!RequireVisible || std::strcmp(RequireVisible, "1") != 0)
    {
        Record(Result, true,
            "Vulkan native RHI presentation proof is explicit opt-in");
    }
    else
    {
        using namespace Stoner;
        Application::FWindowDesc WindowDesc;
        WindowDesc.Title = "Stoner Vulkan Output Transform Probe";
        WindowDesc.ClientWidth = 96;
        WindowDesc.ClientHeight = 64;
        WindowDesc.bVisible = false;
        Application::FWindow Window;
        const bool bWindow = Window.CreateRealWindow(WindowDesc) ==
            Application::EApplicationResult::Success;

        auto Device = Core::MakeShared<FVulkanDevice>();
        FVulkanInstanceDesc DeviceDesc;
        DeviceDesc.RuntimeMode =
            EVulkanInstanceRuntimeMode::DeterministicFallback;
        DeviceDesc.bRequestValidation = false;
        const bool bDevice = bWindow &&
            Device->Initialize(DeviceDesc) == ERHIResult::Success &&
            Device->EnableNativePresentationRuntime(
                Window.GetPlatformWindow()) == ERHIResult::Success;

        FRHIPresentationSurfaceDesc SurfaceDesc;
        if (bWindow) SurfaceDesc.Window = Window.GetPlatformWindow();
        auto Surface = bDevice
            ? Device->CreatePresentationSurface(SurfaceDesc)
            : TRHIObjectResult<IRHIPresentationSurface>{};
        FRHIPresentationCapabilities Capabilities;
        const bool bCapabilities = Surface.Succeeded() &&
            Surface.Object->QueryCapabilities(Capabilities) ==
                ERHIResult::Success;
        FRHIPresentationFormatColorSpacePair SdrPair;
        if (bCapabilities)
        {
            for (const auto& Pair : Capabilities.SupportedPairs)
            {
                if (Pair.ColorSpace ==
                        ERHIPresentationColorSpace::SrgbNonlinear &&
                    (Pair.Format == ERHIFormat::B8G8R8A8_UNorm ||
                     Pair.Format == ERHIFormat::R8G8B8A8_UNorm))
                {
                    SdrPair = Pair;
                    break;
                }
            }
        }
        FRHISwapchainDesc Request;
        Request.Width = bWindow ? Window.GetDrawableWidth() : 0;
        Request.Height = bWindow ? Window.GetDrawableHeight() : 0;
        Request.FramesInFlight = 2;
        Request.PreferredFormat = SdrPair.Format;
        Request.PreferredColorSpace = SdrPair.ColorSpace;
        Request.SurfaceCapabilityGeneration =
            Capabilities.CapabilityGeneration;
        auto Swapchain = bCapabilities && SdrPair.IsValid()
            ? Device->CreateSwapchain(Surface.Object, Request)
            : TRHIObjectResult<IRHISwapchain>{};
        auto Concrete = Swapchain.Succeeded()
            ? std::dynamic_pointer_cast<FVulkanSwapchain>(Swapchain.Object)
            : nullptr;
        FRHIPresentationFrame Frame;
        const ERHIResult AcquireResult = Concrete
            ? Concrete->AcquireNextFrame(29001, Frame)
            : ERHIResult::InvalidState;
        const auto* Native = Concrete
            ? Concrete->GetNativeFrameBindings() : nullptr;
        bool bRecorded = false;
        if (AcquireResult == ERHIResult::Success && Native &&
            Native->CommandBuffer)
        {
            FRHIResourceBarrierDesc ToPresent;
            ToPresent.Texture = Native->OutputTexture;
            ToPresent.RequiredTextureUsage =
                ERHITextureUsage::Present;
            ToPresent.Before = ERHIResourceLayout::Undefined;
            ToPresent.After = ERHIResourceLayout::Present;
            bRecorded = Native->CommandBuffer->Begin() ==
                    ERHIResult::Success &&
                Native->CommandBuffer->RecordLayoutTransition(ToPresent) ==
                    ERHIResult::Success &&
                Native->CommandBuffer->End() == ERHIResult::Success;
        }
        const ERHIResult PresentResult = bRecorded
            ? Concrete->Present(Frame) : ERHIResult::InvalidState;
        const FRHIRuntimeSnapshot Snapshot = Device->GetRuntimeSnapshot();
        Record(Result,
            bWindow && bDevice && bCapabilities && SdrPair.IsValid() &&
                Swapchain.Succeeded() && Frame.IsValid() && bRecorded &&
                PresentResult == ERHIResult::Success &&
                Snapshot.ProvesNativeExecution() &&
                Snapshot.PresentationFormat == SdrPair.Format &&
                Snapshot.PresentationColorSpace == SdrPair.ColorSpace &&
                Snapshot.LastAcquiredFrameToken == 29001 &&
                Snapshot.LastSubmittedFrameToken == 29001 &&
                Snapshot.LastPresentedFrameToken == 29001,
            "Vulkan RHI surface/swapchain owns exact native acquire submission and present provenance");

        const Core::uint64 OldCapabilityGeneration =
            Capabilities.CapabilityGeneration;
        FRHIPresentationCapabilities Refreshed;
        const bool bRefresh = Surface.Succeeded() &&
            Surface.Object->NotifyPresentationEnvironmentChanged() ==
                ERHIResult::Success &&
            Surface.Object->QueryCapabilities(Refreshed) ==
                ERHIResult::Success;
        Record(Result,
            bRefresh && Refreshed.CapabilityGeneration >
                    OldCapabilityGeneration &&
                Refreshed.CapabilityDigest !=
                    Capabilities.CapabilityDigest &&
                Concrete->Reconfigure(Request) == ERHIResult::Unsupported,
            "Vulkan native capability refresh advances generation and rejects stale requests");

        FRHISwapchainDesc RefreshedRequest = Request;
        RefreshedRequest.SurfaceCapabilityGeneration =
            Refreshed.CapabilityGeneration;
        FRHIResolvedPresentationState RefreshedResolved;
        const auto NativeContext = Device->GetNativePresentationContext();
        const bool bPreparedReadback = NativeContext && bRefresh &&
            NativeContext->PrepareVisibleImage(
                RefreshedRequest, RefreshedResolved) ==
                ERHIResult::Success;
        Core::TArray<Core::uint8> SourcePixels;
        if (bPreparedReadback)
        {
            SourcePixels.resize(static_cast<Core::usize>(
                RefreshedResolved.Width) * RefreshedResolved.Height * 4u);
            for (Core::usize Offset = 0; Offset < SourcePixels.size();
                 Offset += 4u)
            {
                SourcePixels[Offset] = 17;
                SourcePixels[Offset + 1u] = 63;
                SourcePixels[Offset + 2u] = 129;
                SourcePixels[Offset + 3u] = 255;
            }
        }
        Core::TArray<Core::uint8> Presented;
        Core::uint32 PresentedWidth = 0;
        Core::uint32 PresentedHeight = 0;
        const ERHIResult MismatchedExtentResult = bPreparedReadback &&
                RefreshedResolved.Width > 1
            ? NativeContext->PresentVisibleRgba8(
                  SourcePixels, RefreshedResolved.Width - 1u,
                  RefreshedResolved.Height,
                  RefreshedResolved.Width * 4u, Presented,
                  PresentedWidth, PresentedHeight)
            : ERHIResult::InvalidState;
        const ERHIResult ReadbackResult = bPreparedReadback
            ? NativeContext->PresentVisibleRgba8(
                  SourcePixels, RefreshedResolved.Width,
                  RefreshedResolved.Height,
                  RefreshedResolved.Width * 4u, Presented,
                  PresentedWidth, PresentedHeight)
            : ERHIResult::InvalidState;
        Record(Result,
            bPreparedReadback &&
                MismatchedExtentResult == ERHIResult::InvalidState &&
                ReadbackResult == ERHIResult::Success &&
                PresentedWidth == RefreshedResolved.Width &&
                PresentedHeight == RefreshedResolved.Height &&
                Presented == SourcePixels,
            "Vulkan native readback/present round-trip is exact-size and performs no hidden resampling");

        if (Surface.Succeeded()) (void)Surface.Object->Invalidate();
        if (Device && Device->IsActive()) (void)Device->Shutdown();
        if (Window.IsRealWindow()) (void)Window.Destroy();
    }
#else
    Record(Result, true,
        "Vulkan output-transform native mapping is Unsupported on this build");
#endif
    return Result;
}
