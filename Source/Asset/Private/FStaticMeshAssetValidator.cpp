#include "FStaticMeshAssetValidator.h"

#include <algorithm>

namespace Stoner::Asset::Private
{
namespace
{

void AddDiagnostic(
    FAssetDiagnosticList* Diagnostics,
    EAssetResult Result,
    const char* Field)
{
    if (Diagnostics == nullptr)
    {
        return;
    }
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = EAssetStage::Validate;
    Diagnostic.Result = Result;
    Diagnostic.Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic.Code = Core::FString("asset.static-mesh.invalid");
    Diagnostic.Participant = Core::FString("asset.static-mesh");
    Diagnostic.Field = Core::FString(Field);
    Diagnostics->push_back(std::move(Diagnostic));
}

bool HasUniqueStableKeys(const auto& Values)
{
    Core::TArray<Core::FString> Keys;
    Keys.reserve(Values.size());
    for (const auto& Value : Values)
    {
        if (Value.StableKey.IsEmpty())
        {
            return false;
        }
        Keys.push_back(Value.StableKey);
    }
    std::sort(Keys.begin(), Keys.end());
    return std::adjacent_find(Keys.begin(), Keys.end()) == Keys.end();
}

bool HasExpectedDependencies(const FStaticMeshAssetDesc& Desc) noexcept
{
    Core::TArray<FAssetDependency> Expected;
    for (const FStaticMeshMaterialSlot& Slot : Desc.MaterialSlots)
    {
        if (!Slot.Material.GetId())
        {
            return false;
        }
        const FAssetDependency Dependency{
            *Slot.Material.GetId(),
            EAssetDependencyRole::Runtime,
            EAssetDependencyStrength::Required,
            EAssetDependencyResolution::Unresolved};
        if (std::none_of(
                Expected.begin(), Expected.end(),
                [&Dependency](const FAssetDependency& Existing)
                {
                    return Existing.SameDeclaration(Dependency);
                }))
        {
            Expected.push_back(Dependency);
        }
    }
    std::sort(Expected.begin(), Expected.end(), [](const auto& Left, const auto& Right)
    {
        return Left.TargetId < Right.TargetId;
    });
    return Expected == Desc.Dependencies;
}

} // namespace

EAssetResult ValidateStaticMeshAsset(
    FStaticMeshAssetDesc& Desc,
    FAssetDiagnosticList* Diagnostics)
{
    if (Diagnostics != nullptr)
    {
        Diagnostics->clear();
    }
    if (!Desc.Id.IsValid() ||
        Desc.Id.GetAssetType() != TAssetTypeTraits<FStaticMeshAsset>::GetAssetType() ||
        Desc.Version.Validate() != EAssetResult::Success ||
        Desc.SchemaVersion != 1 || Desc.Primitives.empty() ||
        Desc.MaterialSlots.empty() || !Desc.Bounds.IsValid() ||
        !Desc.ImportProfileDigest.IsAvailable())
    {
        AddDiagnostic(Diagnostics, EAssetResult::InvalidInput, "header");
        return EAssetResult::InvalidInput;
    }
    if (!HasUniqueStableKeys(Desc.Primitives) ||
        !HasUniqueStableKeys(Desc.MaterialSlots))
    {
        AddDiagnostic(Diagnostics, EAssetResult::InvalidInput, "stableKeys");
        return EAssetResult::InvalidInput;
    }
    for (const FStaticMeshPrimitive& Primitive : Desc.Primitives)
    {
        if (!Primitive.IsValid(
                static_cast<Core::uint32>(Desc.MaterialSlots.size())))
        {
            AddDiagnostic(Diagnostics, EAssetResult::InvalidInput, "primitive");
            return EAssetResult::InvalidInput;
        }
        for (const Core::FVector3& Position : Primitive.Vertices.Positions)
        {
            if (!Desc.Bounds.Contains(Position))
            {
                AddDiagnostic(Diagnostics, EAssetResult::InvalidInput, "bounds");
                return EAssetResult::InvalidInput;
            }
        }
    }
    if (NormalizeSourceManifest(Desc.SourceManifest) != EAssetResult::Success ||
        Desc.SourceManifest.empty())
    {
        AddDiagnostic(Diagnostics, EAssetResult::InvalidInput, "sourceManifest");
        return EAssetResult::InvalidInput;
    }
    std::sort(Desc.Dependencies.begin(), Desc.Dependencies.end(),
        [](const auto& Left, const auto& Right) { return Left.TargetId < Right.TargetId; });
    if (std::adjacent_find(
            Desc.Dependencies.begin(), Desc.Dependencies.end(),
            [](const auto& Left, const auto& Right)
            { return Left.SameDeclaration(Right); }) != Desc.Dependencies.end() ||
        !HasExpectedDependencies(Desc))
    {
        AddDiagnostic(Diagnostics, EAssetResult::DependencyMismatch, "dependencies");
        return EAssetResult::DependencyMismatch;
    }
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
