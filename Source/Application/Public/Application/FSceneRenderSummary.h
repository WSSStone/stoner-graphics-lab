#pragma once

#include "Application/FCameraComponent.h"
#include "Application/FEntity.h"
#include "Application/FLightComponent.h"
#include "Application/FMeshComponent.h"
#include "Application/FSceneDiagnostics.h"
#include "Core/CoreMinimal.h"

namespace Stoner::Application
{

struct FSceneRenderableItem
{
    FEntity Entity;
    Stoner::Core::FTransform WorldTransform = Stoner::Core::FTransform::Identity();
    Stoner::Core::FString MeshId;
    Stoner::Core::FString MaterialId;
    Stoner::Core::int32 SortKey = 0;
    bool bHasSortKey = false;
};

struct FSceneLightItem
{
    FEntity Entity;
    ESceneLightType LightType = ESceneLightType::Point;
    Stoner::Core::FColor Color = Stoner::Core::FColor::OpaqueWhite();
    float Intensity = 1.0f;
    float Range = 1.0f;
    Stoner::Core::FVector3 WorldPosition = Stoner::Core::FVector3::Zero();
    Stoner::Core::FVector3 WorldDirection = Stoner::Core::FVector3(0.0f, 0.0f, -1.0f);
    Stoner::Core::int32 SortKey = 0;
    bool bHasSortKey = false;
};

struct FSceneCameraItem
{
    FEntity Entity;
    Stoner::Core::FTransform WorldTransform = Stoner::Core::FTransform::Identity();
    FCameraComponent Camera;
    Stoner::Core::int32 SortKey = 0;
    bool bHasSortKey = false;
};

struct FSceneRejectedItem
{
    FEntity Entity;
    ESceneComponentType ComponentType = ESceneComponentType::Transform;
    ESceneResult Result = ESceneResult::Success;
    Stoner::Core::FString StableCode;
    Stoner::Core::FString Message;
};

class FSceneRenderSummary
{
public:
    void Clear();
    void AddRenderable(const FSceneRenderableItem& Item);
    void AddLight(const FSceneLightItem& Item);
    void AddCamera(const FSceneCameraItem& Item);
    void AddRejected(const FSceneRejectedItem& Item);
    void AddDiagnostic(const FSceneDiagnosticRecord& Record);
    void SortStable();

    [[nodiscard]] const Stoner::Core::TArray<FSceneRenderableItem>& GetRenderables() const noexcept { return Renderables; }
    [[nodiscard]] const Stoner::Core::TArray<FSceneLightItem>& GetLights() const noexcept { return Lights; }
    [[nodiscard]] const Stoner::Core::TArray<FSceneCameraItem>& GetCameras() const noexcept { return Cameras; }
    [[nodiscard]] const Stoner::Core::TArray<FSceneRejectedItem>& GetRejectedItems() const noexcept { return RejectedItems; }
    [[nodiscard]] const FSceneDiagnosticLog& GetDiagnostics() const noexcept { return Diagnostics; }
    [[nodiscard]] Stoner::Core::FString BuildDebugDump() const;

private:
    Stoner::Core::TArray<FSceneRenderableItem> Renderables;
    Stoner::Core::TArray<FSceneLightItem> Lights;
    Stoner::Core::TArray<FSceneCameraItem> Cameras;
    Stoner::Core::TArray<FSceneRejectedItem> RejectedItems;
    FSceneDiagnosticLog Diagnostics;
};

} // namespace Stoner::Application
