#pragma once

#include "Application/FLabSettingsSnapshot.h"
#include <optional>

namespace Stoner::Application
{
// Resolved by the composition root from one display capability generation.
// No native handles, UI types or device queries belong to this policy service.
struct FLabOutputCapability
{
    Stoner::Core::FString ProfileId;
    float ReferenceWhiteNits = 100;
    float NativePackingWhiteNits = 100;
};
struct FLabDebugStage
{
    Stoner::Core::FString Name;
    Stoner::Renderer::ERenderGraphColorDomain Domain =
        Stoner::Renderer::ERenderGraphColorDomain::Unspecified;
};
struct FLabSettingsCapabilities
{
    Stoner::Core::uint64 DisplayGeneration = 0;
    Stoner::Core::TArray<FLabOutputCapability> Outputs;
    Stoner::Core::TArray<FLabDebugStage> DebugStages;
};
struct FLabSettingsTransaction
{
    Stoner::Core::uint64 Token = 0;
    FLabSettingsSnapshot Settings;
    bool bRequiresOutputTransition = false;
};
class FLabSettingsController
{
public:
    bool Initialize(const FLabSettingsSnapshot&, const FLabSettingsCapabilities&);
    bool Request(const FLabSettingsSnapshot&);
    // Caller supplies eligibility and completes only after required native work.
    const FLabSettingsTransaction* BeginEligible(bool bEligible);
    bool Complete(Stoner::Core::uint64 Token, bool bSuccess, bool bFormerOutputUsable);
    bool RefreshCapabilities(const FLabSettingsCapabilities&, bool bFormerOutputUsable);
    const FLabSettingsSnapshot& GetRequested() const { return Requested; }
    const FLabSettingsSnapshot& GetEffective() const { return Effective; }
    const std::optional<FLabSettingsSnapshot>& GetPending() const { return Pending; }
    const Stoner::Core::FString& GetFailure() const { return Failure; }
    bool IsPaused() const { return bPaused; }
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
