#include "VulkanNativeIntegrationTests.h"
#include "RendererStaticMeshTestSupport.h"
#include "ShaderTestFixtures.h"

#include "Renderer/FStaticMeshRealization.h"
#include "VulkanRHI/FVulkanComputePipeline.h"
#include "VulkanRHI/FVulkanDevice.h"
#include "VulkanRHI/FVulkanGraphicsPipeline.h"
#include "VulkanRHI/FVulkanNativeContext.h"
#include "VulkanRHI/FVulkanShaderModule.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <memory>
#include <utility>

namespace
{

void Record(FVulkanNativeIntegrationTestResult& Result, bool bPassed, const char* Name)
{
    if (bPassed) { ++Result.Passed; std::cout << "[PASS] " << Name << '\n'; }
    else { ++Result.Failed; std::cout << "[FAIL] " << Name << '\n'; }
}

Stoner::Core::TArray<Stoner::Core::uint32> ReadShaderWords(
    const char* Path)
{
    std::ifstream Stream(Path, std::ios::binary | std::ios::ate);
    if (!Stream)
    {
        return {};
    }
    const std::streamsize Size = Stream.tellg();
    if (Size <= 0 || (Size % 4) != 0)
    {
        return {};
    }
    Stoner::Core::TArray<Stoner::Core::uint32> Words(
        static_cast<std::size_t>(Size) /
        sizeof(Stoner::Core::uint32));
    Stream.seekg(0);
    Stream.read(
        reinterpret_cast<char*>(Words.data()),
        Size);
    return Stream.good() ? Words
                         : Stoner::Core::TArray<Stoner::Core::uint32>{};
}

Stoner::RHI::FRHIShaderModuleDesc NativeShaderDesc(
    Stoner::RHI::ERHIShaderStage Stage,
    const char* EntryPoint,
    const char* Identity,
    Stoner::Core::TArray<Stoner::Core::uint32> Words)
{
    Stoner::RHI::FRHIShaderModuleDesc Desc;
    Desc.Stage = Stage;
    Desc.EntryPoint = EntryPoint;
    (void)Stoner::RHI::SetRHIShaderSpirvWords(
        Desc.Payload, Words, Identity);
    return Desc;
}

float HalfToFloat(Stoner::Core::uint16 Value)
{
    const Stoner::Core::uint32 Sign =
        static_cast<Stoner::Core::uint32>(Value & 0x8000U) << 16U;
    Stoner::Core::uint32 Exponent = (Value >> 10U) & 0x1fU;
    Stoner::Core::uint32 Mantissa = Value & 0x03ffU;
    Stoner::Core::uint32 Bits = 0;
    if (Exponent == 0)
    {
        if (Mantissa == 0)
        {
            Bits = Sign;
        }
        else
        {
            Exponent = 1;
            while ((Mantissa & 0x0400U) == 0)
            {
                Mantissa <<= 1U;
                --Exponent;
            }
            Mantissa &= 0x03ffU;
            Bits = Sign |
                ((Exponent + 112U) << 23U) |
                (Mantissa << 13U);
        }
    }
    else if (Exponent == 31)
    {
        Bits = Sign | 0x7f800000U | (Mantissa << 13U);
    }
    else
    {
        Bits = Sign |
            ((Exponent + 112U) << 23U) |
            (Mantissa << 13U);
    }
    float Result = 0.0f;
    std::memcpy(&Result, &Bits, sizeof(Result));
    return Result;
}

} // namespace

FVulkanNativeIntegrationTestResult RunVulkanNativeIntegrationTests()
{
    using namespace Stoner::Backend::Vulkan;
    using namespace Stoner::RHI;
    FVulkanNativeIntegrationTestResult Result;
    constexpr std::array VisibleFailurePoints = {
        EVulkanVisibleFrameFailurePoint::AcquireSuboptimal,
        EVulkanVisibleFrameFailurePoint::Record,
        EVulkanVisibleFrameFailurePoint::SubmitAfterFenceReset};
    bool bVisibleFailureLifecycleStable = true;
    for (EVulkanVisibleFrameFailurePoint FailurePoint : VisibleFailurePoints)
    {
        const FVulkanVisibleFrameFailureReport Report =
            FVulkanNativeContext::RunVisibleFrameFailureLifecycleValidation(
                FailurePoint);
        bVisibleFailureLifecycleStable =
            bVisibleFailureLifecycleStable &&
            Report.InjectedFailure == FailurePoint &&
            Report.bAcquiredStateReleased &&
            Report.bFenceReadyForReuse &&
            Report.NextAcquireResult == ERHIResult::Success &&
            Report.bPassed;
    }
    Record(Result, bVisibleFailureLifecycleStable,
        "Vulkan visible frame failure lifecycle releases acquired state and reusable fences");

    FVulkanNativeContext Context;
    const ERHIResult InitializeResult = Context.Initialize(ERHIRuntimeMode::NativeHeadless);
    if (InitializeResult == ERHIResult::Unsupported || InitializeResult == ERHIResult::Unavailable)
    {
        const char* Required = std::getenv("STONER_REQUIRE_STATIC_MESH_NATIVE");
        Record(Result, Required == nullptr || std::strcmp(Required, "1") != 0,
            "Vulkan native integration reports unavailable runtime explicitly");
        return Result;
    }
    Record(Result, InitializeResult == ERHIResult::Success && Context.GetSnapshot().ProvesNativeExecution(),
        "Vulkan native integration creates real instance and device");
    const FRHIShaderModuleDesc TriangleVertex = NativeShaderDesc(
        ERHIShaderStage::Vertex,
        "main",
        "ShaderPayload:Engine/Shaders/Triangle#payload.vulkan.vertex",
        ReadShaderWords("Content/Shaders/Triangle/Triangle.vert.spv"));
    const FRHIShaderModuleDesc TriangleFragment = NativeShaderDesc(
        ERHIShaderStage::Fragment,
        "main",
        "ShaderPayload:Engine/Shaders/Triangle#payload.vulkan.fragment",
        ReadShaderWords("Content/Shaders/Triangle/Triangle.frag.spv"));
    const bool bIndexedReadback = Context.ExecuteOffscreenTriangle(
        TriangleVertex, TriangleFragment) == ERHIResult::Success;
    Record(Result, bIndexedReadback,
        "Vulkan native integration performs indexed clockwise-culling attachment readback");
    Record(Result, Context.GetSnapshot().GetTotalLiveObjectCount() == 2,
        "Vulkan native integration releases frame-local resources after completion");
    Record(Result, Context.Shutdown() == ERHIResult::Success && Context.GetSnapshot().GetTotalLiveObjectCount() == 0,
        "Vulkan native integration shutdown reaches zero live objects");

    auto StaticMeshDevice = Stoner::Core::MakeShared<FVulkanDevice>();
    FVulkanInstanceDesc StaticMeshDeviceDesc;
    StaticMeshDeviceDesc.RuntimeMode = EVulkanInstanceRuntimeMode::DeterministicFallback;
    StaticMeshDeviceDesc.bRequestValidation = false;
    const auto StaticMeshAsset = Stoner::Tests::StaticMesh::MakeAsset();
    const bool bStaticMeshDeviceReady =
        StaticMeshDevice->Initialize(StaticMeshDeviceDesc) == ERHIResult::Success &&
        StaticMeshDevice->EnableNativeShaderRuntime() == ERHIResult::Success;
    const Stoner::Renderer::FStaticMeshRealizationResult StaticMeshRealization =
        bStaticMeshDeviceReady
        ? Stoner::Renderer::FStaticMeshRealizer::Realize(
              {StaticMeshDevice, StaticMeshAsset, {}})
        : Stoner::Renderer::FStaticMeshRealizationResult{};
    const bool bStaticMeshTransfer = StaticMeshRealization.Succeeded() &&
            StaticMeshDevice->GetTrackedUploadRequestCount() == 2 &&
            StaticMeshRealization.Snapshot->Sections.size() == 2;
    Record(Result, bStaticMeshTransfer,
        "Renderer static mesh realization reaches Vulkan buffer transfer path");
    Record(Result,
        StaticMeshDevice->Shutdown() == ERHIResult::Success,
        "Vulkan static mesh realization releases device-owned resources");

    FVulkanDevice ShaderDevice;
    FVulkanInstanceDesc DeviceDesc;
    DeviceDesc.RuntimeMode = EVulkanInstanceRuntimeMode::DeterministicFallback;
    DeviceDesc.bRequestValidation = false;
    Record(Result,
        ShaderDevice.Initialize(DeviceDesc) == ERHIResult::Success &&
            ShaderDevice.EnableNativeShaderRuntime() == ERHIResult::Success &&
            ShaderDevice.HasNativeShaderRuntime(),
        "Vulkan RHI device enables an owner-safe native shader runtime");

    FRHIShaderModuleDesc ShaderDesc;
    ShaderDesc.Stage = ERHIShaderStage::Vertex;
    ShaderDesc.EntryPoint = "NativeVS";
    ShaderDesc.Payload = Stoner::Tests::MakeMinimalShaderPayload(
        ERHIShaderStage::Vertex, "NativeVS", "native-rhi-shader");
    const auto NativeShader = ShaderDevice.CreateShaderModule(ShaderDesc);
    auto ConcreteShader =
        std::dynamic_pointer_cast<FVulkanShaderModule>(NativeShader.Object);
    Record(Result,
        NativeShader.Succeeded() && ConcreteShader &&
            ConcreteShader->GetRuntimeMode() ==
                ERHIRuntimeObjectMode::RealRuntime &&
            ConcreteShader->GetValidationMode() ==
                ERHIShaderBytecodeValidationMode::Runtime &&
            ConcreteShader->HasNativeObject(),
        "Vulkan RHI shader factory retains a real native shader module");

    FRHIShaderModuleDesc WrongStage = ShaderDesc;
    WrongStage.Stage = ERHIShaderStage::Fragment;
    Record(Result,
        ShaderDevice.CreateShaderModule(WrongStage).Result ==
            ERHIResult::InvalidState,
        "Vulkan native shader factory rejects execution-model mismatch before runtime creation");

    FRHIPipelineLayoutDesc PipelineLayoutDesc;
    PipelineLayoutDesc.Bindings = {{
        0,
        0,
        ERHIDescriptorType::UniformBuffer,
        1,
        ERHIShaderStageFlags::Vertex |
            ERHIShaderStageFlags::Fragment |
            ERHIShaderStageFlags::Compute}};
    const auto PipelineLayout =
        ShaderDevice.CreatePipelineLayout(PipelineLayoutDesc);
    const auto NativeVertex = ShaderDevice.CreateShaderModule(
        NativeShaderDesc(
            ERHIShaderStage::Vertex,
            "main",
            "native-pipeline-vs",
            ReadShaderWords(
                "Content/Shaders/Triangle/Triangle.vert.spv")));
    const auto NativeFragment = ShaderDevice.CreateShaderModule(
        NativeShaderDesc(
            ERHIShaderStage::Fragment,
            "main",
            "native-pipeline-fs",
            ReadShaderWords(
                "Content/Shaders/Triangle/Triangle.frag.spv")));
    const auto NativeCompute = ShaderDevice.CreateShaderModule(
        NativeShaderDesc(
            ERHIShaderStage::Compute,
            "NativeCS",
            "native-pipeline-cs",
            Stoner::Tests::MakeMinimalShaderBytecode(
                ERHIShaderStage::Compute, "NativeCS")));
    FRHIGraphicsPipelineDesc GraphicsDesc;
    GraphicsDesc.PipelineLayout = PipelineLayout.Object;
    GraphicsDesc.ShaderModules = {
        NativeVertex.Object, NativeFragment.Object};
    GraphicsDesc.VertexInput.Stride = sizeof(float) * 5;
    GraphicsDesc.VertexInput.Attributes = {
        {0, ERHIFormat::R32G32_Float, 0},
        {1, ERHIFormat::R32G32B32_Float, sizeof(float) * 2}};
    GraphicsDesc.RenderTargets.ColorFormats = {
        ERHIFormat::R8G8B8A8_UNorm};
    const auto NativeGraphicsPipeline =
        ShaderDevice.CreateGraphicsPipeline(GraphicsDesc);
    FRHIComputePipelineDesc ComputeDesc;
    ComputeDesc.PipelineLayout = PipelineLayout.Object;
    ComputeDesc.ShaderModules = {NativeCompute.Object};
    const auto NativeComputePipeline =
        ShaderDevice.CreateComputePipeline(ComputeDesc);
    auto ConcreteGraphicsPipeline =
        std::dynamic_pointer_cast<FVulkanGraphicsPipeline>(
            NativeGraphicsPipeline.Object);
    auto ConcreteComputePipeline =
        std::dynamic_pointer_cast<FVulkanComputePipeline>(
            NativeComputePipeline.Object);
    Record(Result,
        PipelineLayout.Succeeded() &&
            NativeVertex.Succeeded() &&
            NativeFragment.Succeeded() &&
            NativeCompute.Succeeded() &&
            NativeGraphicsPipeline.Succeeded() &&
            NativeComputePipeline.Succeeded() &&
            ConcreteGraphicsPipeline &&
            ConcreteComputePipeline &&
            ConcreteGraphicsPipeline->GetRuntimeMode() ==
                ERHIRuntimeObjectMode::RealRuntime &&
            ConcreteComputePipeline->GetRuntimeMode() ==
                ERHIRuntimeObjectMode::RealRuntime &&
            ConcreteGraphicsPipeline->HasNativeObject() &&
            ConcreteComputePipeline->HasNativeObject(),
        "Vulkan RHI pipeline factories retain real native graphics and compute pipelines");

    FRHITextureDesc UNormDesc;
    UNormDesc.Width = 2;
    UNormDesc.Height = 2;
    UNormDesc.Format = ERHIFormat::R8G8B8A8_sRGB;
    UNormDesc.Usage =
        ERHITextureUsage::Sampled |
        ERHITextureUsage::CopySource |
        ERHITextureUsage::CopyDestination;
    const auto NativeUNormTexture =
        ShaderDevice.CreateTexture(UNormDesc);
    const std::array<unsigned char, 16> UNormPixels = {
        1, 2, 3, 255,
        17, 18, 19, 255,
        33, 34, 35, 255,
        49, 50, 51, 255};
    FRHITextureUploadDesc UNormUpload;
    UNormUpload.Width = 2;
    UNormUpload.Height = 2;
    UNormUpload.RowPitchBytes = 8;
    UNormUpload.Data = UNormPixels.data();
    UNormUpload.DataSizeBytes = UNormPixels.size();
    Stoner::Core::TArray<Stoner::Core::uint8> UNormReadback;
    bool bUNormWithinTolerance = NativeUNormTexture.Succeeded() &&
        ShaderDevice.UploadTexture(
            NativeUNormTexture.Object, UNormUpload) ==
            ERHIResult::Success &&
        ShaderDevice.ReadbackTextureForTesting(
            NativeUNormTexture.Object, 0, UNormReadback) ==
            ERHIResult::Success &&
        UNormReadback.size() == UNormPixels.size();
    if (bUNormWithinTolerance)
    {
        for (std::size_t Index = 0;
             Index < UNormPixels.size();
             ++Index)
        {
            bUNormWithinTolerance =
                bUNormWithinTolerance &&
                std::abs(
                    static_cast<int>(UNormReadback[Index]) -
                    static_cast<int>(UNormPixels[Index])) <= 1;
        }
    }
    Record(Result, bUNormWithinTolerance,
        "Vulkan native RGBA8 sRGB upload and readback stays within one LSB");

    FRHITextureDesc FP16Desc = UNormDesc;
    FP16Desc.Width = 1;
    FP16Desc.Height = 1;
    FP16Desc.Format = ERHIFormat::R16G16B16A16_Float;
    const auto NativeFP16Texture =
        ShaderDevice.CreateTexture(FP16Desc);
    const std::array<Stoner::Core::uint16, 4> FP16Pixels = {
        0x3c00U, 0x3800U, 0x0000U, 0x4000U};
    FRHITextureUploadDesc FP16Upload;
    FP16Upload.Width = 1;
    FP16Upload.Height = 1;
    FP16Upload.RowPitchBytes = sizeof(FP16Pixels);
    FP16Upload.Data = FP16Pixels.data();
    FP16Upload.DataSizeBytes = sizeof(FP16Pixels);
    Stoner::Core::TArray<Stoner::Core::uint8> FP16Readback;
    bool bFP16WithinTolerance = NativeFP16Texture.Succeeded() &&
        ShaderDevice.UploadTexture(
            NativeFP16Texture.Object, FP16Upload) ==
            ERHIResult::Success &&
        ShaderDevice.ReadbackTextureForTesting(
            NativeFP16Texture.Object, 0, FP16Readback) ==
            ERHIResult::Success &&
        FP16Readback.size() == sizeof(FP16Pixels);
    if (bFP16WithinTolerance)
    {
        for (std::size_t Index = 0; Index < FP16Pixels.size(); ++Index)
        {
            Stoner::Core::uint16 ObservedBits = 0;
            std::memcpy(
                &ObservedBits,
                FP16Readback.data() + Index * sizeof(ObservedBits),
                sizeof(ObservedBits));
            const float Expected = HalfToFloat(FP16Pixels[Index]);
            const float Observed = HalfToFloat(ObservedBits);
            const float Tolerance =
                std::max(1.0e-3f, std::abs(Expected) * 1.0e-3f);
            bFP16WithinTolerance =
                bFP16WithinTolerance &&
                std::abs(Observed - Expected) <= Tolerance;
        }
    }
    Record(Result, bFP16WithinTolerance,
        "Vulkan native RGBA16F upload and readback meets FP16 tolerance");

    FRHITextureDesc FP32Desc = UNormDesc;
    FP32Desc.Width = 1;
    FP32Desc.Height = 1;
    FP32Desc.Format = ERHIFormat::R32G32B32A32_Float;
    const auto NativeFP32Texture =
        ShaderDevice.CreateTexture(FP32Desc);
    const std::array<float, 4> FP32Pixels = {
        1.0f, 0.5f, 2.0f, -1.0f};
    FRHITextureUploadDesc FP32Upload;
    FP32Upload.Width = 1;
    FP32Upload.Height = 1;
    FP32Upload.RowPitchBytes = sizeof(FP32Pixels);
    FP32Upload.Data = FP32Pixels.data();
    FP32Upload.DataSizeBytes = sizeof(FP32Pixels);
    Stoner::Core::TArray<Stoner::Core::uint8> FP32Readback;
    bool bFP32WithinTolerance = NativeFP32Texture.Succeeded() &&
        ShaderDevice.UploadTexture(
            NativeFP32Texture.Object, FP32Upload) ==
            ERHIResult::Success &&
        ShaderDevice.ReadbackTextureForTesting(
            NativeFP32Texture.Object, 0, FP32Readback) ==
            ERHIResult::Success &&
        FP32Readback.size() == sizeof(FP32Pixels);
    if (bFP32WithinTolerance)
    {
        for (std::size_t Index = 0; Index < FP32Pixels.size(); ++Index)
        {
            float Observed = 0.0f;
            std::memcpy(
                &Observed,
                FP32Readback.data() + Index * sizeof(Observed),
                sizeof(Observed));
            const float Tolerance =
                std::max(
                    1.0e-6f,
                    std::abs(FP32Pixels[Index]) * 1.0e-6f);
            bFP32WithinTolerance =
                bFP32WithinTolerance &&
                std::abs(Observed - FP32Pixels[Index]) <=
                    Tolerance;
        }
    }
    Record(Result, bFP32WithinTolerance,
        "Vulkan native RGBA32F upload and readback meets FP32 tolerance");

    constexpr std::array CompressedCandidates = {
        ERHIFormat::BC7_RGBA_UNorm,
        ERHIFormat::BC3_RGBA_UNorm,
        ERHIFormat::ASTC_4x4_RGBA_UNorm,
        ERHIFormat::ETC2_RGBA8_UNorm};
    constexpr auto RequiredCompressedUsage =
        ERHIFormatCapability::SampledImage |
        ERHIFormatCapability::CopySource |
        ERHIFormatCapability::CopyDestination;
    ERHIFormat NativeCompressedFormat = ERHIFormat::Unknown;
    for (ERHIFormat Candidate : CompressedCandidates)
    {
        if (ShaderDevice.GetCapabilities().SupportsFormatUsage(
                Candidate, RequiredCompressedUsage))
        {
            NativeCompressedFormat = Candidate;
            break;
        }
    }

    bool bCompressedCapabilityAgrees = false;
    if (NativeCompressedFormat == ERHIFormat::Unknown)
    {
        FRHITextureDesc UnsupportedCompressedDesc = UNormDesc;
        UnsupportedCompressedDesc.Width = 7;
        UnsupportedCompressedDesc.Height = 5;
        UnsupportedCompressedDesc.Format =
            ERHIFormat::BC7_RGBA_UNorm;
        bCompressedCapabilityAgrees =
            !ShaderDevice.GetCapabilities().SupportsFormatUsage(
                UnsupportedCompressedDesc.Format,
                RequiredCompressedUsage) &&
            ShaderDevice.CreateTexture(
                UnsupportedCompressedDesc).Result ==
                ERHIResult::Unsupported;
    }
    else
    {
        FRHITextureDesc CompressedDesc = UNormDesc;
        CompressedDesc.Width = 7;
        CompressedDesc.Height = 5;
        CompressedDesc.Format = NativeCompressedFormat;
        FRHITextureFootprint CompressedFootprint;
        const bool bFootprintValid =
            TryGetRHITextureFootprint(
                CompressedDesc.Format,
                CompressedDesc.Width,
                CompressedDesc.Height,
                CompressedDesc.Depth,
                CompressedFootprint);
        Stoner::Core::TArray<Stoner::Core::uint8>
            CompressedBlocks;
        if (bFootprintValid)
        {
            CompressedBlocks.resize(
                static_cast<std::size_t>(
                    CompressedFootprint.TotalBytes));
            for (std::size_t Index = 0;
                 Index < CompressedBlocks.size();
                 ++Index)
            {
                CompressedBlocks[Index] =
                    static_cast<unsigned char>(
                        (Index * 17U + 3U) & 0xffU);
            }
        }
        const auto CompressedTexture =
            ShaderDevice.CreateTexture(CompressedDesc);
        FRHITextureUploadDesc CompressedUpload;
        CompressedUpload.Width = CompressedDesc.Width;
        CompressedUpload.Height = CompressedDesc.Height;
        CompressedUpload.RowPitchBytes =
            CompressedFootprint.TightRowBytes;
        CompressedUpload.Data = CompressedBlocks.data();
        CompressedUpload.DataSizeBytes =
            CompressedBlocks.size();
        Stoner::Core::TArray<Stoner::Core::uint8>
            CompressedReadback;
        bCompressedCapabilityAgrees =
            bFootprintValid &&
            CompressedTexture.Succeeded() &&
            ShaderDevice.UploadTexture(
                CompressedTexture.Object,
                CompressedUpload) == ERHIResult::Success &&
            ShaderDevice.ReadbackTextureForTesting(
                CompressedTexture.Object,
                0,
                CompressedReadback) == ERHIResult::Success &&
            CompressedReadback == CompressedBlocks;
    }
    Record(
        Result,
        bCompressedCapabilityAgrees,
        "Vulkan native compressed capability agrees with create upload and raw-block readback");

    const auto ShutdownShader = ShaderDevice.CreateShaderModule(ShaderDesc);
    Record(Result,
        ConcreteShader->Invalidate() == ERHIResult::Success &&
            !ConcreteShader->HasNativeObject() &&
            ConcreteGraphicsPipeline &&
            ConcreteGraphicsPipeline->Invalidate() ==
                ERHIResult::Success &&
            !ConcreteGraphicsPipeline->HasNativeObject() &&
            ShutdownShader.Succeeded() &&
            ShaderDevice.Shutdown() == ERHIResult::Success &&
            ShutdownShader.Object->GetLifecycleState() ==
                ERHIResourceLifecycleState::Invalidated &&
            NativeComputePipeline.Object &&
            NativeComputePipeline.Object->GetLifecycleState() ==
                ERHIResourceLifecycleState::Invalidated &&
            !ShaderDevice.HasNativeShaderRuntime(),
        "Vulkan RHI device destroys native shader and pipeline ownership on explicit invalidation and shutdown");
    if (const char* ReportPath =
            std::getenv("STONER_STATIC_MESH_NATIVE_REPORT"))
    {
        const std::filesystem::path Path(ReportPath);
        if (Path.has_parent_path())
            std::filesystem::create_directories(Path.parent_path());
        std::ofstream Report(Path, std::ios::binary | std::ios::trunc);
        Report << "feature=024-static-mesh-model\n"
               << "runtime=vulkan-native\n"
               << "indexed-clockwise-attachment-readback="
               << (bIndexedReadback ? "pass" : "fail") << '\n'
               << "renderer-buffer-transfer="
               << (bStaticMeshTransfer ? "pass" : "fail") << '\n'
               << "suite-passed=" << (Result.Failed == 0 ? "yes" : "no")
               << '\n';
    }
    return Result;
}
