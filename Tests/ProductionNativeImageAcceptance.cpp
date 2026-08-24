#include "ProductionNativeImageAcceptance.h"

#include "Asset/FAssetDigest.h"
#include "Core/SGPlatform.h"
#include "FStonerDemoApplication.h"
#include "ProductionImageBaselineRegistry.h"

#include "../ThirdParty/yyjson/yyjson.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

namespace
{
using namespace Stoner;

struct FTargetProfileEvidence
{
    Core::FString CpuArchitecture;
    Core::FString Backend;
    Core::FString ShaderProfile;
    Core::FString TextureFormatFamily;
};

struct FJsonDocument
{
    yyjson_doc* Value = nullptr;
    ~FJsonDocument() { yyjson_doc_free(Value); }
};

FProductionNativeImageAcceptanceResult Fail(const char* Reason)
{
    FProductionNativeImageAcceptanceResult Result;
    Result.FirstFailure = Reason;
    return Result;
}

bool ReadTargetProfile(
    const Core::FString& Path,
    FTargetProfileEvidence& Out)
{
    std::ifstream Input(Path.ToStdString(), std::ios::binary);
    if (!Input) return false;
    std::string Bytes{std::istreambuf_iterator<char>(Input), {}};
    FJsonDocument Document;
    Document.Value = yyjson_read(
        const_cast<char*>(Bytes.data()), Bytes.size(), YYJSON_READ_NOFLAG);
    yyjson_val* Root = Document.Value
        ? yyjson_doc_get_root(Document.Value) : nullptr;
    yyjson_val* Cpu = yyjson_obj_get(Root, "cpuArchitecture");
    yyjson_val* Backend = yyjson_obj_get(Root, "graphicsBackend");
    yyjson_val* Choices = yyjson_obj_get(Root, "shaderPayloadChoices");
    yyjson_val* Textures = yyjson_obj_get(Root, "textureCapabilities");
    if (!yyjson_is_obj(Root) || !yyjson_is_str(Cpu) ||
        !yyjson_is_str(Backend) || !yyjson_is_arr(Choices) ||
        yyjson_arr_size(Choices) != 1 || !yyjson_is_arr(Textures))
        return false;
    yyjson_val* Choice = yyjson_arr_get_first(Choices);
    yyjson_val* Profile = yyjson_obj_get(Choice, "profile");
    if (!yyjson_is_obj(Choice) || !yyjson_is_str(Profile)) return false;
    Out.CpuArchitecture = yyjson_get_str(Cpu);
    Out.Backend = yyjson_get_str(Backend);
    Out.ShaderProfile = yyjson_get_str(Profile);

    bool bAstc = false;
    bool bBc = false;
    size_t Index = 0;
    size_t Maximum = 0;
    yyjson_val* Capability = nullptr;
    yyjson_arr_foreach(Textures, Index, Maximum, Capability)
    {
        if (!yyjson_is_str(Capability)) return false;
        const std::string_view Token(yyjson_get_str(Capability),
            yyjson_get_len(Capability));
        bAstc = bAstc || Token.starts_with("astc-");
        bBc = bBc || Token.starts_with("bc");
    }
    Out.TextureFormatFamily = bAstc ? "astc" :
        (bBc ? "bc" : "portable-ktx2");
    return !Out.CpuArchitecture.IsEmpty() && !Out.Backend.IsEmpty() &&
        !Out.ShaderProfile.IsEmpty();
}

bool HostArchitectureMatches(const Core::FString& Architecture)
{
#if defined(__aarch64__) || defined(_M_ARM64)
    return Architecture == Core::FString("arm64");
#elif defined(__x86_64__) || defined(_M_X64)
    return Architecture == Core::FString("x86_64");
#else
    (void)Architecture;
    return false;
#endif
}

bool ContainsCaseInsensitive(
    const Core::FString& Text,
    std::string_view Needle)
{
    std::string Value = Text.ToStdString();
    std::transform(Value.begin(), Value.end(), Value.begin(),
        [](unsigned char Character)
        {
            return static_cast<char>(std::tolower(Character));
        });
    return Value.find(Needle) != std::string::npos;
}

bool DeriveCapabilitySignature(
    const Demo::FDemoProductionExecutionInspection& Inspection,
    const Core::FString& Backend,
    const Core::FString& TargetProfilePath,
    FProductionCapabilitySignature& Out)
{
    FTargetProfileEvidence Profile;
    if (!ReadTargetProfile(TargetProfilePath, Profile) ||
        Profile.Backend != Backend || !HostArchitectureMatches(
            Profile.CpuArchitecture) || !Inspection.Runtime.ProvesNativeExecution())
        return false;

    Out.RegistryVersion = 1;
    Out.CpuArchitecture = Profile.CpuArchitecture;
    Out.ShaderProfile = Profile.ShaderProfile;
    Out.ColorFormat = "rgba8-unorm";
    Out.DepthFormat = "d32-float";
    Out.SampleCount = 1;
    Out.TextureFormatFamily = Profile.TextureFormatFamily;

    if (Backend == Core::FString("metal"))
    {
#if SG_PLATFORM_MAC
        Out.BackendImplementation = "native-metal";
        if (Profile.CpuArchitecture == Core::FString("arm64") &&
            ContainsCaseInsensitive(Inspection.Runtime.AdapterName, "apple"))
            Out.AdapterFamily = "apple8";
        else if (Profile.CpuArchitecture == Core::FString("x86_64"))
            Out.AdapterFamily = "intel-mac";
        else
            return false;
#else
        return false;
#endif
    }
    else if (Backend == Core::FString("vulkan"))
    {
#if SG_PLATFORM_MAC
        if (Profile.CpuArchitecture != Core::FString("arm64") ||
            !ContainsCaseInsensitive(Inspection.Runtime.AdapterName, "apple"))
            return false;
        Out.BackendImplementation = "moltenvk";
        Out.AdapterFamily = "apple8";
#elif SG_PLATFORM_LINUX
        if (!Inspection.Runtime.bSoftwareDevice ||
            (!ContainsCaseInsensitive(Inspection.Runtime.AdapterName, "lavapipe") &&
             !ContainsCaseInsensitive(Inspection.Runtime.AdapterName, "llvmpipe")))
            return false;
        Out.BackendImplementation = "lavapipe";
        Out.AdapterFamily = "software-vulkan";
#elif SG_PLATFORM_WINDOWS
        if (Inspection.Runtime.bSoftwareDevice) return false;
        Out.BackendImplementation = "native-vulkan";
        Out.AdapterFamily = "discrete-vulkan";
#else
        return false;
#endif
    }
    else
    {
        return false;
    }
    return Out.IsValid();
}

const Demo::FDemoProductionReadbackEvidence* FindReadback(
    const Demo::FDemoProductionExecutionInspection& Inspection,
    const char* Name)
{
    const auto Found = std::find_if(
        Inspection.Readbacks.begin(), Inspection.Readbacks.end(),
        [Name](const auto& Candidate)
        {
            return Candidate.Name == Core::FString(Name);
        });
    return Found == Inspection.Readbacks.end() ? nullptr : &*Found;
}

bool ToReadbackView(
    const Demo::FDemoProductionReadbackEvidence& Evidence,
    EProductionColorTransfer Transfer,
    FProductionReadbackView& Out)
{
    EProductionReadbackPixelFormat Format;
    switch (Evidence.Format)
    {
    case RHI::ERHIFormat::R8G8B8A8_UNorm:
    case RHI::ERHIFormat::R8G8B8A8_sRGB:
        Format = EProductionReadbackPixelFormat::RGBA8UNorm;
        break;
    case RHI::ERHIFormat::B8G8R8A8_UNorm:
        Format = EProductionReadbackPixelFormat::BGRA8UNorm;
        break;
    case RHI::ERHIFormat::R16G16B16A16_Float:
        Format = EProductionReadbackPixelFormat::RGBA16Float;
        break;
    case RHI::ERHIFormat::R32G32B32A32_Float:
        Format = EProductionReadbackPixelFormat::RGBA32Float;
        break;
    case RHI::ERHIFormat::R32_Float:
    case RHI::ERHIFormat::D32_Float:
        Format = EProductionReadbackPixelFormat::R32Float;
        break;
    default:
        return false;
    }
    Out = {Evidence.Bytes, Evidence.Width, Evidence.Height,
        Evidence.RowPitchBytes, Format, EProductionImageOrigin::TopLeft,
        Transfer};
    return Evidence.bNonBlank && !Evidence.Bytes.empty();
}

bool LoadPpm(
    const std::filesystem::path& Path,
    EProductionColorTransfer Transfer,
    FProductionCanonicalImage& Out,
    Core::FString& OutFailure)
{
    std::ifstream Input(Path, std::ios::binary);
    std::string Magic;
    Core::uint32 Width = 0;
    Core::uint32 Height = 0;
    Core::uint32 Maximum = 0;
    if (!Input || !(Input >> Magic >> Width >> Height >> Maximum) ||
        Magic != "P6" || Width == 0 || Height == 0 || Maximum != 255 ||
        Input.get() != '\n')
    {
        OutFailure = "reference-image-contract";
        return false;
    }
    const Core::uint64 PixelCount =
        static_cast<Core::uint64>(Width) * Height;
    if (PixelCount > 16384ull * 16384ull)
    {
        OutFailure = "reference-image-bounds";
        return false;
    }
    Core::TArray<Core::uint8> Rgb(static_cast<Core::usize>(PixelCount * 3u));
    Input.read(reinterpret_cast<char*>(Rgb.data()),
        static_cast<std::streamsize>(Rgb.size()));
    if (!Input || Input.peek() != std::char_traits<char>::eof())
    {
        OutFailure = "reference-image-size";
        return false;
    }
    Core::TArray<Core::uint8> Rgba(
        static_cast<Core::usize>(PixelCount * 4u), 255u);
    for (Core::usize Pixel = 0; Pixel < PixelCount; ++Pixel)
        std::copy_n(Rgb.data() + Pixel * 3u, 3u,
            Rgba.data() + Pixel * 4u);
    return NormalizeProductionReadback(
        {Rgba, Width, Height, Width * 4u,
         EProductionReadbackPixelFormat::RGBA8UNorm,
         EProductionImageOrigin::TopLeft, Transfer},
        Out, OutFailure);
}

Core::uint32 At(float Unit, Core::uint32 Extent)
{
    return std::min(Extent - 1u,
        static_cast<Core::uint32>(Unit * static_cast<float>(Extent)));
}

Core::FVector3 Srgb8(Core::uint8 R, Core::uint8 G, Core::uint8 B)
{
    const auto Linear = [](Core::uint8 Value)
    {
        const float Unit = static_cast<float>(Value) / 255.0f;
        return Unit <= 0.04045f
            ? Unit / 12.92f
            : std::pow((Unit + 0.055f) / 1.055f, 2.4f);
    };
    return {Linear(R), Linear(G), Linear(B)};
}

Core::TArray<FProductionRegionProbe> LanternRegions(
    Core::uint32 Width,
    Core::uint32 Height)
{
    const auto Probe = [Width, Height](const char* Name, float X, float Y,
        Core::FVector3 Expected, float Tolerance)
    {
        return FProductionRegionProbe{
            Name, At(X, Width), At(Y, Height), Expected, Tolerance};
    };
    return {
        Probe("background", 0.10f, 0.10f, {0.0f, 0.0f, 0.0f}, 0.01f),
        Probe("orientation", 0.345f, 0.390f, Srgb8(237, 255, 140), 0.30f),
        Probe("primitive-material", 0.598f, 0.390f,
            Srgb8(165, 114, 67), 0.25f),
        Probe("base-color", 0.598f, 0.586f,
            Srgb8(217, 144, 79), 0.30f),
        Probe("normal-response", 0.598f, 0.250f,
            Srgb8(62, 40, 24), 0.12f),
        Probe("metallic-roughness", 0.605f, 0.760f,
            Srgb8(138, 104, 107), 0.12f),
        Probe("emissive", 0.352f, 0.410f,
            Srgb8(222, 158, 72), 0.30f),
    };
}

Core::TArray<FProductionRegionProbe> SponzaRegions(
    Core::uint32 Width,
    Core::uint32 Height)
{
    const auto Probe = [Width, Height](const char* Name, float X, float Y,
        Core::FVector3 Expected, float Tolerance)
    {
        return FProductionRegionProbe{
            Name, At(X, Width), At(Y, Height), Expected, Tolerance};
    };
    return {
        Probe("background", 0.95f, 0.05f, {0.0f, 0.0f, 0.0f}, 0.01f),
        Probe("orientation", 0.10f, 0.10f,
            Srgb8(57, 45, 32), 0.12f),
        Probe("primitive-material", 0.90f, 0.55f,
            Srgb8(86, 80, 69), 0.15f),
        Probe("base-color", 0.18f, 0.68f,
            Srgb8(5, 23, 79), 0.12f),
        Probe("normal-response", 0.238f, 0.529f,
            Srgb8(175, 147, 102), 0.20f),
        Probe("metallic-roughness", 0.78f, 0.53f,
            Srgb8(50, 43, 34), 0.12f),
        Probe("emissive", 0.75f, 0.05f, {0.0f, 0.0f, 0.0f}, 0.01f),
    };
}

std::filesystem::path CapturePath(
    const Core::FString& CaptureRoot,
    const Core::FString& Backend)
{
    return std::filesystem::path(CaptureRoot.ToStdString()) /
        Backend.ToStdString() / "capture-19.ppm";
}

bool ReferencePath(
    const Core::FString& Root,
    const Core::FString& Relative,
    std::filesystem::path& Out)
{
    std::error_code Error;
    const auto CanonicalRoot = std::filesystem::weakly_canonical(
        Root.ToStdString(), Error);
    if (Error) return false;
    const auto Candidate = std::filesystem::weakly_canonical(
        CanonicalRoot / Relative.ToStdString(), Error);
    if (Error) return false;
    const auto RelativeToRoot = std::filesystem::relative(
        Candidate, CanonicalRoot, Error);
    if (Error || RelativeToRoot.empty() ||
        RelativeToRoot.begin() == RelativeToRoot.end() ||
        *RelativeToRoot.begin() == "..")
        return false;
    Out = Candidate;
    return true;
}

bool VerifyDigest(
    const std::filesystem::path& Path,
    const Core::FString& Expected)
{
    std::ifstream Input(Path, std::ios::binary);
    if (!Input) return false;
    Core::TArray<Core::uint8> Bytes{
        std::istreambuf_iterator<char>(Input), {}};
    return Input.good() || Input.eof()
        ? Asset::FAssetDigest::FromBytes(Bytes).ToLowerHex() == Expected
        : false;
}

} // namespace

bool BuildProductionWorkloadRegions(
    const Core::FString& WorkloadRevision,
    Core::uint32 Width,
    Core::uint32 Height,
    Core::TArray<FProductionRegionProbe>& OutRegions)
{
    OutRegions.clear();
    if (WorkloadRevision ==
            Core::FString("production-content-lantern-v2") ||
        WorkloadRevision == Core::FString("production-content-v1"))
        OutRegions = LanternRegions(Width, Height);
    else if (WorkloadRevision ==
        Core::FString("production-content-sponza-v2"))
        OutRegions = SponzaRegions(Width, Height);
    return OutRegions.size() == 7;
}

bool IsProductionWorkloadNormalProbeValid(
    const Stoner::Core::FString& WorkloadRevision,
    const Stoner::Core::FVector3& WorldNormal) noexcept
{
    const float Length = std::sqrt(
        WorldNormal.X * WorldNormal.X + WorldNormal.Y * WorldNormal.Y +
        WorldNormal.Z * WorldNormal.Z);
    if (!std::isfinite(Length) || Length < 0.8f || Length > 1.2f)
        return false;
    if (WorkloadRevision ==
        Stoner::Core::FString("production-content-sponza-v2"))
        return WorldNormal.Y >= 0.8f;
    if (WorkloadRevision ==
        Stoner::Core::FString("production-content-lantern-v2"))
        return WorldNormal.X <= -0.8f;
    return WorkloadRevision ==
        Stoner::Core::FString("production-content-v1");
}

FProductionNativeImageAcceptanceResult RunProductionNativeImageAcceptance(
    const Stoner::Demo::FDemoProductionExecutionInspection& Inspection,
    const Stoner::Core::FString& Backend,
    const Stoner::Core::FString& TargetProfilePath,
    const Stoner::Core::FString& WorkloadRevision,
    const Stoner::Core::FString& BaselineRoot,
    const Stoner::Core::FString& DeviceClassRegistryPath,
    const Stoner::Core::FString& CaptureRoot)
{
    using namespace Stoner;
    if (CaptureRoot.IsEmpty()) return Fail("window-capture-missing");

    FProductionCapabilitySignature Signature;
    if (!DeriveCapabilitySignature(
            Inspection, Backend, TargetProfilePath, Signature))
        return Fail("capability-signature");

    const auto* ColorEvidence = FindReadback(Inspection, "FinalOutput");
    const auto* BaseColorEvidence = FindReadback(Inspection, "BaseColorAO");
    const auto* NormalEvidence = FindReadback(Inspection, "NormalRoughness");
    const auto* DepthEvidence = FindReadback(Inspection, "Depth");
    const auto* LightingEvidence = FindReadback(
        Inspection, "LightingAccumulation");
    if (!ColorEvidence || !BaseColorEvidence || !NormalEvidence ||
        !DepthEvidence || !LightingEvidence)
        return Fail("semantic-readback-missing");

    FProductionReadbackView ColorView;
    FProductionReadbackView BaseColorView;
    FProductionReadbackView NormalView;
    FProductionReadbackView DepthView;
    FProductionReadbackView LightingView;
    if (!ToReadbackView(*ColorEvidence, EProductionColorTransfer::SRGB,
            ColorView) ||
        !ToReadbackView(*BaseColorEvidence, EProductionColorTransfer::Linear,
            BaseColorView) ||
        !ToReadbackView(*NormalEvidence, EProductionColorTransfer::Linear,
            NormalView) ||
        !ToReadbackView(*DepthEvidence, EProductionColorTransfer::Linear,
            DepthView) ||
        !ToReadbackView(*LightingEvidence, EProductionColorTransfer::Linear,
            LightingView))
        return Fail("semantic-readback-layout");
    const bool bSponzaV2 = WorkloadRevision ==
        Core::FString("production-content-sponza-v2");
    const Core::uint32 ProbeX = At(
        0.598f, ColorView.Width);
    const Core::uint32 ProbeY = At(
        0.390f, ColorView.Height);
    Core::FVector3 BaseColorSample;
    Core::FVector3 NormalSample;
    Core::FVector3 DepthSample;
    Core::FVector3 LightingSample;
    Core::FString Failure;
    if (!SampleProductionReadbackPixel(
            BaseColorView, ProbeX, ProbeY, BaseColorSample, Failure) ||
        std::max({BaseColorSample.X, BaseColorSample.Y, BaseColorSample.Z}) <=
            0.02f)
        return Fail("base-color-attachment-region");
    if (!SampleProductionReadbackPixel(
            NormalView, ProbeX, ProbeY, NormalSample, Failure))
        return Fail("normal-attachment-region");
    if (!IsProductionWorkloadNormalProbeValid(
            WorkloadRevision, NormalSample))
        return Fail("normal-attachment-region");
    if (!SampleProductionReadbackPixel(
            DepthView, ProbeX, ProbeY, DepthSample, Failure) ||
        DepthSample.X <= 0.0f || DepthSample.X >= 1.0f)
        return Fail("depth-attachment-region");
    if (!SampleProductionReadbackPixel(
            LightingView, ProbeX, ProbeY, LightingSample, Failure) ||
        std::max({LightingSample.X, LightingSample.Y, LightingSample.Z}) <=
            0.10f)
        return Fail("lighting-attachment-region");
    FProductionCanonicalImage Color;
    FProductionCanonicalImage Normal;
    FProductionCanonicalImage Depth;
    if (!NormalizeProductionReadback(ColorView, Color, Failure) ||
        !NormalizeProductionSignedNormalReadback(NormalView, Normal, Failure) ||
        !NormalizeProductionReadback(DepthView, Depth, Failure))
        return Fail(Failure.CStr());

    FProductionNativeImageAcceptanceResult Result;
    FProductionSemanticProbeRequest Semantic;
    Semantic.Color = &Color;
    Semantic.Normal = &Normal;
    Semantic.Depth = &Depth;
    Semantic.ExpectedFrameToken = Inspection.CompletedCycles;
    Semantic.ObservedFrameToken = Inspection.CompletedCycles;
    Semantic.MinimumCoverageFraction = bSponzaV2 ? 0.75f : 0.01f;
    Semantic.MaximumCoverageFraction = bSponzaV2 ? 0.82f : 0.35f;
    if (!BuildProductionWorkloadRegions(
            WorkloadRevision, Color.Width, Color.Height, Semantic.Regions))
        return Fail("workload-semantic-regions");
    Semantic.RequiredRegionNames = {"background"};
    Result.Semantic = RunProductionSemanticProbes(Semantic);
    if (!Result.Semantic.bPassed)
    {
        Result.FirstFailure = Result.Semantic.FirstFailure;
        return Result;
    }

    FProductionImageBaselineRegistry Registry;
    if (!Registry.LoadDeviceClasses(DeviceClassRegistryPath, Failure))
    {
        Result.FirstFailure = Failure;
        return Result;
    }
    if (!Registry.LoadBaselines(BaselineRoot, Failure))
    {
        Result.FirstFailure = Failure;
        return Result;
    }
    FProductionImageBaseline Baseline;
    if (!Registry.SelectAccepted(
            Signature, WorkloadRevision, Backend, Baseline, Failure))
    {
        Result.FirstFailure = Failure;
        return Result;
    }
    Result.DeviceClass = Baseline.DeviceClass;
    Result.BaselineId = Baseline.BaselineId;

    FProductionNativeImageEvidence Native{
        Backend, Backend, "native", WorkloadRevision,
        Baseline.WorkloadRevision,
        Inspection.RequestedBackend == Inspection.ExecutedBackend &&
            Inspection.Runtime.ProvesNativeExecution(),
        Inspection.bSubmissionCompleted, true, true, true};
    if (!ValidateProductionNativeImageEvidence(Native, Failure))
    {
        Result.FirstFailure = Failure;
        return Result;
    }

    const std::filesystem::path CandidatePath =
        CapturePath(CaptureRoot, Backend);
    std::filesystem::path Reference;
    if (!ReferencePath(BaselineRoot, Baseline.ReferencePath, Reference) ||
        !VerifyDigest(Reference, Baseline.ReferenceSha256))
    {
        Result.FirstFailure = "reference-image-digest";
        return Result;
    }
    FProductionCanonicalImage ReferenceImage;
    FProductionCanonicalImage CandidateImage;
    if (!LoadPpm(Reference, Baseline.ColorTransfer,
            ReferenceImage, Failure) ||
        !LoadPpm(CandidatePath, Baseline.ColorTransfer,
            CandidateImage, Failure) ||
        ReferenceImage.Width != Baseline.Width ||
        ReferenceImage.Height != Baseline.Height)
    {
        Result.FirstFailure = Failure.IsEmpty()
            ? Core::FString("baseline-dimensions") : Failure;
        return Result;
    }
    Result.Flip = CompareProductionImagesWithFlip(
        ReferenceImage, CandidateImage, Baseline.FlipPolicy);
    Result.bPassed = Result.Flip.bMeasured && Result.Flip.bPassed;
    if (!Result.bPassed) Result.FirstFailure = Result.Flip.FailureReason;
    return Result;
}

void PrintProductionNativeImageEvidence(
    const Stoner::Core::FString& Backend,
    const FProductionNativeImageAcceptanceResult& Result)
{
    std::cout << std::fixed << std::setprecision(8)
              << "[IMAGE] backend=" << Backend.CStr()
              << " device-class=" << Result.DeviceClass.CStr()
              << " baseline=" << Result.BaselineId.CStr()
              << " semantic-probes=" << Result.Semantic.PassedProbeCount
              << " mean=" << Result.Flip.Mean
              << " p95=" << Result.Flip.P95
              << " maximum=" << Result.Flip.Maximum
              << " bad-fraction=" << Result.Flip.BadPixelFraction
              << " result=" << (Result.bPassed ? "passed" : "failed")
              << '\n';
}

void PrintProductionReadbackDiagnostics(
    const Stoner::Demo::FDemoProductionExecutionInspection& Inspection)
{
    for (const auto& Evidence : Inspection.Readbacks)
    {
        std::cerr << "[READBACK] name=" << Evidence.Name.CStr()
                  << " format=" << static_cast<int>(Evidence.Format)
                  << " width=" << Evidence.Width
                  << " height=" << Evidence.Height
                  << " row-pitch=" << Evidence.RowPitchBytes
                  << " bytes=" << Evidence.ByteCount
                  << " digest=" << Evidence.Digest.CStr()
                  << " retained=" << Evidence.Bytes.size() << '\n';
        FProductionReadbackView View;
        if (ToReadbackView(Evidence, EProductionColorTransfer::Linear, View))
        {
            Core::FVector3 Sample;
            Core::FString Failure;
            if (SampleProductionReadbackPixel(
                    View, At(0.598f, View.Width), At(0.390f, View.Height),
                    Sample, Failure))
                std::cerr << "[READBACK-SAMPLE] name="
                          << Evidence.Name.CStr() << " rgb="
                          << Sample.X << ',' << Sample.Y << ',' << Sample.Z
                          << '\n';
        }
    }
}
