#include "FDemoValidationMonitor.h"

#include "FDemoDiagnostics.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace Stoner::Demo
{
namespace
{

Stoner::Core::uint64 MedianFive(const Stoner::Core::TArray<FDemoMemorySample>& Samples, bool bFirst)
{
    Stoner::Core::TArray<Stoner::Core::uint64> Values;
    const Stoner::Core::usize Start = bFirst ? 0 : Samples.size() - 5;
    for (Stoner::Core::usize Index = Start; Index < Start + 5; ++Index) Values.push_back(Samples[Index].ResidentBytes);
    std::sort(Values.begin(), Values.end());
    return Values[2];
}

} // namespace

FDemoValidationMonitor::FDemoValidationMonitor(const FDemoConfiguration& InConfiguration)
    : Configuration(InConfiguration)
{
}

bool FDemoValidationMonitor::Sample(Stoner::Core::uint32 CompletedFrame,
    const Stoner::RHI::FRHIRuntimeSnapshot& RuntimeSnapshot)
{
    if (RuntimeSnapshot.ProvesNativeExecution()) RuntimeProofSnapshot = RuntimeSnapshot;
    if (!Configuration.IsBounded() || CompletedFrame <= Configuration.WarmupFrames ||
        (CompletedFrame - Configuration.WarmupFrames) % Configuration.MemorySampleInterval != 0)
    {
        return true;
    }
    const Stoner::Core::FProcessMemorySnapshot Memory = Stoner::Core::FPlatformMemory::QueryProcessMemory();
    if (!Memory.bAvailable || Memory.ResidentBytes == 0)
    {
        bMemoryAvailable = false;
        return false;
    }
    Samples.push_back({CompletedFrame, Memory.ResidentBytes, RuntimeSnapshot});
    return true;
}

void FDemoValidationMonitor::AddSyntheticSample(Stoner::Core::uint32 CompletedFrame,
    Stoner::Core::uint64 ResidentBytes, const Stoner::RHI::FRHIRuntimeSnapshot& RuntimeSnapshot)
{
    if (RuntimeSnapshot.ProvesNativeExecution()) RuntimeProofSnapshot = RuntimeSnapshot;
    Samples.push_back({CompletedFrame, ResidentBytes, RuntimeSnapshot});
}

bool FDemoValidationMonitor::Evaluate()
{
    if (!Configuration.IsBounded())
    {
        bPassed = true;
        return true;
    }
    if (!bMemoryAvailable || Samples.size() < 10 || CompletedFrames != RequestedFrames ||
        FinalSnapshot.GetTotalLiveObjectCount() != 0)
    {
        bPassed = false;
        return false;
    }
    BaselineMedianBytes = MedianFive(Samples, true);
    FinalMedianBytes = MedianFive(Samples, false);
    const Stoner::Core::uint64 RelativeLimit = static_cast<Stoner::Core::uint64>(
        static_cast<double>(BaselineMedianBytes) * Configuration.MaxMemoryGrowthPercent / 100.0);
    const Stoner::Core::uint64 AllowedGrowth = std::max(Configuration.MaxMemoryGrowthBytes, RelativeLimit);
    const Stoner::Core::uint64 Growth = FinalMedianBytes > BaselineMedianBytes ? FinalMedianBytes - BaselineMedianBytes : 0;
    const bool bMemoryWithinLimit = Growth <= AllowedGrowth;
    if (Configuration.RunMode == EDemoRunMode::BoundedNative)
    {
        const bool bRecoveriesValid = RecoveryMilliseconds.size() >= 20 &&
            std::none_of(RecoveryMilliseconds.begin(), RecoveryMilliseconds.end(), [](double Value)
            {
                return Value < 0.0 || Value > 2000.0;
            });
        bPassed = bMemoryWithinLimit && FirstPresentMilliseconds >= 0.0 &&
            FirstPresentMilliseconds <= 5000.0 && bRecoveriesValid;
        return bPassed;
    }
    bPassed = bMemoryWithinLimit;
    return bPassed;
}

Stoner::Core::FString FDemoValidationMonitor::BuildReport(const FDemoDiagnostics& Diagnostics) const
{
    std::ostringstream Stream;
    Stream << std::fixed << std::setprecision(3)
           << "feature=018-triangle-demo-integration\n"
           << "run-id=" << Configuration.EvidenceRunId.CStr() << '\n'
           << "mode=" << ToString(Configuration.RunMode) << '\n'
           << "runtime-object-mode=" << (RuntimeProofSnapshot.ProvesNativeExecution() ? "native" : "deterministic") << '\n'
           << "adapter=" << (RuntimeProofSnapshot.ProvesNativeExecution() ? RuntimeProofSnapshot.AdapterName.CStr() : "Deterministic") << '\n'
           << "software-device=" << (RuntimeProofSnapshot.bSoftwareDevice ? "true" : "false") << '\n'
           << "requested-frames=" << RequestedFrames << '\n'
           << "completed-frames=" << CompletedFrames << '\n'
           << "frames-in-flight=" << Configuration.MaxFramesInFlight << '\n'
           << "time-to-first-present-ms=" << FirstPresentMilliseconds << '\n'
           << "memory-samples=" << Samples.size() << '\n'
           << "memory-baseline-bytes=" << BaselineMedianBytes << '\n'
           << "memory-final-bytes=" << FinalMedianBytes << '\n'
           << "final-live-objects=" << FinalSnapshot.GetTotalLiveObjectCount() << '\n'
           << "validation-result=" << (bPassed ? "pass" : "fail") << '\n';
    Stream << "recovery-count=" << RecoveryMilliseconds.size() << '\n';
    for (Stoner::Core::usize Index = 0; Index < RecoveryMilliseconds.size(); ++Index)
        Stream << "recovery-ms[" << Index << "]=" << RecoveryMilliseconds[Index] << '\n';
    Stream << Diagnostics.BuildStableText().CStr();
    return Stream.str().c_str();
}

bool FDemoValidationMonitor::WriteReport(const FDemoDiagnostics& Diagnostics) const
{
    std::error_code Error;
    const std::filesystem::path Path(Configuration.ValidationOutputPath.CStr());
    if (Path.has_parent_path()) std::filesystem::create_directories(Path.parent_path(), Error);
    if (Error) return false;
    std::ofstream Output(Path, std::ios::binary | std::ios::trunc);
    if (!Output) return false;
    const Stoner::Core::FString Report = BuildReport(Diagnostics);
    Output.write(Report.CStr(), static_cast<std::streamsize>(Report.Len()));
    return Output.good();
}

} // namespace Stoner::Demo
