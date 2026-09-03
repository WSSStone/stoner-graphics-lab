#include "MetalDeferredNativeProbe.h"

#include "Asset/AssetMinimal.h"
#include "Core/SGPlatform.h"
#include "FMetalLibraryCompiler.h"
#include "FSpirvCrossMslDeriver.h"
#include "MetalRHI/FMetalDeviceFactory.h"
#include "Renderer/FDeferredFrameExecutor.h"
#include "Renderer/FDeferredRenderer.h"
#include "RHI/RHIMinimal.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <span>
#include <string>
#include <tuple>

namespace
{

using namespace Stoner;
using namespace Stoner::Asset;
using namespace Stoner::AssetCooker::Private;
using namespace Stoner::Backend::Metal;
using namespace Stoner::Core;
using namespace Stoner::Renderer;
using namespace Stoner::RHI;

#if SG_PLATFORM_MAC

constexpr uint32 Extent = 16;

const char* HostArchitecture()
{
#if defined(__aarch64__) || defined(__arm64__)
    return "arm64";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#else
    return "unsupported";
#endif
}

EShaderStage AssetStage(ERHIShaderStage Stage)
{
    return Stage == ERHIShaderStage::Vertex
        ? EShaderStage::Vertex : EShaderStage::Fragment;
}

ERHIDescriptorType Descriptor(EShaderResourceKind Kind)
{
    switch (Kind)
    {
    case EShaderResourceKind::UniformBuffer:
        return ERHIDescriptorType::UniformBuffer;
    case EShaderResourceKind::SampledTexture:
        return ERHIDescriptorType::SampledTexture;
    case EShaderResourceKind::Sampler:
        return ERHIDescriptorType::Sampler;
    case EShaderResourceKind::StorageBuffer:
        return ERHIDescriptorType::StorageBuffer;
    case EShaderResourceKind::StorageTexture:
        return ERHIDescriptorType::StorageTexture;
    case EShaderResourceKind::CombinedTextureSampler:
        return ERHIDescriptorType::CombinedTextureSampler;
    }
    return ERHIDescriptorType::UniformBuffer;
}

ERHINativeResourceClass NativeClass(EShaderNativeResourceClass Value)
{
    switch (Value)
    {
    case EShaderNativeResourceClass::Buffer:
        return ERHINativeResourceClass::Buffer;
    case EShaderNativeResourceClass::Texture:
        return ERHINativeResourceClass::Texture;
    case EShaderNativeResourceClass::Sampler:
        return ERHINativeResourceClass::Sampler;
    }
    return ERHINativeResourceClass::Buffer;
}

ERHIShaderStageFlags Visibility(
    const TArray<EShaderStage>& Stages)
{
    ERHIShaderStageFlags Result = ERHIShaderStageFlags::None;
    for (const auto Stage : Stages)
        Result |= ToShaderStageFlag(
            Stage == EShaderStage::Vertex
                ? ERHIShaderStage::Vertex
                : Stage == EShaderStage::Fragment
                    ? ERHIShaderStage::Fragment
                    : ERHIShaderStage::Compute);
    return Result;
}

TArray<uint8> ReadBytes(const std::filesystem::path& Path)
{
    std::ifstream Input(Path, std::ios::binary);
    return {std::istreambuf_iterator<char>(Input),
            std::istreambuf_iterator<char>()};
}

FShaderInterfaceBinding Binding(
    uint32 Set,
    uint32 Slot,
    EShaderResourceKind Kind,
    std::initializer_list<EShaderStage> Stages,
    const char* Name)
{
    return {Set, Slot, Kind, 1, TArray<EShaderStage>(Stages), FString(Name)};
}

TArray<FShaderInterfaceBinding> SurfaceInterface()
{
    return {
        Binding(0, 0, EShaderResourceKind::UniformBuffer,
            {EShaderStage::Vertex}, "FrameView"),
        Binding(1, 0, EShaderResourceKind::UniformBuffer,
            {EShaderStage::Vertex, EShaderStage::Fragment}, "DrawMaterial"),
        Binding(1, 1, EShaderResourceKind::CombinedTextureSampler,
            {EShaderStage::Fragment}, "BaseColorTexture"),
        Binding(1, 2, EShaderResourceKind::CombinedTextureSampler,
            {EShaderStage::Fragment}, "MetallicRoughnessTexture"),
        Binding(1, 3, EShaderResourceKind::CombinedTextureSampler,
            {EShaderStage::Fragment}, "NormalTexture"),
        Binding(1, 4, EShaderResourceKind::CombinedTextureSampler,
            {EShaderStage::Fragment}, "OcclusionTexture"),
        Binding(1, 5, EShaderResourceKind::CombinedTextureSampler,
            {EShaderStage::Fragment}, "EmissiveTexture")};
}

TArray<FShaderInterfaceBinding> DirectionalInterface()
{
    return {
        Binding(2, 0, EShaderResourceKind::CombinedTextureSampler,
            {EShaderStage::Fragment}, "BaseColorAOTexture"),
        Binding(2, 1, EShaderResourceKind::CombinedTextureSampler,
            {EShaderStage::Fragment}, "NormalRoughnessTexture"),
        Binding(2, 2, EShaderResourceKind::CombinedTextureSampler,
            {EShaderStage::Fragment}, "EmissiveMetallicTexture"),
        Binding(2, 3, EShaderResourceKind::CombinedTextureSampler,
            {EShaderStage::Fragment}, "DepthTexture"),
        Binding(3, 0, EShaderResourceKind::StorageBuffer,
            {EShaderStage::Fragment}, "Lights")};
}

TArray<FShaderInterfaceBinding> CompositionInterface()
{
    return {
        Binding(2, 0, EShaderResourceKind::CombinedTextureSampler,
            {EShaderStage::Fragment}, "BaseColorAOTexture"),
        Binding(2, 2, EShaderResourceKind::CombinedTextureSampler,
            {EShaderStage::Fragment}, "EmissiveMetallicTexture"),
        Binding(2, 4, EShaderResourceKind::CombinedTextureSampler,
            {EShaderStage::Fragment}, "LightingAccumulationTexture")};
}

bool ConvertBindingMap(
    const FShaderNativeBindingEvidence& Source,
    FRHINativeBindingMap& Out)
{
    Out = {};
    Out.PolicyVersion = Source.PolicyVersion;
    for (const auto& Entry : Source.Entries)
        Out.Entries.push_back({
            Entry.Stage == EShaderStage::Vertex
                ? ERHIShaderStage::Vertex
                : Entry.Stage == EShaderStage::Fragment
                    ? ERHIShaderStage::Fragment
                    : ERHIShaderStage::Compute,
            Entry.SetIndex, Entry.BindingIndex,
            Descriptor(Entry.DescriptorType), Entry.ArrayElement,
            NativeClass(Entry.NativeClass), Entry.NativeIndex});
    for (const auto& Range : Source.ReservedRanges)
        Out.ReservedRanges.push_back({
            Range.Stage == EShaderStage::Vertex
                ? ERHIShaderStage::Vertex
                : Range.Stage == EShaderStage::Fragment
                    ? ERHIShaderStage::Fragment
                    : ERHIShaderStage::Compute,
            NativeClass(Range.NativeClass), Range.FirstIndex,
            Range.Count, Range.Purpose});
    for (const auto& Limit : Source.LimitSnapshot)
        Out.LimitSnapshot.push_back({
            Limit.Stage == EShaderStage::Vertex
                ? ERHIShaderStage::Vertex
                : Limit.Stage == EShaderStage::Fragment
                    ? ERHIShaderStage::Fragment
                    : ERHIShaderStage::Compute,
            NativeClass(Limit.NativeClass), Limit.MaxCount});
    Out.CanonicalDigest.bAvailable = Source.CanonicalDigest.IsAvailable();
    Out.CanonicalDigest.Bytes = Source.CanonicalDigest.GetBytes();
    return IsCanonicalRHINativeBindingMap(Out);
}

enum class EBuildShaderResult
{
    Success,
    Unavailable,
    Failed
};

EBuildShaderResult BuildShader(
    const char* File,
    ERHIShaderStage Stage,
    const TArray<FShaderInterfaceBinding>& Interface,
    const std::filesystem::path& Scratch,
    FRHIShaderModuleDesc& Out,
    FString& OutReason,
    FString& OutNativeEvidenceDigest)
{
    Out = {};
    OutNativeEvidenceDigest = {};
    const auto Bytes = ReadBytes(
        std::filesystem::path("Content/Shaders/Deferred") / File);
    FSpirvCrossMslRequest Request;
    Request.SpirvBytes = Bytes;
    Request.Stage = AssetStage(Stage);
    Request.EntryPoint = FString("main");
    Request.InterfaceBindings = Interface;
    FSpirvCrossMslResult Derived;
    if (DeriveMetalShaderSource(Request, Derived) != EAssetResult::Success)
    {
        OutReason = "metal-deferred-derive";
        return EBuildShaderResult::Failed;
    }

    FAssetId ShaderId;
    if (FAssetId::Create(
            FString("ShaderProgram"),
            FString(std::string("Engine/Shaders/Deferred/Probe/") + File),
            {}, ShaderId) != EAssetResult::Success)
    {
        OutReason = "metal-deferred-identity";
        return EBuildShaderResult::Failed;
    }
    const FString Profile(
        std::string("metal-macos-12-") + HostArchitecture());
    FMetalShaderEvidence Evidence;
    Evidence.ShaderAssetId = std::move(ShaderId);
    Evidence.ShaderAssetVersion = Derived.SpirvDigest;
    Evidence.SpirvDigest = Derived.SpirvDigest;
    Evidence.Stage = Request.Stage;
    Evidence.EntryPoint = Request.EntryPoint;
    Evidence.InterfaceDigest = Derived.InterfaceDigest;
    Evidence.SpirvCrossOptionsDigest = Derived.OptionsDigest;
    Evidence.BindingEvidence = Derived.BindingEvidence;
    Evidence.TargetProfile = Profile;
    Evidence.NormalizedMslDigest = Derived.NormalizedMslDigest;
    if (FinalizeMetalShaderEvidence(Evidence) != EAssetResult::Success)
    {
        OutReason = "metal-deferred-evidence";
        return EBuildShaderResult::Failed;
    }

    std::error_code Error;
    std::filesystem::create_directories(Scratch, Error);
    if (Error)
    {
        OutReason = "metal-deferred-scratch";
        return EBuildShaderResult::Failed;
    }
    FMetalLibraryCompileRequest Compile;
    Compile.WorkingDirectory = FString(Scratch.string());
    Compile.Architecture = FString(HostArchitecture());
    Compile.TargetProfile = Profile;
    Compile.NormalizedMsl = Derived.NormalizedMsl;
    Compile.DerivationEvidence = Evidence;
    const auto Library = FinalizeMetalLibrary(Compile);
    if (!Library.Succeeded())
    {
        OutReason = Library.StableReason;
        return Library.Status == EMetalLibraryFinalizeStatus::HostUnsupported ||
                Library.Status == EMetalLibraryFinalizeStatus::ToolchainUnavailable
            ? EBuildShaderResult::Unavailable
            : EBuildShaderResult::Failed;
    }

    Out.Stage = Stage;
    Out.EntryPoint = "main";
    Out.Payload.Format = ERHIShaderPayloadFormat::MetalLibrary;
    Out.Payload.Bytes = Library.LibraryBytes;
    Out.Payload.PayloadIdentity = FString(
        std::string("DeferredProbe/") + File);
    Out.Payload.TargetProfile = Profile;
    Out.Payload.PayloadDigest = ComputeRHISha256(Out.Payload.Bytes);
    Out.ValidationMode = ERHIShaderBytecodeValidationMode::Runtime;
    Out.RuntimeMode = ERHIRuntimeObjectMode::RealRuntime;
    Out.DebugName = Out.Payload.PayloadIdentity;
    Out.InterfaceMetadata.DebugName = Out.DebugName;
    for (const auto& Value : Interface)
    {
        if (std::find(
                Value.Visibility.begin(), Value.Visibility.end(),
                Request.Stage) == Value.Visibility.end())
            continue;
        Out.InterfaceMetadata.Bindings.push_back({
            Value.SetIndex, Value.BindingIndex, Descriptor(Value.Kind),
            Value.ArrayCount, Visibility(Value.Visibility)});
    }
    if (!ConvertBindingMap(Derived.BindingEvidence, Out.NativeBindingMap) ||
        !IsValidRHIShaderModuleDesc(Out))
    {
        OutReason = "metal-deferred-rhi-shader";
        return EBuildShaderResult::Failed;
    }
    OutNativeEvidenceDigest =
        Library.NativeEvidence.EvidenceDigest.ToLowerHex();
    return EBuildShaderResult::Success;
}

template <typename T>
bool Upload(
    const TSharedPtr<IRHIDevice>& Device,
    const TSharedPtr<IRHIBuffer>& Buffer,
    std::span<const T> Values)
{
    const FRHIBufferUploadDesc UploadDesc{
        0, Values.data(), Values.size_bytes()};
    return Device->UploadBuffer(Buffer, UploadDesc) == ERHIResult::Success;
}

TSharedPtr<IRHIBuffer> Buffer(
    const TSharedPtr<IRHIDevice>& Device,
    uint64 Size,
    ERHIBufferUsage Usage,
    ERHIMemoryAccess Memory = ERHIMemoryAccess::DeviceLocal)
{
    return Device->CreateBuffer({Size, Usage, Memory}).Object;
}

TSharedPtr<IRHITexture> Texture(
    const TSharedPtr<IRHIDevice>& Device,
    ERHIFormat Format,
    ERHITextureUsage Usage)
{
    FRHITextureDesc Desc;
    Desc.Width = Extent;
    Desc.Height = Extent;
    Desc.Format = Format;
    Desc.Usage = Usage;
    return Device->CreateTexture(Desc).Object;
}

bool UploadRGBA8(
    const TSharedPtr<IRHIDevice>& Device,
    const TSharedPtr<IRHITexture>& TextureValue,
    const std::array<uint8, Extent * Extent * 4>& Pixels)
{
    FRHITextureUploadDesc Upload;
    Upload.Width = Extent;
    Upload.Height = Extent;
    Upload.RowPitchBytes = Extent * 4;
    Upload.Data = Pixels.data();
    Upload.DataSizeBytes = Pixels.size();
    return Device->UploadTexture(TextureValue, Upload) == ERHIResult::Success;
}

TSharedPtr<IRHIPipelineLayout> Layout(
    const TSharedPtr<IRHIDevice>& Device,
    const TArray<FShaderInterfaceBinding>& Interface)
{
    FRHIPipelineLayoutDesc Desc;
    for (const auto& Value : Interface)
        Desc.Bindings.push_back({
            Value.SetIndex, Value.BindingIndex, Descriptor(Value.Kind),
            Value.ArrayCount, Visibility(Value.Visibility)});
    return Device->CreatePipelineLayout(Desc).Object;
}

TSharedPtr<IRHIGraphicsPipeline> Pipeline(
    const TSharedPtr<IRHIDevice>& Device,
    const TSharedPtr<IRHIPipelineLayout>& PipelineLayout,
    const TSharedPtr<IRHIShaderModule>& Vertex,
    const TSharedPtr<IRHIShaderModule>& Fragment,
    const FDeferredVertexLayoutContract& VertexLayout,
    TArray<ERHIFormat> Colors,
    ERHIFormat Depth = ERHIFormat::Unknown)
{
    FRHIGraphicsPipelineDesc Desc;
    Desc.ShaderModules = {Vertex, Fragment};
    Desc.PipelineLayout = PipelineLayout;
    Desc.VertexInput.Stride = VertexLayout.Stride;
    Desc.VertexInput.Attributes = VertexLayout.Attributes;
    Desc.Rasterizer.CullMode = ERHICullMode::None;
    Desc.DepthStencil.bDepthTestEnabled = Depth != ERHIFormat::Unknown;
    Desc.DepthStencil.bDepthWriteEnabled = Depth != ERHIFormat::Unknown;
    Desc.DepthStencil.DepthCompare = ERHICompareOp::LessEqual;
    Desc.RenderTargets.ColorFormats = std::move(Colors);
    Desc.RenderTargets.DepthStencilFormat = Depth;
    Desc.RuntimeMode = ERHIRuntimeObjectMode::RealRuntime;
    return Device->CreateGraphicsPipeline(Desc).Object;
}

FDeferredStageBindings Stage(
    const TSharedPtr<IRHIDevice>& Device,
    const TSharedPtr<IRHIGraphicsPipeline>& PipelineObject,
    TArray<FRHIRenderPassAttachmentDesc> Attachments,
    TArray<FRHIFramebufferAttachment> Targets,
    TArray<TSharedPtr<IRHIDescriptorSet>> Sets)
{
    FDeferredStageBindings Result;
    Result.RenderPass = Device->CreateRenderPass({std::move(Attachments)}).Object;
    FRHIFramebufferDesc Framebuffer;
    Framebuffer.RenderPass = Result.RenderPass;
    Framebuffer.Attachments = std::move(Targets);
    Framebuffer.Width = Extent;
    Framebuffer.Height = Extent;
    Result.Framebuffer = Device->CreateFramebuffer(Framebuffer).Object;
    Result.Pipeline = PipelineObject;
    Result.DescriptorSets = std::move(Sets);
    return Result;
}

uint32 BytesPerPixel(ERHIFormat Format)
{
    switch (Format)
    {
    case ERHIFormat::R8G8B8A8_UNorm: return 4;
    case ERHIFormat::R16G16B16A16_Float: return 8;
    case ERHIFormat::D32_Float: return 4;
    default: return 0;
    }
}

double Half(uint16 Value)
{
    const uint32 Sign = Value >> 15;
    const uint32 Exponent = (Value >> 10) & 0x1f;
    const uint32 Fraction = Value & 0x3ff;
    double Magnitude = 0.0;
    if (Exponent == 0) Magnitude = std::ldexp(Fraction, -24);
    else if (Exponent == 31) return Fraction == 0
        ? (Sign ? -INFINITY : INFINITY) : NAN;
    else Magnitude = std::ldexp(1.0 + Fraction / 1024.0,
        static_cast<int>(Exponent) - 15);
    return Sign ? -Magnitude : Magnitude;
}

std::array<double, 4> Pixel(
    const TArray<uint8>& Bytes,
    ERHIFormat Format)
{
    std::array<double, 4> Result{0.0, 0.0, 0.0, 1.0};
    const std::size_t Offset =
        (static_cast<std::size_t>(Extent / 2) * Extent + Extent / 2) *
        BytesPerPixel(Format);
    if (Offset + BytesPerPixel(Format) > Bytes.size()) return Result;
    if (Format == ERHIFormat::R8G8B8A8_UNorm)
        for (uint32 Channel = 0; Channel < 4; ++Channel)
            Result[Channel] = Bytes[Offset + Channel] / 255.0;
    else if (Format == ERHIFormat::R16G16B16A16_Float)
        for (uint32 Channel = 0; Channel < 4; ++Channel)
        {
            uint16 Bits = 0;
            std::memcpy(&Bits, Bytes.data() + Offset + Channel * 2, 2);
            Result[Channel] = Half(Bits);
        }
    else
    {
        float Depth = 0.0f;
        std::memcpy(&Depth, Bytes.data() + Offset, sizeof(Depth));
        Result[0] = Depth;
    }
    return Result;
}

bool Near(double A, double B, double Tolerance)
{
    return std::isfinite(A) && std::abs(A - B) <= Tolerance;
}

#endif

} // namespace

FMetalDeferredNativeProbeReport RunMetalDeferredNativeProbe()
{
    FMetalDeferredNativeProbeReport Report;
#if !SG_PLATFORM_MAC
    Report.Status = EMetalDeferredProbeStatus::Unavailable;
    Report.StableReason = "metal-deferred-host-unsupported";
    return Report;
#else
    const auto Created = CreateMetalDevice();
    if (!Created.Succeeded())
    {
        Report.Status = EMetalDeferredProbeStatus::Unavailable;
        Report.StableReason = "metal-deferred-device-unavailable";
        return Report;
    }
    const auto Device = Created.Device;
    const std::filesystem::path Root =
        std::filesystem::temp_directory_path() /
        "stoner-metal-deferred-native-probe";
    std::error_code Error;
    std::filesystem::remove_all(Root, Error);
    std::filesystem::create_directories(Root, Error);

    struct FShaderBuild
    {
        const char* File;
        ERHIShaderStage Stage;
        TArray<FShaderInterfaceBinding> Interface;
        FRHIShaderModuleDesc Desc;
        TSharedPtr<IRHIShaderModule> Module;
        FString NativeEvidenceDigest;
    };
    TArray<FShaderBuild> Shaders = {
        {"Surface.vert.spv", ERHIShaderStage::Vertex, SurfaceInterface(), {}, {}, {}},
        {"Surface.frag.spv", ERHIShaderStage::Fragment, SurfaceInterface(), {}, {}, {}},
        {"Fullscreen.vert.spv", ERHIShaderStage::Vertex,
            DirectionalInterface(), {}, {}, {}},
        {"DirectionalLight.frag.spv", ERHIShaderStage::Fragment,
            DirectionalInterface(), {}, {}, {}},
        {"Composition.frag.spv", ERHIShaderStage::Fragment,
            CompositionInterface(), {}, {}, {}}};
    for (uint32 Index = 0; Index < Shaders.size(); ++Index)
    {
        auto& Shader = Shaders[Index];
        const auto Built = BuildShader(
            Shader.File, Shader.Stage, Shader.Interface,
            Root / std::to_string(Index), Shader.Desc,
            Report.StableReason, Shader.NativeEvidenceDigest);
        if (Built != EBuildShaderResult::Success)
        {
            Report.Status = Built == EBuildShaderResult::Unavailable
                ? EMetalDeferredProbeStatus::Unavailable
                : EMetalDeferredProbeStatus::Failed;
            (void)Device->Shutdown();
            std::filesystem::remove_all(Root, Error);
            return Report;
        }
        const auto Module = Device->CreateShaderModule(Shader.Desc);
        if (!Module.Succeeded())
        {
            Report.StableReason = "metal-deferred-native-shader";
            (void)Device->Shutdown();
            std::filesystem::remove_all(Root, Error);
            return Report;
        }
        Shader.Module = Module.Object;
        Report.ShaderEvidenceDigests.push_back(
            Shader.NativeEvidenceDigest);
    }

    FDeferredFrameInputs Inputs;
    Inputs.FrameId = "MetalDeferredProbe";
    Inputs.View.Name = "MetalDeferredProbeView";
    Inputs.View.Extent = {Extent, Extent};
    Inputs.View.CameraPosition = {0.0f, 0.0f, 1.0f};
    Inputs.View.DepthPolicy = MakeDeferredDepthPolicy(
        EDeferredDepthConvention::StandardZ, 0.1f, 100.0f);
    Inputs.Output = {"MetalDeferredOutput", ERHIFormat::R16G16B16A16_Float,
        Inputs.View.Extent};
    FDeferredDrawCandidate Draw;
    Draw.Identity = {1, 1};
    Draw.MeshId = 1;
    Draw.MaterialId = 1;
    Draw.Name = "MetalDeferredTriangle";
    Draw.Surface.BaseColor = FColor(0.8f, 0.2f, 0.1f, 1.0f);
    Draw.Surface.AmbientOcclusion = 0.75f;
    Draw.Surface.Normal = {0.0f, 0.0f, 1.0f};
    Draw.Surface.Roughness = 0.42f;
    Draw.Surface.Emissive = FColor(0.3f, 0.05f, 0.0f, 1.0f);
    Draw.Surface.Metallic = 0.65f;
    Inputs.DrawCandidates.push_back(Draw);
    Inputs.DirectionalLights.push_back({
        {2, 1}, "MetalDeferredSun", {0.0f, 0.0f, -1.0f},
        FColor::OpaqueWhite(), 1.0f});

    FDeferredRendererConfiguration Configuration;
    Configuration.bEnableForwardTransparencyHandoff = false;
    Configuration.bEnableValidationReadback = true;
    FDeferredRenderer Renderer(Configuration);
    FDeferredFramePlan Plan;
    if (Renderer.PrepareFrame(Inputs, Plan) != EDeferredResult::Success ||
        Plan.AcceptedDraws.size() != 1 || Plan.Lights.Accepted.size() != 1)
    {
        Report.StableReason = "metal-deferred-plan";
        (void)Device->Shutdown();
        std::filesystem::remove_all(Root, Error);
        return Report;
    }
    const auto Graph = BuildDeferredRenderGraphDeclaration(Plan);
    Report.bUsedSharedRenderer = Graph.bValid;

    FDeferredFrameExecutionBindings Bindings;
    const ERHITextureUsage GBufferUsage =
        ERHITextureUsage::ColorAttachment | ERHITextureUsage::Sampled |
        ERHITextureUsage::CopySource;
    Bindings.BaseColorAO = Texture(
        Device, ERHIFormat::R8G8B8A8_UNorm, GBufferUsage);
    Bindings.NormalRoughness = Texture(
        Device, ERHIFormat::R16G16B16A16_Float, GBufferUsage);
    Bindings.EmissiveMetallic = Texture(
        Device, ERHIFormat::R16G16B16A16_Float, GBufferUsage);
    Bindings.Depth = Texture(
        Device, ERHIFormat::D32_Float,
        ERHITextureUsage::DepthStencilAttachment |
            ERHITextureUsage::Sampled | ERHITextureUsage::CopySource);
    Bindings.LightingAccumulation = Texture(
        Device, ERHIFormat::R16G16B16A16_Float, GBufferUsage);
    Bindings.FinalOutput = Texture(
        Device, ERHIFormat::R16G16B16A16_Float,
        ERHITextureUsage::ColorAttachment | ERHITextureUsage::CopySource);

    constexpr std::array<float, 36> SurfaceVertices = {
        -1.0f, -1.0f, 0.5f, 0.0f, 0.0f, 1.0f,
         1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
         3.0f, -1.0f, 0.5f, 0.0f, 0.0f, 1.0f,
         1.0f,  0.0f, 0.0f, 1.0f, 2.0f, 0.0f,
        -1.0f,  3.0f, 0.5f, 0.0f, 0.0f, 1.0f,
         1.0f,  0.0f, 0.0f, 1.0f, 0.0f, 2.0f};
    constexpr std::array<uint16, 3> Indices = {0, 1, 2};
    constexpr std::array<float, 6> FullscreenVertices = {
        -1.0f, -1.0f, 3.0f, -1.0f, -1.0f, 3.0f};
    Bindings.SurfaceVertexBuffer = Buffer(Device, sizeof(SurfaceVertices),
        ERHIBufferUsage::Vertex | ERHIBufferUsage::CopyDestination);
    Bindings.SurfaceIndexBuffer = Buffer(Device, sizeof(Indices),
        ERHIBufferUsage::Index | ERHIBufferUsage::CopyDestination);
    Bindings.SurfaceIndexCount = Indices.size();
    Bindings.FullscreenVertexBuffer = Buffer(Device, sizeof(FullscreenVertices),
        ERHIBufferUsage::Vertex | ERHIBufferUsage::CopyDestination);
    if (!Upload(Device, Bindings.SurfaceVertexBuffer,
            std::span<const float>(SurfaceVertices)) ||
        !Upload(Device, Bindings.SurfaceIndexBuffer,
            std::span<const uint16>(Indices)) ||
        !Upload(Device, Bindings.FullscreenVertexBuffer,
            std::span<const float>(FullscreenVertices)))
    {
        Report.StableReason = "metal-deferred-geometry-upload";
        (void)Device->Shutdown();
        std::filesystem::remove_all(Root, Error);
        return Report;
    }

    const auto FrameUniform = BuildDeferredFrameViewUniform(Plan.View);
    const auto DrawUniform = BuildDeferredDrawMaterialUniform(
        Plan.AcceptedDraws.front());
    const auto LightUniform = BuildDeferredLightUniform(
        Plan.Lights.Accepted.front());
    const auto FrameBuffer = Buffer(Device, sizeof(FrameUniform),
        ERHIBufferUsage::Uniform | ERHIBufferUsage::CopyDestination);
    const auto DrawBuffer = Buffer(Device, sizeof(DrawUniform),
        ERHIBufferUsage::Uniform | ERHIBufferUsage::CopyDestination);
    const auto LightBuffer = Buffer(Device, sizeof(LightUniform),
        ERHIBufferUsage::Storage | ERHIBufferUsage::CopyDestination);
    const auto SurfaceWhiteTexture = Texture(
        Device, ERHIFormat::R8G8B8A8_UNorm,
        ERHITextureUsage::Sampled | ERHITextureUsage::CopyDestination);
    const auto SurfaceNormalTexture = Texture(
        Device, ERHIFormat::R8G8B8A8_UNorm,
        ERHITextureUsage::Sampled | ERHITextureUsage::CopyDestination);
    std::array<uint8, Extent * Extent * 4> WhitePixels;
    WhitePixels.fill(255);
    std::array<uint8, Extent * Extent * 4> NormalPixels;
    for (uint32 Index = 0; Index < Extent * Extent; ++Index)
    {
        NormalPixels[Index * 4 + 0] = 128;
        NormalPixels[Index * 4 + 1] = 128;
        NormalPixels[Index * 4 + 2] = 255;
        NormalPixels[Index * 4 + 3] = 255;
    }
    if (!Upload(Device, FrameBuffer,
            std::span<const FDeferredFrameViewUniform>(&FrameUniform, 1)) ||
        !Upload(Device, DrawBuffer,
            std::span<const FDeferredDrawMaterialUniform>(&DrawUniform, 1)) ||
        !Upload(Device, LightBuffer,
            std::span<const FDeferredLightUniform>(&LightUniform, 1)) ||
        !UploadRGBA8(Device, SurfaceWhiteTexture, WhitePixels) ||
        !UploadRGBA8(Device, SurfaceNormalTexture, NormalPixels))
    {
        Report.StableReason = "metal-deferred-uniform-upload";
        (void)Device->Shutdown();
        std::filesystem::remove_all(Root, Error);
        return Report;
    }

    const auto SurfaceLayout = Layout(Device, SurfaceInterface());
    const auto DirectionalLayout = Layout(Device, DirectionalInterface());
    const auto CompositionLayout = Layout(Device, CompositionInterface());
    const auto SurfacePipeline = Pipeline(
        Device, SurfaceLayout, Shaders[0].Module, Shaders[1].Module,
        GetDeferredSurfaceVertexLayout(),
        {ERHIFormat::R8G8B8A8_UNorm,
         ERHIFormat::R16G16B16A16_Float,
         ERHIFormat::R16G16B16A16_Float}, ERHIFormat::D32_Float);
    const auto DirectionalPipeline = Pipeline(
        Device, DirectionalLayout, Shaders[2].Module, Shaders[3].Module,
        GetDeferredFullscreenVertexLayout(),
        {ERHIFormat::R16G16B16A16_Float});
    const auto CompositionPipeline = Pipeline(
        Device, CompositionLayout, Shaders[2].Module, Shaders[4].Module,
        GetDeferredFullscreenVertexLayout(),
        {ERHIFormat::R16G16B16A16_Float});

    const auto SurfaceSet0 = Device->CreateDescriptorSet(SurfaceLayout, 0).Object;
    const auto SurfaceSet1 = Device->CreateDescriptorSet(SurfaceLayout, 1).Object;
    const auto DirectionalSet2 = Device->CreateDescriptorSet(DirectionalLayout, 2).Object;
    const auto DirectionalSet3 = Device->CreateDescriptorSet(DirectionalLayout, 3).Object;
    const auto CompositionSet2 = Device->CreateDescriptorSet(CompositionLayout, 2).Object;
    FRHISamplerDesc SamplerDesc;
    SamplerDesc.MinFilter = ERHISamplerFilter::Nearest;
    SamplerDesc.MagFilter = ERHISamplerFilter::Nearest;
    SamplerDesc.MipFilter = ERHISamplerMipFilter::None;
    SamplerDesc.AddressU = ERHISamplerAddressMode::ClampToEdge;
    SamplerDesc.AddressV = ERHISamplerAddressMode::ClampToEdge;
    SamplerDesc.AddressW = ERHISamplerAddressMode::ClampToEdge;
    const auto Sampler = Device->CreateSampler(SamplerDesc).Object;
    const bool bDescriptors = SurfaceSet0 && SurfaceSet1 &&
        DirectionalSet2 && DirectionalSet3 && CompositionSet2 && Sampler &&
        SurfaceSet0->UpdateBuffer(0, 0, FrameBuffer) == ERHIResult::Success &&
        SurfaceSet1->UpdateBuffer(0, 0, DrawBuffer) == ERHIResult::Success &&
        SurfaceSet1->UpdateCombinedTextureSampler(
            1, 0, SurfaceWhiteTexture, Sampler) == ERHIResult::Success &&
        SurfaceSet1->UpdateCombinedTextureSampler(
            2, 0, SurfaceWhiteTexture, Sampler) == ERHIResult::Success &&
        SurfaceSet1->UpdateCombinedTextureSampler(
            3, 0, SurfaceNormalTexture, Sampler) == ERHIResult::Success &&
        SurfaceSet1->UpdateCombinedTextureSampler(
            4, 0, SurfaceWhiteTexture, Sampler) == ERHIResult::Success &&
        SurfaceSet1->UpdateCombinedTextureSampler(
            5, 0, SurfaceWhiteTexture, Sampler) == ERHIResult::Success &&
        DirectionalSet2->UpdateCombinedTextureSampler(
            0, 0, Bindings.BaseColorAO, Sampler) == ERHIResult::Success &&
        DirectionalSet2->UpdateCombinedTextureSampler(
            1, 0, Bindings.NormalRoughness, Sampler) == ERHIResult::Success &&
        DirectionalSet2->UpdateCombinedTextureSampler(
            2, 0, Bindings.EmissiveMetallic, Sampler) == ERHIResult::Success &&
        DirectionalSet2->UpdateCombinedTextureSampler(
            3, 0, Bindings.Depth, Sampler) == ERHIResult::Success &&
        DirectionalSet3->UpdateBuffer(0, 0, LightBuffer) == ERHIResult::Success &&
        CompositionSet2->UpdateCombinedTextureSampler(
            0, 0, Bindings.BaseColorAO, Sampler) == ERHIResult::Success &&
        CompositionSet2->UpdateCombinedTextureSampler(
            2, 0, Bindings.EmissiveMetallic, Sampler) == ERHIResult::Success &&
        CompositionSet2->UpdateCombinedTextureSampler(
            4, 0, Bindings.LightingAccumulation, Sampler) == ERHIResult::Success;

    const auto Color = [](ERHIFormat Format) {
        return FRHIRenderPassAttachmentDesc{
            ERHIAttachmentRole::Color, Format, ERHISampleCount::One,
            ERHIAttachmentLoadOp::Clear, ERHIAttachmentStoreOp::Store};
    };
    Bindings.Surface = Stage(
        Device, SurfacePipeline,
        {Color(ERHIFormat::R8G8B8A8_UNorm),
         Color(ERHIFormat::R16G16B16A16_Float),
         Color(ERHIFormat::R16G16B16A16_Float),
         {ERHIAttachmentRole::DepthStencil, ERHIFormat::D32_Float,
          ERHISampleCount::One, ERHIAttachmentLoadOp::Clear,
          ERHIAttachmentStoreOp::Store}},
        {{Bindings.BaseColorAO}, {Bindings.NormalRoughness},
         {Bindings.EmissiveMetallic}, {Bindings.Depth}},
        {SurfaceSet0, SurfaceSet1});
    Bindings.Directional = Stage(
        Device, DirectionalPipeline,
        {Color(ERHIFormat::R16G16B16A16_Float)},
        {{Bindings.LightingAccumulation}},
        {DirectionalSet2, DirectionalSet3});
    Bindings.Composition = Stage(
        Device, CompositionPipeline,
        {Color(ERHIFormat::R16G16B16A16_Float)},
        {{Bindings.FinalOutput}}, {CompositionSet2});
    Bindings.CommandBuffer = Device->CreateCommandBuffer(
        ERHIQueueType::Graphics).Object;

    struct FReadback
    {
        const char* Name;
        TSharedPtr<IRHITexture> Texture;
        ERHIFormat Format;
        TSharedPtr<IRHIBuffer> Buffer;
        TArray<uint8> Bytes;
    };
    TArray<FReadback> Readbacks = {
        {"BaseColorAO", Bindings.BaseColorAO,
            ERHIFormat::R8G8B8A8_UNorm, {}, {}},
        {"NormalRoughness", Bindings.NormalRoughness,
            ERHIFormat::R16G16B16A16_Float, {}, {}},
        {"EmissiveMetallic", Bindings.EmissiveMetallic,
            ERHIFormat::R16G16B16A16_Float, {}, {}},
        {"Depth", Bindings.Depth, ERHIFormat::D32_Float, {}, {}},
        {"Lighting", Bindings.LightingAccumulation,
            ERHIFormat::R16G16B16A16_Float, {}, {}},
        {"FinalOutput", Bindings.FinalOutput,
            ERHIFormat::R16G16B16A16_Float, {}, {}}};
    for (auto& Readback : Readbacks)
    {
        const uint64 Size = static_cast<uint64>(Extent) * Extent *
            BytesPerPixel(Readback.Format);
        Readback.Buffer = Buffer(
            Device, Size, ERHIBufferUsage::CopyDestination,
            ERHIMemoryAccess::HostVisible);
        Bindings.Readbacks.push_back({
            FString(Readback.Name), Readback.Texture, Readback.Buffer,
            {0, 0, 0, 0, 0, Extent, Extent, 1, 0, 0, 0}});
    }

    const auto Execution = FDeferredFrameExecutor().Execute(
        Plan, Graph, Bindings);
    const auto Queue = Device->CreateCommandQueue(ERHIQueueType::Graphics);
    const auto Fence = Device->CreateFence();
    bool bSubmitted = Graph.bValid && bDescriptors && SurfacePipeline &&
        DirectionalPipeline && CompositionPipeline && Execution.Succeeded() &&
        Execution.FinalState == EDeferredExecutionState::Recorded &&
        Queue.Succeeded() && Fence.Succeeded() &&
        Queue.Object->Submit(Bindings.CommandBuffer, {}, {}, Fence.Object) ==
            ERHIResult::Success &&
        Fence.Object->Wait(5'000'000) == ERHIResult::Success;
    for (auto& Readback : Readbacks)
    {
        const uint64 Size = static_cast<uint64>(Extent) * Extent *
            BytesPerPixel(Readback.Format);
        bSubmitted = bSubmitted &&
            ReadMetalBufferForValidation(
                Device, Readback.Buffer, 0, Size, Readback.Bytes) ==
                ERHIResult::Success &&
            Readback.Bytes.size() == Size;
    }
    Report.bNativeSubmissionCompleted = bSubmitted;
    if (bSubmitted)
    {
        const auto Base = Pixel(Readbacks[0].Bytes, Readbacks[0].Format);
        const auto Normal = Pixel(Readbacks[1].Bytes, Readbacks[1].Format);
        const auto Emissive = Pixel(Readbacks[2].Bytes, Readbacks[2].Format);
        const auto Depth = Pixel(Readbacks[3].Bytes, Readbacks[3].Format);
        const auto Lighting = Pixel(Readbacks[4].Bytes, Readbacks[4].Format);
        const auto Final = Pixel(Readbacks[5].Bytes, Readbacks[5].Format);
        Report.BaseColorAO = {
            static_cast<float>(Base[0]), static_cast<float>(Base[1]),
            static_cast<float>(Base[2]), static_cast<float>(Base[3])};
        Report.NormalRoughness = {
            static_cast<float>(Normal[0]), static_cast<float>(Normal[1]),
            static_cast<float>(Normal[2]), static_cast<float>(Normal[3])};
        Report.EmissiveMetallic = {
            static_cast<float>(Emissive[0]), static_cast<float>(Emissive[1]),
            static_cast<float>(Emissive[2]), static_cast<float>(Emissive[3])};
        Report.Depth = {static_cast<float>(Depth[0]), 0.0f, 0.0f, 0.0f};
        Report.Lighting = {
            static_cast<float>(Lighting[0]), static_cast<float>(Lighting[1]),
            static_cast<float>(Lighting[2]), static_cast<float>(Lighting[3])};
        Report.FinalOutput = {
            static_cast<float>(Final[0]), static_cast<float>(Final[1]),
            static_cast<float>(Final[2]), static_cast<float>(Final[3])};
        Report.bGBufferPassed =
            Near(Base[0], 0.8, 2.0 / 255.0) &&
            Near(Base[1], 0.2, 2.0 / 255.0) &&
            Near(Base[2], 0.1, 2.0 / 255.0) &&
            Near(Base[3], 0.75, 2e-3) &&
            Near(Emissive[0], 0.3, 1e-3) &&
            Near(Emissive[1], 0.05, 1e-3) &&
            Near(Emissive[3], 0.65, 1e-3) &&
            Near(Normal[3], 0.42, 1e-3);
        const double Length = std::sqrt(
            Normal[0] * Normal[0] + Normal[1] * Normal[1] +
            Normal[2] * Normal[2]);
        Report.bWorldNormalPassed = std::isfinite(Length) && Length > 0.0 &&
            Normal[2] / Length >= 0.999;
        Report.bDepthPassed = std::isfinite(Depth[0]) &&
            Depth[0] >= 0.0 && Depth[0] < 1.0;
        Report.bLightingPassed = Near(Lighting[0], 1.0, 1e-3) &&
            Near(Lighting[1], 1.0, 1e-3) &&
            Near(Lighting[2], 1.0, 1e-3);
        Report.bFinalOutputPassed =
            Near(Final[0], 1.1, 2e-3) &&
            Near(Final[1], 0.25, 2e-3) &&
            Near(Final[2], 0.1, 2e-3) &&
            Near(Final[3], 1.0, 2e-3);
        Report.FinalOutputDigest = FAssetDigest::FromBytes(
            Readbacks[5].Bytes).ToLowerHex();
    }
    const bool bPassed = Report.bUsedSharedRenderer &&
        Report.bNativeSubmissionCompleted && Report.bGBufferPassed &&
        Report.bWorldNormalPassed && Report.bDepthPassed &&
        Report.bLightingPassed && Report.bFinalOutputPassed;
    Report.Status = bPassed
        ? EMetalDeferredProbeStatus::Success
        : EMetalDeferredProbeStatus::Failed;
    Report.StableReason = bPassed
        ? FString("metal-deferred-native-success")
        : FString("metal-deferred-native-validation");
    (void)Device->Shutdown();
    std::filesystem::remove_all(Root, Error);
    return Report;
#endif
}
