#include "VulkanRHI/FVulkanSurface.h"

namespace Stoner::Backend::Vulkan
{

struct FVulkanSurface::FState
{
    Stoner::RHI::FRHIPresentationSurfaceDesc Desc;
    std::weak_ptr<FVulkanPresentationOwnerState> Owner;
    Stoner::Core::FString DiagnosticReason;
    bool bValid = false;
};

FVulkanSurface::FVulkanSurface()
    : State(std::make_shared<FState>())
{
}

Stoner::RHI::ERHIResult FVulkanSurface::Create(
    const Stoner::RHI::FRHIPresentationSurfaceDesc& Desc,
    const std::shared_ptr<FVulkanPresentationOwnerState>& Owner,
    FVulkanSurface& OutSurface)
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
    OutSurface.State->Owner = Owner;
    OutSurface.State->bValid = true;
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

Stoner::RHI::ERHIResult FVulkanSurface::Invalidate()
{
    if (!State->bValid)
    {
        return Stoner::RHI::ERHIResult::InvalidState;
    }

    State->bValid = false;
    State->Desc.Window.Clear();
    State->DiagnosticReason = "surface invalidated";
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace Stoner::Backend::Vulkan
