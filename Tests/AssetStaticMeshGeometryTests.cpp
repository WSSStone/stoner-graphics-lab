#include "AssetStaticMeshGeometryTests.h"

#include "Asset/FStaticMeshAsset.h"

#include <iostream>
#include <limits>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Core;

void Record(FAssetStaticMeshGeometryTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FStaticMeshVertexData MakeTriangleVertices()
{
    FStaticMeshVertexData Vertices;
    Vertices.Positions = {
        FVector3(0.0f, 0.0f, 0.0f),
        FVector3(1.0f, 0.0f, 0.0f),
        FVector3(0.0f, 1.0f, 0.0f)};
    Vertices.Normals = {
        FVector3::UnitZ(), FVector3::UnitZ(), FVector3::UnitZ()};
    Vertices.TexCoords[0] = {
        FVector2(0.0f, 0.0f), FVector2(1.0f, 0.0f), FVector2(0.0f, 1.0f)};
    return Vertices;
}

FStaticMeshBounds MakeTriangleBounds()
{
    FStaticMeshBounds Bounds;
    Bounds.Box = FBox(FVector3(0.0f, 0.0f, 0.0f), FVector3(1.0f, 1.0f, 0.0f));
    Bounds.Sphere = FSphere(FVector3(0.5f, 0.5f, 0.0f), 0.75f);
    return Bounds;
}

FAssetId MakeId(const char* Type, const char* Path, const char* Subresource)
{
    FAssetId Id;
    (void)FAssetId::Create(
        FString(Type), FString(Path), std::optional<FString>(FString(Subresource)), Id);
    return Id;
}

FStaticMeshAssetDesc MakeMeshDesc()
{
    FStaticMeshAssetDesc Desc;
    Desc.Id = MakeId("StaticMesh", "Tests/StaticMesh/Triangle", "idx.mesh.0");
    Desc.ImportProfileDigest = FAssetDigest::FromBytes(TArray<uint8>{0x24});
    FStaticMeshMaterialSlot& Slot = Desc.MaterialSlots.emplace_back();
    Slot.StableKey = FString("idx.material.0");
    const FAssetId MaterialId = MakeId("Material", "Tests/StaticMesh/Triangle", "default");
    (void)TSoftAssetRef<FMaterialAsset>::Create(MaterialId, Slot.Material);
    Desc.Dependencies.push_back({
        MaterialId,
        EAssetDependencyRole::Runtime,
        EAssetDependencyStrength::Required,
        EAssetDependencyResolution::Unresolved});
    Desc.SourceManifest.push_back({
        MakeId("StaticMeshSource", "Tests/StaticMesh/Triangle", "source"),
        {},
        EAssetSourceRole::Source});

    FStaticMeshPrimitive& Primitive = Desc.Primitives.emplace_back();
    Primitive.StableKey = FString("idx.mesh.0.primitive.0");
    Primitive.Vertices = MakeTriangleVertices();
    (void)FStaticMeshIndexData::Create({0, 1, 2}, Primitive.Indices);
    Primitive.LocalBounds = MakeTriangleBounds();
    Desc.Bounds = MakeTriangleBounds();
    return Desc;
}

} // namespace

FAssetStaticMeshGeometryTestResult RunAssetStaticMeshGeometryTests()
{
    FAssetStaticMeshGeometryTestResult Result;

    FStaticMeshVertexData Vertices = MakeTriangleVertices();
    Record(Result, Vertices.IsValid(), "canonical vertex streams are valid");
    Vertices.Normals.pop_back();
    Record(Result, !Vertices.IsValid(), "normal stream must align with positions");

    Vertices = MakeTriangleVertices();
    Vertices.Tangents = {
        FVector4(1.0f, 0.0f, 0.0f, 1.0f),
        FVector4(1.0f, 0.0f, 0.0f, -1.0f),
        FVector4(1.0f, 0.0f, 0.0f, 0.0f)};
    Record(Result, !Vertices.IsValid(), "tangent handedness must be canonical");

    FStaticMeshIndexData Narrow;
    const EAssetResult NarrowResult = FStaticMeshIndexData::Create({0, 1, 2}, Narrow);
    Record(Result,
        NarrowResult == EAssetResult::Success && Narrow.Uses16BitIndices() &&
            Narrow.IsValid(3),
        "small triangle selects 16-bit indices");

    FStaticMeshIndexData Wide;
    const EAssetResult WideResult = FStaticMeshIndexData::Create(
        {0, 65536, 1}, Wide);
    Record(Result,
        WideResult == EAssetResult::Success && !Wide.Uses16BitIndices() &&
            Wide.IsValid(65537),
        "large triangle selects 32-bit indices");
    Record(Result,
        !Wide.IsValid(2) &&
            FStaticMeshIndexData::Create({0, 1}, Wide) == EAssetResult::InvalidInput,
        "indices must be in-range triangle lists");

    FStaticMeshPrimitive Primitive;
    Primitive.StableKey = FString("idx.mesh.0.primitive.0");
    Primitive.Vertices = MakeTriangleVertices();
    (void)FStaticMeshIndexData::Create({0, 1, 2}, Primitive.Indices);
    Primitive.LocalBounds = MakeTriangleBounds();
    Record(Result, Primitive.IsValid(1), "primitive requires enclosing bounds");
    Primitive.LocalBounds.Sphere.Radius = 0.1f;
    Record(Result, !Primitive.IsValid(1), "primitive bounds must enclose positions");

    FStaticMeshBounds Bounds = MakeTriangleBounds();
    Record(Result,
        Bounds.IsValid() && Bounds.Contains(FVector3(1.0f, 0.0f, 0.0f)),
        "box and sphere bounds agree on containment");
    Bounds.Box.Min.X = std::numeric_limits<float>::quiet_NaN();
    Record(Result, !Bounds.IsValid(), "bounds reject non-finite boxes");

    FStaticMeshAsset Asset;
    FStaticMeshAssetDesc Desc = MakeMeshDesc();
    Record(Result,
        FStaticMeshAsset::CreateValidated(Desc, Asset) == EAssetResult::Success &&
            Asset.GetAssetType() == FString("StaticMesh") &&
            Asset.GetDesc().Primitives.size() == 1,
        "validated mesh asset retains immutable canonical payload");

    Desc = MakeMeshDesc();
    Desc.Dependencies.clear();
    FAssetDiagnosticList Diagnostics;
    Record(Result,
        FStaticMeshAsset::CreateValidated(Desc, Asset, &Diagnostics) ==
                EAssetResult::DependencyMismatch &&
            Asset.GetDesc().Primitives.empty() && Diagnostics.size() == 1 &&
            Diagnostics.front().Field == FString("dependencies"),
        "asset publication rejects incomplete material dependencies");

    Desc = MakeMeshDesc();
    Desc.Bounds.Sphere.Radius = 0.25f;
    Record(Result,
        FStaticMeshAsset::CreateValidated(Desc, Asset) == EAssetResult::InvalidInput &&
            Asset.GetDesc().Primitives.empty(),
        "asset publication rejects non-enclosing aggregate bounds");

    Desc = MakeMeshDesc();
    Desc.Primitives.push_back(Desc.Primitives.front());
    Record(Result,
        FStaticMeshAsset::CreateValidated(Desc, Asset) == EAssetResult::InvalidInput &&
            Asset.GetDesc().Primitives.empty(),
        "asset publication rejects duplicate stable keys");

    return Result;
}
