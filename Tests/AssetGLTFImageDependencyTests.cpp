#include "AssetGLTFImageDependencyTests.h"

#include "StaticModelTestSupport.h"

#include <filesystem>
#include <iostream>

namespace
{
using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace StaticModelTestSupport;

void Record(FAssetGLTFImageDependencyTestResult& Result, bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

class FFixtureResolver final : public IAssetResolver
{
public:
    FAssetExtensionCapability GetCapability() const override
    {
        FAssetExtensionCapability Capability;
        Capability.Kind = EAssetExtensionKind::Resolver;
        (void)FAssetParticipantId::Create(FString("tests.fixture"), Capability.Participant);
        (void)FAssetProducerVersion::Create(FString("1"), Capability.ProducerVersion);
        Capability.Schemes = {FString("fixture")};
        return Capability;
    }
    FAssetResolveResult Resolve(const FAssetResolveRequest& Request) override
    {
        FAssetResolveResult Result;
        const std::filesystem::path Path(
            Request.Location.GetLocator().ToStdString());
        if (!std::filesystem::exists(Path)) return Result;
        auto Bytes = ReadFixture(Path);
        Result.Result = EAssetResult::Success;
        Result.Descriptor.Location = Request.Location;
        Result.Descriptor.Size = Bytes.size();
        Result.Descriptor.FormatHint = FString(Path.extension() == ".png" ? "png" : "jpeg");
        Result.Source = FAssetSourceLease(MakeShared<FMemorySource>(std::move(Bytes)));
        return Result;
    }
};

TArray<FAssetImportOutput> ImportWithResolver(
    const std::filesystem::path& Path, EAssetResult& OutResult)
{
    FStaticModelImportRequest Request;
    Request.AssetRequest = MakeRequest(Path);
    Request.Profile = MakeShared<FStaticModelImportProfile>();
    Request.DependencyResolver = MakeShared<FFixtureResolver>();
    TArray<FAssetImportOutput> Outputs;
    OutResult = ImportStaticModel(Request, Outputs);
    return Outputs;
}
}

FAssetGLTFImageDependencyTestResult RunAssetGLTFImageDependencyTests()
{
    FAssetGLTFImageDependencyTestResult Result;
    EAssetResult ImportResult = EAssetResult::ProcessingFailure;
    const auto Embedded = Import(MakeRequest(
        "Tests/Fixtures/StaticModel/Valid/Materials/01-pbr-all-embedded.gltf"),
        ImportResult);
    const auto EmbeddedImages = FindPayloads<FImageAsset>(Embedded);
    const auto EmbeddedTextures = FindPayloads<FTextureAsset>(Embedded);
    bool SemanticSplit = ImportResult == EAssetResult::Success &&
        EmbeddedImages.size() == 1 && EmbeddedTextures.size() == 3;
    bool Color = false, Normal = false, Data = false;
    for (const auto& Texture : EmbeddedTextures)
    {
        Color |= Texture->GetSemantic() == ETextureSemantic::Color &&
            Texture->GetColorSpace() == EImageColorSpace::SRGB;
        Normal |= Texture->GetSemantic() == ETextureSemantic::Normal &&
            Texture->GetColorSpace() == EImageColorSpace::Linear;
        Data |= Texture->GetSemantic() == ETextureSemantic::Data &&
            Texture->GetColorSpace() == EImageColorSpace::Linear;
        SemanticSplit = SemanticSplit && Texture->GetImage()->GetId() ==
            EmbeddedImages.front()->GetId();
    }
    Record(Result, SemanticSplit && Color && Normal && Data,
        "shared image emits color normal and data textures with correct color spaces");

    const auto External = ImportWithResolver(
        "Tests/Fixtures/StaticModel/Valid/Materials/02-external-jpeg.gltf",
        ImportResult);
    Record(Result,
        ImportResult == EAssetResult::Success &&
            FindPayloads<FImageAsset>(External).size() == 1 &&
            FindPayloads<FTextureAsset>(External).size() == 1,
        "source-relative JPEG resolves only through the supplied resolver");

    const auto Missing = ImportWithResolver(
        "Tests/Fixtures/StaticModel/Invalid/Materials/01-missing-image.gltf",
        ImportResult);
    Record(Result,
        ImportResult == EAssetResult::NotFound && Missing.empty(),
        "missing external image fails the complete package atomically");
    return Result;
}
