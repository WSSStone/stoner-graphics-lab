#include "Application/FWorld.h"

#include "Application/FRenderSystem.h"

#include <algorithm>
#include <atomic>
#include <iomanip>
#include <sstream>

namespace Stoner::Application
{

namespace
{

std::atomic<Stoner::Core::uint32> GNextSceneWorldId{1};

[[nodiscard]] Stoner::Core::uint32 AllocateWorldId() noexcept
{
    return GNextSceneWorldId.fetch_add(1, std::memory_order_relaxed);
}

[[nodiscard]] bool ContainsEntity(const Stoner::Core::TArray<FEntity>& Entities, FEntity Entity)
{
    return std::find(Entities.begin(), Entities.end(), Entity) != Entities.end();
}

void AppendTransform(std::ostringstream& Stream, const Stoner::Core::FTransform& Transform)
{
    Stream << std::fixed << std::setprecision(3)
        << "pos=" << Transform.Translation.X << ',' << Transform.Translation.Y << ',' << Transform.Translation.Z
        << " rot=" << Transform.Rotation.X << ',' << Transform.Rotation.Y << ',' << Transform.Rotation.Z << ',' << Transform.Rotation.W
        << " scale=" << Transform.Scale.X << ',' << Transform.Scale.Y << ',' << Transform.Scale.Z;
}

} // namespace

FWorld::FWorld(Stoner::Core::uint32 InEntityCapacity)
    : EntityCapacity(InEntityCapacity)
    , WorldId(AllocateWorldId())
{
}

void FWorld::Reset()
{
    Slots.clear();
    FreeSlots.clear();
    RootEntities.clear();
    Diagnostics.Clear();
    LiveEntityCount = 0;
    NextCreationOrder = 1;
    WorldId = AllocateWorldId();
}

FEntity FWorld::CreateEntity()
{
    Stoner::Core::uint32 SlotIndex = 0;
    if (!FreeSlots.empty())
    {
        SlotIndex = FreeSlots.back();
        FreeSlots.pop_back();
    }
    else
    {
        if (Slots.size() >= EntityCapacity)
        {
            Diagnostics.Add(ESceneDiagnosticSeverity::Error,
                ESceneDiagnosticCategory::Entity,
                ESceneResult::CapacityExceeded,
                "SCENE-ENTITY-CAPACITY",
                "world",
                "Entity capacity exceeded");
            return FEntity::Invalid();
        }
        SlotIndex = static_cast<Stoner::Core::uint32>(Slots.size());
        Slots.push_back(FEntitySlot{});
    }

    FEntitySlot& Slot = Slots[SlotIndex];
    Slot.bLive = true;
    Slot.CreationOrder = NextCreationOrder++;
    Slot.Parent = FEntity::Invalid();
    Slot.Children.clear();
    Slot.bHasTransform = false;
    Slot.bHasMesh = false;
    Slot.bHasLight = false;
    Slot.bHasCamera = false;
    const FEntity Entity = MakeEntity(SlotIndex);
    InsertRootSorted(Entity);
    ++LiveEntityCount;
    return Entity;
}

ESceneResult FWorld::DestroyEntity(FEntity Entity)
{
    const ESceneResult Validation = ValidateEntity(Entity);
    if (Validation != ESceneResult::Success)
    {
        AddDiagnostic(ESceneDiagnosticSeverity::Error,
            ESceneDiagnosticCategory::Entity,
            Validation,
            Validation == ESceneResult::StaleEntity ? "SCENE-ENTITY-STALE" : "SCENE-ENTITY-INVALID",
            Entity,
            "Destroy requested for invalid entity");
        return Validation;
    }

    const Stoner::Core::TArray<FEntity> Subtree = BuildSubtreeOrder(Entity);
    MarkSubtreeDestroyed(Subtree);
    return ESceneResult::Success;
}

bool FWorld::IsEntityLive(FEntity Entity) const noexcept
{
    return ValidateEntity(Entity) == ESceneResult::Success;
}

ESceneResult FWorld::ValidateEntity(FEntity Entity) const noexcept
{
    if (!Entity.IsSet() || Entity.WorldId != WorldId)
    {
        return ESceneResult::InvalidEntity;
    }
    if (Entity.SlotIndex >= Slots.size())
    {
        return ESceneResult::InvalidEntity;
    }

    const FEntitySlot& Slot = Slots[Entity.SlotIndex];
    if (Slot.Generation != Entity.Generation)
    {
        return ESceneResult::StaleEntity;
    }
    return Slot.bLive ? ESceneResult::Success : ESceneResult::InvalidEntity;
}

Stoner::Core::TArray<FEntity> FWorld::GetLiveEntities() const
{
    Stoner::Core::TArray<FEntity> Entities;
    for (Stoner::Core::uint32 SlotIndex = 0; SlotIndex < Slots.size(); ++SlotIndex)
    {
        if (Slots[SlotIndex].bLive)
        {
            Entities.push_back(MakeEntity(SlotIndex));
        }
    }
    return Entities;
}

ESceneResult FWorld::AddTransform(FEntity Entity, const FTransformComponent& Component)
{
    FEntitySlot* Slot = GetSlot(Entity);
    if (Slot == nullptr)
    {
        const ESceneResult Result = ValidateEntity(Entity);
        AddDiagnostic(ESceneDiagnosticSeverity::Error, ESceneDiagnosticCategory::Component, Result, "SCENE-COMPONENT-INVALID-ENTITY", Entity, "Add transform requested for invalid entity");
        return Result;
    }
    if (!Component.IsValid())
    {
        AddDiagnostic(ESceneDiagnosticSeverity::Error, ESceneDiagnosticCategory::Component, ESceneResult::InvalidComponentData, "SCENE-COMPONENT-INVALID-DATA", Entity, "Transform component data is invalid");
        return ESceneResult::InvalidComponentData;
    }
    if (Slot->bHasTransform)
    {
        AddDiagnostic(ESceneDiagnosticSeverity::Warning, ESceneDiagnosticCategory::Component, ESceneResult::DuplicateComponent, "SCENE-COMPONENT-DUPLICATE", Entity, "Transform component already exists; update or replace explicitly");
        return ESceneResult::DuplicateComponent;
    }
    Slot->Transform = Component;
    Slot->Transform.bWorldTransformValid = false;
    Slot->bHasTransform = true;
    return ESceneResult::Success;
}

ESceneResult FWorld::UpdateTransform(FEntity Entity, const FTransformComponent& Component)
{
    return ReplaceTransform(Entity, Component);
}

ESceneResult FWorld::ReplaceTransform(FEntity Entity, const FTransformComponent& Component)
{
    FEntitySlot* Slot = GetSlot(Entity);
    if (Slot == nullptr)
    {
        const ESceneResult Result = ValidateEntity(Entity);
        AddDiagnostic(ESceneDiagnosticSeverity::Error, ESceneDiagnosticCategory::Component, Result, "SCENE-COMPONENT-INVALID-ENTITY", Entity, "Replace transform requested for invalid entity");
        return Result;
    }
    if (!Slot->bHasTransform)
    {
        AddDiagnostic(ESceneDiagnosticSeverity::Warning, ESceneDiagnosticCategory::Component, ESceneResult::MissingComponent, "SCENE-COMPONENT-MISSING", Entity, "Transform component is missing");
        return ESceneResult::MissingComponent;
    }
    if (!Component.IsValid())
    {
        AddDiagnostic(ESceneDiagnosticSeverity::Error, ESceneDiagnosticCategory::Component, ESceneResult::InvalidComponentData, "SCENE-COMPONENT-INVALID-DATA", Entity, "Transform component data is invalid");
        return ESceneResult::InvalidComponentData;
    }
    Slot->Transform = Component;
    Slot->Transform.bWorldTransformValid = false;
    return ESceneResult::Success;
}

ESceneResult FWorld::RemoveTransform(FEntity Entity)
{
    FEntitySlot* Slot = GetSlot(Entity);
    if (Slot == nullptr)
    {
        const ESceneResult Result = ValidateEntity(Entity);
        AddDiagnostic(ESceneDiagnosticSeverity::Error, ESceneDiagnosticCategory::Component, Result, "SCENE-COMPONENT-INVALID-ENTITY", Entity, "Remove transform requested for invalid entity");
        return Result;
    }
    if (!Slot->bHasTransform)
    {
        AddDiagnostic(ESceneDiagnosticSeverity::Warning, ESceneDiagnosticCategory::Component, ESceneResult::MissingComponent, "SCENE-COMPONENT-MISSING", Entity, "Transform component is missing");
        return ESceneResult::MissingComponent;
    }
    Slot->bHasTransform = false;
    Slot->Transform = FTransformComponent::Identity();
    return ESceneResult::Success;
}

const FTransformComponent* FWorld::GetTransform(FEntity Entity) const
{
    const FEntitySlot* Slot = GetSlot(Entity);
    return Slot != nullptr && Slot->bHasTransform ? &Slot->Transform : nullptr;
}

FTransformComponent* FWorld::GetMutableTransform(FEntity Entity)
{
    FEntitySlot* Slot = GetSlot(Entity);
    return Slot != nullptr && Slot->bHasTransform ? &Slot->Transform : nullptr;
}

#define SG_SCENE_COMPONENT_METHODS(Name, Member, Flag, Type, ComponentLabel) \
    ESceneResult FWorld::Add##Name(FEntity Entity, const Type& Component) \
    { \
        FEntitySlot* Slot = GetSlot(Entity); \
        if (Slot == nullptr) \
        { \
            const ESceneResult Result = ValidateEntity(Entity); \
            AddDiagnostic(ESceneDiagnosticSeverity::Error, ESceneDiagnosticCategory::Component, Result, "SCENE-COMPONENT-INVALID-ENTITY", Entity, "Add " ComponentLabel " requested for invalid entity"); \
            return Result; \
        } \
        if (Slot->Flag) \
        { \
            AddDiagnostic(ESceneDiagnosticSeverity::Warning, ESceneDiagnosticCategory::Component, ESceneResult::DuplicateComponent, "SCENE-COMPONENT-DUPLICATE", Entity, ComponentLabel " component already exists; update or replace explicitly"); \
            return ESceneResult::DuplicateComponent; \
        } \
        if (!Component.IsValid()) \
        { \
            AddDiagnostic(ESceneDiagnosticSeverity::Error, ESceneDiagnosticCategory::Component, ESceneResult::InvalidComponentData, "SCENE-COMPONENT-INVALID-DATA", Entity, ComponentLabel " component data is invalid"); \
            return ESceneResult::InvalidComponentData; \
        } \
        Slot->Member = Component; \
        Slot->Flag = true; \
        return ESceneResult::Success; \
    } \
    ESceneResult FWorld::Update##Name(FEntity Entity, const Type& Component) \
    { \
        return Replace##Name(Entity, Component); \
    } \
    ESceneResult FWorld::Replace##Name(FEntity Entity, const Type& Component) \
    { \
        FEntitySlot* Slot = GetSlot(Entity); \
        if (Slot == nullptr) \
        { \
            const ESceneResult Result = ValidateEntity(Entity); \
            AddDiagnostic(ESceneDiagnosticSeverity::Error, ESceneDiagnosticCategory::Component, Result, "SCENE-COMPONENT-INVALID-ENTITY", Entity, "Replace " ComponentLabel " requested for invalid entity"); \
            return Result; \
        } \
        if (!Slot->Flag) \
        { \
            AddDiagnostic(ESceneDiagnosticSeverity::Warning, ESceneDiagnosticCategory::Component, ESceneResult::MissingComponent, "SCENE-COMPONENT-MISSING", Entity, ComponentLabel " component is missing"); \
            return ESceneResult::MissingComponent; \
        } \
        if (!Component.IsValid()) \
        { \
            AddDiagnostic(ESceneDiagnosticSeverity::Error, ESceneDiagnosticCategory::Component, ESceneResult::InvalidComponentData, "SCENE-COMPONENT-INVALID-DATA", Entity, ComponentLabel " component data is invalid"); \
            return ESceneResult::InvalidComponentData; \
        } \
        Slot->Member = Component; \
        return ESceneResult::Success; \
    } \
    ESceneResult FWorld::Remove##Name(FEntity Entity) \
    { \
        FEntitySlot* Slot = GetSlot(Entity); \
        if (Slot == nullptr) \
        { \
            const ESceneResult Result = ValidateEntity(Entity); \
            AddDiagnostic(ESceneDiagnosticSeverity::Error, ESceneDiagnosticCategory::Component, Result, "SCENE-COMPONENT-INVALID-ENTITY", Entity, "Remove " ComponentLabel " requested for invalid entity"); \
            return Result; \
        } \
        if (!Slot->Flag) \
        { \
            AddDiagnostic(ESceneDiagnosticSeverity::Warning, ESceneDiagnosticCategory::Component, ESceneResult::MissingComponent, "SCENE-COMPONENT-MISSING", Entity, ComponentLabel " component is missing"); \
            return ESceneResult::MissingComponent; \
        } \
        Slot->Flag = false; \
        Slot->Member = Type(); \
        return ESceneResult::Success; \
    } \
    const Type* FWorld::Get##Name(FEntity Entity) const \
    { \
        const FEntitySlot* Slot = GetSlot(Entity); \
        return Slot != nullptr && Slot->Flag ? &Slot->Member : nullptr; \
    }

SG_SCENE_COMPONENT_METHODS(Mesh, Mesh, bHasMesh, FMeshComponent, "Mesh")
SG_SCENE_COMPONENT_METHODS(Light, Light, bHasLight, FLightComponent, "Light")
SG_SCENE_COMPONENT_METHODS(Camera, Camera, bHasCamera, FCameraComponent, "Camera")

#undef SG_SCENE_COMPONENT_METHODS

ESceneResult FWorld::SetParent(FEntity Child, FEntity Parent, EReparentTransformPreservation Preservation)
{
    FEntitySlot* ChildSlot = GetSlot(Child);
    const FEntitySlot* ParentSlot = GetSlot(Parent);
    if (ChildSlot == nullptr || ParentSlot == nullptr)
    {
        const ESceneResult Result = ChildSlot == nullptr ? ValidateEntity(Child) : ValidateEntity(Parent);
        AddDiagnostic(ESceneDiagnosticSeverity::Error,
            ESceneDiagnosticCategory::Hierarchy,
            Result,
            "SCENE-HIERARCHY-INVALID-ENTITY",
            ChildSlot == nullptr ? Child : Parent,
            "Parenting requested with invalid entity");
        return Result;
    }
    if (Child == Parent || WouldCreateCycle(Child, Parent))
    {
        AddDiagnostic(ESceneDiagnosticSeverity::Error,
            ESceneDiagnosticCategory::Hierarchy,
            ESceneResult::HierarchyCycle,
            "SCENE-HIERARCHY-CYCLE",
            Child,
            "Parenting would create a cycle");
        return ESceneResult::HierarchyCycle;
    }
    if (ChildSlot->Parent == Parent)
    {
        return ESceneResult::Success;
    }

    Stoner::Core::FTransform OriginalWorld = Stoner::Core::FTransform::Identity();
    const bool bHasOriginalWorld = ComputeWorldTransform(Child, OriginalWorld);
    Stoner::Core::FTransform ParentWorld = Stoner::Core::FTransform::Identity();
    const bool bHasParentWorld = ComputeWorldTransform(Parent, ParentWorld);
    Stoner::Core::FTransform ProspectiveLocal = ChildSlot->bHasTransform
        ? ChildSlot->Transform.LocalTransform
        : Stoner::Core::FTransform::Identity();

    if (ChildSlot->bHasTransform)
    {
        bool bTransformRepresentable =
            !ParentSlot->bHasTransform || bHasParentWorld;
        if (Preservation == EReparentTransformPreservation::PreserveWorld)
        {
            bTransformRepresentable = bTransformRepresentable &&
                bHasOriginalWorld &&
                (!bHasParentWorld || OriginalWorld.TryRelativeTo(ParentWorld, ProspectiveLocal));
        }
        else if (bTransformRepresentable && bHasParentWorld)
        {
            Stoner::Core::FTransform ProspectiveWorld;
            bTransformRepresentable =
                ParentWorld.TryCompose(ChildSlot->Transform.LocalTransform, ProspectiveWorld);
        }

        if (!bTransformRepresentable)
        {
            AddDiagnostic(ESceneDiagnosticSeverity::Error,
                ESceneDiagnosticCategory::Hierarchy,
                ESceneResult::InvalidHierarchyOperation,
                "SCENE-HIERARCHY-TRANSFORM-UNREPRESENTABLE",
                Child,
                "Hierarchy change would require shear that editable TRS cannot represent");
            return ESceneResult::InvalidHierarchyOperation;
        }
    }

    if (ChildSlot->Parent.IsSet())
    {
        RemoveChildReference(ChildSlot->Parent, Child);
    }
    else
    {
        RemoveRoot(Child);
    }

    ChildSlot->Parent = Parent;
    Slots[Parent.SlotIndex].Children.push_back(Child);

    if (Preservation == EReparentTransformPreservation::PreserveWorld && ChildSlot->bHasTransform)
    {
        ChildSlot->Transform.LocalTransform = ProspectiveLocal;
        ChildSlot->Transform.bWorldTransformValid = false;
    }
    return ESceneResult::Success;
}

ESceneResult FWorld::Unparent(FEntity Child, EReparentTransformPreservation Preservation)
{
    FEntitySlot* ChildSlot = GetSlot(Child);
    if (ChildSlot == nullptr)
    {
        const ESceneResult Result = ValidateEntity(Child);
        AddDiagnostic(ESceneDiagnosticSeverity::Error,
            ESceneDiagnosticCategory::Hierarchy,
            Result,
            "SCENE-HIERARCHY-INVALID-ENTITY",
            Child,
            "Unparent requested for invalid entity");
        return Result;
    }
    if (!ChildSlot->Parent.IsSet())
    {
        return ESceneResult::Success;
    }

    Stoner::Core::FTransform OriginalWorld = Stoner::Core::FTransform::Identity();
    const bool bHasOriginalWorld = ComputeWorldTransform(Child, OriginalWorld);
    if (Preservation == EReparentTransformPreservation::PreserveWorld &&
        ChildSlot->bHasTransform &&
        !bHasOriginalWorld)
    {
        AddDiagnostic(ESceneDiagnosticSeverity::Error,
            ESceneDiagnosticCategory::Hierarchy,
            ESceneResult::InvalidHierarchyOperation,
            "SCENE-HIERARCHY-TRANSFORM-UNREPRESENTABLE",
            Child,
            "Unparent would require an unavailable exact world transform");
        return ESceneResult::InvalidHierarchyOperation;
    }

    RemoveChildReference(ChildSlot->Parent, Child);
    ChildSlot->Parent = FEntity::Invalid();
    InsertRootSorted(Child);
    if (Preservation == EReparentTransformPreservation::PreserveWorld && ChildSlot->bHasTransform && bHasOriginalWorld)
    {
        ChildSlot->Transform.LocalTransform = OriginalWorld;
        ChildSlot->Transform.bWorldTransformValid = false;
    }
    return ESceneResult::Success;
}

FEntity FWorld::GetParent(FEntity Entity) const
{
    const FEntitySlot* Slot = GetSlot(Entity);
    return Slot != nullptr ? Slot->Parent : FEntity::Invalid();
}

Stoner::Core::TArray<FEntity> FWorld::GetChildren(FEntity Entity) const
{
    const FEntitySlot* Slot = GetSlot(Entity);
    return Slot != nullptr ? Slot->Children : Stoner::Core::TArray<FEntity>();
}

Stoner::Core::TArray<FEntity> FWorld::GetRootEntities() const
{
    return RootEntities;
}

Stoner::Core::TArray<FEntity> FWorld::BuildTopologicalOrder() const
{
    Stoner::Core::TArray<FEntity> Result;
    for (const FEntity& Root : RootEntities)
    {
        const Stoner::Core::TArray<FEntity> Subtree = BuildSubtreeOrder(Root);
        Result.insert(Result.end(), Subtree.begin(), Subtree.end());
    }
    return Result;
}

Stoner::Core::TArray<FEntity> FWorld::BuildSubtreeOrder(FEntity Root) const
{
    Stoner::Core::TArray<FEntity> Result;
    if (ValidateEntity(Root) != ESceneResult::Success)
    {
        return Result;
    }

    Result.push_back(Root);
    const FEntitySlot& Slot = Slots[Root.SlotIndex];
    for (const FEntity& Child : Slot.Children)
    {
        const Stoner::Core::TArray<FEntity> ChildSubtree = BuildSubtreeOrder(Child);
        Result.insert(Result.end(), ChildSubtree.begin(), ChildSubtree.end());
    }
    return Result;
}

bool FWorld::TryGetWorldTransform(FEntity Entity, Stoner::Core::FTransform& OutWorldTransform) const
{
    return ComputeWorldTransform(Entity, OutWorldTransform);
}

FSceneRenderSummary FWorld::CollectRenderSummary() const
{
    return FRenderSystem::Collect(*this);
}

void FWorld::ClearDiagnostics()
{
    Diagnostics.Clear();
}

Stoner::Core::FString FWorld::BuildDebugDump() const
{
    std::ostringstream Stream;
    Stream << "SceneWorld\n";
    Stream << "World id=" << WorldId
        << " live=" << LiveEntityCount
        << " capacity=" << EntityCapacity
        << " slots=" << Slots.size() << '\n';
    Stream << "TopologicalOrder";
    for (const FEntity& Entity : BuildTopologicalOrder())
    {
        Stream << ' ' << FormatEntityIdentity(Entity).CStr();
    }
    Stream << '\n';
    for (const FEntity& Entity : BuildTopologicalOrder())
    {
        const FEntitySlot& Slot = Slots[Entity.SlotIndex];
        Stream << FormatEntityIdentity(Entity).CStr()
            << " parent=" << FormatEntityIdentity(Slot.Parent).CStr()
            << " components="
            << (Slot.bHasTransform ? "T" : "-")
            << (Slot.bHasMesh ? "M" : "-")
            << (Slot.bHasLight ? "L" : "-")
            << (Slot.bHasCamera ? "C" : "-");
        Stoner::Core::FTransform WorldTransform;
        if (ComputeWorldTransform(Entity, WorldTransform))
        {
            Stream << ' ';
            AppendTransform(Stream, WorldTransform);
        }
        Stream << '\n';
    }
    Stream << "Diagnostics\n" << Diagnostics.Format().CStr();
    return Stoner::Core::FString(Stream.str());
}

FWorld::FEntitySlot* FWorld::GetSlot(FEntity Entity)
{
    return ValidateEntity(Entity) == ESceneResult::Success ? &Slots[Entity.SlotIndex] : nullptr;
}

const FWorld::FEntitySlot* FWorld::GetSlot(FEntity Entity) const
{
    return ValidateEntity(Entity) == ESceneResult::Success ? &Slots[Entity.SlotIndex] : nullptr;
}

FEntity FWorld::MakeEntity(Stoner::Core::uint32 SlotIndex) const
{
    return FEntity(SlotIndex, Slots[SlotIndex].Generation, WorldId);
}

bool FWorld::WouldCreateCycle(FEntity Child, FEntity Parent) const
{
    FEntity Current = Parent;
    while (Current.IsSet())
    {
        if (Current == Child)
        {
            return true;
        }
        const FEntitySlot* Slot = GetSlot(Current);
        if (Slot == nullptr)
        {
            return false;
        }
        Current = Slot->Parent;
    }
    return false;
}

bool FWorld::ComputeWorldTransform(FEntity Entity, Stoner::Core::FTransform& OutWorldTransform) const
{
    const FEntitySlot* Slot = GetSlot(Entity);
    if (Slot == nullptr || !Slot->bHasTransform)
    {
        OutWorldTransform = Stoner::Core::FTransform::Identity();
        return false;
    }

    if (Slot->Parent.IsSet())
    {
        const FEntitySlot* ParentSlot = GetSlot(Slot->Parent);
        if (ParentSlot != nullptr && !ParentSlot->bHasTransform)
        {
            OutWorldTransform = Slot->Transform.LocalTransform;
            return true;
        }

        Stoner::Core::FTransform ParentWorld = Stoner::Core::FTransform::Identity();
        if (!ComputeWorldTransform(Slot->Parent, ParentWorld))
        {
            OutWorldTransform = Stoner::Core::FTransform::Identity();
            return false;
        }
        return ParentWorld.TryCompose(Slot->Transform.LocalTransform, OutWorldTransform);
    }

    OutWorldTransform = Slot->Transform.LocalTransform;
    return true;
}

void FWorld::InsertRootSorted(FEntity Entity)
{
    if (ContainsEntity(RootEntities, Entity))
    {
        return;
    }
    RootEntities.push_back(Entity);
    std::stable_sort(RootEntities.begin(), RootEntities.end(), [this](const FEntity& Left, const FEntity& Right) {
        return Slots[Left.SlotIndex].CreationOrder < Slots[Right.SlotIndex].CreationOrder;
    });
}

void FWorld::RemoveRoot(FEntity Entity)
{
    RootEntities.erase(std::remove(RootEntities.begin(), RootEntities.end(), Entity), RootEntities.end());
}

void FWorld::RemoveChildReference(FEntity Parent, FEntity Child)
{
    FEntitySlot* ParentSlot = GetSlot(Parent);
    if (ParentSlot == nullptr)
    {
        return;
    }
    ParentSlot->Children.erase(std::remove(ParentSlot->Children.begin(), ParentSlot->Children.end(), Child), ParentSlot->Children.end());
}

void FWorld::AddDiagnostic(ESceneDiagnosticSeverity Severity,
    ESceneDiagnosticCategory Category,
    ESceneResult Result,
    const char* StableCode,
    const FEntity& Entity,
    const char* Message)
{
    Diagnostics.Add(Severity,
        Category,
        Result,
        StableCode,
        FormatEntityIdentity(Entity),
        Message);
}

void FWorld::MarkSubtreeDestroyed(const Stoner::Core::TArray<FEntity>& Subtree)
{
    if (Subtree.empty())
    {
        return;
    }

    const FEntity Root = Subtree.front();
    FEntitySlot* RootSlot = GetSlot(Root);
    if (RootSlot != nullptr)
    {
        if (RootSlot->Parent.IsSet())
        {
            RemoveChildReference(RootSlot->Parent, Root);
        }
        else
        {
            RemoveRoot(Root);
        }
    }

    for (const FEntity& Entity : Subtree)
    {
        RemoveRoot(Entity);
    }

    for (const FEntity& Entity : Subtree)
    {
        if (Entity.SlotIndex >= Slots.size())
        {
            continue;
        }
        FEntitySlot& Slot = Slots[Entity.SlotIndex];
        if (!Slot.bLive || Slot.Generation != Entity.Generation)
        {
            continue;
        }
        Slot.bLive = false;
        Slot.Parent = FEntity::Invalid();
        Slot.Children.clear();
        Slot.bHasTransform = false;
        Slot.bHasMesh = false;
        Slot.bHasLight = false;
        Slot.bHasCamera = false;
        Slot.Transform = FTransformComponent::Identity();
        Slot.Mesh = FMeshComponent();
        Slot.Light = FLightComponent();
        Slot.Camera = FCameraComponent();
        ++Slot.Generation;
        FreeSlots.push_back(Entity.SlotIndex);
        --LiveEntityCount;
    }
}

Stoner::Core::FString BuildSceneDebugDump(const FWorld& World)
{
    return World.BuildDebugDump();
}

} // namespace Stoner::Application
