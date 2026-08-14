#include "AssetCookerPublishedValidationTests.h"

#include "AssetCookerPublicationTestSupport.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{

void Flip(Stoner::Core::TArray<Stoner::Core::uint8>& Bytes, int Seed)
{
    if (Bytes.size() < 3) return;
    const auto Index = 1 + static_cast<Stoner::Core::usize>(Seed * 37) %
        (Bytes.size() - 2);
    Bytes[Index] ^= 0x01U;
}

void MutatePublishedCase(
    int Case,
    const std::filesystem::path& Output,
    const Stoner::AssetCooker::FAssetCookResult& Seed)
{
    using namespace Stoner::Tests::AssetCookerPublication;
    const auto Generation = Output / "Generations" /
        Seed.Manifest.GenerationId.ToLowerHex().ToStdString();
    const auto Pointer = Output / "Current.json";
    const auto Manifest = Generation / "Manifest.json";
    const auto Payload = Generation /
        Seed.Manifest.Records.front().PayloadLocator.ToStdString();
    if (Case == 1) std::filesystem::remove(Pointer);
    else if (Case == 2)
    {
        auto Value = Read(Pointer);
        Value.resize(Value.size() / 2);
        Write(Pointer, Value);
    }
    else if (Case == 3)
    {
        auto Value = Read(Pointer);
        Value.insert(Value.begin(), ' ');
        Write(Pointer, Value);
    }
    else if (Case <= 9)
    {
        auto Value = Read(Pointer);
        Flip(Value, Case);
        Write(Pointer, Value);
    }
    else if (Case == 10) std::filesystem::remove_all(Generation);
    else if (Case == 11) std::filesystem::remove(Manifest);
    else if (Case == 12)
    {
        auto Value = Read(Manifest);
        Value.resize(Value.size() / 2);
        Write(Manifest, Value);
    }
    else if (Case == 13)
    {
        auto Value = Read(Manifest);
        Value.insert(Value.begin(), ' ');
        Write(Manifest, Value);
    }
    else if (Case <= 27)
    {
        auto Value = Read(Manifest);
        Flip(Value, Case);
        Write(Manifest, Value);
    }
    else if (Case == 28) std::filesystem::remove(Payload);
    else if (Case == 29)
    {
        auto Value = Read(Payload);
        Value.resize(Value.size() / 2);
        Write(Payload, Value);
    }
    else Write(Generation / "unexpected.bin", {0xCA, 0xFE});
}

} // namespace

FAssetCookerPublishedValidationTestResult
RunAssetCookerPublishedValidationTests()
{
    using namespace Stoner;
    using namespace Stoner::Tests::AssetCookerPublication;
    namespace Private = AssetCooker::Private;
    FAssetCookerPublishedValidationTestResult Result;
    const auto Root = std::filesystem::temp_directory_path() /
        "stoner-cooker-published-validation";
    std::filesystem::remove_all(Root);
    std::filesystem::path Content;
    const FRun SeedRun = Seed(Root, Content);
    const auto Output = Root / "Published";
    const auto Published = Private::FCookedGenerationPublisher::Publish(
        Request(SeedRun, Output));
    auto Current = ValidateCurrent(Output);
    Record(Result.Passed, Result.Failed,
        Published.Succeeded() && Current.Result == Asset::EAssetResult::Success &&
            Current.ValidatedPayloads == SeedRun.Result.Manifest.Records.size(),
        "current-pointer validation checks the complete installed generation");

    Private::FPublishedGenerationValidationRequest Direct;
    Direct.SubjectRoot = Published.GenerationDirectory;
    Direct.Subject = Private::EPublishedValidationSubject::GenerationDirectory;
    Direct.ExpectedGenerationId = SeedRun.Result.Manifest.GenerationId;
    const auto DirectResult =
        Private::FPublishedGenerationValidator::Validate(Direct);
    Record(Result.Passed, Result.Failed,
        DirectResult.Result == Asset::EAssetResult::Success,
        "request-local and installed generation directories share one validator");

    std::filesystem::remove_all(Content);
    std::filesystem::remove_all(Root / "Seed" / "DDC");
    Record(Result.Passed, Result.Failed,
        ValidateCurrent(Output).Result == Asset::EAssetResult::Success,
        "standalone validation is independent of source files and derived data");

    Write(Output / "Generations" /
        SeedRun.Result.Manifest.GenerationId.ToLowerHex().ToStdString() /
        "unexpected.bin", {1, 2, 3});
    Record(Result.Passed, Result.Failed,
        ValidateCurrent(Output).Category ==
            Private::EPublishedCorruptionCategory::UnexpectedFile,
        "strict validation rejects unexpected generation files");
    std::filesystem::remove(Output / "Generations" /
        SeedRun.Result.Manifest.GenerationId.ToLowerHex().ToStdString() /
        "unexpected.bin");

    const auto PointerPath = Output / "Current.json";
    const auto PointerBytes = Read(PointerPath);
    Write(PointerPath, {'{', '}'});
    Record(Result.Passed, Result.Failed,
        ValidateCurrent(Output).Category ==
            Private::EPublishedCorruptionCategory::PointerInvalid,
        "standalone validation rejects a malformed current pointer");
    Write(PointerPath, PointerBytes);

    const auto AssetRecord = SeedRun.Result.Manifest.Records.front();
    const auto PayloadPath = Output / "Generations" /
        SeedRun.Result.Manifest.GenerationId.ToLowerHex().ToStdString() /
        AssetRecord.PayloadLocator.ToStdString();
    const auto Payload = Read(PayloadPath);
    auto Truncated = Payload;
    Truncated.pop_back();
    Write(PayloadPath, Truncated);
    Record(Result.Passed, Result.Failed,
        ValidateCurrent(Output).Category ==
            Private::EPublishedCorruptionCategory::PayloadInvalid,
        "standalone validation rejects truncated typed payloads");
    Write(PayloadPath, Payload);

    const auto Corpus = std::filesystem::path(
        "Tests/Fixtures/AssetCooker/CorruptPublished");
    Core::TArray<std::filesystem::path> Cases;
    for (const auto& Entry : std::filesystem::directory_iterator(Corpus))
        if (Entry.is_regular_file() &&
            Entry.path().extension() == ".json") Cases.push_back(Entry.path());
    std::sort(Cases.begin(), Cases.end());
    Core::uint32 Detected = 0;
    for (Core::usize Index = 0; Index < Cases.size(); ++Index)
    {
        const auto CaseOutput = Root / "CorruptCases" /
            Cases[Index].stem().string();
        std::filesystem::create_directories(CaseOutput.parent_path());
        std::filesystem::copy(Output, CaseOutput,
            std::filesystem::copy_options::recursive);
        MutatePublishedCase(
            static_cast<int>(Index + 1), CaseOutput, SeedRun.Result);
        if (ValidateCurrent(CaseOutput).Result != Asset::EAssetResult::Success)
            ++Detected;
    }
    std::cout << "[EVIDENCE] corrupt-published-detected=" << Detected
              << '/' << Cases.size() << '\n';
    Record(Result.Passed, Result.Failed,
        Cases.size() == 30 && Detected == Cases.size(),
        "standalone validation rejects all 30 independent published corruptions");

    std::filesystem::remove_all(Root);
    return Result;
}
