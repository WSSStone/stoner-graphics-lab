#include "ApplicationSceneEcsTests.h"

#include "Application/ApplicationMinimal.h"

#include <iostream>
#include <limits>
#include <string>

namespace
{

using namespace Stoner::Application;
using namespace Stoner::Core;

void Record(FApplicationSceneEcsTestResult& Result, bool bPassed, const char* Name)
{
    if (bPassed)
    {
        ++Result.Passed;
        std::cout << "[PASS] " << Name << '\n';
    }
    else
    {
        ++Result.Failed;
        std::cout << "[FAIL] " << Name << '\n';
    }
}

FTransformComponent TransformAt(float X, float Y = 0.0f, float Z = 0.0f)
{
    return FTransformComponent(FTransform(FVector3(X, Y, Z), FQuat::Identity(), FVector3(1.0f, 1.0f, 1.0f)));
}

bool NearTransformPosition(const FTransform& Transform, float X, float Y, float Z)
{
    return Transform.Translation.NearlyEquals(FVector3(X, Y, Z));
}

void TestEntityLifecycle(FApplicationSceneEcsTestResult& Result)
{
    FWorld World(128);
    TArray<FEntity> Entities;
    for (int Index = 0; Index < 100; ++Index)
    {
        Entities.push_back(World.CreateEntity());
    }
    Record(Result, World.GetLiveEntityCount() == 100 && Entities.front().IsSet() && Entities.back().IsSet(),
        "Scene world creates at least 100 live entities");

    Record(Result, World.DestroyEntity(Entities[10]) == ESceneResult::Success &&
            !World.IsEntityLive(Entities[10]) &&
            World.ValidateEntity(Entities[10]) == ESceneResult::StaleEntity,
        "Scene entity destroy invalidates original handle by generation");

    const FEntity Reused = World.CreateEntity();
    Record(Result, Reused.SlotIndex == Entities[10].SlotIndex &&
            Reused.Generation != Entities[10].Generation &&
            World.ValidateEntity(Entities[10]) == ESceneResult::StaleEntity &&
            World.IsEntityLive(Reused),
        "Scene world reuses destroyed slots without reviving stale handles");

    World.Reset();
    Record(Result, World.GetLiveEntityCount() == 0 &&
            !World.IsEntityLive(Reused) &&
            World.GetDiagnostics().IsEmpty(),
        "Scene world reset clears state and invalidates old handles");
}

void TestComponents(FApplicationSceneEcsTestResult& Result)
{
    FWorld World;
    const FEntity Entity = World.CreateEntity();
    const FTransformComponent FirstTransform = TransformAt(1.0f);
    const FTransformComponent SecondTransform = TransformAt(2.0f);
    Record(Result, World.AddTransform(Entity, FirstTransform) == ESceneResult::Success &&
            World.GetTransform(Entity) != nullptr &&
            World.GetTransform(Entity)->LocalTransform.Translation.X == 1.0f,
        "Scene transform add/read stores requested values");
    Record(Result, World.AddTransform(Entity, SecondTransform) == ESceneResult::DuplicateComponent &&
            World.GetTransform(Entity)->LocalTransform.Translation.X == 1.0f &&
            World.GetDiagnostics().CountByCode("SCENE-COMPONENT-DUPLICATE") == 1,
        "Scene duplicate component add is rejected without overwrite");
    Record(Result, World.UpdateTransform(Entity, SecondTransform) == ESceneResult::Success &&
            World.GetTransform(Entity)->LocalTransform.Translation.X == 2.0f,
        "Scene transform update explicitly mutates existing component");
    Record(Result, World.RemoveTransform(Entity) == ESceneResult::Success &&
            World.RemoveTransform(Entity) == ESceneResult::MissingComponent &&
            World.GetDiagnostics().CountByCode("SCENE-COMPONENT-MISSING") == 1,
        "Scene component removal reports missing component deterministically");

    FTransformComponent InvalidTransform = TransformAt(0.0f);
    InvalidTransform.LocalTransform.Translation.X = std::numeric_limits<float>::infinity();
    Record(Result, World.AddTransform(Entity, InvalidTransform) == ESceneResult::InvalidComponentData &&
            World.GetDiagnostics().CountByCode("SCENE-COMPONENT-INVALID-DATA") == 1,
        "Scene transform rejects non-finite component data");

    Record(Result, World.AddMesh(Entity, FMeshComponent("mesh-a", "mat-a")) == ESceneResult::Success &&
            World.AddLight(Entity, FLightComponent()) == ESceneResult::Success &&
            World.AddCamera(Entity, FCameraComponent()) == ESceneResult::Success &&
            World.GetMesh(Entity) != nullptr &&
            World.GetLight(Entity) != nullptr &&
            World.GetCamera(Entity) != nullptr,
        "Scene mesh, light, and camera components attach to live entity");
}

void TestHierarchy(FApplicationSceneEcsTestResult& Result)
{
    FWorld World;
    const FEntity RootA = World.CreateEntity();
    const FEntity RootB = World.CreateEntity();
    const FEntity ChildA = World.CreateEntity();
    const FEntity ChildB = World.CreateEntity();
    (void)World.AddTransform(RootA, TransformAt(10.0f));
    (void)World.AddTransform(RootB, TransformAt(-5.0f));
    (void)World.AddTransform(ChildA, TransformAt(2.0f));
    (void)World.AddTransform(ChildB, TransformAt(3.0f));
    (void)World.SetParent(ChildA, RootA, EReparentTransformPreservation::PreserveLocal);
    (void)World.SetParent(ChildB, RootA, EReparentTransformPreservation::PreserveLocal);

    FTransform WorldTransform;
    Record(Result, World.TryGetWorldTransform(ChildA, WorldTransform) &&
            NearTransformPosition(WorldTransform, 12.0f, 0.0f, 0.0f),
        "Scene hierarchy composes parent and child world transforms");

    const TArray<FEntity> Order = World.BuildTopologicalOrder();
    Record(Result, Order.size() == 4 &&
            Order[0] == RootA &&
            Order[1] == ChildA &&
            Order[2] == ChildB &&
            Order[3] == RootB,
        "Scene hierarchy uses parent-before-child order with creation/insertion ties");

    Record(Result, World.SetParent(RootA, ChildA) == ESceneResult::HierarchyCycle &&
            World.GetParent(RootA) == FEntity::Invalid(),
        "Scene hierarchy rejects descendant cycles without state mutation");

    Record(Result, World.SetParent(ChildA, RootB) == ESceneResult::Success &&
            World.TryGetWorldTransform(ChildA, WorldTransform) &&
            NearTransformPosition(WorldTransform, 12.0f, 0.0f, 0.0f) &&
            World.GetTransform(ChildA)->LocalTransform.Translation.NearlyEquals(FVector3(17.0f, 0.0f, 0.0f)),
        "Scene reparent preserves world transform by default");

    (void)World.SetParent(ChildA, RootA, EReparentTransformPreservation::PreserveLocal);
    Record(Result, World.TryGetWorldTransform(ChildA, WorldTransform) &&
            NearTransformPosition(WorldTransform, 27.0f, 0.0f, 0.0f),
        "Scene reparent can preserve local transform explicitly");

    const FEntity Parent = World.CreateEntity();
    const FEntity Leaf = World.CreateEntity();
    (void)World.SetParent(Leaf, Parent, EReparentTransformPreservation::PreserveLocal);
    Record(Result, World.DestroyEntity(Parent) == ESceneResult::Success &&
            !World.IsEntityLive(Parent) &&
            !World.IsEntityLive(Leaf),
        "Scene parent destruction recursively invalidates descendants");
}

void TestRenderCollection(FApplicationSceneEcsTestResult& Result)
{
    FWorld World;
    TArray<FEntity> MeshEntities;
    for (int Index = 0; Index < 10; ++Index)
    {
        const FEntity Entity = World.CreateEntity();
        (void)World.AddTransform(Entity, TransformAt(static_cast<float>(Index)));
        FMeshComponent Mesh("mesh-" + std::to_string(Index), "mat");
        if (Index == 8 || Index == 9)
        {
            Mesh.bHasSortKey = true;
            Mesh.OptionalSortKey = 1;
        }
        MeshEntities.push_back(Entity);
        (void)World.AddMesh(Entity, Mesh);
    }

    for (int Index = 0; Index < 4; ++Index)
    {
        const FEntity Entity = World.CreateEntity();
        (void)World.AddTransform(Entity, TransformAt(static_cast<float>(Index), 1.0f, 0.0f));
        FLightComponent Light;
        Light.LightType = Index == 0 ? ESceneLightType::Directional : ESceneLightType::Point;
        Light.Intensity = 2.0f;
        Light.Range = 10.0f;
        (void)World.AddLight(Entity, Light);
    }

    for (int Index = 0; Index < 2; ++Index)
    {
        const FEntity Entity = World.CreateEntity();
        (void)World.AddTransform(Entity, TransformAt(static_cast<float>(Index), 2.0f, 0.0f));
        FCameraComponent Camera;
        Camera.bActiveCamera = Index == 0;
        (void)World.AddCamera(Entity, Camera);
    }

    const FEntity MissingTransform = World.CreateEntity();
    (void)World.AddMesh(MissingTransform, FMeshComponent("bad-mesh"));
    const FEntity InvalidLight = World.CreateEntity();
    (void)World.AddTransform(InvalidLight, TransformAt(0.0f));
    FLightComponent BadLight;
    BadLight.LightType = ESceneLightType::Point;
    BadLight.Range = -1.0f;
    (void)World.AddLight(InvalidLight, BadLight);
    const FEntity InvalidCamera = World.CreateEntity();
    (void)World.AddTransform(InvalidCamera, TransformAt(0.0f));
    FCameraComponent BadCamera;
    BadCamera.NearPlane = 10.0f;
    BadCamera.FarPlane = 1.0f;
    (void)World.AddCamera(InvalidCamera, BadCamera);

    const FSceneRenderSummary Summary = World.CollectRenderSummary();
    const bool bCountsMatch = Summary.GetRenderables().size() == 10 &&
        Summary.GetLights().size() == 4 &&
        Summary.GetCameras().size() == 2 &&
        Summary.GetRejectedItems().size() == 3;
    const bool bSortKeyFirst = Summary.GetRenderables()[0].Entity == MeshEntities[8] &&
        Summary.GetRenderables()[1].Entity == MeshEntities[9];
    const bool bIdentityTie = Summary.GetRenderables()[2].Entity == MeshEntities[0] &&
        Summary.GetRenderables()[9].Entity == MeshEntities[7];
    Record(Result, bCountsMatch && bSortKeyFirst && bIdentityTie,
        "Scene render collection produces deterministic accepted/rejected category ordering");

    const std::string FirstDump = Summary.BuildDebugDump().ToStdString();
    bool bStable = true;
    for (int Index = 0; Index < 20; ++Index)
    {
        const FSceneRenderSummary Repeat = World.CollectRenderSummary();
        bStable = bStable &&
            Repeat.GetRenderables().size() == 10 &&
            Repeat.BuildDebugDump().ToStdString() == FirstDump;
    }
    Record(Result, bStable &&
            FirstDump.find("0x") == std::string::npos &&
            Summary.GetDiagnostics().CountByCode("SCENE-RENDER-MISSING-TRANSFORM") == 1 &&
            Summary.GetDiagnostics().CountByCode("SCENE-RENDER-INVALID-LIGHT") == 1 &&
            Summary.GetDiagnostics().CountByCode("SCENE-RENDER-INVALID-CAMERA") == 1,
        "Scene render collection diagnostics and dumps are byte-stable across repeated runs");
}

void TestDiagnosticsAndCapacity(FApplicationSceneEcsTestResult& Result)
{
    FWorld World(1);
    const FEntity Entity = World.CreateEntity();
    const FEntity Overflow = World.CreateEntity();
    Record(Result, !Overflow.IsSet() &&
            World.GetDiagnostics().CountByCode("SCENE-ENTITY-CAPACITY") == 1,
        "Scene world reports configured capacity exhaustion");

    FWorld OtherWorld;
    const FEntity OtherEntity = OtherWorld.CreateEntity();
    Record(Result, World.AddMesh(OtherEntity, FMeshComponent("foreign")) == ESceneResult::InvalidEntity &&
            World.GetDiagnostics().CountByCode("SCENE-COMPONENT-INVALID-ENTITY") == 1,
        "Scene world rejects handles from another world");

    (void)World.AddTransform(Entity, TransformAt(0.0f));
    (void)World.AddMesh(Entity, FMeshComponent("mesh"));
    const std::string DumpA = World.BuildDebugDump().ToStdString();
    const std::string DumpB = World.BuildDebugDump().ToStdString();
    Record(Result, DumpA == DumpB &&
            DumpA.find("0x") == std::string::npos &&
            DumpA.find("HWND") == std::string::npos &&
            DumpA.find("NSWindow") == std::string::npos,
        "Scene world debug dump is deterministic and omits native details");
}

} // namespace

FApplicationSceneEcsTestResult RunApplicationSceneEcsTests()
{
    FApplicationSceneEcsTestResult Result;
    TestEntityLifecycle(Result);
    TestComponents(Result);
    TestHierarchy(Result);
    TestRenderCollection(Result);
    TestDiagnosticsAndCapacity(Result);
    return Result;
}
