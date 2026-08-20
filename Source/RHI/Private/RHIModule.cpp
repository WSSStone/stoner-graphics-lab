#include "RHI/RHIMinimal.h"

#include <sstream>

namespace Stoner::RHI
{

void RHIInit()
{
    // Placeholder — RHI module initialization
}

bool IsValidRHIDeviceCapabilities(
    const FRHIDeviceCapabilities& Capabilities) noexcept
{
    if (!Capabilities.bSupportsGraphicsQueue ||
        !Capabilities.bSupportsTransferQueue ||
        !Capabilities.bSupportsSynchronization ||
        Capabilities.bSupportsPresentation !=
            Capabilities.bSupportsPresentQueue ||
        !Capabilities.HasValidFormatCapabilities() ||
        Capabilities.Formats.empty() ||
        !Capabilities.HasValidLimits())
    {
        return false;
    }
    return true;
}

Stoner::Core::FString DumpRHIDeviceCapabilities(
    const FRHIDeviceCapabilities& Capabilities)
{
    std::ostringstream Stream;
    Stream << "rhi-capabilities-v1\n"
           << "valid=" << (IsValidRHIDeviceCapabilities(Capabilities) ? 1 : 0) << '\n'
           << "queues=" << (Capabilities.bSupportsGraphicsQueue ? 1 : 0) << ','
           << (Capabilities.bSupportsComputeQueue ? 1 : 0) << ','
           << (Capabilities.bSupportsTransferQueue ? 1 : 0) << ','
           << (Capabilities.bSupportsPresentQueue ? 1 : 0) << '\n'
           << "resource-bytes=" << Capabilities.MaxBufferSizeBytes << ','
           << Capabilities.MaxResourceSizeBytes << '\n'
           << "texture-dimensions=" << Capabilities.MaxTextureDimension1D << ','
           << Capabilities.MaxTextureDimension2D << ','
           << Capabilities.MaxTextureDimension3D << ','
           << Capabilities.MaxTextureArrayLayers << '\n'
           << "stage-bindings=" << Capabilities.MaxPerStageBufferBindings << ','
           << Capabilities.MaxPerStageTextureBindings << ','
           << Capabilities.MaxPerStageSamplerBindings << '\n'
           << "constant-bytes=" << Capabilities.MaxConstantRangeBytes << ','
           << Capabilities.MaxConstantDataBytesPerStage << '\n'
           << "compute-group=" << Capabilities.MaxComputeThreadgroupSizeX << ','
           << Capabilities.MaxComputeThreadgroupSizeY << ','
           << Capabilities.MaxComputeThreadgroupSizeZ << ','
           << Capabilities.MaxComputeThreadsPerThreadgroup << '\n'
           << "compute-dispatch=" << Capabilities.MaxComputeDispatchGroupsX << ','
           << Capabilities.MaxComputeDispatchGroupsY << ','
           << Capabilities.MaxComputeDispatchGroupsZ << '\n'
           << "sample-mask=" << Capabilities.SupportedSampleCounts << '\n'
           << "formats=" << Capabilities.Formats.size() << '\n';
    return Stoner::Core::FString(Stream.str());
}

} // namespace Stoner::RHI
