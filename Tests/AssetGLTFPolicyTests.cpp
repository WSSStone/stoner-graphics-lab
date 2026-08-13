#include "AssetGLTFPolicyTests.h"

#include "Asset/FStaticModelImport.h"
#include "FGLTFGeometryNormalizer.h"
#include "FStaticMeshBounds.h"
#include "FStaticMeshNormalGenerator.h"
#include "FStaticMeshTangentGenerator.h"

#include <iostream>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Core;

void Record(FAssetGLTFPolicyTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FStaticMeshVertexData MakeMissingNormalQuad()
{
    FStaticMeshVertexData Vertices;
    Vertices.Positions = {
        FVector3(0.0f, 0.0f, 0.0f),
        FVector3(1.0f, 0.0f, 0.0f),
        FVector3(1.0f, 1.0f, 0.0f),
        FVector3(0.0f, 1.0f, 0.0f)};
    Vertices.Tangents.assign(4, FVector4(1.0f, 0.0f, 0.0f, 1.0f));
    Vertices.TexCoords[0] = {
        FVector2(0.0f, 0.0f), FVector2(1.0f, 0.0f),
        FVector2(1.0f, 1.0f), FVector2(0.0f, 1.0f)};
    return Vertices;
}

} // namespace

FAssetGLTFPolicyTestResult RunAssetGLTFPolicyTests()
{
    FAssetGLTFPolicyTestResult Result;
    FStaticModelImportProfile Profile;
    const FAssetDigest Baseline = Profile.GetDigest();
    Record(Result,
        Profile.Validate() == EAssetResult::Success && Baseline.IsAvailable() &&
            Profile.NormalPolicy == EStaticMeshNormalPolicy::GenerateFlat &&
            Profile.TangentPolicy == EStaticMeshTangentPolicy::GenerateWhenRequired,
        "default static-model policy is explicit and versioned");

    Profile.MaximumTexCoordSets = 3;
    Record(Result,
        Profile.Validate() == EAssetResult::InvalidInput &&
            !Profile.GetDigest().IsAvailable(),
        "profile rejects unsupported UV-set count");

    Profile = {};
    Profile.CoordinateConvention = FString("OtherConvention");
    Record(Result,
        Profile.Validate() == EAssetResult::InvalidInput,
        "profile rejects a non-canonical coordinate convention");

    Profile = {};
    Profile.Limits.MaxScenes = 0;
    Record(Result,
        Profile.Limits.Validate() == EAssetResult::InvalidInput,
        "all static-model limits are finite non-zero values");

    Profile = {};
    Profile.Limits.MaxSingleDependencyBytes =
        Profile.Limits.MaxAggregateDependencyBytes + 1;
    Record(Result,
        Profile.Limits.Validate() == EAssetResult::InvalidInput,
        "single dependency limit cannot exceed aggregate limit");

    Profile = {};
    Profile.NormalPolicy = EStaticMeshNormalPolicy::RequireSource;
    const FAssetDigest StrictNormal = Profile.GetDigest();
    Profile = {};
    Profile.Limits.MaxPrimitives -= 1;
    const FAssetDigest ChangedLimit = Profile.GetDigest();
    Record(Result,
        StrictNormal.IsAvailable() && ChangedLimit.IsAvailable() &&
            StrictNormal != Baseline && ChangedLimit != Baseline,
        "policy and limit changes invalidate import version evidence");

    FVector3 Position;
    FVector3 Direction;
    FVector4 Tangent;
    Record(Result,
        Private::FGLTFGeometryNormalizer::ConvertPosition(
            FVector3(1.0f, 2.0f, 3.0f), Position) == EAssetResult::Success &&
            Position == FVector3(3.0f, -1.0f, 2.0f) &&
        Private::FGLTFGeometryNormalizer::ConvertDirection(
            FVector3::UnitZ(), Direction) == EAssetResult::Success &&
            Direction == FVector3::UnitX() &&
        Private::FGLTFGeometryNormalizer::ConvertTangent(
            FVector4(1.0f, 0.0f, 0.0f, 1.0f), Tangent) ==
                EAssetResult::Success &&
            Tangent == FVector4(0.0f, -1.0f, 0.0f, -1.0f),
        "basis conversion preserves indices and flips tangent parity once");

    FStaticMeshVertexData FlatVertices = MakeMissingNormalQuad();
    FStaticMeshIndexData FlatIndices;
    (void)FStaticMeshIndexData::Create({0, 1, 2, 0, 2, 3}, FlatIndices);
    Record(Result,
        Private::GenerateFlatStaticMeshNormals(
            FlatVertices, FlatIndices, 64) == EAssetResult::Success &&
            FlatVertices.Positions.size() == 6 &&
            FlatVertices.Normals.size() == 6 &&
            FlatVertices.Tangents.empty() && FlatVertices.IsValid(),
        "default missing-normal policy splits corners and discards source tangents");

    FStaticMeshVertexData Degenerate = MakeMissingNormalQuad();
    FStaticMeshIndexData DegenerateIndices;
    (void)FStaticMeshIndexData::Create({0, 0, 1}, DegenerateIndices);
    Record(Result,
        Private::GenerateFlatStaticMeshNormals(
            Degenerate, DegenerateIndices, 64) == EAssetResult::MalformedSource,
        "flat normal generation rejects degenerate referenced triangles");

    FStaticMeshVertexData TangentVertices = MakeMissingNormalQuad();
    TangentVertices.Tangents.clear();
    TangentVertices.Normals.assign(4, FVector3::UnitZ());
    FStaticMeshIndexData TangentIndices;
    (void)FStaticMeshIndexData::Create({0, 1, 2, 0, 2, 3}, TangentIndices);
    Record(Result,
        Private::GenerateStaticMeshTangents(
            TangentVertices, TangentIndices, 0, 64) == EAssetResult::Success &&
            TangentVertices.Tangents.size() == 6 &&
            TangentVertices.Positions.size() == 6 && TangentVertices.IsValid(),
        "default required-tangent policy uses MikkTSpace with corner splitting");

    TangentVertices = MakeMissingNormalQuad();
    TangentVertices.Normals.assign(4, FVector3::UnitZ());
    TangentVertices.TexCoords[0].clear();
    (void)FStaticMeshIndexData::Create({0, 1, 2}, TangentIndices);
    Record(Result,
        Private::GenerateStaticMeshTangents(
            TangentVertices, TangentIndices, 0, 64) == EAssetResult::InvalidInput,
        "required tangent generation rejects a missing selected UV set");

    FStaticMeshBounds Bounds;
    const TArray<FVector3> BoundsPositions = {
        FVector3(0.0f, 0.0f, 0.0f),
        FVector3(2.0f, 0.0f, 0.0f),
        FVector3(0.0f, 2.0f, 0.0f)};
    Record(Result,
        Private::BuildStaticMeshBounds(BoundsPositions, Bounds) ==
                EAssetResult::Success &&
            Bounds.Box.Min == FVector3(0.0f, 0.0f, 0.0f) &&
            Bounds.Box.Max == FVector3(2.0f, 2.0f, 0.0f) &&
            Bounds.Contains(BoundsPositions[2]),
        "canonical bounds deterministically enclose positions and AABB corners");
    return Result;
}
