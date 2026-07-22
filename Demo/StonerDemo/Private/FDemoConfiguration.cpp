#include "FDemoConfiguration.h"

#include <cerrno>
#include <cstdlib>
#include <limits>
#include <string_view>

namespace Stoner::Demo
{
namespace
{

bool ParseUInt(const char* Text, Stoner::Core::uint32& OutValue, bool bAllowZero)
{
    if (Text == nullptr || Text[0] == '\0' || Text[0] == '-')
    {
        return false;
    }
    errno = 0;
    char* End = nullptr;
    const unsigned long long Value = std::strtoull(Text, &End, 10);
    if (errno != 0 || End == Text || *End != '\0' || Value > std::numeric_limits<Stoner::Core::uint32>::max() ||
        (!bAllowZero && Value == 0))
    {
        return false;
    }
    OutValue = static_cast<Stoner::Core::uint32>(Value);
    return true;
}

bool ParsePositiveDouble(const char* Text, double& OutValue)
{
    if (Text == nullptr || Text[0] == '\0')
    {
        return false;
    }
    errno = 0;
    char* End = nullptr;
    const double Value = std::strtod(Text, &End);
    if (errno != 0 || End == Text || *End != '\0' || !(Value > 0.0))
    {
        return false;
    }
    OutValue = Value;
    return true;
}

void ApplyProfileDefaults(FDemoConfiguration& Config)
{
    if (!Config.IsBounded())
    {
        return;
    }
    if (Config.FrameBudget == 0) Config.FrameBudget = Config.RunMode == EDemoRunMode::BoundedNative ? 10000 : 4096;
    if (Config.WarmupFrames == 0) Config.WarmupFrames = Config.RunMode == EDemoRunMode::BoundedNative ? 1000 : 512;
    if (Config.MemorySampleInterval == 0) Config.MemorySampleInterval = Config.RunMode == EDemoRunMode::BoundedNative ? 120 : 128;
    if (Config.MaxMemoryGrowthBytes == 0)
    {
        Config.MaxMemoryGrowthBytes = static_cast<Stoner::Core::uint64>(
            Config.RunMode == EDemoRunMode::DeterministicHeadless ? 16 : 64) * 1024ull * 1024ull;
    }
    if (Config.MaxMemoryGrowthPercent == 0.0)
    {
        Config.MaxMemoryGrowthPercent = Config.RunMode == EDemoRunMode::DeterministicHeadless ? 5.0 : 10.0;
    }
}

} // namespace

bool FDemoConfiguration::IsBounded() const noexcept
{
    return RunMode != EDemoRunMode::InteractiveNative;
}

bool FDemoConfiguration::RequiresNativeRuntime() const noexcept
{
    return RunMode != EDemoRunMode::DeterministicHeadless;
}

bool FDemoConfiguration::RequiresVisibleWindow() const noexcept
{
    return RunMode == EDemoRunMode::InteractiveNative || RunMode == EDemoRunMode::BoundedNative;
}

bool FDemoConfiguration::IsValid(Stoner::Core::FString* OutReason) const
{
    const auto Fail = [OutReason](const char* Reason)
    {
        if (OutReason) *OutReason = Reason;
        return false;
    };
    if (ClientWidth == 0 || ClientHeight == 0 || ClientWidth > 16384 || ClientHeight > 16384)
        return Fail("width and height must be in range 1..16384");
    if (MaxFramesInFlight == 0) return Fail("frames-in-flight must be positive");
    if (IsBounded())
    {
        if (FrameBudget == 0) return Fail("bounded modes require a positive frame budget");
        if (WarmupFrames >= FrameBudget) return Fail("warmup-frames must be smaller than frames");
        if (MemorySampleInterval == 0) return Fail("memory-sample-interval must be positive");
        if ((FrameBudget - WarmupFrames) / MemorySampleInterval < 10)
            return Fail("profile must permit at least ten post-warmup memory samples");
        if (MaxMemoryGrowthBytes == 0 || !(MaxMemoryGrowthPercent > 0.0))
            return Fail("memory growth limits must be positive");
        if (ValidationOutputPath.IsEmpty()) return Fail("validation-output must not be empty");
    }
    if (ShaderDirectory.IsEmpty()) return Fail("shader-dir must not be empty");
    return true;
}

EDemoExitCode FDemoConfiguration::Parse(int ArgCount, const char* const* Arguments,
    FDemoConfiguration& OutConfiguration, Stoner::Core::FString& OutReason)
{
    FDemoConfiguration Parsed;
    bool bFrameBudgetSpecified = false;
    bool bWarmupSpecified = false;
    bool bIntervalSpecified = false;
    bool bGrowthMiBSpecified = false;
    bool bGrowthPercentSpecified = false;

    for (int Index = 1; Index < ArgCount; ++Index)
    {
        const std::string_view Option(Arguments[Index]);
        if (Option == "--enable-validation")
        {
            Parsed.bEnableValidationLayers = true;
            continue;
        }
        if (Index + 1 >= ArgCount)
        {
            OutReason = "missing value for option";
            return EDemoExitCode::InvalidConfiguration;
        }
        const char* Value = Arguments[++Index];
        if (Option == "--mode")
        {
            const std::string_view Mode(Value);
            if (Mode == "interactive") Parsed.RunMode = EDemoRunMode::InteractiveNative;
            else if (Mode == "validate") Parsed.RunMode = EDemoRunMode::BoundedNative;
            else if (Mode == "headless") Parsed.RunMode = EDemoRunMode::DeterministicHeadless;
            else if (Mode == "headless-vulkan") Parsed.RunMode = EDemoRunMode::NativeHeadless;
            else { OutReason = "unknown mode"; return EDemoExitCode::InvalidConfiguration; }
        }
        else if (Option == "--frames")
        {
            if (!ParseUInt(Value, Parsed.FrameBudget, false)) { OutReason = "invalid frames"; return EDemoExitCode::InvalidConfiguration; }
            bFrameBudgetSpecified = true;
        }
        else if (Option == "--warmup-frames")
        {
            if (!ParseUInt(Value, Parsed.WarmupFrames, true)) { OutReason = "invalid warmup-frames"; return EDemoExitCode::InvalidConfiguration; }
            bWarmupSpecified = true;
        }
        else if (Option == "--memory-sample-interval")
        {
            if (!ParseUInt(Value, Parsed.MemorySampleInterval, false)) { OutReason = "invalid memory-sample-interval"; return EDemoExitCode::InvalidConfiguration; }
            bIntervalSpecified = true;
        }
        else if (Option == "--max-memory-growth-mib")
        {
            Stoner::Core::uint32 MiB = 0;
            if (!ParseUInt(Value, MiB, false)) { OutReason = "invalid max-memory-growth-mib"; return EDemoExitCode::InvalidConfiguration; }
            Parsed.MaxMemoryGrowthBytes = static_cast<Stoner::Core::uint64>(MiB) * 1024ull * 1024ull;
            bGrowthMiBSpecified = true;
        }
        else if (Option == "--max-memory-growth-percent")
        {
            if (!ParsePositiveDouble(Value, Parsed.MaxMemoryGrowthPercent)) { OutReason = "invalid max-memory-growth-percent"; return EDemoExitCode::InvalidConfiguration; }
            bGrowthPercentSpecified = true;
        }
        else if (Option == "--width")
        {
            if (!ParseUInt(Value, Parsed.ClientWidth, false)) { OutReason = "invalid width"; return EDemoExitCode::InvalidConfiguration; }
        }
        else if (Option == "--height")
        {
            if (!ParseUInt(Value, Parsed.ClientHeight, false)) { OutReason = "invalid height"; return EDemoExitCode::InvalidConfiguration; }
        }
        else if (Option == "--frames-in-flight")
        {
            if (!ParseUInt(Value, Parsed.MaxFramesInFlight, false)) { OutReason = "invalid frames-in-flight"; return EDemoExitCode::InvalidConfiguration; }
        }
        else if (Option == "--shader-dir") Parsed.ShaderDirectory = Value;
        else if (Option == "--validation-output") Parsed.ValidationOutputPath = Value;
        else { OutReason = "unknown option"; return EDemoExitCode::InvalidConfiguration; }
    }

    const Stoner::Core::uint32 ExplicitFrames = Parsed.FrameBudget;
    const Stoner::Core::uint32 ExplicitWarmup = Parsed.WarmupFrames;
    const Stoner::Core::uint32 ExplicitInterval = Parsed.MemorySampleInterval;
    const Stoner::Core::uint64 ExplicitGrowth = Parsed.MaxMemoryGrowthBytes;
    const double ExplicitPercent = Parsed.MaxMemoryGrowthPercent;
    ApplyProfileDefaults(Parsed);
    if (bFrameBudgetSpecified) Parsed.FrameBudget = ExplicitFrames;
    if (bWarmupSpecified) Parsed.WarmupFrames = ExplicitWarmup;
    if (bIntervalSpecified) Parsed.MemorySampleInterval = ExplicitInterval;
    if (bGrowthMiBSpecified) Parsed.MaxMemoryGrowthBytes = ExplicitGrowth;
    if (bGrowthPercentSpecified) Parsed.MaxMemoryGrowthPercent = ExplicitPercent;

    if (!Parsed.IsValid(&OutReason)) return EDemoExitCode::InvalidConfiguration;
    if (const char* RunId = std::getenv("STONER_DEMO_RUN_ID"); RunId != nullptr && RunId[0] != '\0') Parsed.EvidenceRunId = RunId;
    OutConfiguration = Parsed;
    return EDemoExitCode::Success;
}

const char* ToString(EDemoRunMode Mode) noexcept
{
    switch (Mode)
    {
    case EDemoRunMode::InteractiveNative: return "interactive";
    case EDemoRunMode::BoundedNative: return "validate";
    case EDemoRunMode::DeterministicHeadless: return "headless";
    case EDemoRunMode::NativeHeadless: return "headless-vulkan";
    }
    return "unknown";
}

} // namespace Stoner::Demo
