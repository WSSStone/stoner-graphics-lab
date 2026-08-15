#include "AssetManagerEquivalenceTests.h"

#include "AssetCookerPublicationTestSupport.h"
#include "AssetManagerTestSupport.h"
#include "Asset/FMaterialAsset.h"
#include "Asset/FMaterialShaderSourceLoader.h"
#include "Asset/FShaderAsset.h"
#include "Asset/FStaticMeshAsset.h"
#include "Asset/FStaticModelAsset.h"
#include "Asset/FTextureAsset.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;
using namespace Stoner::Tests::AssetCookerPublication;

class FFileSource final : public IAssetSource
{
public:
    explicit FFileSource(Core::TArray<Core::uint8> Bytes)
        : Bytes_(std::move(Bytes))
    {
    }

    EAssetResult Read(
        Core::uint64 Offset,
        Core::usize MaximumBytes,
        Core::TArray<Core::uint8>& OutBytes) const override
    {
        OutBytes.clear();
        if (Offset > Bytes_.size()) return EAssetResult::MalformedSource;
        const Core::usize Begin = static_cast<Core::usize>(Offset);
        const Core::usize Count = std::min(MaximumBytes, Bytes_.size() - Begin);
        OutBytes.assign(
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Begin),
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Begin + Count));
        return EAssetResult::Success;
    }

    [[nodiscard]] Core::uint64 Size() const noexcept
    {
        return static_cast<Core::uint64>(Bytes_.size());
    }

private:
    Core::TArray<Core::uint8> Bytes_;
};

class FRepresentativeResolver final : public IAssetResolver
{
public:
    FRepresentativeResolver(
        std::filesystem::path Root,
        Core::TSharedPtr<std::atomic<int>> Calls)
        : Root_(std::move(Root)), Calls_(std::move(Calls))
    {
    }

    FAssetExtensionCapability GetCapability() const override
    {
        FAssetExtensionCapability Result;
        Result.Kind = EAssetExtensionKind::Resolver;
        (void)FAssetParticipantId::Create(
            Core::FString("tests.runtime-representative-resolver"),
            Result.Participant);
        (void)FAssetProducerVersion::Create(
            Core::FString("026-v1"), Result.ProducerVersion);
        Result.Priority = 100;
        Result.Schemes = {Core::FString("asset")};
        Result.bRuntimeCompatible = true;
        return Result;
    }

    FAssetResolveResult Resolve(const FAssetResolveRequest& Request) override
    {
        ++(*Calls_);
        if (Request.RuntimeContext && Request.RuntimeContext->ShouldStop())
            return {EAssetResult::Cancelled, {}, {}};

        const std::string Requested =
            Request.Location.GetLocator().ToStdString();
        std::filesystem::path Source;
        std::string Canonical;
        if (Requested.ends_with("Cooked/representative"))
        {
            Source = Root_ / "representative.png";
            Canonical = "representative.png";
        }
        else if (Requested.ends_with("Engine/Shaders/Deferred/Surface"))
        {
            Source = Root_ / "Surface.shader.json";
            Canonical = "Surface.shader.json";
        }
        else
        {
            const std::filesystem::path RequestedPath(Requested);
            Source = Root_ / RequestedPath.filename();
            Canonical = RequestedPath.filename().generic_string();
        }
        std::ifstream Input(Source, std::ios::binary);
        if (!Input) return {EAssetResult::NotFound, {}, {}};
        Core::TArray<Core::uint8> Bytes{
            std::istreambuf_iterator<char>(Input),
            std::istreambuf_iterator<char>()};
        auto Memory = Core::MakeShared<FFileSource>(std::move(Bytes));
        FAssetSourceDescriptor Descriptor;
        if (FAssetSourceLocator::Create(
                Core::FString("asset"), Core::FString(Canonical),
                Descriptor.Location) != EAssetResult::Success)
            return {EAssetResult::InvalidInput, {}, {}};
        Descriptor.Size = Memory->Size();
        if (Source.filename() == "representative.png")
            Descriptor.FormatHint = Core::FString("png");
        else if (Source.extension() == ".gltf")
            Descriptor.FormatHint = Core::FString("gltf");
        else if (Source.filename() == "Surface.shader.json")
            Descriptor.FormatHint = Core::FString("shader.json");
        return {EAssetResult::Success, std::move(Descriptor),
            FAssetSourceLease(std::move(Memory))};
    }

private:
    std::filesystem::path Root_;
    Core::TSharedPtr<std::atomic<int>> Calls_;
};

struct FRepresentativeExtensions
{
    Core::TSharedPtr<FAssetExtensionRegistry> Registry;
    Core::TSharedPtr<std::atomic<int>> ResolveCalls;
    FAssetRegistrationToken Resolver;
    FAssetRegistrationToken Image;
    FAssetRegistrationToken Material;
    FAssetRegistrationToken StaticModel;
};

FRepresentativeExtensions MakeRepresentativeExtensions(
    const std::filesystem::path& Root)
{
    FRepresentativeExtensions Result;
    Result.Registry = Core::MakeShared<FAssetExtensionRegistry>();
    Result.ResolveCalls = Core::MakeShared<std::atomic<int>>(0);
    (void)Result.Registry->Register(
        Core::MakeShared<FRepresentativeResolver>(Root, Result.ResolveCalls),
        Result.Resolver);
    (void)RegisterImageAssetImporter(*Result.Registry, Result.Image);
    (void)RegisterMaterialShaderDefinitionImporter(
        *Result.Registry, Result.Material);
    (void)RegisterStaticModelImporter(*Result.Registry, Result.StaticModel);
    return Result;
}

void Record(
    FAssetManagerEquivalenceTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

bool EqualTexture(
    const FTextureAsset& Left,
    const FTextureAsset& Right)
{
    if (Left.GetId() != Right.GetId() ||
        Left.GetSemantic() != Right.GetSemantic() ||
        Left.GetColorSpace() != Right.GetColorSpace() ||
        Left.GetMipPolicy() != Right.GetMipPolicy() ||
        Left.GetMips().size() != Right.GetMips().size())
        return false;
    for (Core::usize Index = 0; Index < Left.GetMips().size(); ++Index)
    {
        const auto LeftBytes = Left.GetMips()[Index].GetBytes();
        const auto RightBytes = Right.GetMips()[Index].GetBytes();
        if (Left.GetMips()[Index].GetExtent() !=
                Right.GetMips()[Index].GetExtent() ||
            Left.GetMips()[Index].GetFormat() !=
                Right.GetMips()[Index].GetFormat() ||
            LeftBytes.size() != RightBytes.size() ||
            !std::equal(LeftBytes.begin(), LeftBytes.end(),
                RightBytes.begin()))
            return false;
    }
    return true;
}

template <typename T>
bool Load(
    FAssetManager& Manager,
    const FAssetId& Id,
    TAssetHandle<T>& Out)
{
    FAssetRequestHandle Request;
    FAssetRequestSnapshot Snapshot;
    return Manager.Request<T>(Id, Request) == EAssetResult::Success &&
        WaitForRequestTerminal(Manager, Request, Snapshot) &&
        Snapshot.State == EAssetRequestState::Ready &&
        Manager.GetResult(Request, Out) == EAssetResult::Success;
}

const FAssetCookManifestRecord* FindRecord(
    const FAssetCookManifest& Manifest,
    const char* Type)
{
    const auto Found = std::find_if(
        Manifest.Records.begin(), Manifest.Records.end(),
        [Type](const auto& Value)
        {
            return Value.AssetType == Core::FString(Type);
        });
    return Found == Manifest.Records.end() ? nullptr : &*Found;
}

bool EqualShader(const FShaderAsset& Left, const FShaderAsset& Right)
{
    const auto& A = Left.GetDesc();
    const auto& B = Right.GetDesc();
    return A.Id == B.Id && A.SchemaVersion == B.SchemaVersion &&
        A.ProgramKind == B.ProgramKind && A.Stages.size() == B.Stages.size() &&
        A.AllowedPermutationFlags == B.AllowedPermutationFlags &&
        A.Variants.size() == B.Variants.size() &&
        A.RequiredParameters.size() == B.RequiredParameters.size() &&
        A.InterfaceBindings.size() == B.InterfaceBindings.size() &&
        A.ConstantRanges.size() == B.ConstantRanges.size();
}

bool EqualMaterial(const FMaterialAsset& Left, const FMaterialAsset& Right)
{
    const auto& A = Left.GetDesc();
    const auto& B = Right.GetDesc();
    return A.Id == B.Id && A.SchemaVersion == B.SchemaVersion &&
        A.Domain == B.Domain && A.BlendMode == B.BlendMode &&
        A.RenderState == B.RenderState &&
        A.Shader.GetId() == B.Shader.GetId() &&
        A.PermutationRequest == B.PermutationRequest &&
        A.Parameters == B.Parameters;
}

bool EqualMesh(const FStaticMeshAsset& Left, const FStaticMeshAsset& Right)
{
    const auto& A = Left.GetDesc();
    const auto& B = Right.GetDesc();
    if (A.Id != B.Id || A.SchemaVersion != B.SchemaVersion ||
        A.Primitives.size() != B.Primitives.size() ||
        A.MaterialSlots.size() != B.MaterialSlots.size() ||
        A.Dependencies != B.Dependencies)
        return false;
    for (Core::usize Index = 0; Index < A.Primitives.size(); ++Index)
    {
        const auto& AP = A.Primitives[Index];
        const auto& BP = B.Primitives[Index];
        if (AP.StableKey != BP.StableKey ||
            AP.Vertices.Positions != BP.Vertices.Positions ||
            AP.Vertices.Normals != BP.Vertices.Normals ||
            AP.Vertices.Tangents != BP.Vertices.Tangents ||
            AP.Vertices.TexCoords != BP.Vertices.TexCoords ||
            AP.Indices.GetIndexCount() != BP.Indices.GetIndexCount())
            return false;
        for (Core::uint32 I = 0; I < AP.Indices.GetIndexCount(); ++I)
            if (AP.Indices.GetIndex(I) != BP.Indices.GetIndex(I)) return false;
    }
    return true;
}

bool EqualModel(const FStaticModelAsset& Left, const FStaticModelAsset& Right)
{
    const auto& A = Left.GetDesc();
    const auto& B = Right.GetDesc();
    if (A.Id != B.Id || A.SchemaVersion != B.SchemaVersion ||
        A.SceneStableKey != B.SceneStableKey ||
        A.bSourceDefaultScene != B.bSourceDefaultScene ||
        A.Nodes.size() != B.Nodes.size() ||
        A.RootNodeIndices != B.RootNodeIndices ||
        A.Dependencies != B.Dependencies)
        return false;
    for (Core::usize Index = 0; Index < A.Nodes.size(); ++Index)
    {
        const auto& AN = A.Nodes[Index];
        const auto& BN = B.Nodes[Index];
        if (AN.StableKey != BN.StableKey || AN.DisplayName != BN.DisplayName ||
            !AN.LocalTransform.ToMatrix().NearlyEquals(
                BN.LocalTransform.ToMatrix()) ||
            AN.Children != BN.Children ||
            AN.Mesh.has_value() != BN.Mesh.has_value() ||
            (AN.Mesh && AN.Mesh->GetId() != BN.Mesh->GetId()) ||
            AN.SourceNodeIndex != BN.SourceNodeIndex ||
            AN.bNegativeDeterminant != BN.bNegativeDeterminant)
            return false;
    }
    return true;
}
} // namespace

FAssetManagerEquivalenceTestResult RunAssetManagerEquivalenceTests()
{
    namespace CookerPrivate = Stoner::AssetCooker::Private;
    FAssetManagerEquivalenceTestResult Result;
    const auto Token = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    const auto Root = std::filesystem::temp_directory_path() /
        ("sg-runtime-equivalence-" + std::to_string(Token));
    const std::filesystem::path Content =
        "Tests/Fixtures/AssetCooker/Representative";
    auto CookRequest = Stoner::Tests::AssetCookerDDC::Request(
        Root / "Seed", Content, 4);
    CookRequest.CachePolicy =
        Stoner::AssetCooker::EAssetCookCachePolicy::IgnoreExisting;
    const auto SeedRun = Stoner::Tests::AssetCookerDDC::Run(CookRequest);
    const auto Publication = Root / "Published";
    const auto Published = CookerPrivate::FCookedGenerationPublisher::Publish(
        Request(SeedRun, Publication));
    const auto Coordination = Root / "Coordination";
    std::filesystem::create_directories(Coordination);

    const auto* TextureRecord = FindRecord(
        SeedRun.Result.Manifest, "Texture");
    const auto* ShaderRecord = FindRecord(
        SeedRun.Result.Manifest, "ShaderProgram");
    const auto* MaterialRecord = FindRecord(
        SeedRun.Result.Manifest, "Material");
    const auto* MeshRecord = FindRecord(
        SeedRun.Result.Manifest, "StaticMesh");
    const auto* ModelRecord = FindRecord(
        SeedRun.Result.Manifest, "StaticModel");
    auto Extensions = MakeRepresentativeExtensions(Content);

    FAssetManagerConfig DevelopmentConfig;
    DevelopmentConfig.Mode = EAssetManagerMode::DevelopmentSource;
    DevelopmentConfig.ExtensionRegistry = Extensions.Registry;
    DevelopmentConfig.SourceRoot = Core::FString(Content.generic_string());
    DevelopmentConfig.WorkerCount = 4;
    DevelopmentConfig.Limits.MaxPayloadBytes = 64ULL * 1024ULL * 1024ULL;
    DevelopmentConfig.Limits.MaxAggregatePayloadBytes =
        256ULL * 1024ULL * 1024ULL;
    DevelopmentConfig.TargetEvidence =
        Core::MakeShared<const FAssetTargetProfileEvidence>(
            SeedRun.Result.Manifest.TargetProfile);
    Core::TSharedPtr<FAssetManager> Development;
    FAssetDiagnosticList Diagnostics;
    const auto DevelopmentCreate = FAssetManager::Create(
        DevelopmentConfig, Development, Diagnostics);

    FAssetManagerConfig CookedConfig;
    CookedConfig.Mode = EAssetManagerMode::StrictCooked;
    CookedConfig.ExtensionRegistry = Extensions.Registry;
    CookedConfig.PublicationRoot = Core::FString(Publication.generic_string());
    CookedConfig.LeaseCoordinationRoot =
        Core::FString(Coordination.generic_string());
    CookedConfig.TargetEvidence = DevelopmentConfig.TargetEvidence;
    Core::TSharedPtr<FAssetManager> Cooked;
    Diagnostics.clear();
    const auto CookedCreate = FAssetManager::Create(
        CookedConfig, Cooked, Diagnostics);

    const bool Created = SeedRun.Result.Succeeded() && Published.Succeeded() &&
        TextureRecord && ShaderRecord && MaterialRecord && MeshRecord &&
        ModelRecord &&
        DevelopmentCreate == EAssetResult::Success &&
        CookedCreate == EAssetResult::Success;
    TAssetHandle<FTextureAsset> DevelopmentTexture;
    TAssetHandle<FTextureAsset> CookedTexture;
    TAssetHandle<FShaderAsset> DevelopmentShader;
    TAssetHandle<FShaderAsset> CookedShader;
    TAssetHandle<FStaticModelAsset> DevelopmentModel;
    TAssetHandle<FStaticModelAsset> CookedModel;
    const bool DevelopmentRoots = Created &&
        Load(*Development, TextureRecord->AssetId, DevelopmentTexture) &&
        Load(*Development, ShaderRecord->AssetId, DevelopmentShader) &&
        Load(*Development, ModelRecord->AssetId, DevelopmentModel);
    TAssetHandle<FMaterialAsset> DevelopmentMaterial;
    TAssetHandle<FStaticMeshAsset> DevelopmentMesh;
    const bool DevelopmentChildren = DevelopmentRoots &&
        Load(*Development, MaterialRecord->AssetId, DevelopmentMaterial) &&
        Load(*Development, MeshRecord->AssetId, DevelopmentMesh);
    const int SourceCallsBeforeCooked = Extensions.ResolveCalls->load();
    const bool CookedRoots = DevelopmentChildren &&
        Load(*Cooked, TextureRecord->AssetId, CookedTexture) &&
        Load(*Cooked, ShaderRecord->AssetId, CookedShader) &&
        Load(*Cooked, ModelRecord->AssetId, CookedModel);
    TAssetHandle<FMaterialAsset> CookedMaterial;
    TAssetHandle<FStaticMeshAsset> CookedMesh;
    const bool Loaded = CookedRoots &&
        Load(*Cooked, MaterialRecord->AssetId, CookedMaterial) &&
        Load(*Cooked, MeshRecord->AssetId, CookedMesh);
    Record(Result,
        Loaded && EqualTexture(*DevelopmentTexture, *CookedTexture),
        "development and strict cooked modes preserve texture semantics");
    Record(Result,
        Loaded && EqualShader(*DevelopmentShader, *CookedShader),
        "development and strict cooked modes preserve shader semantics");
    Record(Result,
        Loaded && EqualMaterial(*DevelopmentMaterial, *CookedMaterial),
        "development and strict cooked modes preserve material semantics");
    Record(Result,
        Loaded && EqualMesh(*DevelopmentMesh, *CookedMesh) &&
            EqualModel(*DevelopmentModel, *CookedModel),
        "development and strict cooked modes preserve model package semantics");
    Record(Result,
        Extensions.ResolveCalls->load() == SourceCallsBeforeCooked,
        "strict cooked mode performs zero resolver or source fallback calls");

    if (Development) (void)Development->Shutdown();
    if (Cooked) (void)Cooked->Shutdown();
    std::error_code Error;
    std::filesystem::remove_all(Root, Error);
    return Result;
}
