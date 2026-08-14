#pragma once

#include "Asset/AssetMinimal.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

namespace StaticModelTestSupport
{

using namespace Stoner::Asset;
using namespace Stoner::Core;

class FMemorySource final : public IAssetSource
{
public:
    explicit FMemorySource(TArray<uint8> Bytes)
        : Bytes_(std::move(Bytes))
    {
    }

    EAssetResult Read(
        uint64 Offset,
        usize MaximumBytes,
        TArray<uint8>& OutBytes) const override
    {
        OutBytes.clear();
        if (Offset > Bytes_.size()) return EAssetResult::MalformedSource;
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

class FFixtureResolver final : public IAssetResolver
{
public:
    FAssetExtensionCapability GetCapability() const override
    {
        FAssetExtensionCapability Capability;
        Capability.Kind = EAssetExtensionKind::Resolver;
        (void)FAssetParticipantId::Create(
            FString("tests.static-model-fixture"), Capability.Participant);
        (void)FAssetProducerVersion::Create(
            FString("1"), Capability.ProducerVersion);
        Capability.Schemes = {FString("fixture")};
        return Capability;
    }

    FAssetResolveResult Resolve(const FAssetResolveRequest& Request) override
    {
        FAssetResolveResult Result;
        const std::filesystem::path Path(
            Request.Location.GetLocator().ToStdString());
        if (!std::filesystem::is_regular_file(Path)) return Result;
        std::ifstream Stream(Path, std::ios::binary);
        TArray<uint8> Bytes{
            std::istreambuf_iterator<char>(Stream),
            std::istreambuf_iterator<char>()};
        Result.Result = EAssetResult::Success;
        Result.Descriptor.Location = Request.Location;
        Result.Descriptor.Size = Bytes.size();
        const std::string Extension = Path.extension().string();
        Result.Descriptor.FormatHint = FString(
            Extension == ".png" ? "png" : Extension == ".hdr" ? "hdr" : "jpeg");
        Result.Source = FAssetSourceLease(
            MakeShared<FMemorySource>(std::move(Bytes)));
        return Result;
    }
};

inline TArray<uint8> ReadFixture(const std::filesystem::path& Path)
{
    std::ifstream Stream(Path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(Stream),
        std::istreambuf_iterator<char>()};
}

inline TArray<std::filesystem::path> ValidFixturePaths()
{
    TArray<std::filesystem::path> Paths;
    const std::filesystem::path Root = "Tests/Fixtures/StaticModel";
    for (const auto& Entry : std::filesystem::recursive_directory_iterator(Root))
    {
        const bool InInvalidCorpus =
            Entry.path().generic_string().find("/Invalid/") != std::string::npos;
        if (!InInvalidCorpus && Entry.is_regular_file() &&
            (Entry.path().extension() == ".gltf" ||
             Entry.path().extension() == ".glb"))
        {
            Paths.push_back(Entry.path());
        }
    }
    std::sort(Paths.begin(), Paths.end());
    return Paths;
}

inline FAssetImportRequest MakeRequest(
    const std::filesystem::path& Path,
    const FString& LogicalPath = {},
    FStaticModelImportProfile Profile = {})
{
    TArray<uint8> Bytes = ReadFixture(Path);
    FAssetImportRequest Request;
    const FString Location = LogicalPath.IsEmpty()
        ? FString(Path.generic_string()) : LogicalPath;
    (void)FAssetSourceLocator::Create(
        FString("fixture"), Location, Request.Descriptor.Location);
    Request.Descriptor.Size = Bytes.size();
    Request.Descriptor.FormatHint = FString(
        Path.extension() == ".glb" ? "glb" : "gltf");
    Request.Source = FAssetSourceLease(
        MakeShared<FMemorySource>(std::move(Bytes)));
    Request.Parameters =
        MakeShared<FStaticModelImportProfile>(std::move(Profile));
    return Request;
}

inline FAssetImportRequest MakeMemoryRequest(
    TArray<uint8> Bytes,
    const FString& LogicalPath,
    FStaticModelImportProfile Profile = {})
{
    FAssetImportRequest Request;
    (void)FAssetSourceLocator::Create(
        FString("fixture"), LogicalPath, Request.Descriptor.Location);
    Request.Descriptor.Size = Bytes.size();
    Request.Descriptor.FormatHint = FString("gltf");
    Request.Source = FAssetSourceLease(
        MakeShared<FMemorySource>(std::move(Bytes)));
    Request.Parameters =
        MakeShared<FStaticModelImportProfile>(std::move(Profile));
    return Request;
}

inline TArray<FAssetImportOutput> Import(
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

inline TArray<FAssetImportOutput> ImportPackage(
    const std::filesystem::path& Path,
    EAssetResult& OutResult,
    FAssetDiagnosticList* Diagnostics = nullptr,
    FStaticModelImportProfile Profile = {})
{
    FStaticModelImportRequest Request;
    Request.AssetRequest = MakeRequest(Path, {}, std::move(Profile));
    Request.Profile = std::dynamic_pointer_cast<const FStaticModelImportProfile>(
        Request.AssetRequest.Parameters);
    Request.DependencyResolver = MakeShared<FFixtureResolver>();
    TArray<FAssetImportOutput> Outputs;
    OutResult = ImportStaticModel(Request, Outputs, Diagnostics);
    return Outputs;
}

template <typename TPayload>
inline TArray<TSharedPtr<const TPayload>> FindPayloads(
    const TArray<FAssetImportOutput>& Outputs)
{
    TArray<TSharedPtr<const TPayload>> Result;
    for (const FAssetImportOutput& Output : Outputs)
    {
        if (auto Payload = std::dynamic_pointer_cast<const TPayload>(Output.Payload))
            Result.push_back(std::move(Payload));
    }
    return Result;
}

inline TArray<FAssetId> SortedIds(const TArray<FAssetImportOutput>& Outputs)
{
    TArray<FAssetId> Ids;
    for (const FAssetImportOutput& Output : Outputs) Ids.push_back(Output.Metadata.Id);
    std::sort(Ids.begin(), Ids.end());
    return Ids;
}

} // namespace StaticModelTestSupport
