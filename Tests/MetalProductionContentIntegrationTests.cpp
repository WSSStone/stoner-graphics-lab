#include "MetalProductionContentIntegrationTests.h"

#include "Core/SGPlatform.h"
#include "Application/FWindow.h"
#include "FDemoBackendFactory.h"
#include "FStonerDemoApplication.h"
#include "FProductionWindowCaptureWriter.h"
#include "ProductionNativeImageAcceptance.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace
{

using namespace Stoner;
using namespace Stoner::Demo;

void Record(
    FMetalProductionContentIntegrationTestResult& Result,
    bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

const char* Environment(const char* Name)
{
    const char* Value = std::getenv(Name);
    return Value && *Value != '\0' ? Value : nullptr;
}

bool Required()
{
    const char* Value = Environment("STONER_REQUIRE_METAL_PRODUCTION");
    return Value && std::string_view(Value) == "1";
}

bool VisibleRequested()
{
    const char* Value = Environment("STONER_PRODUCTION_VISIBLE");
    return Value && std::string_view(Value) == "1";
}

#if SG_PLATFORM_MAC
bool ImageAcceptanceRequired()
{
    const char* Value = Environment(
        "STONER_REQUIRE_PRODUCTION_IMAGE_ACCEPTANCE");
    return Value && std::string_view(Value) == "1";
}

bool IsTwentyFrameImageAcceptance(Core::uint32 Cycles)
{
    return Cycles == 20 && VisibleRequested() && ImageAcceptanceRequired();
}
#endif

bool RunVisibleSurfaceSmoke()
{
#if SG_PLATFORM_MAC
    Application::FWindow Window;
    Application::FWindowDesc Desc;
    Desc.Title = "Stoner Production Metal Surface Smoke";
    Desc.ClientWidth = 64;
    Desc.ClientHeight = 64;
    if (Window.CreateRealWindow(Desc) !=
        Application::EApplicationResult::Success)
        return false;
    auto Created = FDemoBackendFactory().Create(EDemoGraphicsBackend::Metal);
    if (!Created.Succeeded())
    {
        (void)Window.Destroy();
        return false;
    }
    const Core::uint32 Width = Window.GetDrawableWidth();
    const Core::uint32 Height = Window.GetDrawableHeight();
    Core::TArray<Core::uint8> Pixels(64u * 64u * 4u, 0);
    for (Core::usize Offset = 0; Offset < Pixels.size(); Offset += 4u)
    {
        Pixels[Offset] = 31;
        Pixels[Offset + 1u] = 127;
        Pixels[Offset + 2u] = 223;
        Pixels[Offset + 3u] = 255;
    }
    FDemoProductionPresentationResult Presented;
    const bool bPassed = Width > 0 && Height > 0 &&
        Created.Runtime->Initialize(
            EDemoRunMode::BoundedNative, Window.GetPlatformWindow(),
            2, false) == RHI::ERHIResult::Success &&
        Created.Runtime->PrepareProductionPresentation(Width, Height) ==
            RHI::ERHIResult::Success &&
        Created.Runtime->PresentProductionImage(
            Pixels, 64, 64, 64 * 4, Presented) ==
            RHI::ERHIResult::Success &&
        Presented.bPresented && Presented.Width == Width &&
        Presented.Height == Height && Presented.RowPitchBytes == Width * 4u &&
        !Presented.Rgba8.empty() && Presented.Rgba8[0] == 31 &&
        Presented.Rgba8[1] == 127 && Presented.Rgba8[2] == 223;
    (void)Created.Runtime->Shutdown();
    (void)Window.Destroy();
    return bPassed;
#else
    return false;
#endif
}

#if SG_PLATFORM_MAC
bool ReadLifecycleSettings(Core::uint32& OutCycles, Core::uint32& OutWarmup)
{
    OutCycles = 20;
    OutWarmup = 2;
    const auto Parse = [](const char* Name, Core::uint32& OutValue)
    {
        const char* Value = Environment(Name);
        if (!Value) return true;
        const std::string_view Text(Value);
        const auto Result = std::from_chars(
            Text.data(), Text.data() + Text.size(), OutValue);
        return Result.ec == std::errc{} && Result.ptr == Text.data() + Text.size();
    };
    if (!Parse("STONER_PRODUCTION_LIFECYCLE_CYCLES", OutCycles) ||
        !Parse("STONER_PRODUCTION_WARMUP_CYCLES", OutWarmup))
        return false;
    return (OutCycles == 20 && OutWarmup == 2) ||
        (OutCycles == 1000 && OutWarmup == 20);
}

bool HasReadback(
    const FDemoProductionExecutionInspection& Inspection,
    const char* Name)
{
    const auto Found = std::find_if(
        Inspection.Readbacks.begin(), Inspection.Readbacks.end(),
        [Name](const auto& Evidence) { return Evidence.Name == Name; });
    return Found != Inspection.Readbacks.end() && Found->ByteCount > 0 &&
        Found->bNonBlank && !Found->Digest.IsEmpty();
}

Core::usize CaptureCount(
    const FDemoProductionExecutionInspection& Inspection,
    const char* Name)
{
    if (std::string_view(Name) == "FinalOutput")
        return Inspection.FinalOutputCaptureCount;
    if (std::string_view(Name) == "ForwardColor")
        return Inspection.ForwardColorCaptureCount;
    return 0;
}

Core::usize PresentedCaptureCount(
    const FDemoProductionExecutionInspection& Inspection,
    const char* Name)
{
    return std::string_view(Name) == "FinalOutput"
        ? Inspection.PresentedFinalOutputCaptureCount : 0;
}

Core::usize RetainedPresentedCaptureCount(
    const FDemoProductionExecutionInspection& Inspection,
    const char* Name)
{
    return static_cast<Core::usize>(std::count_if(
        Inspection.Captures.begin(), Inspection.Captures.end(),
        [Name](const auto& Capture)
        {
            return Capture.Name == Name && Capture.bPresented &&
                Capture.bWindowOnlyCapture && !Capture.Bytes.empty();
        }));
}

bool HasBalancedLifecycleOwnership(
    const FDemoProductionExecutionInspection& Inspection)
{
    return !Inspection.LifecycleSamples.empty() && std::all_of(
        Inspection.LifecycleSamples.begin(), Inspection.LifecycleSamples.end(),
        [](const auto& Sample)
        {
            return Sample.Counters.IsAtBaseline() &&
                Sample.Counters.bStaleHandleRejected;
        });
}

bool RunPath(
    EDemoRenderPath Path,
    const char* Publication,
    const char* Lease,
    const char* Generation,
    const char* Profile,
    Core::uint32 Cycles,
    Core::uint32 WarmupCycles,
    FDemoProductionExecutionInspection& OutInspection,
    Core::FString& OutDiagnostics)
{
    FDemoConfiguration Config;
    const bool bVisible = Environment("STONER_PRODUCTION_VISIBLE") &&
        std::string_view(Environment("STONER_PRODUCTION_VISIBLE")) == "1";
    Config.RunMode = bVisible
        ? EDemoRunMode::BoundedNative
        : EDemoRunMode::NativeHeadless;
    Config.GraphicsBackend = EDemoGraphicsBackend::Metal;
    Config.Workload = EDemoWorkload::ProductionContent;
    Config.RenderPath = Path;
    // Keep the non-visible lifecycle gate focused on ownership and native
    // execution. Hardware image acceptance retains the calibrated extent.
    Config.ClientWidth = bVisible ? 256 : 64;
    Config.ClientHeight = bVisible ? 256 : 64;
    Config.FrameBudget = 4096;
    Config.WarmupFrames = 512;
    Config.MemorySampleInterval = 128;
    Config.CookedPublicationRoot = Publication;
    Config.LeaseCoordinationRoot = Lease;
    Config.TargetProfilePath = Profile;
    Config.ProductionRoot = Environment("STONER_PRODUCTION_ROOT")
        ? Environment("STONER_PRODUCTION_ROOT")
        : "StaticModel:Lantern.glb#idx.scene.0";
    Config.StrictGeneration = Generation;
    Config.WorkloadRevision = Environment("STONER_PRODUCTION_WORKLOAD_REVISION")
        ? Environment("STONER_PRODUCTION_WORKLOAD_REVISION")
        : "production-content-lantern-v2";
    Config.BaselineRoot = "Content/ProductionAcceptance/Baselines";
    Config.DeviceClassRegistryPath =
        "Config/Validation/ProductionContent/DeviceClasses.json";
    Config.ProductionCaptureRoot =
        Environment("STONER_PRODUCTION_CAPTURE_ROOT")
        ? Environment("STONER_PRODUCTION_CAPTURE_ROOT") : "";
    Config.ProductionLifecycleCycles = Cycles;
    Config.ProductionWarmupCycles = WarmupCycles;
    Config.bVisibleCapture = bVisible;

    FStonerDemoApplication Application(std::move(Config));
    const bool bPassed = Application.Run() == EDemoExitCode::Success;
    OutInspection = Application.GetProductionExecutionInspection();
    OutDiagnostics = Application.GetDiagnostics().BuildStableText();
    return bPassed || (IsTwentyFrameImageAcceptance(Cycles) &&
        OutInspection.CompletedCycles == Cycles &&
        OutInspection.Runtime.ProvesNativeExecution() &&
        OutInspection.bSubmissionCompleted &&
        OutInspection.bSynchronizationCompleted &&
        HasBalancedLifecycleOwnership(OutInspection));
}
#endif

} // namespace

FMetalProductionContentIntegrationTestResult
RunMetalProductionContentIntegrationTests()
{
    FMetalProductionContentIntegrationTestResult Result;
    if (VisibleRequested())
        Record(Result, RunVisibleSurfaceSmoke(),
            "Metal production application surface presents and reads back one exact RGBA frame");
#if SG_PLATFORM_MAC
    const char* Publication = Environment(
        "STONER_PRODUCTION_METAL_PUBLICATION_ROOT");
    const char* Lease = Environment("STONER_PRODUCTION_METAL_LEASE_ROOT");
    const char* Generation = Environment(
        "STONER_PRODUCTION_METAL_GENERATION");
    const char* Profile = Environment(
        "STONER_PRODUCTION_METAL_TARGET_PROFILE");
    if (!Publication || !Lease || !Generation || !Profile)
    {
        Record(Result, !Required(), Required()
            ? "required Metal production generation is explicitly configured"
            : "Metal production integration is controlled unavailable");
        return Result;
    }
    Core::uint32 ExpectedCycles = 0;
    Core::uint32 ExpectedWarmup = 0;
    if (!ReadLifecycleSettings(ExpectedCycles, ExpectedWarmup))
    {
        Record(Result, false,
            "Metal production lifecycle settings use 20/2 or 1000/20");
        return Result;
    }

    FDemoProductionExecutionInspection Deferred;
    Core::FString Diagnostics;
    const bool bDeferred = RunPath(
        EDemoRenderPath::DeferredFull, Publication, Lease, Generation,
        Profile, ExpectedCycles, ExpectedWarmup, Deferred, Diagnostics);
    const bool bTwentyFrameImageAcceptance =
        IsTwentyFrameImageAcceptance(ExpectedCycles);
    const char* CaptureRoot = Environment("STONER_PRODUCTION_CAPTURE_ROOT");
    const bool bCaptureEvidenceWritten = !CaptureRoot ||
        ValidateProductionWindowCaptureSet("metal", CaptureRoot, 20);
    if (Deferred.LifecycleSamples.size() >= 2)
    {
        const auto& Warmup = Deferred.LifecycleSamples[ExpectedWarmup - 1];
        const auto& Terminal = Deferred.LifecycleSamples.back();
        const Core::uint64 Peak = std::max_element(
            Deferred.LifecycleSamples.begin(), Deferred.LifecycleSamples.end(),
            [](const auto& Left, const auto& Right)
            {
                return Left.ResidentBytes < Right.ResidentBytes;
            })->ResidentBytes;
        const Core::uint64 Growth =
            Terminal.ResidentBytes >= Warmup.ResidentBytes
                ? Terminal.ResidentBytes - Warmup.ResidentBytes
                : 0;
        const Core::uint64 TerminalOwners =
            Terminal.Counters.AssetOwners + Terminal.Counters.RendererOwners +
            Terminal.Counters.RHIObjects + Terminal.Counters.NativeObjects +
            Terminal.Counters.PresentationObjects;
        std::cout << (bTwentyFrameImageAcceptance
                ? "[OBSERVATION] " : "[EVIDENCE] ")
                  << "backend=metal cycles=" << ExpectedCycles
                  << " warmup-cycle=" << ExpectedWarmup
                  << " warmup-rss=" << Warmup.ResidentBytes
                  << " terminal-rss=" << Terminal.ResidentBytes
                  << " peak-rss=" << Peak << " growth=" << Growth
                  << " captures=" << Deferred.CaptureCount
                  << " readbacks=" << Deferred.Readbacks.size()
                  << " counters=" << TerminalOwners
                  << " stale="
                  << (Terminal.Counters.bStaleHandleRejected ? 1 : 0)
                  << '\n';
    }
    const bool bDeferredPassed =
        bDeferred &&
            (bTwentyFrameImageAcceptance
                ? Deferred.Runtime.ProvesNativeExecution() &&
                    Deferred.bSubmissionCompleted &&
                    Deferred.bSynchronizationCompleted &&
                    HasBalancedLifecycleOwnership(Deferred)
                : Deferred.ProvesNativeExecution()) &&
            Deferred.ExecutedBackend == EDemoGraphicsBackend::Metal &&
            Deferred.CompletedCycles == ExpectedCycles &&
            Deferred.CaptureCount == ExpectedCycles * 2u &&
            CaptureCount(Deferred, "FinalOutput") == ExpectedCycles &&
            Deferred.LifecycleSamples.size() == ExpectedCycles &&
            (bTwentyFrameImageAcceptance || Deferred.bLifecyclePassed);
    if (!bDeferredPassed)
    {
        std::cerr << "Metal production failure: app=" << (bDeferred ? 1 : 0)
                  << " native=" << (Deferred.ProvesNativeExecution() ? 1 : 0)
                  << " cycles=" << Deferred.CompletedCycles
                  << " captures=" << Deferred.CaptureCount
                  << " samples=" << Deferred.LifecycleSamples.size()
                  << " lifecycle=" << (Deferred.bLifecyclePassed ? 1 : 0)
                  << '\n';
        if (!Deferred.LifecycleSamples.empty())
        {
            const auto& Last = Deferred.LifecycleSamples.back();
            std::cerr << "terminal asset=" << Last.Counters.AssetOwners
                      << " renderer=" << Last.Counters.RendererOwners
                      << " rhi=" << Last.Counters.RHIObjects
                      << " native=" << Last.Counters.NativeObjects
                      << " presentation="
                      << Last.Counters.PresentationObjects
                      << " stale="
                      << (Last.Counters.bStaleHandleRejected ? 1 : 0)
                      << '\n';
            if (Deferred.LifecycleSamples.size() <= 20)
            {
                for (const auto& Sample : Deferred.LifecycleSamples)
                {
                    std::cerr << "lifecycle-rss cycle="
                              << Sample.CompletedCycle
                              << " bytes=" << Sample.ResidentBytes << '\n';
                }
            }
        }
        std::cerr << Diagnostics.CStr();
    }
    Record(Result, bDeferredPassed,
        "Metal production Deferred uses the requested native backend and synchronized submission");
    if (bTwentyFrameImageAcceptance)
        Record(Result, HasBalancedLifecycleOwnership(Deferred),
            "Metal 20-frame image acceptance preserves per-cycle ownership while the 1,000-cycle hardware profile owns RSS acceptance");
    Record(Result,
        ExpectedCycles != 1000 ||
            (Deferred.bCookedEnvelopeAuthenticationReused &&
             Deferred.bCookedGenerationValidationReused),
        "Metal 1,000-cycle production reuses one generation-bound authentication context and validated metadata authority across isolated manager lifetimes");
    const bool bVisible = Environment("STONER_PRODUCTION_VISIBLE") &&
        std::string_view(Environment("STONER_PRODUCTION_VISIBLE")) == "1";
    Record(Result,
        !bVisible || PresentedCaptureCount(
            Deferred, "FinalOutput") == ExpectedCycles,
        "Metal production Deferred visible mode presents and reads back every selected application-window frame");
    Record(Result,
        !bVisible || RetainedPresentedCaptureCount(
            Deferred, "FinalOutput") == (CaptureRoot
                ? 0u : std::min(ExpectedCycles, 20u)),
        "Metal production visible evidence streams or retains exactly the bounded calibration sample");
    Record(Result,
        !CaptureRoot || (bVisible && bCaptureEvidenceWritten),
        "Metal production writes the bounded application-window calibration corpus");
    Record(Result,
        Deferred.Readbacks.size() == 7 &&
            HasReadback(Deferred, "BaseColorAO") &&
            HasReadback(Deferred, "NormalRoughness") &&
            HasReadback(Deferred, "EmissiveMetallic") &&
            HasReadback(Deferred, "Depth") &&
            HasReadback(Deferred, "LightingAccumulation") &&
            HasReadback(Deferred, "FinalOutput"),
        "Metal production Deferred returns all six GPU attachment readbacks");

    if (ImageAcceptanceRequired())
    {
        const auto Image = RunProductionNativeImageAcceptance(
            Deferred, "metal", Profile,
            Environment("STONER_PRODUCTION_WORKLOAD_REVISION")
                ? Environment("STONER_PRODUCTION_WORKLOAD_REVISION")
                : "production-content-lantern-v2",
            "Content/ProductionAcceptance/Baselines",
            "Config/Validation/ProductionContent/DeviceClasses.json",
            CaptureRoot ? CaptureRoot : "");
        PrintProductionNativeImageEvidence("metal", Image);
        if (!Image.bPassed)
        {
            std::cerr << "Metal production image failure: "
                      << Image.FirstFailure.CStr() << '\n';
            PrintProductionReadbackDiagnostics(Deferred);
        }
        Record(Result, bVisible && Image.bPassed,
            "Metal production native image passes semantic probes, exact accepted baseline selection, and FLIP");
    }

    Record(Result,
        CaptureCount(Deferred, "ForwardColor") == ExpectedCycles &&
            HasReadback(Deferred, "ForwardColor"),
        "Metal production Forward smoke returns synchronized GPU color readback");
#else
    Record(Result, !Required(), Required()
        ? "required Metal production execution is available on this platform"
        : "Metal production integration is excluded off macOS");
#endif
    return Result;
}
