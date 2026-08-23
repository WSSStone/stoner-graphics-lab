#include "ProductionAssetClosureTestSupport.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iterator>
#include <string>
#include <thread>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;

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
        const Core::usize Count = std::min(
            MaximumBytes, Bytes_.size() - Begin);
        OutBytes.assign(
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Begin),
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Begin + Count));
        return EAssetResult::Success;
    }

private:
    Core::TArray<Core::uint8> Bytes_;
};

class FProductionResolver final : public IAssetResolver
{
public:
    FProductionResolver(
        std::filesystem::path PackageRoot,
        std::filesystem::path ShaderRoot)
        : PackageRoot_(std::move(PackageRoot)),
          ShaderRoot_(std::move(ShaderRoot))
    {
    }

    FAssetExtensionCapability GetCapability() const override
    {
        FAssetExtensionCapability Result;
        Result.Kind = EAssetExtensionKind::Resolver;
        (void)FAssetParticipantId::Create(
            Core::FString("resolver.production-content"),
            Result.Participant);
        (void)FAssetProducerVersion::Create(
            Core::FString("028-v1"), Result.ProducerVersion);
        Result.Priority = 100;
        Result.Schemes = {Core::FString("asset")};
        Result.bRuntimeCompatible = true;
        return Result;
    }

    FAssetResolveResult Resolve(
        const FAssetResolveRequest& Request) override
    {
        FAssetResolveResult Result;
        if (Request.RuntimeContext && Request.RuntimeContext->ShouldStop())
        {
            Result.Result = EAssetResult::Cancelled;
            return Result;
        }
        if (Request.Location.GetScheme() != Core::FString("asset"))
            return Result;

        std::string Locator = Request.Location.GetLocator().ToStdString();
        if (Locator.starts_with("production/"))
            Locator.erase(0, std::string("production/").size());
        std::filesystem::path Path;
        constexpr std::string_view DeferredPrefix =
            "Engine/Shaders/Deferred/";
        if (Locator.starts_with(DeferredPrefix))
        {
            const std::string ProgramName =
                Locator.substr(DeferredPrefix.size());
            const std::filesystem::path Relative(ProgramName);
            if (ProgramName.empty() || Relative.has_parent_path())
            {
                Result.Result = EAssetResult::AccessDenied;
                return Result;
            }
            Path = ShaderRoot_ / (ProgramName + ".shader.json");
        }
        else
        {
            const std::filesystem::path Relative(Locator);
            if (Relative.is_absolute() ||
                std::find(Relative.begin(), Relative.end(), "..") !=
                    Relative.end())
            {
                Result.Result = EAssetResult::AccessDenied;
                return Result;
            }
            const auto PackageCandidate =
                (PackageRoot_ / Relative).lexically_normal();
            const auto ShaderCandidate =
                (ShaderRoot_ / Relative).lexically_normal();
            if (std::filesystem::is_regular_file(PackageCandidate))
                Path = PackageCandidate;
            else if (std::filesystem::is_regular_file(ShaderCandidate))
                Path = ShaderCandidate;
            else
            {
                Result.Result = EAssetResult::NotFound;
                return Result;
            }
        }

        std::ifstream Input(Path, std::ios::binary);
        if (!Input)
        {
            Result.Result = EAssetResult::NotFound;
            return Result;
        }
        Core::TArray<Core::uint8> Bytes{
            std::istreambuf_iterator<char>(Input),
            std::istreambuf_iterator<char>()};
        auto Source = Core::MakeShared<FFileSource>(std::move(Bytes));
        FAssetSourceLocator Location;
        if (FAssetSourceLocator::Create(
                Core::FString("asset"),
                Core::FString(Path.filename().generic_string()),
                Location) != EAssetResult::Success)
        {
            Result.Result = EAssetResult::InvalidInput;
            return Result;
        }
        Result.Descriptor.Location = std::move(Location);
        Result.Descriptor.Size = std::filesystem::file_size(Path);
        const std::string Extension = Path.extension().string();
        if (Extension == ".glb")
            Result.Descriptor.FormatHint = Core::FString("glb");
        else if (Extension == ".gltf")
            Result.Descriptor.FormatHint = Core::FString("gltf");
        else if (Extension == ".png")
            Result.Descriptor.FormatHint = Core::FString("png");
        else if (Extension == ".jpg" || Extension == ".jpeg")
            Result.Descriptor.FormatHint = Core::FString("jpeg");
        else if (Extension == ".hdr")
            Result.Descriptor.FormatHint = Core::FString("hdr");
        else if (Path.filename().string().ends_with(".shader.json"))
            Result.Descriptor.FormatHint = Core::FString("shader.json");
        Result.Source = FAssetSourceLease(std::move(Source));
        Result.Result = EAssetResult::Success;
        return Result;
    }

private:
    std::filesystem::path PackageRoot_;
    std::filesystem::path ShaderRoot_;
};

bool WaitForTerminal(
    const FAssetManager& Manager,
    FAssetRequestHandle Request,
    FAssetRequestSnapshot& Out,
    Core::uint64 RequestTimeoutMilliseconds)
{
    const auto Deadline = std::chrono::steady_clock::now() +
        std::chrono::milliseconds(RequestTimeoutMilliseconds + 1000);
    while (std::chrono::steady_clock::now() < Deadline)
    {
        if (Manager.Query(Request, Out) != EAssetResult::Success)
            return false;
        if (Out.State == EAssetRequestState::Ready ||
            Out.State == EAssetRequestState::Failed ||
            Out.State == EAssetRequestState::Cancelled)
            return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return false;
}

template <typename T>
bool LoadTyped(
    FAssetManager& Manager,
    const FAssetId& AssetId,
    FProductionAssetHandle& Out,
    Core::FString& OutFailure,
    Core::uint64 RequestTimeoutMilliseconds)
{
    FAssetRequestHandle Request;
    if (Manager.Request<T>(AssetId, Request) != EAssetResult::Success)
    {
        OutFailure = Core::FString("request-rejected");
        return false;
    }
    FAssetRequestSnapshot Snapshot;
    if (!WaitForTerminal(
            Manager, Request, Snapshot, RequestTimeoutMilliseconds) ||
        Snapshot.State != EAssetRequestState::Ready)
    {
        std::string Failure =
            "request-not-ready:" + AssetId.ToString().ToStdString() + ":" +
            std::to_string(static_cast<unsigned int>(Snapshot.Result));
        const FAssetManagerInspection Inspection = Manager.Inspect();
        const auto Operation = std::find_if(
            Inspection.Operations.begin(), Inspection.Operations.end(),
            [&AssetId](const auto& Candidate)
            { return Candidate.AssetId == AssetId; });
        if (Operation != Inspection.Operations.end() &&
            !Operation->FailurePath.empty())
        {
            Failure += ":path=";
            for (Core::usize Index = 0;
                 Index < Operation->FailurePath.size(); ++Index)
            {
                if (Index != 0) Failure += "->";
                Failure += Operation->FailurePath[Index]
                    .ToString().ToStdString();
            }
        }
        OutFailure = Core::FString(std::move(Failure));
        (void)Manager.ReleaseRequest(Request);
        return false;
    }
    TAssetHandle<T> Handle;
    if (Manager.GetResult(Request, Handle) != EAssetResult::Success ||
        !Handle.IsValid())
    {
        OutFailure = Core::FString("result-invalid");
        (void)Manager.ReleaseRequest(Request);
        return false;
    }
    Out = std::move(Handle);
    (void)Manager.ReleaseRequest(Request);
    return true;
}

bool LoadRecord(
    FAssetManager& Manager,
    const FAssetCookManifestRecord& Record,
    bool bStrictCooked,
    FProductionAssetHandle& Out,
    Core::FString& OutFailure,
    Core::uint64 RequestTimeoutMilliseconds)
{
    if (Record.AssetType == Core::FString("Image"))
        return LoadTyped<FImageAsset>(
            Manager, Record.AssetId, Out, OutFailure,
            RequestTimeoutMilliseconds);
    if (Record.AssetType == Core::FString("Texture"))
        return bStrictCooked
            ? LoadTyped<FKTX2TextureArtifact>(
                Manager, Record.AssetId, Out, OutFailure,
                RequestTimeoutMilliseconds)
            : LoadTyped<FTextureAsset>(
                Manager, Record.AssetId, Out, OutFailure,
                RequestTimeoutMilliseconds);
    if (Record.AssetType == Core::FString("ShaderSource"))
        return LoadTyped<FShaderSourceAsset>(
            Manager, Record.AssetId, Out, OutFailure,
            RequestTimeoutMilliseconds);
    if (Record.AssetType == Core::FString("ShaderPayload"))
        return LoadTyped<FShaderPayloadAsset>(
            Manager, Record.AssetId, Out, OutFailure,
            RequestTimeoutMilliseconds);
    if (Record.AssetType == Core::FString("ShaderProgram"))
        return LoadTyped<FShaderAsset>(
            Manager, Record.AssetId, Out, OutFailure,
            RequestTimeoutMilliseconds);
    if (Record.AssetType == Core::FString("Material"))
        return LoadTyped<FMaterialAsset>(
            Manager, Record.AssetId, Out, OutFailure,
            RequestTimeoutMilliseconds);
    if (Record.AssetType == Core::FString("MaterialInstance"))
        return LoadTyped<FMaterialInstanceAsset>(
            Manager, Record.AssetId, Out, OutFailure,
            RequestTimeoutMilliseconds);
    if (Record.AssetType == Core::FString("StaticMesh"))
        return LoadTyped<FStaticMeshAsset>(
            Manager, Record.AssetId, Out, OutFailure,
            RequestTimeoutMilliseconds);
    if (Record.AssetType == Core::FString("StaticModel"))
        return LoadTyped<FStaticModelAsset>(
            Manager, Record.AssetId, Out, OutFailure,
            RequestTimeoutMilliseconds);
    OutFailure = Core::FString("unsupported-asset-type");
    return false;
}

} // namespace

bool FProductionAssetExtensionSet::IsComplete() const noexcept
{
    return Registry && Resolver.IsActive() && ImageImporter.IsActive() &&
        MaterialImporter.IsActive() && StaticModelImporter.IsActive() &&
        KTX2Loader.IsActive() && Cooked.IsComplete();
}

const Stoner::Asset::FAssetPayload*
FProductionAssetClosureEntry::GetPayload() const
{
    return std::visit(
        [](const auto& Value) -> const Stoner::Asset::FAssetPayload*
        {
            return Value.Get();
        }, Handle);
}

const FProductionAssetClosureEntry* FProductionAssetClosure::Find(
    const Stoner::Asset::FAssetId& AssetId) const
{
    const auto Found = std::lower_bound(
        Entries.begin(), Entries.end(), AssetId,
        [](const auto& Entry, const auto& Id)
        {
            return Entry.AssetId < Id;
        });
    return Found == Entries.end() || Found->AssetId != AssetId
        ? nullptr : &*Found;
}

bool CreateProductionAssetExtensionSet(
    const std::filesystem::path& PackageRoot,
    const std::filesystem::path& ShaderRoot,
    FProductionAssetExtensionSet& Out)
{
    Out = {};
    Out.Registry =
        Stoner::Core::MakeShared<Stoner::Asset::FAssetExtensionRegistry>();
    if (Out.Registry->Register(
            Stoner::Core::MakeShared<FProductionResolver>(
                PackageRoot, ShaderRoot), Out.Resolver) !=
            Stoner::Asset::EAssetResult::Success ||
        Stoner::Asset::RegisterImageAssetImporter(
            *Out.Registry, Out.ImageImporter) !=
            Stoner::Asset::EAssetResult::Success ||
        Stoner::Asset::RegisterMaterialShaderDefinitionImporter(
            *Out.Registry, Out.MaterialImporter) !=
            Stoner::Asset::EAssetResult::Success ||
        Stoner::Asset::RegisterStaticModelImporter(
            *Out.Registry, Out.StaticModelImporter) !=
            Stoner::Asset::EAssetResult::Success ||
        Stoner::Asset::RegisterKTX2TextureLoader(
            *Out.Registry, Out.KTX2Loader) !=
            Stoner::Asset::EAssetResult::Success ||
        Stoner::Asset::RegisterCookedAssetExtensions(
            *Out.Registry, Out.Cooked) !=
            Stoner::Asset::EAssetResult::Success)
    {
        Out = {};
        return false;
    }
    return Out.IsComplete();
}

bool LoadProductionAssetClosure(
    Stoner::Asset::FAssetManager& Manager,
    const Stoner::Asset::FAssetCookManifest& Manifest,
    bool bStrictCooked,
    FProductionAssetClosure& Out,
    Stoner::Core::FString& OutFailure,
    Stoner::Core::uint64 RequestTimeoutMilliseconds)
{
    Out = {};
    OutFailure = {};
    FProductionAssetClosure Built;
    Built.GenerationIdentity = Manifest.GenerationId;
    Built.Entries.reserve(Manifest.Records.size());
    Core::TArray<const FAssetCookManifestRecord*> OrderedRecords;
    OrderedRecords.reserve(Manifest.Records.size());
    for (const FAssetId& Root : Manifest.Selection.Roots)
    {
        const auto Found = std::find_if(
            Manifest.Records.begin(), Manifest.Records.end(),
            [&Root](const auto& Record) { return Record.AssetId == Root; });
        if (Found == Manifest.Records.end())
        {
            OutFailure = Stoner::Core::FString("manifest-root-missing");
            return false;
        }
        OrderedRecords.push_back(&*Found);
    }
    for (const auto& Record : Manifest.Records)
    {
        if (std::find(Manifest.Selection.Roots.begin(),
                Manifest.Selection.Roots.end(), Record.AssetId) ==
            Manifest.Selection.Roots.end())
            OrderedRecords.push_back(&Record);
    }
    for (const FAssetCookManifestRecord* Record : OrderedRecords)
    {
        FProductionAssetHandle Handle;
        if (!LoadRecord(
                Manager, *Record, bStrictCooked, Handle, OutFailure,
                RequestTimeoutMilliseconds))
            return false;
        FProductionAssetClosureEntry Entry;
        Entry.AssetId = Record->AssetId;
        Entry.AssetType = Record->AssetType;
        Entry.Handle = std::move(Handle);
        Built.Entries.push_back(std::move(Entry));
    }
    std::sort(Built.Entries.begin(), Built.Entries.end(),
        [](const auto& Left, const auto& Right)
        {
            return Left.AssetId < Right.AssetId;
        });
    const bool bHasDuplicate = std::adjacent_find(
        Built.Entries.begin(), Built.Entries.end(),
        [](const auto& Left, const auto& Right)
        {
            return Left.AssetId == Right.AssetId;
        }) != Built.Entries.end();
    if (bHasDuplicate)
    {
        OutFailure = Stoner::Core::FString("closure-identity-duplicate");
        return false;
    }
    Out = std::move(Built);
    return true;
}
