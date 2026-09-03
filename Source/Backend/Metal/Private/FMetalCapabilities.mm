#include "FMetalCapabilities.h"

#include "FMetalFormat.h"

#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    #define GLFW_INCLUDE_NONE
    #define GLFW_EXPOSE_NATIVE_COCOA
    #include <GLFW/glfw3.h>
    #include <GLFW/glfw3native.h>
#endif

#import <AppKit/AppKit.h>
#import <Metal/Metal.h>

#include <algorithm>
#include <cmath>
#include <limits>
#include <sstream>

namespace Stoner::Backend::Metal::Private
{

RHI::FRHIDeviceCapabilities QueryMetalCapabilities(
    void* NativeDevice) noexcept
{
    RHI::FRHIDeviceCapabilities Result;
    if (NativeDevice == nullptr) return Result;
    @autoreleasepool
    {
        id<MTLDevice> Device = (__bridge id<MTLDevice>)NativeDevice;
        Result.bSupportsGraphicsQueue = true;
        Result.bSupportsComputeQueue = true;
        Result.bSupportsTransferQueue = true;
        Result.bSupportsPresentQueue = true;
        Result.bSupportsPresentation = true;
        Result.bSupportsSynchronization = true;
        Result.MaxInFlightFrames = 3;
        Result.MaxCommandBuffersPerQueue = 4096;
        Result.MaxQueuesPerType = 1;
        Result.MaxBufferSizeBytes = Device.maxBufferLength;
        Result.MaxResourceSizeBytes = Device.maxBufferLength;
        Result.MaxTextureDimension1D = 16384;
        Result.MaxTextureDimension2D = 16384;
        Result.MaxTextureDimension3D = 2048;
        Result.MaxTextureArrayLayers = 2048;
        Result.MaxPerStageBufferBindings = 31;
        Result.MaxPerStageTextureBindings = 128;
        Result.MaxPerStageSamplerBindings = 16;
        Result.MaxConstantRangeBytes = 4096;
        Result.MaxConstantDataBytesPerStage = 4096;
        const MTLSize Threads = Device.maxThreadsPerThreadgroup;
        Result.MaxComputeThreadgroupSizeX = static_cast<Core::uint32>(Threads.width);
        Result.MaxComputeThreadgroupSizeY = static_cast<Core::uint32>(Threads.height);
        Result.MaxComputeThreadgroupSizeZ = static_cast<Core::uint32>(Threads.depth);
        Result.MaxComputeThreadsPerThreadgroup = static_cast<Core::uint32>(
            std::min<NSUInteger>(Device.maxThreadsPerThreadgroup.width,
                std::numeric_limits<Core::uint32>::max()));
        Result.MaxComputeDispatchGroupsX = 65535;
        Result.MaxComputeDispatchGroupsY = 65535;
        Result.MaxComputeDispatchGroupsZ = 65535;
        for (RHI::ERHISampleCount Count : {
                 RHI::ERHISampleCount::One,
                 RHI::ERHISampleCount::Two,
                 RHI::ERHISampleCount::Four,
                 RHI::ERHISampleCount::Eight})
        {
            if ([Device supportsTextureSampleCount:
                    static_cast<NSUInteger>(Count)])
                Result.SupportedSampleCounts |=
                    static_cast<Core::uint32>(Count);
        }
        for (int Value = static_cast<int>(RHI::ERHIFormat::Unknown) + 1;
             Value < static_cast<int>(RHI::ERHIFormat::Count); ++Value)
        {
            const auto Format = static_cast<RHI::ERHIFormat>(Value);
            if (IsMetalFormatSupported(NativeDevice, Format))
                Result.Formats.push_back(RHI::MakeRHIFormatCapabilities(Format));
        }
    }
    return Result;
}

RHI::ERHIResult QueryMetalPresentationCapabilities(
    void* NativeDevice,
    const Core::FPlatformWindow& Window,
    Core::uint64 SurfaceId,
    Core::uint64 CapabilityGeneration,
    RHI::FRHIPresentationCapabilities& OutCapabilities) noexcept
{
    OutCapabilities = {};
#if !defined(STONER_GLFW_AVAILABLE) || !STONER_GLFW_AVAILABLE
    (void)NativeDevice;
    (void)Window;
    (void)SurfaceId;
    (void)CapabilityGeneration;
    return RHI::ERHIResult::Unsupported;
#else
    if (NativeDevice == nullptr || !Window.IsValid() ||
        SurfaceId == 0 || CapabilityGeneration == 0)
        return RHI::ERHIResult::InvalidState;

    __block CGFloat CurrentHeadroom = 1.0;
    __block CGFloat PotentialHeadroom = 1.0;
    __block bool bHasScreen = false;
    const auto QueryOnMain = ^{
        auto* NativeWindow = static_cast<GLFWwindow*>(
            Window.GetNativeHandle());
        NSWindow* CocoaWindow = NativeWindow
            ? glfwGetCocoaWindow(NativeWindow) : nil;
        NSScreen* Screen = CocoaWindow.screen;
        if (!Screen) return;
        CurrentHeadroom = std::max<CGFloat>(
            1.0, Screen.maximumExtendedDynamicRangeColorComponentValue);
        PotentialHeadroom = std::max<CGFloat>(
            CurrentHeadroom,
            Screen.maximumPotentialExtendedDynamicRangeColorComponentValue);
        bHasScreen = std::isfinite(CurrentHeadroom) &&
            std::isfinite(PotentialHeadroom);
    };
    if ([NSThread isMainThread]) QueryOnMain();
    else dispatch_sync(dispatch_get_main_queue(), QueryOnMain);
    if (!bHasScreen) return RHI::ERHIResult::Unavailable;

    OutCapabilities.SurfaceId = SurfaceId;
    OutCapabilities.CapabilityGeneration = CapabilityGeneration;
    OutCapabilities.SupportedPairs.push_back({
        RHI::ERHIFormat::B8G8R8A8_UNorm,
        RHI::ERHIPresentationColorSpace::SrgbNonlinear});
    OutCapabilities.SupportedPairs.push_back({
        RHI::ERHIFormat::R8G8B8A8_UNorm,
        RHI::ERHIPresentationColorSpace::SrgbNonlinear});
    OutCapabilities.SupportedPairs.push_back({
        RHI::ERHIFormat::B8G8R8A8_UNorm,
        RHI::ERHIPresentationColorSpace::Bt709Nonlinear});
    OutCapabilities.SupportedPairs.push_back({
        RHI::ERHIFormat::B8G8R8A8_UNorm,
        RHI::ERHIPresentationColorSpace::SdrPassThrough});
    const bool bExtendedRange = PotentialHeadroom > 1.0;
    if (bExtendedRange && IsMetalFormatSupported(
            NativeDevice, RHI::ERHIFormat::R10G10B10A2_UNorm))
    {
        OutCapabilities.SupportedPairs.push_back({
            RHI::ERHIFormat::R10G10B10A2_UNorm,
            RHI::ERHIPresentationColorSpace::Hdr10St2084});
    }
    if (bExtendedRange && IsMetalFormatSupported(
            NativeDevice, RHI::ERHIFormat::R16G16B16A16_Float))
    {
        OutCapabilities.SupportedPairs.push_back({
            RHI::ERHIFormat::R16G16B16A16_Float,
            RHI::ERHIPresentationColorSpace::ExtendedSrgbLinear});
    }
    // Feature 029 deliberately does not opt into CAEDRMetadata system tone
    // mapping. HDR10 static metadata remains Renderer/profile intent and may
    // be applied by backends with a compatible native mechanism (Vulkan).
    OutCapabilities.bSupportsHDRMetadata = false;
    OutCapabilities.bSupportsExtendedRange = bExtendedRange;
    // Apple EDR code value 1.0 denotes the SDR/reference-white signal. The
    // public API exposes headroom ratios rather than physical panel nits, so
    // Feature 029 uses its declared 100-nit reference signal and never claims
    // a photometric display measurement.
    OutCapabilities.NativeReferenceWhiteNits = 100.0f;
    OutCapabilities.CurrentHeadroom =
        static_cast<float>(CurrentHeadroom);
    OutCapabilities.PotentialHeadroom =
        static_cast<float>(PotentialHeadroom);
    std::ostringstream Digest;
    Digest << "metal-presentation-capabilities-v1|surface=" << SurfaceId
           << "|generation=" << CapabilityGeneration
           << "|current=" << OutCapabilities.CurrentHeadroom
           << "|potential=" << OutCapabilities.PotentialHeadroom
           << "|hdr=" << (bExtendedRange ? 1 : 0);
    OutCapabilities.CapabilityDigest = Digest.str().c_str();
    return OutCapabilities.IsValid()
        ? RHI::ERHIResult::Success
        : RHI::ERHIResult::Failed;
#endif
}

} // namespace Stoner::Backend::Metal::Private
