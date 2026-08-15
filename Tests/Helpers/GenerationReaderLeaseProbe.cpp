#include "Asset/FGenerationReaderLease.h"
#include "Core/FPlatformFileLease.h"

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

namespace
{
int ParseMilliseconds(const char* Text)
{
    if (!Text) return -1;
    char* End = nullptr;
    const long Value = std::strtol(Text, &End, 10);
    return End == Text || *End != '\0' || Value < 0 || Value > 600000
        ? -1
        : static_cast<int>(Value);
}
} // namespace

int main(int ArgCount, char* Arguments[])
{
    if (ArgCount != 7) return 2;
    const std::string Mode(Arguments[1]);
    const int Timeout = ParseMilliseconds(Arguments[5]);
    const int Hold = ParseMilliseconds(Arguments[6]);
    Stoner::Asset::FAssetDigest Generation;
    if (Timeout < 0 || Hold < 0 ||
        Stoner::Asset::FAssetDigest::ParseLowerHex(
            Stoner::Core::FString(Arguments[4]), Generation) !=
            Stoner::Asset::EAssetResult::Success)
        return 2;

    if (Mode == "reader" || Mode == "reader-crash")
    {
        Stoner::Asset::FGenerationReaderLease Lease;
        const auto Result = Stoner::Asset::FGenerationReaderLease::Acquire(
            Stoner::Core::FString(Arguments[2]),
            Stoner::Core::FString(Arguments[3]), Generation,
            static_cast<Stoner::Core::uint64>(Timeout), Lease);
        if (Result == Stoner::Asset::EAssetResult::TransientFailure) return 9;
        if (Result != Stoner::Asset::EAssetResult::Success) return 10;
        if (Mode == "reader-crash") std::_Exit(0);
        std::this_thread::sleep_for(std::chrono::milliseconds(Hold));
        return 0;
    }
    if (Mode != "exclusive") return 2;

    Stoner::Asset::FAssetDigest Namespace;
    if (Stoner::Asset::FGenerationReaderLease::DerivePublicationNamespace(
            Stoner::Core::FString(Arguments[2]), Namespace) !=
        Stoner::Asset::EAssetResult::Success)
        return 10;
    const auto LeasePath = std::filesystem::path(Arguments[3]) /
        Namespace.ToLowerHex().ToStdString() /
        (Generation.ToLowerHex().ToStdString() + ".lease");
    Stoner::Core::FPlatformFileLease Lease;
    const auto Status = Stoner::Core::FPlatformFileLease::Acquire(
        Stoner::Core::FString(LeasePath.generic_string()),
        Stoner::Core::EPlatformFileLeaseMode::Exclusive,
        static_cast<Stoner::Core::uint64>(Timeout),
        Stoner::Core::FString("owner=generation-maintenance-probe\n"), Lease);
    if (Status.Result == Stoner::Core::EPlatformFileResult::TimedOut) return 9;
    if (!Status.IsSuccess()) return 10;
    std::this_thread::sleep_for(std::chrono::milliseconds(Hold));
    return 0;
}
