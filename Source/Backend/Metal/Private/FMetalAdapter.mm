#include "FMetalAdapter.h"

#import <Metal/Metal.h>

#include <algorithm>
#include <new>

namespace Stoner::Backend::Metal::Private
{
namespace
{

void ReleaseRetained(void*& Value) noexcept
{
    if (Value == nullptr) return;
    CFBridgingRelease(Value);
    Value = nullptr;
}

FMetalAdapterSummary Summarize(id<MTLDevice> Device)
{
    FMetalAdapterSummary Result;
    Result.RegistryId = Device.registryID;
    Result.Name = Core::FString(Device.name.UTF8String ?: "Metal Device");
    Result.bLowPower = Device.lowPower;
    Result.bRemovable = Device.removable;
    Result.bHeadless = Device.headless;
    return Result;
}

int PreferenceRank(
    const FMetalAdapterSummary& Candidate,
    EMetalAdapterPreference Preference) noexcept
{
    if (Candidate.bHeadless) return 3;
    if (Preference == EMetalAdapterPreference::LowPower)
        return Candidate.bLowPower ? 0 : 1;
    if (Preference == EMetalAdapterPreference::HighPerformance)
        return Candidate.bLowPower ? 1 : 0;
    return Candidate.bLowPower ? 1 : 0;
}

} // namespace

RHI::ERHIResult SelectMetalAdapterCandidate(
    const Core::TArray<FMetalAdapterSummary>& Candidates,
    const FMetalBackendConfig& Config,
    Core::usize& OutIndex,
    Core::FString& OutStableReason) noexcept
{
    OutIndex = 0;
    OutStableReason = Core::FString("metal-no-eligible-adapter");
    bool bFound = false;
    int BestRank = 0;
    Core::uint64 BestRegistry = 0;
    for (Core::usize Index = 0; Index < Candidates.size(); ++Index)
    {
        const auto& Candidate = Candidates[Index];
        if (Candidate.RegistryId == 0 ||
            (Config.bRequirePresentation && Candidate.bHeadless))
            continue;
        if (Config.PreferredRegistryId.has_value() &&
            Candidate.RegistryId != *Config.PreferredRegistryId)
            continue;
        const int Rank = PreferenceRank(Candidate, Config.AdapterPreference);
        if (!bFound || Rank < BestRank ||
            (Rank == BestRank && Candidate.RegistryId < BestRegistry))
        {
            bFound = true;
            BestRank = Rank;
            BestRegistry = Candidate.RegistryId;
            OutIndex = Index;
        }
    }
    if (!bFound)
    {
        if (Config.PreferredRegistryId.has_value())
            OutStableReason = Core::FString(
                "metal-explicit-adapter-unavailable");
        return RHI::ERHIResult::Unavailable;
    }
    OutStableReason = Core::FString("metal-adapter-selected");
    return RHI::ERHIResult::Success;
}

FMetalAdapterSelection::~FMetalAdapterSelection()
{
    ReleaseRetained(RetainedNativeDevice);
}

FMetalAdapterSelection::FMetalAdapterSelection(
    FMetalAdapterSelection&& Other) noexcept
    : Result(Other.Result),
      RetainedNativeDevice(Other.ReleaseNativeDevice()),
      Selected(std::move(Other.Selected)),
      Candidates(std::move(Other.Candidates)),
      StableReason(std::move(Other.StableReason))
{
}

FMetalAdapterSelection& FMetalAdapterSelection::operator=(
    FMetalAdapterSelection&& Other) noexcept
{
    if (this == &Other) return *this;
    ReleaseRetained(RetainedNativeDevice);
    Result = Other.Result;
    RetainedNativeDevice = Other.ReleaseNativeDevice();
    Selected = std::move(Other.Selected);
    Candidates = std::move(Other.Candidates);
    StableReason = std::move(Other.StableReason);
    return *this;
}

void* FMetalAdapterSelection::ReleaseNativeDevice() noexcept
{
    void* Result = RetainedNativeDevice;
    RetainedNativeDevice = nullptr;
    return Result;
}

FMetalAdapterSelection SelectMetalAdapter(
    const FMetalBackendConfig& Config) noexcept
{
    FMetalAdapterSelection Result;
    try
    {
        @autoreleasepool
        {
            @try
            {
                NSArray<id<MTLDevice>>* Devices = MTLCopyAllDevices();
            struct FCandidate
            {
                __strong id<MTLDevice> Device;
                FMetalAdapterSummary Summary;
            };
            Core::TArray<FCandidate> Eligible;
            for (id<MTLDevice> Device in Devices)
            {
                if (Device == nil) continue;
                FMetalAdapterSummary Summary = Summarize(Device);
                Result.Candidates.push_back(Summary);
                if (Config.bRequirePresentation && Summary.bHeadless) continue;
                Eligible.push_back({Device, std::move(Summary)});
            }
            std::sort(Result.Candidates.begin(), Result.Candidates.end(),
                [](const auto& Left, const auto& Right) {
                    return Left.RegistryId < Right.RegistryId;
                });
            Core::TArray<FMetalAdapterSummary> EligibleSummaries;
            EligibleSummaries.reserve(Eligible.size());
            for (const FCandidate& Candidate : Eligible)
                EligibleSummaries.push_back(Candidate.Summary);
            Core::usize SelectedIndex = 0;
            Result.Result = SelectMetalAdapterCandidate(
                EligibleSummaries, Config, SelectedIndex, Result.StableReason);
            if (Result.Result != RHI::ERHIResult::Success)
            {
                return Result;
            }
            const auto& Selected = Eligible[SelectedIndex];
            Result.Selected = Selected.Summary;
            Result.RetainedNativeDevice =
                (__bridge_retained void*)Selected.Device;
            Result.Result = RHI::ERHIResult::Success;
                Result.StableReason = Core::FString("metal-adapter-selected");
                return Result;
            }
            @catch (NSException*)
            {
                Result.Result = RHI::ERHIResult::Failed;
                Result.StableReason = Core::FString(
                    "metal-adapter-enumeration-failed");
                return Result;
            }
        }
    }
    catch (const std::bad_alloc&)
    {
        Result.Result = RHI::ERHIResult::Failed;
        Result.StableReason = Core::FString("metal-adapter-capacity");
        return Result;
    }
}

} // namespace Stoner::Backend::Metal::Private
