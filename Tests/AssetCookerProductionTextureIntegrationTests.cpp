#include "AssetCookerProductionTextureIntegrationTests.h"

#include "AssetCookerDerivedDataTestSupport.h"

#include <filesystem>
#include <iostream>

namespace
{

using namespace Stoner;
using namespace Stoner::Tests::AssetCookerDDC;

void Record(
    FAssetCookerProductionTextureIntegrationTestResult& Result,
    bool Passed,
    const char* Label)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Label << '\n';
}

bool IsKtxTextureArtifact(
    const AssetCooker::FAssetCookArtifact& Artifact)
{
    Core::TSharedPtr<const Asset::FAssetPayload> Payload;
    Asset::FAssetCookedPayloadEnvelope Envelope;
    return Asset::FAssetCookContractCodec::LoadTypedPayload(
               Artifact.Bytes, {}, Payload, &Envelope) ==
            Asset::EAssetResult::Success &&
        Envelope.Header.CodecId == Core::FString("stoner.ktx2") &&
        std::dynamic_pointer_cast<const Asset::FKTX2TextureArtifact>(Payload) !=
            nullptr;
}

} // namespace

FAssetCookerProductionTextureIntegrationTestResult
RunAssetCookerProductionTextureIntegrationTests()
{
    using namespace Stoner;
    using namespace Stoner::Tests::AssetCookerDDC;
    FAssetCookerProductionTextureIntegrationTestResult Result;
    const auto Root = std::filesystem::temp_directory_path() /
        "stoner-production-texture-integration";
    std::filesystem::remove_all(Root);
    const auto Content = Root / "Content";
    SeedPng(Content);

    const auto First = Run(Request(Root, Content));
    bool KtxPublished = First.Result.Succeeded();
    bool KtxProducer = false;
    for (const auto& Artifact : First.Result.Artifacts)
        if (Artifact.AssetId.GetAssetType() == Core::FString("Texture"))
            KtxPublished = KtxPublished && IsKtxTextureArtifact(Artifact);
    for (const auto& RecordValue : First.Result.Manifest.Records)
        if (RecordValue.AssetType == Core::FString("Texture"))
            KtxProducer = RecordValue.Cooker.Id.ToString() ==
                Core::FString("cooker.ktx2");
    Record(Result, KtxPublished && KtxProducer,
        "clean cook publishes a typed KTX2 envelope from cooker.ktx2");

    const auto Second = Run(Request(Root, Content));
    Record(Result,
        First.Result.Succeeded() && Second.Result.Succeeded() &&
            First.Result.Manifest.GenerationId ==
                Second.Result.Manifest.GenerationId &&
            EqualArtifacts(First.Result.Artifacts, Second.Result.Artifacts) &&
            Second.Report.Counts.Reused ==
                Second.Report.Counts.ReuseEligible,
        "warm production texture cook reuses the deterministic KTX2 DDC entry");

    std::filesystem::path TextureEntry;
    for (const auto& AssetReport : First.Report.Assets)
    {
        if (AssetReport.AssetId.GetAssetType() != Core::FString("Texture"))
            continue;
        const std::string Key = AssetReport.DerivedKey.ToString().ToStdString();
        TextureEntry = Root / "DDC" / "Entries" / Key.substr(0, 2) / Key /
            "Payload.sgasset";
    }
    auto CorruptBytes = Read(TextureEntry);
    if (!CorruptBytes.empty()) CorruptBytes.back() ^= 0x5aU;
    Write(TextureEntry, CorruptBytes);
    const auto Repaired = Run(Request(Root, Content));
    bool RepairedKtx = Repaired.Result.Succeeded() &&
        Repaired.Report.Counts.Invalidated == 1;
    for (const auto& Artifact : Repaired.Result.Artifacts)
        if (Artifact.AssetId.GetAssetType() == Core::FString("Texture"))
            RepairedKtx = RepairedKtx && IsKtxTextureArtifact(Artifact);
    Record(Result, RepairedKtx,
        "corrupt KTX2 DDC payload is quarantined and rebuilt as a strict envelope");

    std::filesystem::remove_all(Root);
    return Result;
}
