#include "Asset/AssetMinimal.h"
#include "Core/FPlatformFileLease.h"
#include "Core/FPlatformFileSystem.h"
#include "FCookedGenerationPublisher.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

namespace
{

int ParseNonNegative(const char* Text)
{
    if (!Text) return -1;
    char* End = nullptr;
    const long Value = std::strtol(Text, &End, 10);
    return End == Text || *End != '\0' || Value < 0 || Value > 600000
        ? -1 : static_cast<int>(Value);
}

Stoner::Core::FString Join(
    const std::filesystem::path& Root,
    const char* Relative)
{
    return Stoner::Core::FString((Root / Relative).generic_string());
}

} // namespace

int main(int ArgCount, char* Arguments[])
{
    if (ArgCount != 6) return 2;
    const std::string Mode(Arguments[1]);
    const std::filesystem::path Image(Arguments[2]);
    const std::filesystem::path Output(Arguments[3]);
    const int Timeout = ParseNonNegative(Arguments[4]);
    const int Hold = ParseNonNegative(Arguments[5]);
    if (Timeout < 0 || Hold < 0) return 2;
    if (!Stoner::Core::FPlatformFileSystem::CreateDirectory(
            Stoner::Core::FString(Output.generic_string()))) return 10;

    if (Mode == "hold" || Mode == "crash")
    {
        Stoner::Core::FPlatformFileLease Lease;
        const auto Status = Stoner::Core::FPlatformFileLease::Acquire(
            Join(Output, ".publish.lock"), static_cast<Stoner::Core::uint64>(Timeout),
            Stoner::Core::FString("owner=publication-probe"), Lease);
        if (Status.Result == Stoner::Core::EPlatformFileResult::TimedOut) return 9;
        if (!Status.IsSuccess()) return 10;
        if (Mode == "crash") std::_Exit(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(Hold));
        return 0;
    }
    if (Mode != "publish") return 2;

    Stoner::Core::TArray<Stoner::Core::uint8> ManifestBytes;
    if (!Stoner::Core::FPlatformFileSystem::ReadFile(
            Join(Image, "Manifest.json"), ManifestBytes)) return 10;
    Stoner::Asset::FAssetCookManifest Manifest;
    if (Stoner::Asset::FAssetCookContractCodec::ParseManifest(
            ManifestBytes, {}, Manifest) != Stoner::Asset::EAssetResult::Success)
        return 10;
    Stoner::AssetCooker::Private::FCookedGenerationPublicationRequest Request;
    Request.RequestImageRoot = Stoner::Core::FString(Image.generic_string());
    Request.OutputRoot = Stoner::Core::FString(Output.generic_string());
    Request.Manifest = std::move(Manifest);
    Request.CanonicalManifest = Stoner::Core::FString(std::string(
        reinterpret_cast<const char*>(ManifestBytes.data()), ManifestBytes.size()));
    Request.LeaseTimeout = std::chrono::milliseconds(Timeout);
    Request.RevalidateInputs = [] { return Stoner::Asset::EAssetResult::Success; };
    const auto Result =
        Stoner::AssetCooker::Private::FCookedGenerationPublisher::Publish(Request);
    if (Result.Category ==
        Stoner::AssetCooker::EAssetCookResultCategory::LeaseTimeout) return 9;
    return Result.Succeeded() ? 0 : 10;
}
