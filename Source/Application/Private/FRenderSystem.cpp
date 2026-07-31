#include "Application/FRenderSystem.h"

#include "Application/FWorld.h"

namespace Stoner::Application
{

namespace
{

void Reject(FSceneRenderSummary& Summary,
    FEntity Entity,
    ESceneComponentType ComponentType,
    ESceneResult Result,
    const char* StableCode,
    const char* Message)
{
    Summary.AddRejected(FSceneRejectedItem{
        Entity,
        ComponentType,
        Result,
        StableCode,
        Message,
    });
}

} // namespace

FSceneRenderSummary FRenderSystem::Collect(const FWorld& World)
{
    FSceneRenderSummary Summary;
    for (const FEntity& Entity : World.GetLiveEntities())
    {
        Stoner::Core::FTransform WorldTransform;
        const bool bHasWorldTransform = World.TryGetWorldTransform(Entity, WorldTransform);

        if (const FMeshComponent* Mesh = World.GetMesh(Entity))
        {
            if (!bHasWorldTransform)
            {
                Reject(Summary, Entity, ESceneComponentType::Transform, ESceneResult::MissingComponent, "SCENE-RENDER-MISSING-TRANSFORM", "Renderable is missing transform component");
            }
            else if (!Mesh->IsValid())
            {
                Reject(Summary, Entity, ESceneComponentType::Mesh, ESceneResult::InvalidComponentData, "SCENE-RENDER-INVALID-MESH", "Renderable mesh identity is invalid");
            }
            else
            {
                Summary.AddRenderable(FSceneRenderableItem{
                    Entity,
                    WorldTransform,
                    Mesh->MeshId,
                    Mesh->MaterialId,
                    Mesh->OptionalSortKey,
                    Mesh->bHasSortKey,
                });
            }
        }

        if (const FLightComponent* Light = World.GetLight(Entity))
        {
            if (!bHasWorldTransform)
            {
                Reject(Summary, Entity, ESceneComponentType::Transform, ESceneResult::MissingComponent, "SCENE-RENDER-MISSING-TRANSFORM", "Light is missing transform component");
            }
            else if (!Light->IsValid())
            {
                Reject(Summary, Entity, ESceneComponentType::Light, ESceneResult::InvalidComponentData, "SCENE-RENDER-INVALID-LIGHT", "Light component data is invalid");
            }
            else
            {
                Summary.AddLight(FSceneLightItem{
                    Entity,
                    Light->LightType,
                    Light->Color,
                    Light->Intensity,
                    Light->Range,
                    WorldTransform.Translation,
                    WorldTransform.Rotation.RotateVector(Stoner::Core::FCoordinateConvention::Forward()).GetSafeNormal(),
                    Light->OptionalSortKey,
                    Light->bHasSortKey,
                });
            }
        }

        if (const FCameraComponent* Camera = World.GetCamera(Entity))
        {
            if (!bHasWorldTransform)
            {
                Reject(Summary, Entity, ESceneComponentType::Transform, ESceneResult::MissingComponent, "SCENE-RENDER-MISSING-TRANSFORM", "Camera is missing transform component");
            }
            else if (!Camera->IsValid())
            {
                Reject(Summary, Entity, ESceneComponentType::Camera, ESceneResult::InvalidComponentData, "SCENE-RENDER-INVALID-CAMERA", "Camera component data is invalid");
            }
            else
            {
                Summary.AddCamera(FSceneCameraItem{
                    Entity,
                    WorldTransform,
                    *Camera,
                    Camera->OptionalSortKey,
                    Camera->bHasSortKey,
                });
            }
        }
    }

    Summary.SortStable();
    return Summary;
}

} // namespace Stoner::Application
