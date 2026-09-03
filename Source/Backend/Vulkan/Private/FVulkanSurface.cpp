#include "VulkanRHI/FVulkanSurface.h"

#include "VulkanRHI/FVulkanNativeContext.h"

#include <cstdint>

namespace Stoner::Backend::Vulkan
{

struct FVulkanSurface::FState
{
    Stoner::RHI::FRHIPresentationSurfaceDesc Desc;
    std::weak_ptr<FVulkanPresentationOwnerState> Owner;
    Stoner::Core::FString DiagnosticReason;
    Stoner::RHI::FRHIPresentationCapabilities Capabilities;
    Stoner::Core::TSharedPtr<FVulkanNativeContext> NativeContext;
    bool bValid = false;
};

FVulkanSurface::FVulkanSurface()
    : State(std::make_shared<FState>())
{
}

Stoner::RHI::ERHIResult FVulkanSurface::Create(
    const Stoner::RHI::FRHIPresentationSurfaceDesc& Desc,
    const std::shared_ptr<FVulkanPresentationOwnerState>& Owner,
    FVulkanSurface& OutSurface,
    Stoner::Core::TSharedPtr<FVulkanNativeContext> NativeContext)
{
    OutSurface = FVulkanSurface{};
    if (!Desc.IsValid())
    {
        OutSurface.State->DiagnosticReason = "invalid Core platform window wrapper";
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (!Owner || !Owner->bActive)
    {
        OutSurface.State->DiagnosticReason = "inactive Vulkan presentation owner";
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    OutSurface.State->Desc = Desc;
    if (OutSurface.State->Desc.SurfaceId == 0)
    {
        OutSurface.State->Desc.SurfaceId = static_cast<Stoner::Core::uint64>(
            reinterpret_cast<std::uintptr_t>(
                Desc.Window.GetNativeHandle()));
    }
    OutSurface.State->Owner = Owner;
    OutSurface.State->Capabilities.SurfaceId =
        OutSurface.State->Desc.SurfaceId;
    OutSurface.State->Capabilities.CapabilityGeneration = 1;
    OutSurface.State->Capabilities.SupportedPairs.push_back({
        Stoner::RHI::ERHIFormat::B8G8R8A8_UNorm,
        Stoner::RHI::ERHIPresentationColorSpace::SrgbNonlinear});
    OutSurface.State->Capabilities.CapabilityDigest =
        "vulkan-surface-capabilities-pending-native-enumeration";
    OutSurface.State->NativeContext = std::move(NativeContext);
    if (OutSurface.State->NativeContext)
    {
        Stoner::RHI::FRHIPresentationCapabilities NativeCapabilities;
        const auto NativeResult = OutSurface.State->NativeContext
            ->QueryVisiblePresentationCapabilities(NativeCapabilities);
        if (NativeResult != Stoner::RHI::ERHIResult::Success)
        {
            OutSurface.State->DiagnosticReason =
                "native Vulkan surface capability enumeration failed";
            OutSurface.State->NativeContext.reset();
            return NativeResult;
        }
        NativeCapabilities.SurfaceId = OutSurface.State->Desc.SurfaceId;
        OutSurface.State->Capabilities = std::move(NativeCapabilities);
    }
    OutSurface.State->bValid = true;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanSurface::QueryCapabilities(
    Stoner::RHI::FRHIPresentationCapabilities& OutCapabilities) const
{
    OutCapabilities = {};
    if (!IsValid() || !State->Capabilities.IsValid())
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    OutCapabilities = State->Capabilities;
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::Core::uint64 FVulkanSurface::GetCapabilityGeneration() const noexcept
{
    return IsValid() ? State->Capabilities.CapabilityGeneration : 0;
}

Stoner::RHI::ERHIResult
FVulkanSurface::NotifyPresentationEnvironmentChanged()
{
    if (!IsValid())
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    if (State->NativeContext)
    {
        Stoner::RHI::FRHIPresentationCapabilities Refreshed;
        const auto Result = State->NativeContext
            ->RefreshVisiblePresentationCapabilities(Refreshed);
        if (Result != Stoner::RHI::ERHIResult::Success)
        {
            return Result;
        }
        Refreshed.SurfaceId = State->Desc.SurfaceId;
        State->Capabilities = std::move(Refreshed);
    }
    else
    {
        ++State->Capabilities.CapabilityGeneration;
        State->Capabilities.CapabilityDigest =
            "vulkan-surface-capabilities-refresh-required";
    }
    return Stoner::RHI::ERHIResult::Success;
}

Stoner::RHI::ERHIResult FVulkanSurface::UpdateCapabilities(
    const Stoner::RHI::FRHIPresentationCapabilities& Capabilities)
{
    if (!IsValid() || !Capabilities.IsValid() ||
        Capabilities.SurfaceId != State->Desc.SurfaceId ||
        Capabilities.CapabilityGeneration <
            State->Capabilities.CapabilityGeneration)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }
    State->Capabilities = Capabilities;
    return Stoner::RHI::ERHIResult::Success;
}

const Stoner::RHI::FRHIPresentationSurfaceDesc& FVulkanSurface::GetDesc() const noexcept
{
    return State->Desc;
}

bool FVulkanSurface::IsValid() const noexcept
{
    const auto Owner = State->Owner.lock();
    return State->bValid && State->Desc.IsValid() && Owner && Owner->bActive;
}

void* FVulkanSurface::GetNativeHandle() const noexcept
{
    return IsValid() ? State->Desc.Window.GetNativeHandle() : nullptr;
}

const char* FVulkanSurface::GetDiagnosticReason() const noexcept
{
    return State->DiagnosticReason.CStr();
}

bool FVulkanSurface::BelongsTo(
    const std::shared_ptr<FVulkanPresentationOwnerState>& Owner) const noexcept
{
    const auto CurrentOwner = State->Owner.lock();
    return Owner && CurrentOwner && Owner == CurrentOwner;
}

Stoner::Core::TSharedPtr<FVulkanNativeContext>
FVulkanSurface::GetNativeContext() const noexcept
{
    return IsValid() ? State->NativeContext : nullptr;
}

Stoner::RHI::ERHIResult FVulkanSurface::Invalidate()
{
    if (!State->bValid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    State->bValid = false;
    ++State->Capabilities.CapabilityGeneration;
    State->Desc.Window.Clear();
    State->DiagnosticReason = "surface invalidated";
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
