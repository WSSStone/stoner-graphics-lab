#include "MetalRHI/FMetalDeviceFactory.h"

#include "Core/SGPlatform.h"
#include "FMetalFailureInjector.h"

#if !SG_PLATFORM_MAC

namespace Stoner::Backend::Metal
{

FMetalDeviceCreateResult CreateMetalDevice(
    const FMetalBackendConfig&) noexcept
{
    FMetalDeviceCreateResult Result;
    if (Private::FMetalFailureInjector::ShouldFail(
            Private::EMetalFailurePoint::DeviceInitialization))
    {
        Result.Result = RHI::ERHIResult::Failed;
        Result.Diagnostics.Records.push_back({
            Core::FString("CreateMetalDevice"),
            Core::FString("initialization"),
            Result.Result,
            Core::FString(Private::ToStableName(
                Private::EMetalFailurePoint::DeviceInitialization))});
        return Result;
    }
    Result.Result = RHI::ERHIResult::Unsupported;
    Result.Diagnostics.Records.push_back({
        Core::FString("CreateMetalDevice"),
        Core::FString("host"),
        RHI::ERHIResult::Unsupported,
        Core::FString("metal-host-unsupported")});
    return Result;
}

bool InspectMetalDevice(
    const Core::TSharedPtr<RHI::IRHIDevice>&,
    FMetalBackendInspection& OutInspection) noexcept
{
    OutInspection = {};
    return false;
}

bool InspectMetalDiagnostics(
    const Core::TSharedPtr<RHI::IRHIDevice>&,
    FMetalBackendDiagnostics& OutDiagnostics) noexcept
{
    OutDiagnostics = {};
    return false;
}

RHI::ERHIResult ReadMetalBufferForValidation(
    const Core::TSharedPtr<RHI::IRHIDevice>&,
    const Core::TSharedPtr<RHI::IRHIBuffer>&,
    Core::uint64,
    Core::uint64,
    Core::TArray<Core::uint8>& OutBytes) noexcept
{
    OutBytes.clear();
    return RHI::ERHIResult::Unsupported;
}

} // namespace Stoner::Backend::Metal

#endif
