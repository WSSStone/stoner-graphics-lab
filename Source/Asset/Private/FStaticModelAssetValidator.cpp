#include "FStaticModelAssetValidator.h"

#include <algorithm>
#include <set>

namespace Stoner::Asset::Private
{
namespace
{

void AddDiagnostic(
    FAssetDiagnosticList* Diagnostics,
    EAssetResult Result,
    const char* Field)
{
    if (Diagnostics == nullptr) return;
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = EAssetStage::Validate;
    Diagnostic.Result = Result;
    Diagnostic.Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic.Code = Core::FString("asset.static-model.invalid");
    Diagnostic.Participant = Core::FString("asset.static-model");
    Diagnostic.Field = Core::FString(Field);
    Diagnostics->push_back(std::move(Diagnostic));
}

bool HasUniqueStableKeys(const Core::TArray<FStaticModelNode>& Nodes)
{
    Core::TArray<Core::FString> Keys;
    Keys.reserve(Nodes.size());
    for (const FStaticModelNode& Node : Nodes)
    {
        if (Node.StableKey.IsEmpty()) return false;
        Keys.push_back(Node.StableKey);
    }
    std::sort(Keys.begin(), Keys.end());
    return std::adjacent_find(Keys.begin(), Keys.end()) == Keys.end();
}

bool ValidateHierarchy(
    const FStaticModelAssetDesc& Desc,
    Core::uint32 MaximumHierarchyDepth)
{
    const Core::uint32 NodeCount = static_cast<Core::uint32>(Desc.Nodes.size());
    Core::TArray<Core::uint32> ParentCounts(NodeCount, 0);
    for (const FStaticModelNode& Node : Desc.Nodes)
    {
        std::set<Core::uint32> UniqueChildren;
        for (const Core::uint32 Child : Node.Children)
        {
            if (Child >= NodeCount || !UniqueChildren.insert(Child).second ||
                ++ParentCounts[Child] > 1)
            {
                return false;
            }
        }
    }
    Core::TArray<Core::uint32> ExpectedRoots;
    for (Core::uint32 Index = 0; Index < NodeCount; ++Index)
    {
        if (ParentCounts[Index] == 0) ExpectedRoots.push_back(Index);
    }
    if (ExpectedRoots != Desc.RootNodeIndices || ExpectedRoots.empty())
    {
        return false;
    }

    enum class EVisit : Core::uint8 { Unvisited, Visiting, Visited };
    Core::TArray<EVisit> Visits(NodeCount, EVisit::Unvisited);
    struct FFrame { Core::uint32 Node; Core::usize NextChild; Core::uint32 Depth; };
    for (const Core::uint32 Root : Desc.RootNodeIndices)
    {
        Core::TArray<FFrame> Stack{{Root, 0, 1}};
        while (!Stack.empty())
        {
            FFrame& Frame = Stack.back();
            if (Frame.Depth > MaximumHierarchyDepth) return false;
            if (Visits[Frame.Node] == EVisit::Unvisited)
                Visits[Frame.Node] = EVisit::Visiting;
            const auto& Children = Desc.Nodes[Frame.Node].Children;
            if (Frame.NextChild < Children.size())
            {
                const Core::uint32 Child = Children[Frame.NextChild++];
                if (Visits[Child] == EVisit::Visiting) return false;
                if (Visits[Child] == EVisit::Unvisited)
                    Stack.push_back({Child, 0, Frame.Depth + 1});
            }
            else
            {
                Visits[Frame.Node] = EVisit::Visited;
                Stack.pop_back();
            }
        }
    }
    return std::all_of(Visits.begin(), Visits.end(),
        [](EVisit Value) { return Value == EVisit::Visited; });
}

bool HasExpectedDependencies(FStaticModelAssetDesc& Desc)
{
    Core::TArray<FAssetDependency> Expected;
    for (const FStaticModelNode& Node : Desc.Nodes)
    {
        if (!Node.Mesh) continue;
        const auto& Id = Node.Mesh->GetId();
        if (!Id) return false;
        const FAssetDependency Dependency{
            *Id,
            EAssetDependencyRole::Runtime,
            EAssetDependencyStrength::Required,
            EAssetDependencyResolution::Unresolved};
        if (std::none_of(Expected.begin(), Expected.end(),
            [&Dependency](const FAssetDependency& Existing)
            {
                return Existing.SameDeclaration(Dependency);
            }))
        {
            Expected.push_back(Dependency);
        }
    }
    const auto Less = [](const FAssetDependency& Left, const FAssetDependency& Right)
    {
        return Left.TargetId < Right.TargetId;
    };
    std::sort(Expected.begin(), Expected.end(), Less);
    std::sort(Desc.Dependencies.begin(), Desc.Dependencies.end(), Less);
    if (Expected.size() != Desc.Dependencies.size()) return false;
    for (Core::usize Index = 0; Index < Expected.size(); ++Index)
    {
        if (!Expected[Index].SameDeclaration(Desc.Dependencies[Index])) return false;
    }
    return true;
}

} // namespace

EAssetResult ValidateStaticModelAsset(
    FStaticModelAssetDesc& Desc,
    Core::uint32 MaximumHierarchyDepth,
    FAssetDiagnosticList* Diagnostics)
{
    if (Diagnostics != nullptr) Diagnostics->clear();
    if (!Desc.Id.IsValid() ||
        Desc.Id.GetAssetType() != TAssetTypeTraits<FStaticModelAsset>::GetAssetType() ||
        Desc.Version.Validate() != EAssetResult::Success ||
        Desc.SchemaVersion != 1 || Desc.SceneStableKey.IsEmpty() ||
        Desc.Nodes.empty() || MaximumHierarchyDepth == 0 ||
        !Desc.Bounds.IsValid() || !Desc.ImportProfileDigest.IsAvailable())
    {
        AddDiagnostic(Diagnostics, EAssetResult::InvalidInput, "header");
        return EAssetResult::InvalidInput;
    }
    if (!HasUniqueStableKeys(Desc.Nodes))
    {
        AddDiagnostic(Diagnostics, EAssetResult::InvalidInput, "stableKeys");
        return EAssetResult::InvalidInput;
    }
    for (const FStaticModelNode& Node : Desc.Nodes)
    {
        if (!Node.LocalTransform.IsFinite() ||
            (Node.Mesh && (!Node.Mesh->GetId() ||
                Node.Mesh->GetId()->GetAssetType() !=
                    TAssetTypeTraits<FStaticMeshAsset>::GetAssetType())))
        {
            AddDiagnostic(Diagnostics, EAssetResult::InvalidInput, "node");
            return EAssetResult::InvalidInput;
        }
    }
    if (!ValidateHierarchy(Desc, MaximumHierarchyDepth))
    {
        AddDiagnostic(Diagnostics, EAssetResult::InvalidInput, "hierarchy");
        return EAssetResult::InvalidInput;
    }
    if (!HasExpectedDependencies(Desc))
    {
        AddDiagnostic(Diagnostics, EAssetResult::DependencyMismatch, "dependencies");
        return EAssetResult::DependencyMismatch;
    }
    if (NormalizeSourceManifest(Desc.SourceManifest) != EAssetResult::Success ||
        Desc.SourceManifest.empty())
    {
        AddDiagnostic(Diagnostics, EAssetResult::InvalidInput, "sourceManifest");
        return EAssetResult::InvalidInput;
    }
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
