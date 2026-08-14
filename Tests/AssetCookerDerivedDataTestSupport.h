#pragma once

#include "Asset/AssetMinimal.h"
#include "AssetCooker/FAssetCookRunner.h"

#include <filesystem>
#include <fstream>

namespace Stoner::Tests::AssetCookerDDC
{

inline Core::TArray<Core::uint8> Read(const std::filesystem::path& Path)
{
    std::ifstream Input(Path, std::ios::binary);
    return {std::istreambuf_iterator<char>(Input), std::istreambuf_iterator<char>()};
}

inline void Write(
    const std::filesystem::path& Path,
    const Core::TArray<Core::uint8>& Bytes)
{
    std::filesystem::create_directories(Path.parent_path());
    std::ofstream Output(Path, std::ios::binary | std::ios::trunc);
    Output.write(reinterpret_cast<const char*>(Bytes.data()),
        static_cast<std::streamsize>(Bytes.size()));
}

inline AssetCooker::FAssetCookRequest Request(
    const std::filesystem::path& Root,
    const std::filesystem::path& Content,
    Core::uint32 Workers = 4)
{
    AssetCooker::FAssetCookRequest Value;
    Value.SourceRoots = {Core::FString(Content.generic_string())};
    Value.SelectionMode = Asset::EAssetCookSelectionMode::CookAll;
    Value.TargetProfilePath = Core::FString(
        "Tests/Fixtures/AssetCooker/Contracts/Profiles/mac-vulkan.json");
    (void)Asset::FAssetCookContractCodec::ParseTargetProfile(
        Read(Value.TargetProfilePath.ToStdString()), Value.TargetProfile);
    Value.OutputRoot = Core::FString((Root / "Output").generic_string());
    Value.DerivedDataRoot = Core::FString((Root / "DDC").generic_string());
    Value.ScratchRoot = Core::FString((Root / "Scratch").generic_string());
    Value.WorkerCount = Workers;
    return Value;
}

struct FRun
{
    AssetCooker::FAssetCookResult Result;
    AssetCooker::FAssetCookReport Report;
};

inline FRun Run(const AssetCooker::FAssetCookRequest& Request)
{
    FRun Value;
    Value.Result = AssetCooker::FAssetCookRunner::Run(Request, Value.Report);
    return Value;
}

inline void SeedPng(const std::filesystem::path& Content)
{
    Write(Content / "Representative.png",
        Read("Tests/Fixtures/Images/Valid/png-rgb-3x5.png"));
}

inline std::filesystem::path FirstEntry(const std::filesystem::path& Ddc)
{
    const auto Entries = Ddc / "Entries";
    if (!std::filesystem::exists(Entries)) return {};
    for (const auto& Shard : std::filesystem::directory_iterator(Entries))
        for (const auto& Entry : std::filesystem::directory_iterator(Shard.path()))
            if (Entry.is_directory()) return Entry.path();
    return {};
}

inline bool EqualArtifacts(
    const Core::TArray<AssetCooker::FAssetCookArtifact>& Left,
    const Core::TArray<AssetCooker::FAssetCookArtifact>& Right)
{
    if (Left.size() != Right.size()) return false;
    for (Core::usize Index = 0; Index < Left.size(); ++Index)
        if (Left[Index].AssetId != Right[Index].AssetId ||
            Left[Index].RelativeLocator != Right[Index].RelativeLocator ||
            Left[Index].EnvelopeDigest != Right[Index].EnvelopeDigest ||
            Left[Index].Bytes != Right[Index].Bytes) return false;
    return true;
}

} // namespace Stoner::Tests::AssetCookerDDC
