#pragma once

#include "Application/FLabSettingsSnapshot.h"
#include <optional>

namespace Stoner::Application
{
class FLabSettingsController
{
public:
    bool Initialize(const FLabSettingsSnapshot&, const FLabSettingsCapabilities&);
    bool Request(const FLabSettingsSnapshot&);
    // Preset preparation never substitutes fallback for unsupported intent.
    bool RequestStrict(const FLabSettingsSnapshot&);
    // Caller supplies eligibility and completes only after required native work.
    const FLabSettingsTransaction* BeginEligible(bool bEligible);
    bool Complete(Stoner::Core::uint64 Token, bool bSuccess, bool bFormerOutputUsable);
    bool RefreshCapabilities(const FLabSettingsCapabilities&, bool bFormerOutputUsable);
    const std::optional<FLabSettingsTransaction>& GetActive() const { return Active; }
    const FLabSettingsSnapshot& GetRequested() const { return Requested; }
    const FLabSettingsSnapshot& GetEffective() const { return Effective; }
    const FLabSettingsCapabilities& GetCapabilities() const { return Capabilities; }
    const std::optional<FLabSettingsSnapshot>& GetPending() const { return Pending; }
    const Stoner::Core::FString& GetFailure() const { return Failure; }
    bool IsPaused() const { return bPaused; }
    void MarkOutputUnusable() { bPaused = true; }
private:
    bool Resolve(const FLabSettingsSnapshot&, FLabSettingsSnapshot&, bool bAllowFallback);
    bool Queue(const FLabSettingsSnapshot&, bool bAllowFallback);
    FLabSettingsCapabilities Capabilities;
    FLabSettingsSnapshot Requested, Effective;
    std::optional<FLabSettingsSnapshot> Pending;
    std::optional<FLabSettingsTransaction> Active;
    Stoner::Core::FString Failure;
    Stoner::Core::uint64 Revision = 0, NextToken = 0;
    bool bInitialized = false, bPaused = true;
};
}
