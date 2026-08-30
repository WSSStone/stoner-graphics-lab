#include "ProductionNativeImageAcceptance.h"

#include "Asset/FAssetDigest.h"
#include "Core/SGPlatform.h"
#include "FStonerDemoApplication.h"
#include "ProductionImageBaselineRegistry.h"
#include "ProductionImageReference.h"

#include "../ThirdParty/yyjson/yyjson.h"

#if STONER_TEST_VULKAN_RUNTIME_AVAILABLE && SG_PLATFORM_WINDOWS
#include <vulkan/vulkan.h>
#endif

#include <algorithm>
#include <array>
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

struct FVulkanDriverEvidence
{
    bool bAvailable = false;
    Core::uint32 DriverId = 0;
    Core::uint32 DriverVersion = 0;
    std::string PipelineCacheUuid;
    Core::FString FloatControlsSha256;
    Core::FString Digest;
};

FVulkanDriverEvidence QueryVulkanDriverEvidence(
    const Core::FString& ExpectedAdapter)
{
    FVulkanDriverEvidence Result;
#if STONER_TEST_VULKAN_RUNTIME_AVAILABLE && SG_PLATFORM_WINDOWS
    VkApplicationInfo Application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    Application.pApplicationName = "StonerProductionValidation";
    Application.apiVersion = VK_API_VERSION_1_2;
    VkInstanceCreateInfo CreateInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    CreateInfo.pApplicationInfo = &Application;
    VkInstance Instance = VK_NULL_HANDLE;
    if (vkCreateInstance(&CreateInfo, nullptr, &Instance) != VK_SUCCESS)
        return Result;
    Core::uint32 Count = 0;
    if (vkEnumeratePhysicalDevices(Instance, &Count, nullptr) != VK_SUCCESS ||
        Count == 0)
    {
        vkDestroyInstance(Instance, nullptr);
        return Result;
    }
    Core::TArray<VkPhysicalDevice> Devices(Count);
    if (vkEnumeratePhysicalDevices(
            Instance, &Count, Devices.data()) != VK_SUCCESS)
    {
        vkDestroyInstance(Instance, nullptr);
        return Result;
    }
    for (const VkPhysicalDevice Device : Devices)
    {
        VkPhysicalDeviceFloatControlsProperties FloatControls{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FLOAT_CONTROLS_PROPERTIES};
        VkPhysicalDeviceDriverProperties Driver{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
        Driver.pNext = &FloatControls;
        VkPhysicalDeviceProperties2 Properties{
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        Properties.pNext = &Driver;
        vkGetPhysicalDeviceProperties2(Device, &Properties);
        if (ExpectedAdapter != Core::FString(
                Properties.properties.deviceName))
            continue;

        std::ostringstream FloatText;
        FloatText
            << static_cast<Core::uint32>(
                FloatControls.denormBehaviorIndependence) << '|'
            << static_cast<Core::uint32>(
                FloatControls.roundingModeIndependence);
        const std::array<VkBool32, 15> Capabilities = {
            FloatControls.shaderSignedZeroInfNanPreserveFloat16,
            FloatControls.shaderSignedZeroInfNanPreserveFloat32,
            FloatControls.shaderSignedZeroInfNanPreserveFloat64,
            FloatControls.shaderDenormPreserveFloat16,
            FloatControls.shaderDenormPreserveFloat32,
            FloatControls.shaderDenormPreserveFloat64,
            FloatControls.shaderDenormFlushToZeroFloat16,
            FloatControls.shaderDenormFlushToZeroFloat32,
            FloatControls.shaderDenormFlushToZeroFloat64,
            FloatControls.shaderRoundingModeRTEFloat16,
            FloatControls.shaderRoundingModeRTEFloat32,
            FloatControls.shaderRoundingModeRTEFloat64,
            FloatControls.shaderRoundingModeRTZFloat16,
            FloatControls.shaderRoundingModeRTZFloat32,
            FloatControls.shaderRoundingModeRTZFloat64};
        for (const VkBool32 Capability : Capabilities)
            FloatText << '|' << static_cast<Core::uint32>(Capability);
        const std::string FloatBytes = FloatText.str();
        Result.FloatControlsSha256 = Asset::FAssetDigest::FromBytes(
            std::span<const Core::uint8>(
                reinterpret_cast<const Core::uint8*>(FloatBytes.data()),
                FloatBytes.size())).ToLowerHex();

        std::ostringstream Uuid;
        Uuid << std::hex << std::setfill('0');
        for (const Core::uint8 Byte :
             Properties.properties.pipelineCacheUUID)
            Uuid << std::setw(2) << static_cast<Core::uint32>(Byte);
        Result.PipelineCacheUuid = Uuid.str();
        Result.DriverId = static_cast<Core::uint32>(Driver.driverID);
        Result.DriverVersion = Properties.properties.driverVersion;
        const std::string Canonical =
            std::to_string(Result.DriverId) + "|" +
            std::to_string(Result.DriverVersion) + "|" +
            Result.PipelineCacheUuid + "|" +
            Result.FloatControlsSha256.ToStdString();
        Result.Digest = Asset::FAssetDigest::FromBytes(
            std::span<const Core::uint8>(
                reinterpret_cast<const Core::uint8*>(Canonical.data()),
                Canonical.size())).ToLowerHex();
        Result.bAvailable = Result.PipelineCacheUuid.size() ==
            VK_UUID_SIZE * 2u;
        break;
    }
    vkDestroyInstance(Instance, nullptr);
#else
    (void)ExpectedAdapter;
#endif
    return Result;
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

bool IsRequiredFrameReadbackName(const Core::FString& Name)
{
    return Name == Core::FString("FinalOutput") ||
        Name == Core::FString("BaseColorAO") ||
        Name == Core::FString("NormalRoughness") ||
        Name == Core::FString("EmissiveMetallic") ||
        Name == Core::FString("Depth") ||
        Name == Core::FString("LightingAccumulation");
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

Core::uint32 At(float Unit, Core::uint32 Extent)
{
    return std::min(Extent - 1u,
        static_cast<Core::uint32>(Unit * static_cast<float>(Extent)));
}

FProductionPixelRegion RegionAt(
    float UnitX,
    float UnitY,
    Core::uint32 Width,
    Core::uint32 Height,
    Core::uint32 Radius = 4)
{
    const Core::uint32 CenterX = At(UnitX, Width);
    const Core::uint32 CenterY = At(UnitY, Height);
    return {
        CenterX > Radius ? CenterX - Radius : 0u,
        CenterY > Radius ? CenterY - Radius : 0u,
        std::min(Width, CenterX + Radius + 1u),
        std::min(Height, CenterY + Radius + 1u)};
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
        Core::FVector3 Expected, float Tolerance,
        float MinimumValidSampleFraction = 0.50f)
    {
        return FProductionRegionProbe{
            Name, RegionAt(X, Y, Width, Height), Expected, Tolerance,
            MinimumValidSampleFraction, EProductionRegionStatistic::Median,
            0.5f};
    };
    return {
        Probe("background", 0.10f, 0.10f, {0.0f, 0.0f, 0.0f}, 0.01f, 0.90f),
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
        Core::FVector3 Expected, float Tolerance,
        float MinimumValidSampleFraction = 0.50f)
    {
        return FProductionRegionProbe{
            Name, RegionAt(X, Y, Width, Height), Expected, Tolerance,
            MinimumValidSampleFraction, EProductionRegionStatistic::Median,
            0.5f};
    };
    return {
        Probe("background", 0.95f, 0.05f, {0.0f, 0.0f, 0.0f}, 0.01f, 0.90f),
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

bool ValidateProductionAuthoritativeFrameBundle(
    const Stoner::Demo::FDemoProductionExecutionInspection& Inspection,
    Stoner::Core::FString& OutFailure)
{
    using namespace Stoner;
    OutFailure = {};
    constexpr const char* RequiredNames[] = {
        "FinalOutput", "BaseColorAO", "NormalRoughness",
        "EmissiveMetallic", "Depth", "LightingAccumulation"};
    const Core::uint64 FrameToken = Inspection.AuthoritativeFrameToken;
    if (FrameToken == 0)
    {
        OutFailure = "authoritative-frame-bundle";
        return false;
    }
    for (const char* RequiredName : RequiredNames)
    {
        const auto Count = std::count_if(
            Inspection.Readbacks.begin(), Inspection.Readbacks.end(),
            [RequiredName](const auto& Evidence)
            {
                return Evidence.Name == Core::FString(RequiredName);
            });
        const auto* Evidence = FindReadback(Inspection, RequiredName);
        if (Count != 1 || !Evidence || Evidence->FrameToken != FrameToken)
        {
            OutFailure = "authoritative-frame-bundle";
            return false;
        }
    }
    if (std::count_if(
            Inspection.Readbacks.begin(), Inspection.Readbacks.end(),
            [](const auto& Evidence)
            {
                return IsRequiredFrameReadbackName(Evidence.Name);
            }) != std::size(RequiredNames))
    {
        OutFailure = "authoritative-frame-bundle";
        return false;
    }
    const auto& Presented = Inspection.AuthoritativeCapture;
    if (Presented.FrameToken != FrameToken ||
        Presented.ExpectedFrameToken != FrameToken || !Presented.bPresented ||
        !Presented.bWindowOnlyCapture || Presented.Bytes.empty() ||
        Inspection.LastLifecyclePresentedFrameToken == 0 ||
        Inspection.LastLifecyclePresentedFrameToken >= FrameToken)
    {
        OutFailure = "authoritative-frame-bundle";
        return false;
    }
    const auto& StaleCapture = Inspection.LastLifecyclePresentedCapture;
    if (StaleCapture.FrameToken !=
            Inspection.LastLifecyclePresentedFrameToken ||
        StaleCapture.ExpectedFrameToken != StaleCapture.FrameToken ||
        !StaleCapture.bPresented || !StaleCapture.bWindowOnlyCapture ||
        StaleCapture.FrameToken == FrameToken || StaleCapture.Bytes.empty())
    {
        OutFailure = "stale-frame-bundle-evidence";
        return false;
    }
    return true;
}

Core::FString DecodedPixelDigest(
    const FProductionCanonicalImage& Image)
{
    if (!Image.IsValid()) return {};
    const auto* Bytes = reinterpret_cast<const Core::uint8*>(
        Image.LinearRgb.data());
    return Asset::FAssetDigest::FromBytes(std::span<const Core::uint8>(
        Bytes, Image.LinearRgb.size() * sizeof(float))).ToLowerHex();
}

Core::FString ReadbackDecodedPixelDigest(
    const Demo::FDemoProductionReadbackEvidence& Evidence)
{
    FProductionReadbackView View;
    const EProductionColorTransfer Transfer =
        Evidence.Name == Core::FString("FinalOutput")
        ? EProductionColorTransfer::SRGB
        : EProductionColorTransfer::Linear;
    if (!ToReadbackView(Evidence, Transfer, View)) return {};
    Core::TArray<float> Decoded;
    Decoded.reserve(
        static_cast<Core::usize>(View.Width) * View.Height * 3u);
    Core::FString Failure;
    for (Core::uint32 Y = 0; Y < View.Height; ++Y)
        for (Core::uint32 X = 0; X < View.Width; ++X)
        {
            Core::FVector3 Sample;
            if (!SampleProductionReadbackPixel(
                    View, X, Y, Sample, Failure))
                return {};
            Decoded.push_back(Sample.X);
            Decoded.push_back(Sample.Y);
            Decoded.push_back(Sample.Z);
        }
    const auto* Bytes = reinterpret_cast<const Core::uint8*>(Decoded.data());
    return Asset::FAssetDigest::FromBytes(std::span<const Core::uint8>(
        Bytes, Decoded.size() * sizeof(float))).ToLowerHex();
}

Core::FString CaptureDecodedPixelDigest(
    const Demo::FDemoProductionCapture& Capture)
{
    const FProductionReadbackView View{
        Capture.Bytes, Capture.Width, Capture.Height,
        Capture.RowPitchBytes, EProductionReadbackPixelFormat::RGBA8UNorm,
        EProductionImageOrigin::TopLeft, EProductionColorTransfer::SRGB};
    FProductionCanonicalImage Image;
    Core::FString Failure;
    return NormalizeProductionReadback(View, Image, Failure)
        ? DecodedPixelDigest(Image) : Core::FString{};
}

bool SelectProductionMatchedReference(
    const Stoner::Core::TArray<FProductionReferenceComparison>& Comparisons,
    Stoner::Core::FString& OutReferenceId,
    FProductionFlipResult& OutFlip)
{
    OutReferenceId = {};
    OutFlip = {};
    for (const auto& Comparison : Comparisons)
    {
        if (Comparison.ReferenceId.IsEmpty() ||
            !Comparison.Flip.bMeasured || !Comparison.Flip.bPassed)
            continue;
        OutReferenceId = Comparison.ReferenceId;
        OutFlip = Comparison.Flip;
        return true;
    }
    return false;
}

bool BuildProductionWorkloadRegions(
    const Core::FString& WorkloadRevision,
    Core::uint32 Width,
    Core::uint32 Height,
    Core::TArray<FProductionRegionProbe>& OutRegions)
{
    OutRegions.clear();
    if (Width != 512 || Height != 512) return false;
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
    const auto* EmissiveEvidence = FindReadback(
        Inspection, "EmissiveMetallic");
    const auto* DepthEvidence = FindReadback(Inspection, "Depth");
    const auto* LightingEvidence = FindReadback(
        Inspection, "LightingAccumulation");
    if (!ColorEvidence || !BaseColorEvidence || !NormalEvidence ||
        !EmissiveEvidence || !DepthEvidence || !LightingEvidence)
        return Fail("semantic-readback-missing");
    Core::FString FrameBundleFailure;
    if (!ValidateProductionAuthoritativeFrameBundle(
            Inspection, FrameBundleFailure))
        return Fail(FrameBundleFailure.CStr());
    const Core::uint64 FrameToken = ColorEvidence->FrameToken;
    const auto& Presented = Inspection.AuthoritativeCapture;

    FProductionReadbackView ColorView;
    FProductionReadbackView BaseColorView;
    FProductionReadbackView NormalView;
    FProductionReadbackView EmissiveView;
    FProductionReadbackView DepthView;
    FProductionReadbackView LightingView;
    if (!ToReadbackView(*ColorEvidence, EProductionColorTransfer::SRGB,
            ColorView) ||
        !ToReadbackView(*BaseColorEvidence, EProductionColorTransfer::Linear,
            BaseColorView) ||
        !ToReadbackView(*NormalEvidence, EProductionColorTransfer::Linear,
            NormalView) ||
        !ToReadbackView(*EmissiveEvidence, EProductionColorTransfer::Linear,
            EmissiveView) ||
        !ToReadbackView(*DepthEvidence, EProductionColorTransfer::Linear,
            DepthView) ||
        !ToReadbackView(*LightingEvidence, EProductionColorTransfer::Linear,
            LightingView))
        return Fail("semantic-readback-layout");
    constexpr Core::uint32 FormalExtent = 512;
    const auto HasFormalExtent = [](const FProductionReadbackView& View)
    {
        return View.Width == FormalExtent && View.Height == FormalExtent;
    };
    if (!HasFormalExtent(ColorView) || !HasFormalExtent(BaseColorView) ||
        !HasFormalExtent(NormalView) || !HasFormalExtent(DepthView) ||
        !HasFormalExtent(EmissiveView) || !HasFormalExtent(LightingView))
        return Fail("formal-image-extent");

    Core::FString Failure;
    FProductionNativeImageAcceptanceResult Result;
    const bool bSponzaV2 = WorkloadRevision ==
        Core::FString("production-content-sponza-v2");
    const Core::FVector3 ExpectedNormal = bSponzaV2
        ? Core::FVector3::UnitY() : -Core::FVector3::UnitX();
    const FProductionPixelRegion AttachmentRegion = RegionAt(
        0.598f, 0.390f, FormalExtent, FormalExtent);
    FProductionReadbackRegionSample BaseColorSample;
    FProductionReadbackRegionSample EmissiveSample;
    FProductionReadbackRegionSample DepthSample;
    FProductionReadbackRegionSample LightingSample;
    float NormalCoverage = 0.0f;
    float OppositeNormalCoverage = 0.0f;
    if (!SampleProductionReadbackRegion(
            BaseColorView, AttachmentRegion, 0.5f,
            BaseColorSample, Failure) ||
        BaseColorSample.ValidSampleFraction < 0.75f ||
        std::max({BaseColorSample.Value.X, BaseColorSample.Value.Y,
            BaseColorSample.Value.Z}) <= 0.02f)
        return Fail("base-color-attachment-region");
    if (!SampleProductionReadbackRegion(
            EmissiveView, AttachmentRegion, 0.5f,
            EmissiveSample, Failure) ||
        EmissiveSample.ValidSampleFraction < 0.75f)
        return Fail("emissive-metallic-attachment-region");
    if (!MeasureProductionReadbackDirectionalCoverage(
            NormalView, AttachmentRegion, ExpectedNormal, 0.8f,
            NormalCoverage, Failure) || NormalCoverage < 0.60f)
        return Fail("normal-attachment-region");
    if (!MeasureProductionReadbackDirectionalCoverage(
            NormalView, AttachmentRegion, -ExpectedNormal, 0.8f,
            OppositeNormalCoverage, Failure) ||
        OppositeNormalCoverage >= 0.60f)
        return Fail("opposite-normal-mutation");
    if (!SampleProductionReadbackRegion(
            DepthView, AttachmentRegion, 0.5f, DepthSample, Failure) ||
        DepthSample.ValidSampleFraction < 0.75f ||
        DepthSample.Value.X <= 0.0f || DepthSample.Value.X >= 1.0f)
        return Fail("depth-attachment-region");
    if (!SampleProductionReadbackRegion(
            LightingView, AttachmentRegion, 0.5f,
            LightingSample, Failure) ||
        LightingSample.ValidSampleFraction < 0.75f ||
        std::max({LightingSample.Value.X, LightingSample.Value.Y,
            LightingSample.Value.Z}) <= 0.10f)
        return Fail("lighting-attachment-region");
    FProductionCanonicalImage Color;
    FProductionCanonicalImage PresentedColor;
    FProductionCanonicalImage StaleColor;
    FProductionCanonicalImage Normal;
    FProductionCanonicalImage Depth;
    const FProductionReadbackView PresentedView{
        Presented.Bytes, Presented.Width, Presented.Height,
        Presented.RowPitchBytes, EProductionReadbackPixelFormat::RGBA8UNorm,
        EProductionImageOrigin::TopLeft, EProductionColorTransfer::SRGB};
    const auto& StaleCapture =
        Inspection.LastLifecyclePresentedCapture;
    const FProductionReadbackView StaleView{
        StaleCapture.Bytes, StaleCapture.Width, StaleCapture.Height,
        StaleCapture.RowPitchBytes,
        EProductionReadbackPixelFormat::RGBA8UNorm,
        EProductionImageOrigin::TopLeft, EProductionColorTransfer::SRGB};
    if (!NormalizeProductionReadback(ColorView, Color, Failure) ||
        !NormalizeProductionReadback(PresentedView, PresentedColor, Failure) ||
        !NormalizeProductionReadback(StaleView, StaleColor, Failure) ||
        !NormalizeProductionSignedNormalReadback(NormalView, Normal, Failure) ||
        !NormalizeProductionReadback(DepthView, Depth, Failure))
        return Fail(Failure.CStr());
    if (PresentedColor.Width != Color.Width ||
        PresentedColor.Height != Color.Height ||
        PresentedColor.LinearRgb != Color.LinearRgb)
        return Fail("presented-frame-pixels");
    FProductionSemanticProbeRequest StaleSemantic;
    StaleSemantic.Color = &StaleColor;
    StaleSemantic.ExpectedFrameToken = FrameToken;
    StaleSemantic.ObservedFrameToken = StaleCapture.FrameToken;
    const auto StaleResult = RunProductionSemanticProbes(StaleSemantic);
    if (StaleResult.bPassed ||
        StaleResult.FirstFailure != Core::FString("current-frame"))
        return Fail("stale-frame-bundle-evidence");

    FProductionSemanticProbeRequest Semantic;
    Semantic.Color = &Color;
    Semantic.Normal = &Normal;
    Semantic.Depth = &Depth;
    Semantic.ExpectedFrameToken = Inspection.AuthoritativeFrameToken;
    Semantic.ObservedFrameToken = ColorEvidence->FrameToken;
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
    for (const char* ProbeId : {
             "base-color-attachment", "normal-attachment-direction",
             "emissive-metallic-attachment", "depth-attachment",
             "lighting-attachment", "final-output-readback",
             "presented-window-capture"})
        Result.Semantic.PassedProbeIds.push_back(ProbeId);
    Result.Semantic.PassedProbeCount = static_cast<Core::uint32>(
        Result.Semantic.PassedProbeIds.size());
    Result.FrameToken = FrameToken;

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
        Result.DeviceClass = Baseline.DeviceClass;
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

    if (Baseline.Width != FormalExtent || Baseline.Height != FormalExtent ||
        Color.Width != FormalExtent || Color.Height != FormalExtent ||
        Baseline.References.empty())
    {
        Result.FirstFailure = "formal-image-extent";
        return Result;
    }
    for (const auto& AcceptedReference : Baseline.References)
    {
        std::filesystem::path Reference;
        if (!ReferencePath(
                BaselineRoot, AcceptedReference.ReferencePath, Reference) ||
            !VerifyDigest(Reference, AcceptedReference.ReferenceSha256))
        {
            Result.FirstFailure = "reference-image-digest";
            return Result;
        }
        FProductionCanonicalImage ReferenceImage;
        if (!LoadProductionReferenceImage(
                Reference, Baseline.ColorTransfer, ReferenceImage, Failure))
        {
            Result.FirstFailure = Failure;
            return Result;
        }
        if (ReferenceImage.Width != FormalExtent ||
            ReferenceImage.Height != FormalExtent)
        {
            Result.FirstFailure = "formal-image-extent";
            return Result;
        }
        FProductionReferenceComparison Comparison;
        Comparison.ReferenceId = AcceptedReference.ReferenceId;
        Comparison.Flip = CompareProductionImagesWithFlip(
            ReferenceImage, Color, AcceptedReference.FlipPolicy);
        Result.ReferenceComparisons.push_back(Comparison);
    }
    Result.bPassed = SelectProductionMatchedReference(
        Result.ReferenceComparisons, Result.MatchedReferenceId, Result.Flip);
    if (!Result.bPassed)
    {
        Result.Flip = Result.ReferenceComparisons.front().Flip;
        Result.FirstFailure = "reference-set-no-match";
    }
    return Result;
}

void PrintProductionNativeImageEvidence(
    const Stoner::Core::FString& Backend,
    const FProductionNativeImageAcceptanceResult& Result)
{
    std::string ProbeIds;
    for (const auto& ProbeId : Result.Semantic.PassedProbeIds)
    {
        if (!ProbeIds.empty()) ProbeIds.push_back(',');
        ProbeIds += ProbeId.ToStdString();
    }
    std::cout << std::fixed << std::setprecision(8)
              << "[IMAGE] backend=" << Backend.CStr()
              << " device-class=" << Result.DeviceClass.CStr()
              << " baseline=" << Result.BaselineId.CStr()
              << " matched-reference=" << Result.MatchedReferenceId.CStr()
              << " frame-token=" << Result.FrameToken
              << " semantic-probes=" << Result.Semantic.PassedProbeCount
              << " probe-ids=" << ProbeIds
              << " mean=" << Result.Flip.Mean
              << " p95=" << Result.Flip.P95
              << " maximum=" << Result.Flip.Maximum
              << " bad-fraction=" << Result.Flip.BadPixelFraction
              << " result=" << (Result.bPassed ? "passed" : "failed")
              << '\n';
    for (const auto& Comparison : Result.ReferenceComparisons)
    {
        std::cout << "[IMAGE-REFERENCE] reference="
                  << Comparison.ReferenceId.CStr()
                  << " mean=" << Comparison.Flip.Mean
                  << " p95=" << Comparison.Flip.P95
                  << " maximum=" << Comparison.Flip.Maximum
                  << " bad-fraction=" << Comparison.Flip.BadPixelFraction
                  << " result="
                  << (Comparison.Flip.bMeasured && Comparison.Flip.bPassed
                      ? "passed" : "failed") << '\n';
    }
}

void PrintProductionReadbackDiagnostics(
    const Stoner::Demo::FDemoProductionExecutionInspection& Inspection)
{
    const FVulkanDriverEvidence VulkanDriver =
        QueryVulkanDriverEvidence(Inspection.Runtime.AdapterName);
    const Stoner::Core::FString DeviceFingerprint =
        VulkanDriver.bAvailable
        ? VulkanDriver.Digest : Inspection.DeviceFingerprint;
    for (const auto& Evidence : Inspection.Readbacks)
    {
        std::cerr << "[READBACK] name=" << Evidence.Name.CStr()
                  << " frame-token=" << Evidence.FrameToken
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
    std::cerr << "[FRAME-FINGERPRINT] frame-token="
              << Inspection.AuthoritativeFrameToken
              << " snapshot=" << Inspection.SnapshotFingerprint.CStr()
              << " uniform=" << Inspection.UniformFingerprint.CStr()
              << " shader=" << Inspection.ShaderFingerprint.CStr()
              << " pipeline=" << Inspection.PipelineFingerprint.CStr()
              << " descriptor=" << Inspection.DescriptorFingerprint.CStr()
              << " device=" << DeviceFingerprint.CStr()
              << '\n';
    std::cerr << "[FRAME-BUNDLE] {\"attachments\":{";
    bool bFirst = true;
    for (const char* Name : {
             "BaseColorAO", "Depth", "EmissiveMetallic", "FinalOutput",
             "LightingAccumulation", "NormalRoughness"})
    {
        const auto* Evidence = FindReadback(Inspection, Name);
        if (!bFirst) std::cerr << ',';
        bFirst = false;
        std::cerr << '\"' << Name << "\":\""
                  << (Evidence
                      ? ReadbackDecodedPixelDigest(*Evidence).CStr() : "")
                  << '\"';
    }
    std::cerr << "},\"cpu\":{"
              << "\"descriptor\":\""
              << Inspection.DescriptorFingerprint.CStr()
              << "\",\"device\":\""
              << DeviceFingerprint.CStr()
              << "\",\"pipeline\":\""
              << Inspection.PipelineFingerprint.CStr()
              << "\",\"shader\":\""
              << Inspection.ShaderFingerprint.CStr()
              << "\",\"snapshot\":\""
              << Inspection.SnapshotFingerprint.CStr()
              << "\",\"uniform\":\""
              << Inspection.UniformFingerprint.CStr()
              << "\"},\"frameToken\":"
              << Inspection.AuthoritativeFrameToken
              << ",\"vulkanDriver\":";
    if (VulkanDriver.bAvailable)
    {
        std::cerr << "{\"driverId\":" << VulkanDriver.DriverId
                  << ",\"driverVersion\":"
                  << VulkanDriver.DriverVersion
                  << ",\"floatControlsSha256\":\""
                  << VulkanDriver.FloatControlsSha256.CStr()
                  << "\",\"pipelineCacheUuid\":\""
                  << VulkanDriver.PipelineCacheUuid << "\"}";
    }
    else
    {
        std::cerr << "null";
    }
    std::cerr
              << ",\"windowCapture\":\""
              << CaptureDecodedPixelDigest(
                     Inspection.AuthoritativeCapture).CStr()
              << "\"}\n";
}
