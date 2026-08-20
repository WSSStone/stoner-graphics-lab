#include "MetalShaderPublicationTests.h"

#include "Asset/FPublishedGenerationValidator.h"
#include "MetalShaderCookedTestSupport.h"

#include <filesystem>
#include <iostream>

namespace
{
using namespace Stoner;
using namespace Stoner::Tests::MetalShaderCooked;

void Record(
    FMetalShaderPublicationTestResult& Result,
    bool Passed,
    const char* Label)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Label << '\n';
}

Asset::FPublishedGenerationValidationResult ValidateCurrent(
    const std::filesystem::path& Output)
{
    Asset::FPublishedGenerationValidationRequest Request;
    Request.SubjectRoot = Core::FString(Output.generic_string());
    return Asset::FPublishedGenerationValidator::Validate(Request);
}

Core::TSharedPtr<const Asset::FShaderPayloadAsset> SpirvPayload()
{
    const Core::TArray<Core::uint8> Bytes = {
        0x03, 0x02, 0x23, 0x07, 0x00, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x0f, 0x00, 0x05, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
        0x6d, 0x61, 0x69, 0x6e, 0x00, 0x00, 0x00, 0x00};
    Asset::FAssetVersion Version;
    Version.SourceDigest = Asset::FAssetDigest::FromBytes(Bytes);
    Version.ContentDigest = Version.SourceDigest;
    Asset::FShaderPayloadAsset Payload;
    const auto Created = Asset::FShaderPayloadAsset::Create(
        Id("ShaderPayload", "Tests/Metal/Strict", "payload.vulkan.vertex"),
        std::move(Version), Asset::EShaderBackendFamily::Vulkan,
        Core::FString("vulkan-1.3"), Asset::EShaderPayloadFormat::SPIRV,
        Asset::EShaderStage::Vertex, Core::FString("main"), {}, Bytes, Payload);
    return Created == Asset::EAssetResult::Success
        ? Core::MakeShared<const Asset::FShaderPayloadAsset>(std::move(Payload))
        : nullptr;
}

} // namespace

FMetalShaderPublicationTestResult RunMetalShaderPublicationTests()
{
    using namespace Stoner;
    using namespace Stoner::Tests::MetalShaderCooked;
    namespace Cooker = AssetCooker::Private;

    FMetalShaderPublicationTestResult Result;
    const auto Root = std::filesystem::temp_directory_path() /
        "stoner-metal-shader-publication";
    std::error_code Error;
    std::filesystem::remove_all(Root, Error);
    const auto Output = Root / "Published";

    FGeneration Valid;
    const bool Built = BuildGeneration(
        Root / "Valid",
        {MetalPayload(Asset::EShaderStage::Vertex, "payload.vulkan.vertex",
             "source-vertex-v1", 1),
         MetalPayload(Asset::EShaderStage::Fragment, "payload.vulkan.fragment",
             "source-fragment-v1", 2)},
        Valid);
    const auto Published = Built
        ? Cooker::FCookedGenerationPublisher::Publish(
              PublicationRequest(Valid, Output))
        : Cooker::FCookedGenerationPublicationResult{};
    const auto Current = ValidateCurrent(Output);
    bool EvidenceExact = Built;
    for (const auto& Payload : Valid.Payloads)
    {
        const auto* Native = Payload ? Payload->GetNativeLibraryEvidence() : nullptr;
        EvidenceExact = EvidenceExact && Native &&
            Native->Validate() == Asset::EAssetResult::Success &&
            Native->LibraryDigest ==
                Asset::FAssetDigest::FromBytes(Payload->GetBytes()) &&
            Native->SizeBytes == Payload->GetBytes().size();
    }
    Record(Result,
        Built && Published.Succeeded() && Published.bCommitted &&
            Current.Result == Asset::EAssetResult::Success &&
            Current.ValidatedPayloads == 2 && EvidenceExact,
        "complete v2 MetalLibrary envelopes publish with exact native evidence");

    const auto CurrentPath = Output / "Current.json";
    const auto BeforePointer = Read(CurrentPath);
    Core::TArray<Core::TArray<Core::uint8>> BeforePayloads;
    for (const auto& RecordValue : Valid.Manifest.Records)
    {
        BeforePayloads.push_back(Read(
            Output / "Generations" /
            Valid.Manifest.GenerationId.ToLowerHex().ToStdString() /
            RecordValue.PayloadLocator.ToStdString()));
    }

    FGeneration SourceOnly;
    const bool SourceOnlyBuilt = BuildGeneration(
        Root / "SourceOnly", {SpirvPayload()}, SourceOnly);
    Record(Result,
        !SourceOnlyBuilt && Read(CurrentPath) == BeforePointer &&
            ValidateCurrent(Output).Result == Asset::EAssetResult::Success,
        "source-only SPIR-V cannot form or replace a strict Metal generation");

    FGeneration WrongArchitecture;
    const bool WrongArchitectureBuilt = BuildGeneration(
        Root / "WrongArchitecture",
        {MetalPayload(Asset::EShaderStage::Vertex, "payload.vulkan.vertex",
             "source-vertex-v1", 1)},
        WrongArchitecture,
        "Config/AssetCooker/Profiles/Mac-Metal-X86_64.json");
    Record(Result,
        !WrongArchitectureBuilt && Read(CurrentPath) == BeforePointer,
        "CPU-incompatible native evidence is rejected before publication");

    const auto CorruptRecord = Valid.Manifest.Records.front();
    const auto CorruptPath = std::filesystem::path(Valid.ImageRoot.ToStdString()) /
        CorruptRecord.PayloadLocator.ToStdString();
    auto CorruptBytes = Read(CorruptPath);
    if (!CorruptBytes.empty()) CorruptBytes.back() ^= 1U;
    {
        std::ofstream OutputFile(CorruptPath, std::ios::binary | std::ios::trunc);
        OutputFile.write(
            reinterpret_cast<const char*>(CorruptBytes.data()),
            static_cast<std::streamsize>(CorruptBytes.size()));
    }
    const auto CorruptAttempt = Cooker::FCookedGenerationPublisher::Publish(
        PublicationRequest(Valid, Output));
    bool InstalledUnchanged = Read(CurrentPath) == BeforePointer;
    for (Core::usize Index = 0; Index < Valid.Manifest.Records.size(); ++Index)
    {
        InstalledUnchanged = InstalledUnchanged &&
            Read(Output / "Generations" /
                Valid.Manifest.GenerationId.ToLowerHex().ToStdString() /
                Valid.Manifest.Records[Index].PayloadLocator.ToStdString()) ==
                BeforePayloads[Index];
    }
    Record(Result,
        !CorruptAttempt.Succeeded() && !CorruptAttempt.bCommitted &&
            InstalledUnchanged &&
            ValidateCurrent(Output).Result == Asset::EAssetResult::Success,
        "failed finalization image rolls back while old generation stays immutable");

    Cooker::FCookedGenerationImageRequest EmptyFinalization;
    EmptyFinalization.ScratchRoot = Core::FString(
        (Root / "EmptyFinalization").generic_string());
    EmptyFinalization.Manifest = Valid.Manifest;
    EmptyFinalization.CanonicalManifest = Valid.CanonicalManifest;
    const auto Empty =
        Cooker::FCookedGenerationPublisher::BuildRequestImage(EmptyFinalization);
    Record(Result,
        !Empty.Succeeded() && Empty.ImageRoot.IsEmpty() &&
            Read(CurrentPath) == BeforePointer,
        "failed native finalization emits no publishable generation image");

    std::filesystem::remove_all(Root, Error);
    return Result;
}
