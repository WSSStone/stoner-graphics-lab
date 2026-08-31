#include "FDemoConfiguration.h"

#include "Core/SGPlatform.h"

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

void ApplyBackendDefaults(FDemoConfiguration& Config)
{
    if (Config.GraphicsBackend != EDemoGraphicsBackend::Metal) return;
#if defined(__aarch64__) || defined(__arm64__)
    constexpr const char* Architecture = "arm64";
    constexpr const char* Profile =
        "Config/AssetCooker/Profiles/Mac-Metal-Arm64.json";
#else
    constexpr const char* Architecture = "x86_64";
    constexpr const char* Profile =
        "Config/AssetCooker/Profiles/Mac-Metal-X86_64.json";
#endif
    if (Config.CookedPublicationRoot.IsEmpty())
        Config.CookedPublicationRoot = Stoner::Core::FString(
            std::string("Build/Feature027Cook/") + Architecture);
    if (Config.LeaseCoordinationRoot.IsEmpty())
        Config.LeaseCoordinationRoot = Stoner::Core::FString(
            std::string("Build/Feature027Cook/Lease-") + Architecture);
    if (Config.TargetProfilePath.IsEmpty())
        Config.TargetProfilePath = Profile;
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
    if (ValidationLifecycleCycles > 0 &&
        (RunMode != EDemoRunMode::BoundedNative ||
            ValidationLifecycleCycles > 1000))
        return Fail("lifecycle-cycles require bounded visible mode and range 1..1000");
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
    if (GraphicsBackend == EDemoGraphicsBackend::Metal && RequiresNativeRuntime() &&
        (CookedPublicationRoot.IsEmpty() || LeaseCoordinationRoot.IsEmpty() ||
            TargetProfilePath.IsEmpty()))
        return Fail("native Metal requires cooked, lease, and target-profile paths");
    if (Workload == EDemoWorkload::Triangle)
    {
        if (RenderPath != EDemoRenderPath::Triangle)
            return Fail("triangle workload requires the triangle render path");
        if (!ProductionRoot.IsEmpty() || !StrictGeneration.IsEmpty() ||
            !WorkloadRevision.IsEmpty() || bVisibleCapture ||
            bProductionCameraPreview || !ProductionCameraPresetOutput.IsEmpty())
            return Fail("production options require the production-content workload");
    }
    else
    {
        if (RenderPath == EDemoRenderPath::Triangle)
            return Fail("production workload requires an explicit render path");
        if (ProductionRoot.IsEmpty() || StrictGeneration.IsEmpty() ||
            WorkloadRevision.IsEmpty() || CookedPublicationRoot.IsEmpty() ||
            LeaseCoordinationRoot.IsEmpty() || TargetProfilePath.IsEmpty() ||
            DeviceClassRegistryPath.IsEmpty())
            return Fail("production workload requires strict root, generation, revision, paths, and registry");
        const bool bRegular = ProductionLifecycleCycles == 20 &&
            ProductionWarmupCycles == 2;
        const bool bScaleLifecycle = ProductionLifecycleCycles == 100 &&
            ProductionWarmupCycles == 10 &&
            WorkloadRevision == Core::FString("production-content-sponza-v2");
        const bool bExtended = ProductionLifecycleCycles == 1000 &&
            ProductionWarmupCycles == 20;
        if (!bRegular && !bScaleLifecycle && !bExtended)
            return Fail(
                "production lifecycle must use fixed 20/2, Sponza-v2 100/10, or 1000/20 cycles");
        if (ProductionMaxRssGrowthBytes != 16ULL * 1024ULL * 1024ULL)
            return Fail("production RSS growth limit must be exactly 16 MiB");
        if (bVisibleCapture &&
            (!RequiresNativeRuntime() || !RequiresVisibleWindow() ||
                BaselineRoot.IsEmpty()))
            return Fail("visible capture requires native visible mode and a baseline root");
        if (bVisibleCapture &&
            (ClientWidth != ProductionImageAcceptanceExtent ||
             ClientHeight != ProductionImageAcceptanceExtent))
            return Fail("formal image acceptance requires exactly 512x512");
        if (bProductionCameraPreview)
        {
            if (RunMode != EDemoRunMode::InteractiveNative ||
                RenderPath != EDemoRenderPath::DeferredFull ||
                ProductionCameraPresetOutput.IsEmpty() || bVisibleCapture)
                return Fail("camera preview requires interactive Deferred production mode, output path, and no acceptance capture");
        }
        else if (!ProductionCameraPresetOutput.IsEmpty())
            return Fail("camera preset output requires camera preview mode");
    }
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
        if (Option == "--visible-capture")
        {
            Parsed.bVisibleCapture = true;
            continue;
        }
        if (Option == "--production-camera-preview")
        {
            Parsed.bProductionCameraPreview = true;
            continue;
        }
        if (Option == "--device-class")
        {
            OutReason = "device class must be registry-derived";
            return EDemoExitCode::InvalidConfiguration;
        }
        if (Option == "--execution-class")
        {
            OutReason = "execution class must be workflow-derived";
            return EDemoExitCode::InvalidConfiguration;
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
        else if (Option == "--backend")
        {
            const std::string_view Backend(Value);
            if (Backend == "vulkan")
                Parsed.GraphicsBackend = EDemoGraphicsBackend::Vulkan;
            else if (Backend == "metal")
                Parsed.GraphicsBackend = EDemoGraphicsBackend::Metal;
            else
            {
                OutReason = "unknown backend";
                return EDemoExitCode::InvalidConfiguration;
            }
        }
        else if (Option == "--workload")
        {
            const std::string_view Workload(Value);
            if (Workload == "triangle")
            {
                Parsed.Workload = EDemoWorkload::Triangle;
                Parsed.RenderPath = EDemoRenderPath::Triangle;
            }
            else if (Workload == "production-content")
                Parsed.Workload = EDemoWorkload::ProductionContent;
            else
            {
                OutReason = "unknown workload";
                return EDemoExitCode::InvalidConfiguration;
            }
        }
        else if (Option == "--render-path")
        {
            const std::string_view Path(Value);
            if (Path == "deferred-full")
                Parsed.RenderPath = EDemoRenderPath::DeferredFull;
            else if (Path == "forward-smoke")
                Parsed.RenderPath = EDemoRenderPath::ForwardSmoke;
            else
            {
                OutReason = "unknown render path";
                return EDemoExitCode::InvalidConfiguration;
            }
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
        else if (Option == "--lifecycle-cycles")
        {
            if (!ParseUInt(Value, Parsed.ValidationLifecycleCycles, false)) { OutReason = "invalid lifecycle-cycles"; return EDemoExitCode::InvalidConfiguration; }
        }
        else if (Option == "--shader-dir") Parsed.ShaderDirectory = Value;
        else if (Option == "--cooked-root") Parsed.CookedPublicationRoot = Value;
        else if (Option == "--lease-root") Parsed.LeaseCoordinationRoot = Value;
        else if (Option == "--target-profile") Parsed.TargetProfilePath = Value;
        else if (Option == "--production-root") Parsed.ProductionRoot = Value;
        else if (Option == "--strict-generation") Parsed.StrictGeneration = Value;
        else if (Option == "--workload-revision") Parsed.WorkloadRevision = Value;
        else if (Option == "--baseline-root") Parsed.BaselineRoot = Value;
        else if (Option == "--device-class-registry")
            Parsed.DeviceClassRegistryPath = Value;
        else if (Option == "--camera-preset-output")
            Parsed.ProductionCameraPresetOutput = Value;
        else if (Option == "--production-cycles")
        {
            if (!ParseUInt(Value, Parsed.ProductionLifecycleCycles, false))
            {
                OutReason = "invalid production-cycles";
                return EDemoExitCode::InvalidConfiguration;
            }
        }
        else if (Option == "--production-warmup-cycles")
        {
            if (!ParseUInt(Value, Parsed.ProductionWarmupCycles, false))
            {
                OutReason = "invalid production-warmup-cycles";
                return EDemoExitCode::InvalidConfiguration;
            }
        }
        else if (Option == "--production-max-rss-growth-mib")
        {
            Stoner::Core::uint32 MiB = 0;
            if (!ParseUInt(Value, MiB, false))
            {
                OutReason = "invalid production-max-rss-growth-mib";
                return EDemoExitCode::InvalidConfiguration;
            }
            Parsed.ProductionMaxRssGrowthBytes =
                static_cast<Stoner::Core::uint64>(MiB) * 1024ULL * 1024ULL;
        }
        else if (Option == "--validation-output") Parsed.ValidationOutputPath = Value;
        else { OutReason = "unknown option"; return EDemoExitCode::InvalidConfiguration; }
    }

    const Stoner::Core::uint32 ExplicitFrames = Parsed.FrameBudget;
    const Stoner::Core::uint32 ExplicitWarmup = Parsed.WarmupFrames;
    const Stoner::Core::uint32 ExplicitInterval = Parsed.MemorySampleInterval;
    const Stoner::Core::uint64 ExplicitGrowth = Parsed.MaxMemoryGrowthBytes;
    const double ExplicitPercent = Parsed.MaxMemoryGrowthPercent;
    ApplyProfileDefaults(Parsed);
    ApplyBackendDefaults(Parsed);
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

const char* ToString(EDemoGraphicsBackend Backend) noexcept
{
    switch (Backend)
    {
    case EDemoGraphicsBackend::Vulkan: return "vulkan";
    case EDemoGraphicsBackend::Metal: return "metal";
    }
    return "unknown";
}

const char* ToString(EDemoWorkload Workload) noexcept
{
    switch (Workload)
    {
    case EDemoWorkload::Triangle: return "triangle";
    case EDemoWorkload::ProductionContent: return "production-content";
    }
    return "unknown";
}

const char* ToString(EDemoRenderPath Path) noexcept
{
    switch (Path)
    {
    case EDemoRenderPath::Triangle: return "triangle";
    case EDemoRenderPath::DeferredFull: return "deferred-full";
    case EDemoRenderPath::ForwardSmoke: return "forward-smoke";
    }
    return "unknown";
}

} // namespace Stoner::Demo
