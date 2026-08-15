#include "FAssetSourceCatalog.h"

#include "Asset/AssetMinimal.h"
#include "Core/FPlatformFileSystem.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <map>
#include <set>
#include <string>

namespace Stoner::AssetCooker::Private
{
namespace
{

class FMemorySource final : public Asset::IAssetSource
{
public:
    explicit FMemorySource(Core::TArray<Core::uint8> Bytes)
        : Bytes_(std::move(Bytes)) {}

    Asset::EAssetResult Read(
        Core::uint64 Offset,
        Core::usize MaximumBytes,
        Core::TArray<Core::uint8>& OutBytes) const override
    {
        OutBytes.clear();
        if (Offset > Bytes_.size()) return Asset::EAssetResult::MalformedSource;
        const Core::usize Count = std::min(
            MaximumBytes, Bytes_.size() - static_cast<Core::usize>(Offset));
        OutBytes.assign(
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset),
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset + Count));
        return Asset::EAssetResult::Success;
    }

private:
    Core::TArray<Core::uint8> Bytes_;
};

class FLocalResolver final : public Asset::IAssetResolver
{
public:
    explicit FLocalResolver(std::filesystem::path Root) : Root_(std::move(Root)) {}

    Asset::FAssetExtensionCapability GetCapability() const override
    {
        Asset::FAssetExtensionCapability Value;
        Value.Kind = Asset::EAssetExtensionKind::Resolver;
        (void)Asset::FAssetParticipantId::Create(
            Core::FString("resolver.asset-cooker-local"), Value.Participant);
        (void)Asset::FAssetProducerVersion::Create(
            Core::FString("025-v1"), Value.ProducerVersion);
        Value.Priority = 100;
        Value.Schemes = {Core::FString("content")};
        return Value;
    }

    Asset::FAssetResolveResult Resolve(
        const Asset::FAssetResolveRequest& Request) override
    {
        Asset::FAssetResolveResult Result;
        Result.Descriptor.Location = Request.Location;
        if (Request.Location.GetScheme() != Core::FString("content")) return Result;
        const std::filesystem::path Relative(
            Request.Location.GetLocator().ToStdString());
        if (Relative.is_absolute() ||
            std::find(Relative.begin(), Relative.end(), "..") != Relative.end())
        {
            Result.Result = Asset::EAssetResult::AccessDenied;
            return Result;
        }
        const std::filesystem::path Path = (Root_ / Relative).lexically_normal();
        std::ifstream Input(Path, std::ios::binary);
        if (!Input)
        {
            Result.Result = Asset::EAssetResult::NotFound;
            return Result;
        }
        Core::TArray<Core::uint8> Bytes{
            std::istreambuf_iterator<char>(Input),
            std::istreambuf_iterator<char>()};
        Result.Descriptor.Size = Bytes.size();
        Result.Descriptor.FormatHint = Core::FString(Path.filename().string());
        Result.Source = Asset::FAssetSourceLease(
            Core::MakeShared<FMemorySource>(std::move(Bytes)));
        Result.Result = Asset::EAssetResult::Success;
        Resolved_[Result.Descriptor.Location] = {
            {Result.Descriptor, Result.Source, Core::FString("consumed")}, Path};
        return Result;
    }

    struct FResolvedRecord
    {
        FCookInputSource Source;
        std::filesystem::path PhysicalPath;
    };

    [[nodiscard]] const std::map<Asset::FAssetSourceLocator, FResolvedRecord>&
    GetResolved() const noexcept
    {
        return Resolved_;
    }

private:
    std::filesystem::path Root_;
    std::map<Asset::FAssetSourceLocator, FResolvedRecord> Resolved_;
};

bool Supported(std::string Extension, const std::string& Filename)
{
    std::transform(Extension.begin(), Extension.end(), Extension.begin(),
        [](unsigned char Value) { return static_cast<char>(std::tolower(Value)); });
    return Extension == ".png" || Extension == ".jpg" ||
        Extension == ".jpeg" || Extension == ".hdr" ||
        Extension == ".gltf" || Extension == ".glb" ||
        Extension == ".ktx2" ||
        Filename.ends_with(".shader.json") ||
        Filename.ends_with(".material.json") ||
        Filename.ends_with(".material-instance.json");
}

Core::FString LogicalPath(std::string Relative)
{
    const std::size_t Dot = Relative.find('.');
    if (Dot != std::string::npos) Relative.erase(Dot);
    return Core::FString("Cooked/" + Relative);
}

Asset::EAssetResult ImportImage(
    const Asset::FAssetSourceDescriptor& Descriptor,
    const Asset::FAssetSourceLease& Source,
    std::string Relative,
    Core::TArray<Asset::FAssetImportOutput>& Out)
{
    Asset::FAssetExtensionRegistry Registry;
    Asset::FAssetRegistrationToken Token;
    if (Asset::RegisterImageAssetImporter(Registry, Token) != Asset::EAssetResult::Success)
        return Asset::EAssetResult::ProcessingFailure;
    auto Parameters = Core::MakeShared<Asset::FImageImportParameters>();
    const Core::FString Path = LogicalPath(std::move(Relative));
    (void)Asset::FAssetId::Create(
        Core::FString("Image"), Path,
        Core::FString("image"), Parameters->ImageId);
    (void)Asset::FAssetId::Create(
        Core::FString("Texture"), Path,
        Core::FString("texture"), Parameters->TextureId);
    Parameters->Settings.Semantic = Asset::ETextureSemantic::Color;
    Parameters->Settings.ColorSpace = Asset::EImageColorSpace::SRGB;
    Parameters->Settings.MipPolicy = Asset::EImageMipPolicy::FullChain;
    return Asset::FAssetDispatch::Import(
        Registry,
        Asset::FAssetImportRequest{Descriptor, Source, Parameters, {}},
        Out);
}

Asset::EAssetResult ImportMaterialShader(
    const Asset::FAssetSourceDescriptor& Descriptor,
    const Asset::FAssetSourceLease& Source,
    const Core::TSharedPtr<FLocalResolver>& Resolver,
    Core::TArray<Asset::FAssetImportOutput>& Out)
{
    Asset::FAssetExtensionRegistry Registry;
    Asset::FAssetRegistrationToken ResolverToken;
    if (Registry.Register(Resolver, ResolverToken) != Asset::EAssetResult::Success)
        return Asset::EAssetResult::ProcessingFailure;
    Asset::FMaterialShaderLoadRequest Request;
    Request.Extensions = &Registry;
    Request.Descriptor = Descriptor;
    Request.Source = Source;
    const Asset::FMaterialShaderLoadResult Loaded =
        Asset::FMaterialShaderSourceLoader::Load(Request);
    if (!Loaded.Succeeded() || Loaded.Payloads.size() != Loaded.Metadata.size())
        return Loaded.Result;
    for (Core::usize Index = 0; Index < Loaded.Payloads.size(); ++Index)
        Out.push_back({Loaded.Metadata[Index], Loaded.Payloads[Index]});
    return Asset::EAssetResult::Success;
}

Asset::EAssetResult ImportStaticModel(
    const Asset::FAssetSourceDescriptor& Descriptor,
    const Asset::FAssetSourceLease& Source,
    const Core::TSharedPtr<FLocalResolver>& Resolver,
    Core::TArray<Asset::FAssetImportOutput>& Out)
{
    Asset::FStaticModelImportRequest Request;
    Request.AssetRequest.Descriptor = Descriptor;
    Request.AssetRequest.Source = Source;
    Request.DependencyResolver = Resolver;
    Request.Profile = Core::MakeShared<Asset::FStaticModelImportProfile>();
    return Asset::ImportStaticModel(Request, Out);
}

Asset::EAssetResult ImportKTX2(
    const Asset::FAssetSourceDescriptor& Descriptor,
    const Core::TArray<Core::uint8>& Bytes,
    Core::TArray<Asset::FAssetImportOutput>& Out)
{
    Asset::FKTX2TextureInfo Info;
    Asset::EAssetResult Result = Asset::FKTX2TextureCodec::Inspect(
        Bytes, {}, Info);
    if (Result != Asset::EAssetResult::Success) return Result;
    Asset::FKTX2TextureArtifact Artifact;
    Result = Asset::FKTX2TextureCodec::Open(
        Info.TextureId, Bytes, {}, Artifact);
    if (Result != Asset::EAssetResult::Success) return Result;
    Asset::FAssetMetadata Metadata;
    Metadata.Id = Info.TextureId;
    Metadata.Source = Descriptor.Location;
    Metadata.Version.SourceDigest = Info.SourceDigest;
    Metadata.Version.ContentDigest = Info.ContentDigest;
    (void)Asset::FAssetParticipantId::Create(
        Core::FString("cooker.ktx2"), Metadata.Producer);
    (void)Asset::FAssetProducerVersion::Create(
        Info.ProducerVersion, Metadata.ProducerVersion);
    Out.push_back({
        std::move(Metadata),
        Core::MakeShared<Asset::FKTX2TextureArtifact>(std::move(Artifact))});
    return Asset::EAssetResult::Success;
}

} // namespace

Asset::EAssetResult FAssetSourceCatalog::Discover(
    const FAssetCookRequest& Request,
    FAssetSourceCatalogResult& OutCatalog)
{
    OutCatalog = {};
    FAssetSourceCatalogResult Built;
    if (Request.Validate() != Asset::EAssetResult::Success)
        return Asset::EAssetResult::InvalidInput;
    std::set<std::string> CaseFoldedLocators;
    std::map<Asset::FAssetId, Asset::FAssetVersion> Identities;
    std::map<Asset::FAssetSourceLocator, std::filesystem::path> RevalidationPaths;
    for (const Core::FString& RootText : Request.SourceRoots)
    {
        const std::filesystem::path Root(RootText.ToStdString());
        Core::FPlatformFileEnumerationOptions Options;
        Options.MaxFiles = Request.TargetProfile.Profile.Limits.MaxDiscoveredSources;
        Options.MaxDepth = Request.TargetProfile.Profile.Limits.MaxDependencyDepth;
        Core::TArray<Core::FPlatformFileInfo> Files;
        if (!Core::FPlatformFileSystem::EnumerateRegularFiles(
                RootText, Options, Files).IsSuccess())
            return Asset::EAssetResult::AccessDenied;
        auto Resolver = Core::MakeShared<FLocalResolver>(Root);
        for (const auto& File : Files)
        {
            const std::filesystem::path Physical(File.Path.ToStdString());
            std::error_code Error;
            const std::string Relative = std::filesystem::relative(
                Physical, Root, Error).generic_string();
            if (Error || Relative.empty() || Relative.starts_with(".."))
                return Asset::EAssetResult::AccessDenied;
            if (!Supported(Physical.extension().string(), Physical.filename().string()))
                continue;
            std::string Folded = Relative;
            std::transform(Folded.begin(), Folded.end(), Folded.begin(),
                [](unsigned char Value) { return static_cast<char>(std::tolower(Value)); });
            if (!CaseFoldedLocators.insert(Folded).second)
                return Asset::EAssetResult::Conflict;
            Asset::FAssetSourceLocator Locator;
            if (Asset::FAssetSourceLocator::Create(
                    Core::FString("content"), Core::FString(Relative), Locator) !=
                Asset::EAssetResult::Success)
                return Asset::EAssetResult::InvalidIdentity;
            Asset::FAssetResolveResult Resolved = Resolver->Resolve({Locator, {}});
            if (Resolved.Result != Asset::EAssetResult::Success)
                return Resolved.Result;
            Core::TArray<Core::uint8> Bytes;
            const Asset::EAssetResult Read = Resolved.Source.ReadBounded(
                Request.TargetProfile.Profile.Limits.MaxSourceBytes,
                Resolved.Descriptor.Size, Bytes);
            if (Read != Asset::EAssetResult::Success) return Read;
            FDiscoveredAssetSource Discovered;
            Discovered.Descriptor = Resolved.Descriptor;
            Discovered.NormalizedRelativePath = Core::FString(Relative);
            Discovered.SourceVersion.SourceDigest = Asset::FAssetDigest::FromBytes(Bytes);
            Discovered.SourceVersion.ContentDigest =
                Discovered.SourceVersion.SourceDigest;
            const Asset::FAssetSourceLease Pinned(
                Core::MakeShared<FMemorySource>(Bytes));
            const std::string Filename = Physical.filename().string();
            std::string Extension = Physical.extension().string();
            std::transform(Extension.begin(), Extension.end(), Extension.begin(),
                [](unsigned char Value) { return static_cast<char>(std::tolower(Value)); });
            Asset::EAssetResult Imported = Asset::EAssetResult::Unsupported;
            if (Extension == ".png" ||
                Extension == ".jpg" || Extension == ".jpeg" ||
                Extension == ".hdr")
                Imported = ImportImage(
                    Resolved.Descriptor, Pinned, Relative, Discovered.Outputs);
            else if (Filename.ends_with(".shader.json") ||
                     Filename.ends_with(".material.json") ||
                     Filename.ends_with(".material-instance.json"))
                Imported = ImportMaterialShader(
                    Resolved.Descriptor, Pinned, Resolver, Discovered.Outputs);
            else if (Extension == ".gltf" || Extension == ".glb")
                Imported = ImportStaticModel(
                    Resolved.Descriptor, Pinned, Resolver, Discovered.Outputs);
            else if (Extension == ".ktx2")
                Imported = ImportKTX2(
                    Resolved.Descriptor, Bytes, Discovered.Outputs);
            if (Imported == Asset::EAssetResult::Unsupported) continue;
            if (Imported != Asset::EAssetResult::Success || Discovered.Outputs.empty())
                return Imported;
            std::sort(Discovered.Outputs.begin(), Discovered.Outputs.end(),
                [](const auto& Left, const auto& Right)
                { return Left.Metadata.Id < Right.Metadata.Id; });
            for (const auto& Output : Discovered.Outputs)
            {
                const auto [Found, Inserted] = Identities.emplace(
                    Output.Metadata.Id, Output.Metadata.Version);
                if (!Inserted && Found->second != Output.Metadata.Version)
                    return Asset::EAssetResult::Conflict;
                if (Inserted) Built.Outputs.push_back(Output);
            }
            Built.SnapshotSources.push_back({
                Resolved.Descriptor, Pinned, Core::FString("primary")});
            Built.Sources.push_back(std::move(Discovered));
        }
        for (const auto& [Locator, Record] : Resolver->GetResolved())
        {
            const bool AlreadyPinned = std::any_of(
                Built.SnapshotSources.begin(), Built.SnapshotSources.end(),
                [&Locator](const auto& Existing)
                { return Existing.Descriptor.Location == Locator; });
            if (!AlreadyPinned) Built.SnapshotSources.push_back(Record.Source);
            const auto [Found, Inserted] = RevalidationPaths.emplace(
                Locator, Record.PhysicalPath);
            if (!Inserted && Found->second != Record.PhysicalPath)
                return Asset::EAssetResult::Conflict;
        }
    }
    std::sort(Built.Sources.begin(), Built.Sources.end(),
        [](const auto& Left, const auto& Right)
        { return Left.Descriptor.Location < Right.Descriptor.Location; });
    std::sort(Built.Outputs.begin(), Built.Outputs.end(),
        [](const auto& Left, const auto& Right)
        { return Left.Metadata.Id < Right.Metadata.Id; });
    if (Built.Outputs.empty()) return Asset::EAssetResult::NotFound;
    Built.Revalidate = [Paths = std::move(RevalidationPaths)](
        const Asset::FAssetSourceLocator& Locator)
    {
        Asset::FAssetResolveResult Result;
        const auto Found = Paths.find(Locator);
        if (Found == Paths.end())
        {
            Result.Result = Asset::EAssetResult::NotFound;
            return Result;
        }
        std::ifstream Input(Found->second, std::ios::binary);
        if (!Input)
        {
            Result.Result = Asset::EAssetResult::NotFound;
            return Result;
        }
        Core::TArray<Core::uint8> Bytes{
            std::istreambuf_iterator<char>(Input),
            std::istreambuf_iterator<char>()};
        Result.Descriptor.Location = Locator;
        Result.Descriptor.Size = Bytes.size();
        Result.Descriptor.FormatHint = Core::FString(
            Found->second.filename().string());
        Result.Source = Asset::FAssetSourceLease(
            Core::MakeShared<FMemorySource>(std::move(Bytes)));
        Result.Result = Asset::EAssetResult::Success;
        return Result;
    };
    OutCatalog = std::move(Built);
    return Asset::EAssetResult::Success;
}

} // namespace Stoner::AssetCooker::Private
