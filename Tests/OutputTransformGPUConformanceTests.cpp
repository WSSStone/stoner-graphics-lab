#include "OutputTransformGPUConformanceTests.h"

#include "Asset/AssetMinimal.h"
#include "Asset/FAssetDigest.h"
#include "Core/SGPlatform.h"
#include "FMetalLibraryCompiler.h"
#include "FOutputTransformReference.h"
#include "FOutputTransformShaderParameters.h"
#include "FSpirvCrossMslDeriver.h"
#include "MetalRHI/FMetalDeviceFactory.h"
#include "Renderer/FOutputTransformSettings.h"
#include "RHI/RHIMinimal.h"
#include "VulkanRHI/FVulkanDevice.h"
#include "../ThirdParty/yyjson/yyjson.h"

#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace
{

using namespace Stoner;
using namespace Stoner::Renderer;

void Record(FOutputTransformGPUConformanceTestResult& Result, bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

bool Near(double Actual, double Expected) noexcept
{
    const double Tolerance = std::max(1.0e-10,
        1.0e-10 * std::abs(Expected));
    return std::isfinite(Actual) && std::isfinite(Expected) &&
        std::abs(Actual - Expected) <= Tolerance;
}

bool ReadRgb(yyjson_val* Value, FOutputTransformReferenceRgb& Out) noexcept
{
    if (!yyjson_is_arr(Value) || yyjson_arr_size(Value) != 3) return false;
    yyjson_val* R = yyjson_arr_get(Value, 0);
    yyjson_val* G = yyjson_arr_get(Value, 1);
    yyjson_val* B = yyjson_arr_get(Value, 2);
    if (!yyjson_is_num(R) || !yyjson_is_num(G) || !yyjson_is_num(B))
    {
        return false;
    }
    Out = {yyjson_get_num(R), yyjson_get_num(G), yyjson_get_num(B)};
    return FOutputTransformReference::IsFinite(Out);
}

bool NearRgb(const FOutputTransformReferenceRgb& Actual,
    const FOutputTransformReferenceRgb& Expected) noexcept
{
    return Near(Actual.R, Expected.R) && Near(Actual.G, Expected.G) &&
        Near(Actual.B, Expected.B);
}

struct FGpuVectorSample
{
    Core::FString CaseId;
    FOutputTransformReferenceRgb SceneLinear;
    float ExposureStops = 0.0f;
    Core::FString ProfileId;
    Core::FString TransformVersionId;
    Core::FString EncodingId;
    float ReferenceWhiteNits = 100.0f;
    FOutputTransformReferenceRgb ExpectedEncoded;
    FOutputTransformReferenceRgb ExpectedDecoded;
    FOutputTransformReferenceRgb DecodedRgbTolerance;
    FOutputTransformReferenceXyz ExpectedXyz;
    FOutputTransformReferenceXyz DecodedXyzTolerance;
};

bool ValidateFrozenVectorAuthority(
    Stoner::Core::uint32& OutExpectationCount,
    Stoner::Core::uint32& OutNativeEncodingCount,
    std::vector<FGpuVectorSample>* OutSamples = nullptr)
{
    if (OutSamples) OutSamples->clear();
    constexpr const char* Path =
        "Tests/Fixtures/OutputTransform/vectors-v1.json";
    std::ifstream Input(Path, std::ios::binary);
    if (!Input) return false;
    const std::string Bytes{
        std::istreambuf_iterator<char>(Input),
        std::istreambuf_iterator<char>()};
    const auto Digest = Stoner::Asset::FAssetDigest::FromBytes(
        std::span<const Stoner::Core::uint8>(
            reinterpret_cast<const Stoner::Core::uint8*>(Bytes.data()),
            Bytes.size()));
    if (Digest.ToLowerHex() != GOutputTransformVectorSetSha256) return false;

    yyjson_read_err Error{};
    yyjson_doc* Document = yyjson_read_opts(
        const_cast<char*>(Bytes.data()), Bytes.size(), YYJSON_READ_NOFLAG,
        nullptr, &Error);
    yyjson_val* Root = Document ? yyjson_doc_get_root(Document) : nullptr;
    yyjson_val* Schema = yyjson_obj_get(Root, "schema");
    yyjson_val* VectorSet = yyjson_obj_get(Root, "vectorSetId");
    yyjson_val* Cases = yyjson_obj_get(Root, "cases");
    bool bValid = yyjson_is_obj(Root) && yyjson_is_str(Schema) &&
        std::string_view(yyjson_get_str(Schema), yyjson_get_len(Schema)) ==
            "stoner.output-transform-vectors" &&
        yyjson_is_str(VectorSet) &&
        std::string_view(yyjson_get_str(VectorSet),
            yyjson_get_len(VectorSet)) == GOutputTransformVectorSetId &&
        yyjson_is_arr(Cases) && yyjson_arr_size(Cases) == 32;

    OutExpectationCount = 0;
    OutNativeEncodingCount = 0;
    yyjson_val* Case = nullptr;
    size_t CaseIndex = 0;
    size_t CaseMaximum = 0;
    yyjson_arr_foreach(Cases, CaseIndex, CaseMaximum, Case)
    {
        yyjson_val* CaseId = yyjson_obj_get(Case, "caseId");
        yyjson_val* Scene = yyjson_obj_get(Case, "sceneLinearRec709");
        yyjson_val* Exposure = yyjson_obj_get(Case, "exposureStops");
        yyjson_val* Expectations = yyjson_obj_get(Case, "expectations");
        FOutputTransformReferenceRgb SceneRgb;
        bValid = bValid && yyjson_is_str(CaseId) &&
            ReadRgb(Scene, SceneRgb) &&
            yyjson_is_num(Exposure) && yyjson_is_arr(Expectations) &&
            yyjson_arr_size(Expectations) == 7;
        if (!bValid) break;

        yyjson_val* Expectation = nullptr;
        size_t ExpectationIndex = 0;
        size_t ExpectationMaximum = 0;
        yyjson_arr_foreach(Expectations, ExpectationIndex,
            ExpectationMaximum, Expectation)
        {
            ++OutExpectationCount;
            yyjson_val* ProfileValue =
                yyjson_obj_get(Expectation, "profileId");
            yyjson_val* TransformValue =
                yyjson_obj_get(Expectation, "transformVersionId");
            yyjson_val* ExpectedXyz =
                yyjson_obj_get(Expectation, "expectedXyz");
            yyjson_val* NativeEncodings =
                yyjson_obj_get(Expectation, "nativeEncodings");
            if (!yyjson_is_str(ProfileValue) ||
                !yyjson_is_str(TransformValue) ||
                !yyjson_is_arr(NativeEncodings))
            {
                bValid = false;
                break;
            }
            const std::string Profile(yyjson_get_str(ProfileValue),
                yyjson_get_len(ProfileValue));
            const std::string Transform(yyjson_get_str(TransformValue),
                yyjson_get_len(TransformValue));
            FOutputTransformReferenceRgb Xyz;
            if (!ReadRgb(ExpectedXyz, Xyz))
            {
                bValid = false;
                break;
            }

            yyjson_val* Native = nullptr;
            size_t NativeIndex = 0;
            size_t NativeMaximum = 0;
            yyjson_arr_foreach(NativeEncodings, NativeIndex,
                NativeMaximum, Native)
            {
                ++OutNativeEncodingCount;
                yyjson_val* EncodingValue =
                    yyjson_obj_get(Native, "encodingId");
                FOutputTransformReferenceRgb Encoded;
                FOutputTransformReferenceRgb Decoded;
                if (!yyjson_is_str(EncodingValue) ||
                    !ReadRgb(yyjson_obj_get(Native, "encodedRgb"), Encoded) ||
                    !ReadRgb(yyjson_obj_get(Native, "decodedLinearRgb"),
                        Decoded))
                {
                    bValid = false;
                    break;
                }
                const std::string Encoding(yyjson_get_str(EncodingValue),
                    yyjson_get_len(EncodingValue));
                FOutputTransformSettings Settings;
                Settings.OutputDeviceProfileId = Profile;
                Settings.ManualExposureStops =
                    static_cast<float>(yyjson_get_num(Exposure));
                Settings.bRequirePresentation = false;
                Settings.bRequireReadback = true;
                if (Profile.starts_with("Hdr."))
                {
                    Settings.DynamicRange = EOutputDynamicRange::HDR;
                    Settings.HDRViewingVersion = Transform;
                    if (Encoding == "pq-rec2020")
                    {
                        Settings.PreferredNativeEncoding =
                            RHI::ERHIPresentationNativeEncoding::Pq;
                    }
                    else if (Encoding == "scrgb80")
                    {
                        Settings.PreferredNativeEncoding =
                            RHI::ERHIPresentationNativeEncoding::ScRgb80;
                    }
                    else if (Encoding == "metal-edr")
                    {
                        Settings.PreferredNativeEncoding =
                            RHI::ERHIPresentationNativeEncoding::MetalEdr;
                        yyjson_val* ReferenceWhite =
                            yyjson_obj_get(Native, "referenceWhiteNits");
                        if (!yyjson_is_num(ReferenceWhite))
                        {
                            bValid = false;
                            break;
                        }
                        Settings.NativeReferenceWhiteNits =
                            static_cast<float>(yyjson_get_num(ReferenceWhite));
                    }
                    else
                    {
                        bValid = false;
                        break;
                    }
                }
                else
                {
                    Settings.SDRToneMapVersion = Transform;
                    if (Encoding != "sdr-explicit")
                    {
                        bValid = false;
                        break;
                    }
                }
                const auto Resolved =
                    FOutputTransformSettingsValidator().Validate(Settings);
                const auto Report = Resolved.Succeeded()
                    ? Private::EvaluateOutputTransformConformance(
                        Resolved.Settings, SceneRgb)
                    : Private::FOutputTransformConformanceReport{};
                FOutputTransformReferenceRgb ExpectedTolerance;
                yyjson_val* ToleranceValue = Profile.starts_with("Hdr.")
                    ? yyjson_obj_get(Native, "decodedRgbTolerance")
                    : yyjson_obj_get(Expectation, "rgbTolerance");
                bValid = bValid && ReadRgb(ToleranceValue, ExpectedTolerance) &&
                    Report.IsValid() && NearRgb(Report.Encoded, Encoded) &&
                    NearRgb(Report.Decoded, Decoded) &&
                    NearRgb(Report.DecodedTolerance, ExpectedTolerance) &&
                    Near(Report.ExpectedXyz.X, Xyz.R) &&
                    Near(Report.ExpectedXyz.Y, Xyz.G) &&
                    Near(Report.ExpectedXyz.Z, Xyz.B);
                if (bValid && OutSamples)
                {
                    FGpuVectorSample Sample;
                    Sample.CaseId = Core::FString(std::string(
                        yyjson_get_str(CaseId), yyjson_get_len(CaseId)));
                    Sample.SceneLinear = SceneRgb;
                    Sample.ExposureStops =
                        static_cast<float>(yyjson_get_num(Exposure));
                    Sample.ProfileId = Core::FString(Profile);
                    Sample.TransformVersionId = Core::FString(Transform);
                    Sample.EncodingId = Core::FString(Encoding);
                    Sample.ExpectedEncoded = Encoded;
                    Sample.ExpectedDecoded = Decoded;
                    Sample.DecodedRgbTolerance = ExpectedTolerance;
                    Sample.ExpectedXyz = {Xyz.R, Xyz.G, Xyz.B};
                    if (Profile.starts_with("Hdr."))
                    {
                        yyjson_val* ReferenceWhite =
                            yyjson_obj_get(Native, "referenceWhiteNits");
                        FOutputTransformReferenceRgb XyzTolerance;
                        bValid = yyjson_is_num(ReferenceWhite) &&
                            ReadRgb(yyjson_obj_get(
                                Native, "decodedXyzTolerance"), XyzTolerance);
                        if (bValid)
                        {
                            Sample.ReferenceWhiteNits = static_cast<float>(
                                yyjson_get_num(ReferenceWhite));
                            Sample.DecodedXyzTolerance = {
                                XyzTolerance.R, XyzTolerance.G,
                                XyzTolerance.B};
                        }
                    }
                    else
                    {
                        Sample.DecodedXyzTolerance =
                            FOutputTransformReference::
                                PropagateRgbToleranceToXyz(
                                    ExpectedTolerance,
                                    EOutputTransformReferenceColorSpace::
                                        Rec709D65);
                    }
                    if (bValid) OutSamples->push_back(std::move(Sample));
                }
                if (!bValid) break;
            }
            if (!bValid) break;
        }
        if (!bValid) break;
    }
    yyjson_doc_free(Document);
    const bool bComplete = bValid && OutExpectationCount == 224 &&
        OutNativeEncodingCount == 288 &&
        (!OutSamples || OutSamples->size() == 288);
    if (!bComplete && OutSamples) OutSamples->clear();
    return bComplete;
}

FOutputTransformSettings SettingsForSample(const FGpuVectorSample& Sample)
{
    FOutputTransformSettings Settings;
    Settings.ManualExposureStops = Sample.ExposureStops;
    Settings.OutputDeviceProfileId = Sample.ProfileId;
    Settings.bRequirePresentation = false;
    Settings.bRequireReadback = true;
    if (Sample.ProfileId.View().starts_with("Hdr."))
    {
        Settings.DynamicRange = EOutputDynamicRange::HDR;
        Settings.HDRViewingVersion = Sample.TransformVersionId;
        if (Sample.EncodingId == Core::FString("pq-rec2020"))
        {
            Settings.PreferredNativeEncoding =
                RHI::ERHIPresentationNativeEncoding::Pq;
        }
        else if (Sample.EncodingId == Core::FString("scrgb80"))
        {
            Settings.PreferredNativeEncoding =
                RHI::ERHIPresentationNativeEncoding::ScRgb80;
        }
        else if (Sample.EncodingId == Core::FString("metal-edr"))
        {
            Settings.PreferredNativeEncoding =
                RHI::ERHIPresentationNativeEncoding::MetalEdr;
            Settings.NativeReferenceWhiteNits = Sample.ReferenceWhiteNits;
        }
    }
    else
    {
        Settings.SDRToneMapVersion = Sample.TransformVersionId;
    }
    return Settings;
}

EOutputTransformReferenceTransfer ReferenceTransfer(
    EOutputTransferFunction Transfer) noexcept
{
    switch (Transfer)
    {
    case EOutputTransferFunction::Srgb:
        return EOutputTransformReferenceTransfer::Srgb;
    case EOutputTransferFunction::Bt709:
        return EOutputTransformReferenceTransfer::Bt709;
    case EOutputTransferFunction::Gamma22:
        return EOutputTransformReferenceTransfer::Gamma22;
    case EOutputTransferFunction::St2084:
        return EOutputTransformReferenceTransfer::St2084;
    default:
        return EOutputTransformReferenceTransfer::Srgb;
    }
}

EOutputTransformReferenceNativeEncoding ReferenceNativeEncoding(
    RHI::ERHIPresentationNativeEncoding Encoding) noexcept
{
    switch (Encoding)
    {
    case RHI::ERHIPresentationNativeEncoding::Pq:
        return EOutputTransformReferenceNativeEncoding::PqPacked10;
    case RHI::ERHIPresentationNativeEncoding::ScRgb80:
        return EOutputTransformReferenceNativeEncoding::ScRgb80;
    case RHI::ERHIPresentationNativeEncoding::MetalEdr:
        return EOutputTransformReferenceNativeEncoding::MetalEdr;
    default:
        return EOutputTransformReferenceNativeEncoding::PqPacked10;
    }
}

bool DecodeGpuResult(
    const FResolvedOutputTransformSettings& Settings,
    const FOutputTransformReferenceRgb& Encoded,
    FOutputTransformReferenceRgb& OutDecoded,
    FOutputTransformReferenceXyz& OutXyz) noexcept
{
    if (!FOutputTransformReference::IsFinite(Encoded)) return false;
    if (Settings.DynamicRange == EOutputDynamicRange::SDR)
    {
        const auto Transfer = ReferenceTransfer(Settings.Transfer);
        const auto R = FOutputTransformReference::DecodeTransfer(
            Encoded.R, Transfer);
        const auto G = FOutputTransformReference::DecodeTransfer(
            Encoded.G, Transfer);
        const auto B = FOutputTransformReference::DecodeTransfer(
            Encoded.B, Transfer);
        if (!R.IsSuccess() || !G.IsSuccess() || !B.IsSuccess()) return false;
        OutDecoded = {R.Value, G.Value, B.Value};
    }
    else if (Settings.NativeEncoding ==
        RHI::ERHIPresentationNativeEncoding::Pq)
    {
        const auto R = FOutputTransformReference::DecodeTransfer(
            Encoded.R, EOutputTransformReferenceTransfer::St2084);
        const auto G = FOutputTransformReference::DecodeTransfer(
            Encoded.G, EOutputTransformReferenceTransfer::St2084);
        const auto B = FOutputTransformReference::DecodeTransfer(
            Encoded.B, EOutputTransformReferenceTransfer::St2084);
        if (!R.IsSuccess() || !G.IsSuccess() || !B.IsSuccess()) return false;
        OutDecoded = {R.Value, G.Value, B.Value};
    }
    else
    {
        const auto Decoded = FOutputTransformReference::DecodeLinearHdr(
            Encoded, ReferenceNativeEncoding(Settings.NativeEncoding),
            Settings.ReferenceWhiteNits);
        if (!Decoded.IsSuccess()) return false;
        OutDecoded = Decoded.Value;
    }
    const auto ColorSpace = Settings.TargetPrimaries ==
        EOutputColorPrimaries::Rec2020
        ? EOutputTransformReferenceColorSpace::Rec2020D65
        : EOutputTransformReferenceColorSpace::Rec709D65;
    OutXyz = FOutputTransformReference::ConvertToXyz(
        OutDecoded, ColorSpace);
    return FOutputTransformReference::IsFinite(OutDecoded) &&
        std::isfinite(OutXyz.X) && std::isfinite(OutXyz.Y) &&
        std::isfinite(OutXyz.Z);
}

bool WithinTolerance(double Actual, double Expected, double Tolerance) noexcept
{
    return std::isfinite(Actual) && std::isfinite(Expected) &&
        std::isfinite(Tolerance) && Tolerance >= 0.0 &&
        std::abs(Actual - Expected) <= Tolerance;
}

Core::TArray<Core::uint8> ReadFileBytes(
    const std::filesystem::path& Path)
{
    std::ifstream Input(Path, std::ios::binary);
    return {std::istreambuf_iterator<char>(Input),
            std::istreambuf_iterator<char>()};
}

Core::TArray<Asset::FShaderInterfaceBinding> OutputTransformInterface(
    Asset::EShaderStage Stage)
{
    if (Stage != Asset::EShaderStage::Fragment) return {};
    return {
        {0, 0, Asset::EShaderResourceKind::CombinedTextureSampler, 1,
         {Asset::EShaderStage::Fragment},
         Core::FString("InputColorTexture")},
        {0, 1, Asset::EShaderResourceKind::UniformBuffer, 1,
         {Asset::EShaderStage::Fragment},
         Core::FString("OutputTransformParameters")}};
}

RHI::ERHIDescriptorType RhiDescriptor(
    Asset::EShaderResourceKind Kind) noexcept
{
    switch (Kind)
    {
    case Asset::EShaderResourceKind::UniformBuffer:
        return RHI::ERHIDescriptorType::UniformBuffer;
    case Asset::EShaderResourceKind::SampledTexture:
        return RHI::ERHIDescriptorType::SampledTexture;
    case Asset::EShaderResourceKind::Sampler:
        return RHI::ERHIDescriptorType::Sampler;
    case Asset::EShaderResourceKind::StorageBuffer:
        return RHI::ERHIDescriptorType::StorageBuffer;
    case Asset::EShaderResourceKind::StorageTexture:
        return RHI::ERHIDescriptorType::StorageTexture;
    case Asset::EShaderResourceKind::CombinedTextureSampler:
        return RHI::ERHIDescriptorType::CombinedTextureSampler;
    }
    return RHI::ERHIDescriptorType::UniformBuffer;
}

RHI::ERHIShaderStage RhiStage(Asset::EShaderStage Stage) noexcept
{
    switch (Stage)
    {
    case Asset::EShaderStage::Vertex: return RHI::ERHIShaderStage::Vertex;
    case Asset::EShaderStage::Fragment: return RHI::ERHIShaderStage::Fragment;
    case Asset::EShaderStage::Compute: return RHI::ERHIShaderStage::Compute;
    default: return RHI::ERHIShaderStage::Unknown;
    }
}

RHI::ERHIShaderStageFlags RhiVisibility(
    const Core::TArray<Asset::EShaderStage>& Stages) noexcept
{
    RHI::ERHIShaderStageFlags Result = RHI::ERHIShaderStageFlags::None;
    for (const auto Stage : Stages)
        Result |= RHI::ToShaderStageFlag(RhiStage(Stage));
    return Result;
}

void AddRhiInterfaceMetadata(
    const Core::TArray<Asset::FShaderInterfaceBinding>& Interface,
    Asset::EShaderStage Stage,
    RHI::FRHIShaderModuleDesc& Out)
{
    for (const auto& Binding : Interface)
    {
        if (std::find(Binding.Visibility.begin(), Binding.Visibility.end(),
                Stage) == Binding.Visibility.end())
            continue;
        Out.InterfaceMetadata.Bindings.push_back({
            Binding.SetIndex, Binding.BindingIndex,
            RhiDescriptor(Binding.Kind), Binding.ArrayCount,
            RhiVisibility(Binding.Visibility)});
    }
}

bool BuildVulkanShader(
    const std::filesystem::path& Path,
    Asset::EShaderStage Stage,
    RHI::FRHIShaderModuleDesc& Out)
{
    Out = {};
    Out.Stage = RhiStage(Stage);
    Out.EntryPoint = "main";
    Out.Payload.Format = RHI::ERHIShaderPayloadFormat::SPIRV;
    Out.Payload.Bytes = ReadFileBytes(Path);
    Out.Payload.PayloadIdentity = Core::FString(
        std::string("OutputTransformGPU/") + Path.filename().string());
    Out.Payload.TargetProfile = "vulkan-1.3";
    Out.Payload.PayloadDigest = RHI::ComputeRHISha256(Out.Payload.Bytes);
    Out.ValidationMode = RHI::ERHIShaderBytecodeValidationMode::Runtime;
    Out.RuntimeMode = RHI::ERHIRuntimeObjectMode::RealRuntime;
    Out.DebugName = Out.Payload.PayloadIdentity;
    Out.InterfaceMetadata.DebugName = Out.DebugName;
    AddRhiInterfaceMetadata(OutputTransformInterface(Stage), Stage, Out);
    return RHI::IsValidRHIShaderModuleDesc(Out);
}

#if SG_PLATFORM_MAC

const char* HostArchitecture() noexcept
{
#if defined(__aarch64__) || defined(__arm64__)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#else
    return "unsupported";
#endif
}

bool BuildMetalShader(
    const std::filesystem::path& Path,
    Asset::EShaderStage Stage,
    const std::filesystem::path& Scratch,
    RHI::FRHIShaderModuleDesc& Out,
    Core::FString& OutReason);

#endif

enum class EGpuBackend
{
    Vulkan,
    Metal
};

struct FGpuExecutionResult
{
    bool bSucceeded = false;
    FOutputTransformReferenceRgb Encoded;
    float Alpha = 0.0f;
    Core::FString StableReason;
};

class FOutputTransformGpuHarness final
{
public:
    ~FOutputTransformGpuHarness()
    {
        if (Device_ && Device_->IsActive()) (void)Device_->Shutdown();
    }

    bool Initialize(EGpuBackend SelectedBackend)
    {
        Backend_ = SelectedBackend;
        RHI::FRHIShaderModuleDesc VertexDesc;
        RHI::FRHIShaderModuleDesc FragmentDesc;
        if (SelectedBackend == EGpuBackend::Vulkan)
        {
#if STONER_TEST_VULKAN_RUNTIME_AVAILABLE
            VulkanDevice_ = Core::MakeShared<Backend::Vulkan::FVulkanDevice>();
            Backend::Vulkan::FVulkanInstanceDesc Desc;
            Desc.RuntimeMode =
                Backend::Vulkan::EVulkanInstanceRuntimeMode::
                    DeterministicFallback;
            Desc.bRequestValidation = false;
            if (VulkanDevice_->Initialize(Desc) != RHI::ERHIResult::Success ||
                VulkanDevice_->EnableNativeShaderRuntime() !=
                    RHI::ERHIResult::Success)
                return Fail("output-transform-gpu-vulkan-device");
            Device_ = VulkanDevice_;
            if (!BuildVulkanShader(
                    "Content/Shaders/PostProcess/Fullscreen.vert.spv",
                    Asset::EShaderStage::Vertex, VertexDesc) ||
                !BuildVulkanShader(
                    "Content/Shaders/PostProcess/OutputTransform.frag.spv",
                    Asset::EShaderStage::Fragment, FragmentDesc))
                return Fail("output-transform-gpu-vulkan-shader-desc");
#else
            return Fail("output-transform-gpu-vulkan-unavailable");
#endif
        }
        else
        {
#if SG_PLATFORM_MAC
            const auto Created = Backend::Metal::CreateMetalDevice();
            if (!Created.Succeeded())
                return Fail("output-transform-gpu-metal-device");
            Device_ = Created.Device;
            const std::filesystem::path Root =
                std::filesystem::temp_directory_path() /
                "stoner-output-transform-gpu-metal";
            if (!BuildMetalShader(
                    "Content/Shaders/PostProcess/Fullscreen.vert.spv",
                    Asset::EShaderStage::Vertex, Root / "vertex",
                    VertexDesc, Reason_) ||
                !BuildMetalShader(
                    "Content/Shaders/PostProcess/OutputTransform.frag.spv",
                    Asset::EShaderStage::Fragment, Root / "fragment",
                    FragmentDesc, Reason_))
                return false;
#else
            return Fail("output-transform-gpu-metal-host-unsupported");
#endif
        }

        RHI::FRHIPipelineLayoutDesc LayoutDesc;
        LayoutDesc.Bindings = {
            {0, 0, RHI::ERHIDescriptorType::CombinedTextureSampler, 1,
             RHI::ERHIShaderStageFlags::Fragment},
            {0, 1, RHI::ERHIDescriptorType::UniformBuffer, 1,
             RHI::ERHIShaderStageFlags::Fragment}};
        const auto Layout = Device_->CreatePipelineLayout(LayoutDesc);
        const auto Vertex = Device_->CreateShaderModule(VertexDesc);
        const auto Fragment = Device_->CreateShaderModule(FragmentDesc);
        if (!Layout.Succeeded() || !Vertex.Succeeded() ||
            !Fragment.Succeeded())
            return Fail("output-transform-gpu-pipeline-inputs");
        Layout_ = Layout.Object;

        RHI::FRHIGraphicsPipelineDesc PipelineDesc;
        PipelineDesc.ShaderModules = {Vertex.Object, Fragment.Object};
        PipelineDesc.PipelineLayout = Layout_;
        PipelineDesc.VertexInput.Stride = sizeof(float);
        PipelineDesc.VertexInput.Attributes = {
            {0, RHI::ERHIFormat::R32_Float, 0}};
        PipelineDesc.Rasterizer.CullMode = RHI::ERHICullMode::None;
        PipelineDesc.RenderTargets.ColorFormats = {
            RHI::ERHIFormat::R32G32B32A32_Float};
        PipelineDesc.RuntimeMode = RHI::ERHIRuntimeObjectMode::RealRuntime;
        const auto Pipeline = Device_->CreateGraphicsPipeline(PipelineDesc);
        if (!Pipeline.Succeeded())
            return Fail("output-transform-gpu-pipeline");
        Pipeline_ = Pipeline.Object;

        RHI::FRHIRenderPassDesc PassDesc;
        PassDesc.Attachments = {{RHI::ERHIAttachmentRole::Color,
            RHI::ERHIFormat::R32G32B32A32_Float,
            RHI::ERHISampleCount::One, RHI::ERHIAttachmentLoadOp::Clear,
            RHI::ERHIAttachmentStoreOp::Store}};
        const auto Pass = Device_->CreateRenderPass(PassDesc);
        if (!Pass.Succeeded())
            return Fail("output-transform-gpu-render-pass");
        RenderPass_ = Pass.Object;

        RHI::FRHITextureDesc InputDesc;
        InputDesc.Format = RHI::ERHIFormat::R32G32B32A32_Float;
        InputDesc.Usage = RHI::ERHITextureUsage::Sampled |
            RHI::ERHITextureUsage::CopyDestination;
        const auto Input = Device_->CreateTexture(InputDesc);
        if (!Input.Succeeded())
            return Fail("output-transform-gpu-input-texture");
        Input_ = Input.Object;

        for (Core::uint32 Index = 0; Index < Targets_.size(); ++Index)
        {
            RHI::FRHITextureDesc TargetDesc;
            TargetDesc.Format = RHI::ERHIFormat::R32G32B32A32_Float;
            TargetDesc.Usage = RHI::ERHITextureUsage::ColorAttachment |
                (Index + 1 < Targets_.size()
                    ? RHI::ERHITextureUsage::Sampled
                    : RHI::ERHITextureUsage::CopySource);
            const auto Target = Device_->CreateTexture(TargetDesc);
            if (!Target.Succeeded())
                return Fail("output-transform-gpu-target-texture");
            Targets_[Index] = Target.Object;

            RHI::FRHIFramebufferDesc FrameDesc;
            FrameDesc.RenderPass = RenderPass_;
            FrameDesc.Attachments = {{Targets_[Index]}};
            FrameDesc.Width = 1;
            FrameDesc.Height = 1;
            const auto Framebuffer = Device_->CreateFramebuffer(FrameDesc);
            if (!Framebuffer.Succeeded())
                return Fail("output-transform-gpu-framebuffer");
            Framebuffers_[Index] = Framebuffer.Object;

            const auto Uniform = Device_->CreateBuffer({
                sizeof(Private::FOutputTransformShaderParameters),
                RHI::ERHIBufferUsage::Uniform,
                RHI::ERHIMemoryAccess::HostVisible});
            if (!Uniform.Succeeded())
                return Fail("output-transform-gpu-uniform");
            Uniforms_[Index] = Uniform.Object;
        }

        RHI::FRHISamplerDesc SamplerDesc;
        SamplerDesc.MinFilter = RHI::ERHISamplerFilter::Nearest;
        SamplerDesc.MagFilter = RHI::ERHISamplerFilter::Nearest;
        SamplerDesc.MipFilter = RHI::ERHISamplerMipFilter::None;
        SamplerDesc.AddressU = RHI::ERHISamplerAddressMode::ClampToEdge;
        SamplerDesc.AddressV = RHI::ERHISamplerAddressMode::ClampToEdge;
        SamplerDesc.AddressW = RHI::ERHISamplerAddressMode::ClampToEdge;
        const auto Sampler = Device_->CreateSampler(SamplerDesc);
        if (!Sampler.Succeeded())
            return Fail("output-transform-gpu-sampler");
        Sampler_ = Sampler.Object;

        for (Core::uint32 Index = 0; Index < DescriptorSets_.size(); ++Index)
        {
            const auto Set = Device_->CreateDescriptorSet(Layout_, 0);
            const auto& Source = Index == 0 ? Input_ : Targets_[Index - 1];
            if (!Set.Succeeded() ||
                Set.Object->UpdateCombinedTextureSampler(
                    0, 0, Source, Sampler_) != RHI::ERHIResult::Success ||
                Set.Object->UpdateBuffer(
                    1, 0, Uniforms_[Index]) != RHI::ERHIResult::Success)
                return Fail("output-transform-gpu-descriptor-set");
            DescriptorSets_[Index] = Set.Object;
        }

        const auto VertexBuffer = Device_->CreateBuffer({
            sizeof(float) * 3, RHI::ERHIBufferUsage::Vertex,
            RHI::ERHIMemoryAccess::HostVisible});
        const auto Readback = Device_->CreateBuffer({
            sizeof(float) * 4, RHI::ERHIBufferUsage::CopyDestination,
            RHI::ERHIMemoryAccess::HostVisible});
        const auto Commands =
            Device_->CreateCommandBuffer(RHI::ERHIQueueType::Graphics);
        const auto Queue =
            Device_->CreateCommandQueue(RHI::ERHIQueueType::Graphics);
        const auto Fence = Device_->CreateFence();
        const std::array<float, 3> DummyVertices = {0.0f, 0.0f, 0.0f};
        if (!VertexBuffer.Succeeded() || !Readback.Succeeded() ||
            !Commands.Succeeded() || !Queue.Succeeded() ||
            !Fence.Succeeded() ||
            Device_->UploadBuffer(VertexBuffer.Object,
                {0, DummyVertices.data(), sizeof(DummyVertices)}) !=
                RHI::ERHIResult::Success)
            return Fail("output-transform-gpu-execution-resources");
        VertexBuffer_ = VertexBuffer.Object;
        Readback_ = Readback.Object;
        Commands_ = Commands.Object;
        Queue_ = Queue.Object;
        Fence_ = Fence.Object;
        return true;
    }

    FGpuExecutionResult Execute(
        const FResolvedOutputTransformSettings& Settings,
        FOutputTransformReferenceRgb SceneLinear)
    {
        FGpuExecutionResult Out;
        if (!Device_ || !Settings.IsValid() ||
            !FOutputTransformReference::IsFinite(SceneLinear))
        {
            Out.StableReason = "output-transform-gpu-invalid-execute";
            return Out;
        }
        const std::array<float, 4> Source = {
            static_cast<float>(SceneLinear.R),
            static_cast<float>(SceneLinear.G),
            static_cast<float>(SceneLinear.B), 1.0f};
        RHI::FRHITextureUploadDesc Upload;
        Upload.Width = 1;
        Upload.Height = 1;
        Upload.RowPitchBytes = sizeof(Source);
        Upload.Data = Source.data();
        Upload.DataSizeBytes = sizeof(Source);
        if (Device_->UploadTexture(Input_, Upload) != RHI::ERHIResult::Success)
        {
            Out.StableReason = "output-transform-gpu-source-upload";
            return Out;
        }

        constexpr std::array Stages = {
            Private::EOutputTransformShaderStageMode::ManualExposure,
            Private::EOutputTransformShaderStageMode::ToneOrViewing,
            Private::EOutputTransformShaderStageMode::OutputDevice};
        for (Core::uint32 Index = 0; Index < Stages.size(); ++Index)
        {
            const auto Binding =
                Private::FOutputTransformShaderParameterBuilder::Build(
                    Settings, Stages[Index]);
            if (!Binding.IsValid() ||
                Device_->UploadBuffer(Uniforms_[Index],
                    {0, &Binding.Parameters,
                     sizeof(Binding.Parameters)}) != RHI::ERHIResult::Success)
            {
                Out.StableReason = "output-transform-gpu-parameter-upload";
                return Out;
            }
        }

        if (bExecuted_ &&
            (Commands_->Reset() != RHI::ERHIResult::Success ||
             Fence_->Reset() != RHI::ERHIResult::Success))
        {
            Out.StableReason = "output-transform-gpu-reset";
            return Out;
        }
        if (Commands_->Begin() != RHI::ERHIResult::Success)
        {
            Out.StableReason = "output-transform-gpu-command-begin";
            return Out;
        }
        for (Core::uint32 Index = 0; Index < Targets_.size(); ++Index)
        {
            const RHI::ERHIResourceLayout Before = !bExecuted_
                ? RHI::ERHIResourceLayout::Undefined
                : Index + 1 < Targets_.size()
                    ? RHI::ERHIResourceLayout::ShaderReadOnly
                    : RHI::ERHIResourceLayout::CopySource;
            if (!RecordPass(Index, Before))
            {
                Out.StableReason = "output-transform-gpu-record-pass";
                return Out;
            }
        }
        RHI::FRHITextureBufferCopyRegion Copy;
        Copy.Width = 1;
        Copy.Height = 1;
        if (Commands_->RecordTextureToBufferCopy(
                Targets_.back(), Readback_, Copy) !=
                RHI::ERHIResult::Success ||
            Commands_->End() != RHI::ERHIResult::Success ||
            Queue_->Submit(Commands_, {}, {}, Fence_) !=
                RHI::ERHIResult::Success ||
            Fence_->Wait(5'000'000) != RHI::ERHIResult::Success ||
            Queue_->WaitIdle() != RHI::ERHIResult::Success)
        {
            Out.StableReason = "output-transform-gpu-submit";
            return Out;
        }
        bExecuted_ = true;

        Core::TArray<Core::uint8> Bytes;
        RHI::ERHIResult ReadResult = RHI::ERHIResult::Unsupported;
        if (Backend_ == EGpuBackend::Vulkan)
        {
#if STONER_TEST_VULKAN_RUNTIME_AVAILABLE
            ReadResult = VulkanDevice_->ReadbackBufferForTesting(
                Readback_, 0, sizeof(float) * 4, Bytes);
#endif
        }
        else
        {
#if SG_PLATFORM_MAC
            ReadResult = Backend::Metal::ReadMetalBufferForValidation(
                Device_, Readback_, 0, sizeof(float) * 4, Bytes);
#endif
        }
        if (ReadResult != RHI::ERHIResult::Success ||
            Bytes.size() != sizeof(float) * 4)
        {
            Out.StableReason = "output-transform-gpu-readback";
            return Out;
        }
        std::array<float, 4> Values{};
        std::memcpy(Values.data(), Bytes.data(), Bytes.size());
        Out.Encoded = {Values[0], Values[1], Values[2]};
        Out.Alpha = Values[3];
        Out.bSucceeded = FOutputTransformReference::IsFinite(Out.Encoded) &&
            std::isfinite(Out.Alpha);
        Out.StableReason = Out.bSucceeded
            ? Core::FString("output-transform-gpu-success")
            : Core::FString("output-transform-gpu-nonfinite");
        return Out;
    }

    [[nodiscard]] const Core::FString& GetReason() const noexcept
    {
        return Reason_;
    }

private:
    bool Fail(const char* Reason)
    {
        Reason_ = Reason;
        return false;
    }

    bool RecordPass(
        Core::uint32 Index,
        RHI::ERHIResourceLayout Before)
    {
        RHI::FRHIResourceBarrierDesc ToColor;
        ToColor.Texture = Targets_[Index];
        ToColor.RequiredTextureUsage = RHI::ERHITextureUsage::ColorAttachment;
        ToColor.Before = Before;
        ToColor.After = RHI::ERHIResourceLayout::ColorAttachment;
        if (Commands_->RecordLayoutTransition(ToColor) !=
                RHI::ERHIResult::Success ||
            Commands_->BeginRenderPass(
                RenderPass_, Framebuffers_[Index]) !=
                RHI::ERHIResult::Success ||
            Commands_->BindGraphicsPipeline(Pipeline_) !=
                RHI::ERHIResult::Success ||
            Commands_->BindVertexBuffer(VertexBuffer_) !=
                RHI::ERHIResult::Success ||
            Commands_->BindDescriptorSet(DescriptorSets_[Index]) !=
                RHI::ERHIResult::Success ||
            Commands_->SetViewport({0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 1.0f}) !=
                RHI::ERHIResult::Success ||
            Commands_->SetScissor({0, 0, 1, 1}) !=
                RHI::ERHIResult::Success ||
            Commands_->RecordDraw(3, 1) != RHI::ERHIResult::Success ||
            Commands_->EndRenderPass() != RHI::ERHIResult::Success)
            return false;
        RHI::FRHIResourceBarrierDesc After;
        After.Texture = Targets_[Index];
        After.RequiredTextureUsage = Index + 1 < Targets_.size()
            ? RHI::ERHITextureUsage::Sampled
            : RHI::ERHITextureUsage::CopySource;
        After.Before = RHI::ERHIResourceLayout::ColorAttachment;
        After.After = Index + 1 < Targets_.size()
            ? RHI::ERHIResourceLayout::ShaderReadOnly
            : RHI::ERHIResourceLayout::CopySource;
        return Commands_->RecordLayoutTransition(After) ==
            RHI::ERHIResult::Success;
    }

    EGpuBackend Backend_ = EGpuBackend::Vulkan;
    Core::TSharedPtr<RHI::IRHIDevice> Device_;
#if STONER_TEST_VULKAN_RUNTIME_AVAILABLE
    Core::TSharedPtr<Backend::Vulkan::FVulkanDevice> VulkanDevice_;
#endif
    Core::TSharedPtr<RHI::IRHIPipelineLayout> Layout_;
    Core::TSharedPtr<RHI::IRHIGraphicsPipeline> Pipeline_;
    Core::TSharedPtr<RHI::IRHIRenderPass> RenderPass_;
    Core::TSharedPtr<RHI::IRHITexture> Input_;
    std::array<Core::TSharedPtr<RHI::IRHITexture>, 3> Targets_;
    std::array<Core::TSharedPtr<RHI::IRHIFramebuffer>, 3> Framebuffers_;
    Core::TSharedPtr<RHI::IRHISampler> Sampler_;
    std::array<Core::TSharedPtr<RHI::IRHIBuffer>, 3> Uniforms_;
    std::array<Core::TSharedPtr<RHI::IRHIDescriptorSet>, 3> DescriptorSets_;
    Core::TSharedPtr<RHI::IRHIBuffer> VertexBuffer_;
    Core::TSharedPtr<RHI::IRHIBuffer> Readback_;
    Core::TSharedPtr<RHI::IRHICommandBuffer> Commands_;
    Core::TSharedPtr<RHI::IRHICommandQueue> Queue_;
    Core::TSharedPtr<RHI::IRHIFence> Fence_;
    Core::FString Reason_;
    bool bExecuted_ = false;
};

#if SG_PLATFORM_MAC

RHI::ERHINativeResourceClass RhiNativeClass(
    Asset::EShaderNativeResourceClass Value) noexcept
{
    switch (Value)
    {
    case Asset::EShaderNativeResourceClass::Buffer:
        return RHI::ERHINativeResourceClass::Buffer;
    case Asset::EShaderNativeResourceClass::Texture:
        return RHI::ERHINativeResourceClass::Texture;
    case Asset::EShaderNativeResourceClass::Sampler:
        return RHI::ERHINativeResourceClass::Sampler;
    }
    return RHI::ERHINativeResourceClass::Buffer;
}

bool ConvertBindingMap(
    const Asset::FShaderNativeBindingEvidence& Source,
    RHI::FRHINativeBindingMap& Out)
{
    Out = {};
    Out.PolicyVersion = Source.PolicyVersion;
    for (const auto& Entry : Source.Entries)
        Out.Entries.push_back({RhiStage(Entry.Stage), Entry.SetIndex,
            Entry.BindingIndex, RhiDescriptor(Entry.DescriptorType),
            Entry.ArrayElement, RhiNativeClass(Entry.NativeClass),
            Entry.NativeIndex});
    for (const auto& Range : Source.ReservedRanges)
        Out.ReservedRanges.push_back({RhiStage(Range.Stage),
            RhiNativeClass(Range.NativeClass), Range.FirstIndex,
            Range.Count, Range.Purpose});
    for (const auto& Limit : Source.LimitSnapshot)
        Out.LimitSnapshot.push_back({RhiStage(Limit.Stage),
            RhiNativeClass(Limit.NativeClass), Limit.MaxCount});
    Out.CanonicalDigest.bAvailable = Source.CanonicalDigest.IsAvailable();
    Out.CanonicalDigest.Bytes = Source.CanonicalDigest.GetBytes();
    return RHI::IsCanonicalRHINativeBindingMap(Out);
}

bool BuildMetalShader(
    const std::filesystem::path& Path,
    Asset::EShaderStage Stage,
    const std::filesystem::path& Scratch,
    RHI::FRHIShaderModuleDesc& Out,
    Core::FString& OutReason)
{
    using namespace AssetCooker::Private;
    Out = {};
    const auto Interface = OutputTransformInterface(Stage);
    const auto SpirvBytes = ReadFileBytes(Path);
    FSpirvCrossMslRequest Derive;
    Derive.SpirvBytes = SpirvBytes;
    Derive.Stage = Stage;
    Derive.EntryPoint = "main";
    Derive.InterfaceBindings = Interface;
    FSpirvCrossMslResult Derived;
    if (DeriveMetalShaderSource(Derive, Derived) !=
        Asset::EAssetResult::Success)
    {
        OutReason = "output-transform-gpu-metal-derive";
        return false;
    }

    Asset::FAssetId ShaderId;
    if (Asset::FAssetId::Create(
            "ShaderProgram",
            Core::FString(std::string(
                "Engine/Shaders/PostProcess/GPUConformance/") +
                Path.filename().string()), {}, ShaderId) !=
        Asset::EAssetResult::Success)
    {
        OutReason = "output-transform-gpu-metal-identity";
        return false;
    }
    const Core::FString Target(
        std::string("metal-macos-12-") + HostArchitecture());
    FMetalShaderEvidence Evidence;
    Evidence.ShaderAssetId = std::move(ShaderId);
    Evidence.ShaderAssetVersion = Derived.SpirvDigest;
    Evidence.SpirvDigest = Derived.SpirvDigest;
    Evidence.Stage = Stage;
    Evidence.EntryPoint = "main";
    Evidence.InterfaceDigest = Derived.InterfaceDigest;
    Evidence.SpirvCrossOptionsDigest = Derived.OptionsDigest;
    Evidence.BindingEvidence = Derived.BindingEvidence;
    Evidence.TargetProfile = Target;
    Evidence.NormalizedMslDigest = Derived.NormalizedMslDigest;
    if (FinalizeMetalShaderEvidence(Evidence) !=
        Asset::EAssetResult::Success)
    {
        OutReason = "output-transform-gpu-metal-evidence";
        return false;
    }
    std::error_code Error;
    std::filesystem::remove_all(Scratch, Error);
    Error.clear();
    std::filesystem::create_directories(Scratch, Error);
    if (Error)
    {
        OutReason = "output-transform-gpu-metal-scratch";
        return false;
    }
    FMetalLibraryCompileRequest Compile;
    Compile.WorkingDirectory = Core::FString(Scratch.string());
    Compile.Architecture = HostArchitecture();
    Compile.TargetProfile = Target;
    Compile.NormalizedMsl = Derived.NormalizedMsl;
    Compile.DerivationEvidence = Evidence;
    const auto Library = FinalizeMetalLibrary(Compile);
    std::filesystem::remove_all(Scratch, Error);
    if (!Library.Succeeded())
    {
        OutReason = Library.StableReason;
        return false;
    }

    Out.Stage = RhiStage(Stage);
    Out.EntryPoint = "main";
    Out.Payload.Format = RHI::ERHIShaderPayloadFormat::MetalLibrary;
    Out.Payload.Bytes = Library.LibraryBytes;
    Out.Payload.PayloadIdentity = Core::FString(
        std::string("OutputTransformGPU/") + Path.filename().string());
    Out.Payload.TargetProfile = Target;
    Out.Payload.PayloadDigest = RHI::ComputeRHISha256(Out.Payload.Bytes);
    Out.ValidationMode = RHI::ERHIShaderBytecodeValidationMode::Runtime;
    Out.RuntimeMode = RHI::ERHIRuntimeObjectMode::RealRuntime;
    Out.DebugName = Out.Payload.PayloadIdentity;
    Out.InterfaceMetadata.DebugName = Out.DebugName;
    AddRhiInterfaceMetadata(Interface, Stage, Out);
    if (!ConvertBindingMap(Derived.BindingEvidence, Out.NativeBindingMap) ||
        !RHI::IsValidRHIShaderModuleDesc(Out))
    {
        OutReason = "output-transform-gpu-metal-rhi-shader";
        return false;
    }
    return true;
}

#endif

struct FGpuMatrixSummary
{
    bool bAvailable = false;
    bool bSucceeded = false;
    bool bComparedDecodedDomain = false;
    bool bComparedRawLinearHdrCodes = false;
    bool bRepeatStable = false;
    Core::uint32 ComparedSampleCount = 0;
    Core::uint32 FailedSampleCount = 0;
    Core::uint32 RepeatCount = 0;
    double MaximumEncodedCodeError = 0.0;
    double MaximumDecodedRgbError = 0.0;
    double MaximumDecodedXyzError = 0.0;
    Core::FString StableReason;
};

FGpuMatrixSummary RunGpuMatrix(EGpuBackend Backend,
    const std::vector<FGpuVectorSample>& Samples)
{
    FGpuMatrixSummary Summary;
    FOutputTransformGpuHarness Harness;
    if (!Harness.Initialize(Backend))
    {
        Summary.StableReason = Harness.GetReason();
        return Summary;
    }
    Summary.bAvailable = true;
    Summary.bComparedDecodedDomain = true;
    FOutputTransformSettingsValidator Validator;
    Core::uint32 PrintedFailures = 0;
    for (const auto& Sample : Samples)
    {
        const auto Resolved = Validator.Validate(SettingsForSample(Sample));
        const auto Gpu = Resolved.Succeeded()
            ? Harness.Execute(Resolved.Settings, Sample.SceneLinear)
            : FGpuExecutionResult{};
        FOutputTransformReferenceRgb Decoded;
        FOutputTransformReferenceXyz Xyz;
        const bool bDecoded = Gpu.bSucceeded && Resolved.Succeeded() &&
            DecodeGpuResult(Resolved.Settings, Gpu.Encoded, Decoded, Xyz);
        const double EncodedError = bDecoded ? std::max({
            std::abs(Gpu.Encoded.R - Sample.ExpectedEncoded.R),
            std::abs(Gpu.Encoded.G - Sample.ExpectedEncoded.G),
            std::abs(Gpu.Encoded.B - Sample.ExpectedEncoded.B)}) : 0.0;
        const double DecodedRgbError = bDecoded ? std::max({
            std::abs(Decoded.R - Sample.ExpectedDecoded.R),
            std::abs(Decoded.G - Sample.ExpectedDecoded.G),
            std::abs(Decoded.B - Sample.ExpectedDecoded.B)}) : 0.0;
        const double DecodedXyzError = bDecoded ? std::max({
            std::abs(Xyz.X - Sample.ExpectedXyz.X),
            std::abs(Xyz.Y - Sample.ExpectedXyz.Y),
            std::abs(Xyz.Z - Sample.ExpectedXyz.Z)}) : 0.0;
        Summary.MaximumEncodedCodeError = std::max(
            Summary.MaximumEncodedCodeError, EncodedError);
        Summary.MaximumDecodedRgbError = std::max(
            Summary.MaximumDecodedRgbError, DecodedRgbError);
        Summary.MaximumDecodedXyzError = std::max(
            Summary.MaximumDecodedXyzError, DecodedXyzError);
        const bool bPassed = bDecoded &&
            WithinTolerance(Decoded.R, Sample.ExpectedDecoded.R,
                Sample.DecodedRgbTolerance.R) &&
            WithinTolerance(Decoded.G, Sample.ExpectedDecoded.G,
                Sample.DecodedRgbTolerance.G) &&
            WithinTolerance(Decoded.B, Sample.ExpectedDecoded.B,
                Sample.DecodedRgbTolerance.B) &&
            WithinTolerance(Xyz.X, Sample.ExpectedXyz.X,
                Sample.DecodedXyzTolerance.X) &&
            WithinTolerance(Xyz.Y, Sample.ExpectedXyz.Y,
                Sample.DecodedXyzTolerance.Y) &&
            WithinTolerance(Xyz.Z, Sample.ExpectedXyz.Z,
                Sample.DecodedXyzTolerance.Z) &&
            WithinTolerance(Gpu.Alpha, 1.0, 1.0e-6);
        ++Summary.ComparedSampleCount;
        if (!bPassed)
        {
            ++Summary.FailedSampleCount;
            if (PrintedFailures < 8)
            {
                ++PrintedFailures;
                std::cout << "[INFO] output-transform-gpu-failure backend="
                          << (Backend == EGpuBackend::Vulkan
                              ? "Vulkan" : "Metal")
                          << " case=" << Sample.CaseId.CStr()
                          << " profile=" << Sample.ProfileId.CStr()
                          << " encoding=" << Sample.EncodingId.CStr()
                          << " reason=" << Gpu.StableReason.CStr()
                          << " encoded=" << Gpu.Encoded.R << ','
                          << Gpu.Encoded.G << ',' << Gpu.Encoded.B
                          << " decoded-error=" << DecodedRgbError
                          << " xyz-error=" << DecodedXyzError << '\n';
            }
        }
    }

    const auto Probe = std::find_if(Samples.begin(), Samples.end(),
        [](const FGpuVectorSample& Sample)
        {
            return Sample.CaseId == Core::FString("neutral-e0") &&
                Sample.ProfileId ==
                    Core::FString("Hdr.PQ.Rec2020.1000.v1");
        });
    if (Probe != Samples.end())
    {
        const auto Resolved = Validator.Validate(SettingsForSample(*Probe));
        FGpuExecutionResult Baseline;
        Summary.bRepeatStable = Resolved.Succeeded();
        for (Core::uint32 Repeat = 0;
             Repeat < 20 && Summary.bRepeatStable; ++Repeat)
        {
            const auto Gpu = Harness.Execute(
                Resolved.Settings, Probe->SceneLinear);
            if (Repeat == 0)
            {
                Baseline = Gpu;
                Summary.bRepeatStable = Gpu.bSucceeded;
            }
            else
            {
                Summary.bRepeatStable = Gpu.bSucceeded &&
                    Gpu.Encoded.R == Baseline.Encoded.R &&
                    Gpu.Encoded.G == Baseline.Encoded.G &&
                    Gpu.Encoded.B == Baseline.Encoded.B &&
                    Gpu.Alpha == Baseline.Alpha;
            }
            ++Summary.RepeatCount;
        }
    }
    Summary.bSucceeded = Summary.ComparedSampleCount == Samples.size() &&
        Summary.FailedSampleCount == 0 && Summary.RepeatCount == 20 &&
        Summary.bRepeatStable && Summary.bComparedDecodedDomain &&
        !Summary.bComparedRawLinearHdrCodes;
    Summary.StableReason = Summary.bSucceeded
        ? Core::FString("output-transform-gpu-matrix-passed")
        : Core::FString("output-transform-gpu-matrix-failed");
    return Summary;
}

} // namespace

FOutputTransformGPUConformanceTestResult
RunOutputTransformGPUConformanceTests()
{
    FOutputTransformGPUConformanceTestResult Result;
    const auto Frozen = Private::LoadFrozenOutputTransformConformanceAuthority();
    Record(Result, Frozen.IsValid() && Frozen.CaseCount == 32 &&
        Frozen.ExpectationCount == 224 && !Frozen.bCanGenerateExpectedValues,
        "GPU conformance loads the checked-in vector authority read-only");

    Stoner::Core::uint32 ExpectationCount = 0;
    Stoner::Core::uint32 NativeEncodingCount = 0;
    std::vector<FGpuVectorSample> Samples;
    Record(Result, ValidateFrozenVectorAuthority(
            ExpectationCount, NativeEncodingCount, &Samples) &&
            ExpectationCount == Frozen.ExpectationCount &&
            NativeEncodingCount == 288 && Samples.size() == 288,
        "checked-in expectations drive decoded conformance without regeneration");

    FOutputTransformSettingsValidator Validator;
    constexpr std::array<const char*, 3> SdrProfiles = {
        "Sdr.sRGB.v1", "Sdr.BT709.v1", "Sdr.ExplicitGamma22.v1"};
    bool bSdrReports = true;
    for (const char* Profile : SdrProfiles)
    {
        FOutputTransformSettings Settings;
        Settings.OutputDeviceProfileId = Profile;
        const auto Resolved = Validator.Validate(Settings);
        const auto Report = Resolved.Succeeded()
            ? Private::EvaluateOutputTransformConformance(
                Resolved.Settings, {0.18, 1.0, 8.0})
            : Private::FOutputTransformConformanceReport{};
        bSdrReports = bSdrReports && Report.IsValid() &&
            Report.bComparedDecodedDomain &&
            Report.ComparisonDomain == EOutputComparisonDomain::LinearRec709 &&
            Report.DecodedTolerance.R == 2.0 / 255.0;
    }
    Record(Result, bSdrReports,
        "offscreen SDR reports compare decoded linear Rec709 rather than encoded bytes");

    constexpr std::array<RHI::ERHIPresentationNativeEncoding, 2> LinearEncodings = {
        RHI::ERHIPresentationNativeEncoding::ScRgb80,
        RHI::ERHIPresentationNativeEncoding::MetalEdr};
    bool bLinearHdr = true;
    for (const auto Encoding : LinearEncodings)
    {
        FOutputTransformSettings Settings;
        Settings.DynamicRange = EOutputDynamicRange::HDR;
        Settings.OutputDeviceProfileId = "Hdr.Linear.1000.v1";
        Settings.PreferredNativeEncoding = Encoding;
        Settings.NativeReferenceWhiteNits = Encoding ==
            RHI::ERHIPresentationNativeEncoding::MetalEdr ? 100.0f : 0.0f;
        const auto Resolved = Validator.Validate(Settings);
        const auto Report = Resolved.Succeeded()
            ? Private::EvaluateOutputTransformConformance(
                Resolved.Settings, {0.18, 0.18, 0.18})
            : Private::FOutputTransformConformanceReport{};
        bLinearHdr = bLinearHdr && Report.IsValid() &&
            Report.bComparedDecodedDomain &&
            !Report.bComparedRawLinearHdrCodes &&
            Report.ComparisonDomain ==
                EOutputComparisonDomain::AbsoluteNitsXyz &&
            Report.DecodedTolerance.R >= 0.02;
    }
    Record(Result, bLinearHdr,
        "scRGB80 and Metal EDR conformance decodes to nits and never cross-compares raw codes");

    FOutputTransformSettings Pq;
    Pq.DynamicRange = EOutputDynamicRange::HDR;
    Pq.OutputDeviceProfileId = "Hdr.PQ.Rec2020.1000.v1";
    Pq.PreferredNativeEncoding = RHI::ERHIPresentationNativeEncoding::Pq;
    const auto PqResolved = Validator.Validate(Pq);
    const auto PqReport = PqResolved.Succeeded()
        ? Private::EvaluateOutputTransformConformance(
            PqResolved.Settings, {0.5, 0.4, 0.3})
        : Private::FOutputTransformConformanceReport{};
    Record(Result, PqReport.IsValid() && PqReport.bComparedDecodedDomain &&
        PqReport.StorageEncoding ==
            RHI::ERHIPresentationNativeEncoding::Pq &&
        PqReport.DecodedTolerance.R >= 0.02 &&
        std::isfinite(PqReport.ExpectedXyz.X),
        "PQ conformance reports encoded storage and decoded nits XYZ tolerances");

#if STONER_TEST_VULKAN_RUNTIME_AVAILABLE
    const auto Vulkan = RunGpuMatrix(EGpuBackend::Vulkan, Samples);
    std::cout << "[EVIDENCE] output-transform-gpu backend=Vulkan"
              << " samples=" << Vulkan.ComparedSampleCount
              << " repeats=" << Vulkan.RepeatCount
              << " max-encoded-code-error="
              << Vulkan.MaximumEncodedCodeError
              << " max-decoded-rgb-error="
              << Vulkan.MaximumDecodedRgbError
              << " max-decoded-xyz-error="
              << Vulkan.MaximumDecodedXyzError
              << " raw-linear-hdr-cross-compare="
              << Vulkan.bComparedRawLinearHdrCodes
              << " reason=" << Vulkan.StableReason.CStr() << '\n';
    Record(Result, Vulkan.bAvailable && Vulkan.bSucceeded &&
        Vulkan.ComparedSampleCount == 288 && Vulkan.RepeatCount == 20,
        "Vulkan executes the full frozen three-stage shader matrix with decoded-domain readback");
#else
    Record(Result, true,
        "Vulkan output-transform GPU matrix is build-time host-gated when the Vulkan runtime is unavailable");
#endif

#if SG_PLATFORM_MAC
    const auto Metal = RunGpuMatrix(EGpuBackend::Metal, Samples);
    std::cout << "[EVIDENCE] output-transform-gpu backend=Metal"
              << " samples=" << Metal.ComparedSampleCount
              << " repeats=" << Metal.RepeatCount
              << " max-encoded-code-error="
              << Metal.MaximumEncodedCodeError
              << " max-decoded-rgb-error="
              << Metal.MaximumDecodedRgbError
              << " max-decoded-xyz-error="
              << Metal.MaximumDecodedXyzError
              << " raw-linear-hdr-cross-compare="
              << Metal.bComparedRawLinearHdrCodes
              << " reason=" << Metal.StableReason.CStr() << '\n';
    Record(Result, Metal.bAvailable && Metal.bSucceeded &&
        Metal.ComparedSampleCount == 288 && Metal.RepeatCount == 20,
        "Metal executes the full frozen three-stage shader matrix with decoded-domain readback");
#else
    Record(Result, true,
        "Metal output-transform GPU matrix is host-gated off macOS");
#endif
    return Result;
}
