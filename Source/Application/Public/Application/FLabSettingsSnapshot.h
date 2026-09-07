#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FPostProcessInsertion.h"
#include "Renderer/FRenderGraphResource.h"

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

} // namespace Stoner::Application
