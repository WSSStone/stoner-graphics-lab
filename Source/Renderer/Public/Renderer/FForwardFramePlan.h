#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FForwardDiagnostics.h"
#include "Renderer/FForwardLightData.h"
#include "Renderer/FForwardRenderGraphDeclaration.h"
#include "Renderer/FForwardViewData.h"
#include "Renderer/FMeshDrawCommand.h"

namespace Stoner::Renderer
{

struct FForwardStableId
{
    Stoner::Core::uint32 Value = 0;

    [[nodiscard]] bool IsValid() const noexcept { return IsStableForwardId(Value); }
};

using FForwardObjectId = FForwardStableId;
using FForwardMeshId = FForwardStableId;
using FForwardMaterialId = FForwardStableId;
using FForwardPassId = FForwardStableId;
using FForwardOutputId = FForwardStableId;
using FForwardFrameId = FForwardStableId;

enum class EForwardPassStage
{
    Depth,
    Opaque,
    SkyBackground,
    Transparent
};

enum class EForwardBackgroundMode
{
    Clear,
    Sky,
    EnvironmentReference
};

struct FForwardOutputTarget
{
    Stoner::Core::FString ColorTargetName;
    Stoner::Core::FString DepthTargetName;
    Stoner::Core::FString FormatSummary = "RGBA8";
    FForwardExtent2D Extent;

    [[nodiscard]] bool IsValid(const FForwardViewData& View, FForwardDiagnosticLog* Diagnostics = nullptr) const;
};

struct FForwardEnvironmentBackground
{
    EForwardBackgroundMode Mode = EForwardBackgroundMode::Clear;
    Stoner::Core::FString BackgroundName = "Clear";
    Stoner::Core::FString ResourceReference;
};

struct FForwardPassRecord
{
    FForwardPassId PassId;
    EForwardPassStage Stage = EForwardPassStage::Depth;
    Stoner::Core::FString Name;
    Stoner::Core::uint32 DrawCount = 0;
};

struct FForwardAmbientFallbackRecord
{
    bool bActive = false;
    Stoner::Core::FColor AmbientColor = Stoner::Core::FColor(0.03f, 0.03f, 0.03f, 1.0f);
};

struct FForwardFramePlan
{
    FForwardFrameId FrameId;
    Stoner::Core::FString FrameName;
    FForwardViewData ViewData;
    FForwardOutputTarget OutputTarget;
    FForwardEnvironmentBackground Environment;
    FForwardAmbientFallbackRecord AmbientFallback;
    FForwardLightSet LightSet;
    Stoner::Core::TArray<FForwardPassRecord> PassOrder;
    Stoner::Core::TArray<FMeshDrawCommand> AcceptedOpaqueDraws;
    Stoner::Core::TArray<FMeshDrawCommand> AcceptedTransparentDraws;
    Stoner::Core::TArray<FMeshDrawCandidate> RejectedDraws;
    FForwardRenderGraphDeclaration GraphDeclaration;
    FForwardDiagnosticLog Diagnostics;
    Stoner::Core::FString DebugDump;
    bool bValid = false;

    void Reset();
    void AddPass(EForwardPassStage Stage, Stoner::Core::FString Name, Stoner::Core::uint32 DrawCount);
    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] bool HasRenderableGeometry() const noexcept;
    [[nodiscard]] const FForwardPassRecord* FindPass(EForwardPassStage Stage) const noexcept;
};

[[nodiscard]] bool ValidateForwardOutputTarget(const FForwardOutputTarget& Output,
    const FForwardViewData& View,
    FForwardDiagnosticLog* Diagnostics = nullptr);
[[nodiscard]] FForwardEnvironmentBackground ValidateForwardEnvironmentBackground(const FForwardEnvironmentBackground& Environment,
    bool bEnableSkyBackground,
    FForwardDiagnosticLog* Diagnostics = nullptr);
[[nodiscard]] const char* ToString(EForwardPassStage Stage) noexcept;
[[nodiscard]] const char* ToString(EForwardBackgroundMode Mode) noexcept;

} // namespace Stoner::Renderer
