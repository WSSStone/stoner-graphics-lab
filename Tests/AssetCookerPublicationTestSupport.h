#pragma once

#include "AssetCookerDerivedDataTestSupport.h"
#include "FCookedGenerationPublisher.h"
#include "FPublishedGenerationValidator.h"

#include <filesystem>
#include <iostream>

namespace Stoner::Tests::AssetCookerPublication
{

using AssetCookerDDC::FRun;

inline FRun Seed(
    const std::filesystem::path& Root,
    std::filesystem::path& OutContent)
{
    OutContent = Root / "Content";
    AssetCookerDDC::SeedPng(OutContent);
    auto Request = AssetCookerDDC::Request(Root / "Seed", OutContent, 1);
    Request.CachePolicy = AssetCooker::EAssetCookCachePolicy::IgnoreExisting;
    return AssetCookerDDC::Run(Request);
}

inline AssetCooker::Private::FCookedGenerationPublicationRequest Request(
    const FRun& SeedRun,
    const std::filesystem::path& Output,
    const AssetCooker::Private::FPublicationTestHooks* Hooks = nullptr)
{
    AssetCooker::Private::FCookedGenerationPublicationRequest Value;
    Value.RequestImageRoot = SeedRun.Result.GenerationImageRoot;
    Value.OutputRoot = Core::FString(Output.generic_string());
    Value.Manifest = SeedRun.Result.Manifest;
    Value.CanonicalManifest = SeedRun.Result.CanonicalManifest;
    Value.RevalidateInputs = [] { return Asset::EAssetResult::Success; };
    Value.TestHooks = Hooks;
    return Value;
}

inline AssetCooker::Private::FPublishedGenerationValidationResult ValidateCurrent(
    const std::filesystem::path& Output)
{
    AssetCooker::Private::FPublishedGenerationValidationRequest Request;
    Request.SubjectRoot = Core::FString(Output.generic_string());
    return AssetCooker::Private::FPublishedGenerationValidator::Validate(Request);
}

inline Core::TArray<Core::uint8> Read(const std::filesystem::path& Path)
{
    return AssetCookerDDC::Read(Path);
}

inline void Write(
    const std::filesystem::path& Path,
    const Core::TArray<Core::uint8>& Bytes)
{
    AssetCookerDDC::Write(Path, Bytes);
}

inline void Record(
    int& Passed,
    int& Failed,
    bool Condition,
    const char* Label)
{
    (Condition ? ++Passed : ++Failed);
    std::cout << (Condition ? "[PASS] " : "[FAIL] ") << Label << '\n';
}

} // namespace Stoner::Tests::AssetCookerPublication
