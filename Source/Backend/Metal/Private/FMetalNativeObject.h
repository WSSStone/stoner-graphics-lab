#pragma once

#include "FMetalDeviceOwnerState.h"
#include "RHI/ERHIPipelineState.h"
#include "RHI/ERHIResult.h"

#include <atomic>

namespace Stoner::Backend::Metal::Private
{

class FMetalNativeObject
{
public:
    explicit FMetalNativeObject(
        Core::TSharedPtr<FMetalDeviceOwnerState> Owner,
        EMetalOwnershipCategory Category) noexcept
        : Owner_(std::move(Owner)),
          Category_(Category),
          OwnerIdentity_(Owner_ ? Owner_->GetOwnerIdentity() : 0),
          Generation_(Owner_ ? Owner_->GetGeneration() : 0),
          bRegistered_(Owner_ && Owner_->TryRegisterObject(Category_))
    {
        if (!bRegistered_)
            Lifecycle_.store(
                RHI::ERHIResourceLifecycleState::Invalidated,
                std::memory_order_release);
    }

    virtual ~FMetalNativeObject()
    {
        ReleaseRegistration();
    }

    [[nodiscard]] bool IsCompatible(
        const Core::TSharedPtr<FMetalDeviceOwnerState>& Owner) const noexcept
    {
        return Owner && Owner.get() == Owner_.get() &&
            Lifecycle_.load(std::memory_order_acquire) ==
                RHI::ERHIResourceLifecycleState::Valid &&
            Owner->IsCompatible(OwnerIdentity_, Generation_);
    }

    [[nodiscard]] RHI::ERHIResourceLifecycleState GetLifecycle() const noexcept
    {
        const auto Lifecycle = Lifecycle_.load(std::memory_order_acquire);
        if (Lifecycle != RHI::ERHIResourceLifecycleState::Valid)
            return Lifecycle;
        return Owner_ && Owner_->IsCompatible(OwnerIdentity_, Generation_)
            ? RHI::ERHIResourceLifecycleState::Valid
            : RHI::ERHIResourceLifecycleState::Invalidated;
    }

protected:
    [[nodiscard]] RHI::ERHIResult InvalidateObject() noexcept
    {
        auto Expected = RHI::ERHIResourceLifecycleState::Valid;
        if (!Lifecycle_.compare_exchange_strong(
                Expected,
                RHI::ERHIResourceLifecycleState::Invalidated,
                std::memory_order_acq_rel))
            return RHI::ERHIResult::InvalidState;
        ReleaseRegistration();
        return RHI::ERHIResult::Success;
    }

    [[nodiscard]] const Core::TSharedPtr<FMetalDeviceOwnerState>&
    GetOwner() const noexcept
    {
        return Owner_;
    }

private:
    void ReleaseRegistration() noexcept
    {
        if (bRegistered_.exchange(false, std::memory_order_acq_rel) && Owner_)
            Owner_->ReleaseObject(Category_);
    }

    Core::TSharedPtr<FMetalDeviceOwnerState> Owner_;
    EMetalOwnershipCategory Category_;
    Core::uint64 OwnerIdentity_ = 0;
    Core::uint64 Generation_ = 0;
    std::atomic<RHI::ERHIResourceLifecycleState> Lifecycle_{
        RHI::ERHIResourceLifecycleState::Valid};
    std::atomic<bool> bRegistered_{false};
};

} // namespace Stoner::Backend::Metal::Private
