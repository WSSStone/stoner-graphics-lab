#include "AssetStaticMeshGeometryTests.h"

#include "Asset/FStaticMeshAsset.h"
#include "Asset/FStaticMeshInspection.h"
#include "Asset/FAssetDispatch.h"
#include "Asset/FStaticModelImport.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
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

class FMemoryStaticModelSource final : public IAssetSource
{
public:
    explicit FMemoryStaticModelSource(TArray<uint8> Bytes)
        : Bytes_(std::move(Bytes))
    {
    }

    EAssetResult Read(
        uint64 Offset,
        usize MaximumBytes,
        TArray<uint8>& OutBytes) const override
    {
        OutBytes.clear();
        if (Offset > Bytes_.size())
        {
            return EAssetResult::MalformedSource;
        }
        const usize Count = std::min(
            MaximumBytes, Bytes_.size() - static_cast<usize>(Offset));
        OutBytes.assign(
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset),
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset + Count));
        return EAssetResult::Success;
    }

private:
    TArray<uint8> Bytes_;
};

TArray<uint8> ReadFixture(const std::filesystem::path& Path)
{
    std::ifstream Stream(Path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(Stream),
        std::istreambuf_iterator<char>()};
}

FAssetImportRequest MakeImportRequest(
    const std::filesystem::path& Path,
    FStaticModelImportProfile Profile = {})
{
    TArray<uint8> Bytes = ReadFixture(Path);
    FAssetImportRequest Request;
    (void)FAssetSourceLocator::Create(
        FString("fixture"),
        FString(Path.generic_string()),
        Request.Descriptor.Location);
    Request.Descriptor.Size = Bytes.size();
    Request.Descriptor.FormatHint = FString(
        Path.extension() == ".glb" ? "glb" : "gltf");
    Request.Source = FAssetSourceLease(
        MakeShared<FMemoryStaticModelSource>(std::move(Bytes)));
    Request.Parameters = MakeShared<FStaticModelImportProfile>(std::move(Profile));
    return Request;
}

TArray<FAssetImportOutput> ImportFixture(
    const FAssetImportRequest& Request,
    EAssetResult& OutResult)
{
    FAssetExtensionRegistry Extensions;
    FAssetRegistrationToken Token;
    TArray<FAssetImportOutput> Outputs;
    if (RegisterStaticModelImporter(Extensions, Token) != EAssetResult::Success)
    {
        OutResult = EAssetResult::ProcessingFailure;
        return Outputs;
    }
    OutResult = FAssetDispatch::Import(Extensions, Request, Outputs);
    return Outputs;
}

TArray<std::filesystem::path> GeometryFixtures()
{
    TArray<std::filesystem::path> Paths;
    const std::filesystem::path Root =
        "Tests/Fixtures/StaticModel/Valid/Geometry";
    for (const auto& Entry : std::filesystem::directory_iterator(Root))
    {
        if (Entry.path().extension() == ".gltf" ||
            Entry.path().extension() == ".glb")
        {
            Paths.push_back(Entry.path());
        }
    }
    std::sort(Paths.begin(), Paths.end());
    return Paths;
}

bool HasExpectedCanonicalGeometry(const FStaticMeshAsset& Mesh)
{
    const FStaticMeshAssetDesc& Desc = Mesh.GetDesc();
    if (Desc.Primitives.empty()) return false;
    const FStaticMeshPrimitive& Primitive = Desc.Primitives.front();
    return Primitive.Vertices.Positions.size() >= 3 &&
        Primitive.Vertices.Positions[0].NearlyEquals(FVector3(0.0f, 0.0f, 0.0f)) &&
        Primitive.Vertices.Positions[1].NearlyEquals(FVector3(0.0f, -2.0f, 0.0f)) &&
        Primitive.Vertices.Positions[2].NearlyEquals(FVector3(0.0f, 0.0f, 3.0f)) &&
        Primitive.Indices.GetIndex(0) == 0 && Primitive.Indices.GetIndex(1) == 1 &&
        Primitive.Indices.GetIndex(2) == 2 &&
        Primitive.LocalBounds.Box.Min.NearlyEquals(FVector3(0.0f, -2.0f, 0.0f)) &&
        Primitive.LocalBounds.Box.Max.NearlyEquals(FVector3(0.0f, 0.0f, 3.0f));
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

    const TArray<std::filesystem::path> Fixtures = GeometryFixtures();
    bool CorpusAccepted = Fixtures.size() == 12;
    bool Deterministic = true;
    bool GoldenGeometry = true;
    for (const std::filesystem::path& Path : Fixtures)
    {
        const FAssetImportRequest Request = MakeImportRequest(Path);
        EAssetResult ImportResult = EAssetResult::ProcessingFailure;
        const TArray<FAssetImportOutput> First =
            ImportFixture(Request, ImportResult);
        const auto FirstMeshIt = std::find_if(First.begin(), First.end(),
            [](const FAssetImportOutput& Output)
            {
                return std::dynamic_pointer_cast<const FStaticMeshAsset>(
                    Output.Payload) != nullptr;
            });
        const auto FirstMesh = FirstMeshIt == First.end()
            ? TSharedPtr<const FStaticMeshAsset>{}
            : std::dynamic_pointer_cast<const FStaticMeshAsset>(
                FirstMeshIt->Payload);
        CorpusAccepted = CorpusAccepted &&
            ImportResult == EAssetResult::Success && First.size() == 2 && FirstMesh &&
            (Path.filename() == "12-two-primitives.glb"
                ? FirstMesh->GetDesc().Primitives.size() == 2
                : FirstMesh->GetDesc().Primitives.size() == 1);
        GoldenGeometry = GoldenGeometry && FirstMesh &&
            HasExpectedCanonicalGeometry(*FirstMesh);
        if (!FirstMesh) continue;
        std::cout << "[GEOMETRY] " << Path.filename().string()
                  << " digest="
                  << FStaticMeshInspection::ComputeGeometryDigest(*FirstMesh)
                         .ToLowerHex().CStr()
                  << '\n';
        const FString FirstInspection = FStaticMeshInspection::Format(*FirstMesh);
        for (int Iteration = 0; Iteration < 20; ++Iteration)
        {
            EAssetResult RepeatedResult = EAssetResult::ProcessingFailure;
            const TArray<FAssetImportOutput> Repeated =
                ImportFixture(Request, RepeatedResult);
            Deterministic = Deterministic &&
                RepeatedResult == EAssetResult::Success &&
                Repeated.size() == First.size() &&
                Repeated.front().Metadata.Id == First.front().Metadata.Id &&
                Repeated.front().Metadata.Version == First.front().Metadata.Version;
            const auto RepeatedMeshIt = std::find_if(
                Repeated.begin(), Repeated.end(),
                [](const FAssetImportOutput& Output)
                {
                    return std::dynamic_pointer_cast<const FStaticMeshAsset>(
                        Output.Payload) != nullptr;
                });
            const auto RepeatedMesh = RepeatedMeshIt == Repeated.end()
                ? TSharedPtr<const FStaticMeshAsset>{}
                : std::dynamic_pointer_cast<const FStaticMeshAsset>(
                    RepeatedMeshIt->Payload);
            Deterministic = Deterministic && RepeatedMesh &&
                FStaticMeshInspection::Format(*RepeatedMesh) == FirstInspection;
        }
    }
    Record(Result, CorpusAccepted,
        "repository geometry corpus imports through scoped public dispatch");
    Record(Result, GoldenGeometry,
        "SC-004 corpus matches canonical basis index and bounds expectations");
    Record(Result, Deterministic,
        "twenty repeated imports preserve output identity and version evidence");

    FStaticModelImportProfile StrictProfile;
    StrictProfile.NormalPolicy = EStaticMeshNormalPolicy::RequireSource;
    const std::filesystem::path MissingNormal =
        "Tests/Fixtures/StaticModel/Valid/Geometry/08-missing-normal.gltf";
    EAssetResult StrictResult = EAssetResult::Success;
    Record(Result,
        ImportFixture(MakeImportRequest(MissingNormal, StrictProfile), StrictResult).empty() &&
            StrictResult == EAssetResult::MalformedSource,
        "strict normal policy rejects a missing source normal stream atomically");

    FAssetExtensionRegistry Extensions;
    FAssetRegistrationToken Token;
    const bool Registered =
        RegisterStaticModelImporter(Extensions, Token) == EAssetResult::Success;
    Token.Reset();
    TArray<FAssetImportOutput> InactiveOutputs;
    Record(Result,
        Registered && FAssetDispatch::Import(
            Extensions, MakeImportRequest(Fixtures.front()), InactiveOutputs) ==
                EAssetResult::NoMatchingImporter && InactiveOutputs.empty(),
        "static-model importer registration is scoped by its token");

    return Result;
}
