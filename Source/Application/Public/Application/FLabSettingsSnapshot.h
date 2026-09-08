#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FPostProcessInsertion.h"
#include "Renderer/FRenderGraphResource.h"
#include <functional>

namespace Stoner::Application
{

struct FLabDebugBypass
{
    Stoner::Renderer::EOutputTransformDebugBypassMode Mode =
        Stoner::Renderer::EOutputTransformDebugBypassMode::Disabled;
    Stoner::Core::FString StageName;
    Stoner::Renderer::ERenderGraphColorDomain SourceDomain =
        Stoner::Renderer::ERenderGraphColorDomain::Unspecified;
    float VisualizationMinimum = 0.0f;
    float VisualizationMaximum = 1.0f;

    [[nodiscard]] bool IsValid() const noexcept;

    // IsValid performs structural checks only. The later settings controller
    // supplies the freshly resolved stage and domain through this seam; no
    // stage registry is owned by this value type.
    [[nodiscard]] bool IsValidForResolvedStageDomain(
        const Stoner::Core::FString& ResolvedStageName,
        Stoner::Renderer::ERenderGraphColorDomain ResolvedSourceDomain) const
        noexcept;
};

struct FLabSettingsSnapshot
{
    Stoner::Core::uint64 CameraRevision = 0;
    Stoner::Core::uint64 SettingsRevision = 0;
    Stoner::Core::uint64 DisplayGeneration = 0;
    Stoner::Core::uint64 OutputModeGeneration = 0;
    Stoner::Core::FString RequestedProfileId;
    Stoner::Core::FString EffectiveProfileId;
    Stoner::Core::FString SdrToneMapVersion;
    Stoner::Core::FString HdrViewingVersion;
    float ExposureStops = 0.0f;
    FLabDebugBypass DebugBypass;
    bool bUIVisible = true;
    float UIWhiteMultiplier = 1.0f;
    float UIReferenceWhiteNits = 100.0f;
    float NativePackingWhiteNits = 100.0f;

    [[nodiscard]] bool IsValid() const noexcept;
};

// Sampled facts from the live composition root; unavailable GPU timing is
// intentionally absent instead of represented as a fabricated zero.
struct FLabRuntimeInfo
{
    Stoner::Core::FString Workload, RootIdentity, CookedGeneration;
    Stoner::Core::FString RequestedProfile, EffectiveProfile, TransformVersion;
    Stoner::Core::FString Failure;
    float ExposureStops = 0;
    Stoner::Core::uint64 Submitted = 0, RenderCompleted = 0, PresentQueued = 0;
    Stoner::Core::uint64 UIFrames = 0, SceneFallbackFrames = 0;
};

// Feature-owned controls prepare a complete candidate; only the session may
// admit it. Callbacks must be bounded and must not perform native/UI work.
struct FLabSectionCommand
{
    Stoner::Core::FString Id, Label;
    std::function<bool(FLabSettingsSnapshot&)> PrepareEdit;
};
struct FLabSectionDebugView
{
    Stoner::Core::FString Id, Label;
    FLabDebugBypass Selection;
};
struct FLabControlSection
{
    Stoner::Core::FString Id, Title;
    Stoner::Core::TArray<FLabSectionCommand> Commands;
    Stoner::Core::TArray<FLabSectionDebugView> DebugViews;
};

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
    // Empty applies to every supported output; otherwise this exact profile.
    Stoner::Core::FString ProfileId{};
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

} // namespace Stoner::Application
