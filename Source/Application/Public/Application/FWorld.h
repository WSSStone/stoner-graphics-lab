#pragma once

#include "Application/FCameraComponent.h"
#include "Application/FEntityHierarchy.h"
#include "Application/FLightComponent.h"
#include "Application/FMeshComponent.h"
#include "Application/FSceneRenderSummary.h"
#include "Application/FTransformComponent.h"

namespace Stoner::Application
{

class FWorld
{
public:
    static constexpr Stoner::Core::uint32 DefaultEntityCapacity = 1024;

    explicit FWorld(Stoner::Core::uint32 InEntityCapacity = DefaultEntityCapacity);

    void Reset();
    [[nodiscard]] FEntity CreateEntity();
    [[nodiscard]] ESceneResult DestroyEntity(FEntity Entity);
    [[nodiscard]] bool IsEntityLive(FEntity Entity) const noexcept;
    [[nodiscard]] ESceneResult ValidateEntity(FEntity Entity) const noexcept;
    [[nodiscard]] Stoner::Core::usize GetLiveEntityCount() const noexcept { return LiveEntityCount; }
    [[nodiscard]] Stoner::Core::uint32 GetEntityCapacity() const noexcept { return EntityCapacity; }
    [[nodiscard]] Stoner::Core::TArray<FEntity> GetLiveEntities() const;

    [[nodiscard]] ESceneResult AddTransform(FEntity Entity, const FTransformComponent& Component);
    [[nodiscard]] ESceneResult UpdateTransform(FEntity Entity, const FTransformComponent& Component);
    [[nodiscard]] ESceneResult ReplaceTransform(FEntity Entity, const FTransformComponent& Component);
    [[nodiscard]] ESceneResult RemoveTransform(FEntity Entity);
    [[nodiscard]] const FTransformComponent* GetTransform(FEntity Entity) const;
    [[nodiscard]] FTransformComponent* GetMutableTransform(FEntity Entity);

    [[nodiscard]] ESceneResult AddMesh(FEntity Entity, const FMeshComponent& Component);
    [[nodiscard]] ESceneResult UpdateMesh(FEntity Entity, const FMeshComponent& Component);
    [[nodiscard]] ESceneResult ReplaceMesh(FEntity Entity, const FMeshComponent& Component);
    [[nodiscard]] ESceneResult RemoveMesh(FEntity Entity);
    [[nodiscard]] const FMeshComponent* GetMesh(FEntity Entity) const;

    [[nodiscard]] ESceneResult AddLight(FEntity Entity, const FLightComponent& Component);
    [[nodiscard]] ESceneResult UpdateLight(FEntity Entity, const FLightComponent& Component);
    [[nodiscard]] ESceneResult ReplaceLight(FEntity Entity, const FLightComponent& Component);
    [[nodiscard]] ESceneResult RemoveLight(FEntity Entity);
    [[nodiscard]] const FLightComponent* GetLight(FEntity Entity) const;

    [[nodiscard]] ESceneResult AddCamera(FEntity Entity, const FCameraComponent& Component);
    [[nodiscard]] ESceneResult UpdateCamera(FEntity Entity, const FCameraComponent& Component);
    [[nodiscard]] ESceneResult ReplaceCamera(FEntity Entity, const FCameraComponent& Component);
    [[nodiscard]] ESceneResult RemoveCamera(FEntity Entity);
    [[nodiscard]] const FCameraComponent* GetCamera(FEntity Entity) const;

    [[nodiscard]] ESceneResult SetParent(FEntity Child,
        FEntity Parent,
        EReparentTransformPreservation Preservation = EReparentTransformPreservation::PreserveWorld);
    [[nodiscard]] ESceneResult Unparent(FEntity Child,
        EReparentTransformPreservation Preservation = EReparentTransformPreservation::PreserveWorld);
    [[nodiscard]] FEntity GetParent(FEntity Entity) const;
    [[nodiscard]] Stoner::Core::TArray<FEntity> GetChildren(FEntity Entity) const;
    [[nodiscard]] Stoner::Core::TArray<FEntity> GetRootEntities() const;
    [[nodiscard]] Stoner::Core::TArray<FEntity> BuildTopologicalOrder() const;
    [[nodiscard]] Stoner::Core::TArray<FEntity> BuildSubtreeOrder(FEntity Root) const;
    [[nodiscard]] bool TryGetWorldTransform(FEntity Entity, Stoner::Core::FTransform& OutWorldTransform) const;

    [[nodiscard]] FSceneRenderSummary CollectRenderSummary() const;
    [[nodiscard]] const FSceneDiagnosticLog& GetDiagnostics() const noexcept { return Diagnostics; }
    void ClearDiagnostics();
    [[nodiscard]] Stoner::Core::FString BuildDebugDump() const;

private:
    struct FEntitySlot
    {
        Stoner::Core::uint32 Generation = 1;
        bool bLive = false;
        Stoner::Core::uint64 CreationOrder = 0;
        FEntity Parent;
        Stoner::Core::TArray<FEntity> Children;
        bool bHasTransform = false;
        bool bHasMesh = false;
        bool bHasLight = false;
        bool bHasCamera = false;
        FTransformComponent Transform;
        FMeshComponent Mesh;
        FLightComponent Light;
        FCameraComponent Camera;
    };

    [[nodiscard]] FEntitySlot* GetSlot(FEntity Entity);
    [[nodiscard]] const FEntitySlot* GetSlot(FEntity Entity) const;
    [[nodiscard]] FEntity MakeEntity(Stoner::Core::uint32 SlotIndex) const;
    [[nodiscard]] bool WouldCreateCycle(FEntity Child, FEntity Parent) const;
    [[nodiscard]] bool ComputeWorldTransform(FEntity Entity, Stoner::Core::FTransform& OutWorldTransform) const;
    void InsertRootSorted(FEntity Entity);
    void RemoveRoot(FEntity Entity);
    void RemoveChildReference(FEntity Parent, FEntity Child);
    void AddDiagnostic(ESceneDiagnosticSeverity Severity,
        ESceneDiagnosticCategory Category,
        ESceneResult Result,
        const char* StableCode,
        const FEntity& Entity,
        const char* Message);
    void MarkSubtreeDestroyed(const Stoner::Core::TArray<FEntity>& Subtree);

    Stoner::Core::uint32 EntityCapacity = DefaultEntityCapacity;
    Stoner::Core::uint32 WorldId = 0;
    Stoner::Core::uint64 NextCreationOrder = 1;
    Stoner::Core::usize LiveEntityCount = 0;
    Stoner::Core::TArray<FEntitySlot> Slots;
    Stoner::Core::TArray<Stoner::Core::uint32> FreeSlots;
    Stoner::Core::TArray<FEntity> RootEntities;
    FSceneDiagnosticLog Diagnostics;
};

[[nodiscard]] Stoner::Core::FString BuildSceneDebugDump(const FWorld& World);

} // namespace Stoner::Application
