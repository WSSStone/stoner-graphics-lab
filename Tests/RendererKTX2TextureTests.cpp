#include "RendererKTX2TextureTests.h"

#include "Asset/AssetMinimal.h"
#include "Renderer/FKTX2TextureRealization.h"
#include "Renderer/FTextureTargetProfile.h"
#include "RHI/RHIMinimal.h"

#include <algorithm>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <span>
#include <type_traits>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace Stoner::Renderer;
using namespace Stoner::RHI;

void Record(
    FRendererKTX2TextureTestResult& Result,
    bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ")
              << Name << '\n';
}

struct FSelectionCase
{
    ERHIFormat RHIFormat = ERHIFormat::Unknown;
    ETextureTranscodeFormat AssetFormat =
        ETextureTranscodeFormat::Unknown;
    ETextureSemantic Semantic = ETextureSemantic::Color;
    EImageColorSpace ColorSpace = EImageColorSpace::Linear;
    EImageAlphaMode AlphaMode = EImageAlphaMode::None;
    uint32 Channels = 4;
};

constexpr std::array<FSelectionCase, 20> SelectionCases{{
    {ERHIFormat::BC1_RGBA_UNorm,
     ETextureTranscodeFormat::BC1_RGBA_UNorm,
     ETextureSemantic::Color, EImageColorSpace::Linear,
     EImageAlphaMode::None, 3},
    {ERHIFormat::BC1_RGBA_sRGB,
     ETextureTranscodeFormat::BC1_RGBA_SRGB,
     ETextureSemantic::Color, EImageColorSpace::SRGB,
     EImageAlphaMode::None, 3},
    {ERHIFormat::BC3_RGBA_UNorm,
     ETextureTranscodeFormat::BC3_RGBA_UNorm,
     ETextureSemantic::Color, EImageColorSpace::Linear,
     EImageAlphaMode::Straight, 4},
    {ERHIFormat::BC3_RGBA_sRGB,
     ETextureTranscodeFormat::BC3_RGBA_SRGB,
     ETextureSemantic::Color, EImageColorSpace::SRGB,
     EImageAlphaMode::Straight, 4},
    {ERHIFormat::BC4_R_UNorm,
     ETextureTranscodeFormat::BC4_R_UNorm,
     ETextureSemantic::Data, EImageColorSpace::Linear,
     EImageAlphaMode::None, 1},
    {ERHIFormat::BC5_RG_UNorm,
     ETextureTranscodeFormat::BC5_RG_UNorm,
     ETextureSemantic::Normal, EImageColorSpace::Linear,
     EImageAlphaMode::None, 2},
    {ERHIFormat::BC7_RGBA_UNorm,
     ETextureTranscodeFormat::BC7_RGBA_UNorm,
     ETextureSemantic::Color, EImageColorSpace::Linear,
     EImageAlphaMode::Straight, 4},
    {ERHIFormat::BC7_RGBA_sRGB,
     ETextureTranscodeFormat::BC7_RGBA_SRGB,
     ETextureSemantic::Color, EImageColorSpace::SRGB,
     EImageAlphaMode::Straight, 4},
    {ERHIFormat::ETC2_RGB8_UNorm,
     ETextureTranscodeFormat::ETC2_RGB8_UNorm,
     ETextureSemantic::Color, EImageColorSpace::Linear,
     EImageAlphaMode::None, 3},
    {ERHIFormat::ETC2_RGB8_sRGB,
     ETextureTranscodeFormat::ETC2_RGB8_SRGB,
     ETextureSemantic::Color, EImageColorSpace::SRGB,
     EImageAlphaMode::None, 3},
    {ERHIFormat::ETC2_RGBA8_UNorm,
     ETextureTranscodeFormat::ETC2_RGBA8_UNorm,
     ETextureSemantic::Color, EImageColorSpace::Linear,
     EImageAlphaMode::Straight, 4},
    {ERHIFormat::ETC2_RGBA8_sRGB,
     ETextureTranscodeFormat::ETC2_RGBA8_SRGB,
     ETextureSemantic::Color, EImageColorSpace::SRGB,
     EImageAlphaMode::Straight, 4},
    {ERHIFormat::EAC_R11_UNorm,
     ETextureTranscodeFormat::EAC_R11_UNorm,
     ETextureSemantic::Data, EImageColorSpace::Linear,
     EImageAlphaMode::None, 1},
    {ERHIFormat::EAC_RG11_UNorm,
     ETextureTranscodeFormat::EAC_RG11_UNorm,
     ETextureSemantic::Data, EImageColorSpace::Linear,
     EImageAlphaMode::None, 2},
    {ERHIFormat::ASTC_4x4_RGBA_UNorm,
     ETextureTranscodeFormat::ASTC_4x4_RGBA_UNorm,
     ETextureSemantic::Color, EImageColorSpace::Linear,
     EImageAlphaMode::Straight, 4},
    {ERHIFormat::ASTC_4x4_RGBA_sRGB,
     ETextureTranscodeFormat::ASTC_4x4_RGBA_SRGB,
     ETextureSemantic::Color, EImageColorSpace::SRGB,
     EImageAlphaMode::Straight, 4},
    {ERHIFormat::R8_UNorm,
     ETextureTranscodeFormat::R8_UNorm,
     ETextureSemantic::Data, EImageColorSpace::Linear,
     EImageAlphaMode::None, 1},
    {ERHIFormat::R8G8_UNorm,
     ETextureTranscodeFormat::R8G8_UNorm,
     ETextureSemantic::Normal, EImageColorSpace::Linear,
     EImageAlphaMode::None, 2},
    {ERHIFormat::R8G8B8A8_UNorm,
     ETextureTranscodeFormat::R8G8B8A8_UNorm,
     ETextureSemantic::Color, EImageColorSpace::Linear,
     EImageAlphaMode::Straight, 4},
    {ERHIFormat::R8G8B8A8_sRGB,
     ETextureTranscodeFormat::R8G8B8A8_SRGB,
     ETextureSemantic::Color, EImageColorSpace::SRGB,
     EImageAlphaMode::Straight, 4},
}};

FKTX2TextureInfo MakeBasisInfo(const FSelectionCase& Case)
{
    FKTX2TextureInfo Info;
    Info.CompressionPolicy =
        ETextureCompressionPolicy::UASTC;
    Info.BasisModel = EKTX2BasisModel::UASTC;
    Info.Semantic = Case.Semantic;
    Info.ColorSpace = Case.ColorSpace;
    Info.AlphaMode = Case.AlphaMode;
    Info.SourceChannelCount = Case.Channels;
    Info.BaseExtent = {7, 5};
    Info.Levels = {{0, {7, 5}, 0, 1, 1}};
    return Info;
}

FTextureTargetProfile MakeProfile(
    TArray<ERHIFormat> Formats,
    bool bAllowFallback = true)
{
    FTextureTargetProfile Profile;
    Profile.Name = FString("test.explicit");
    Profile.PreferredFormats = std::move(Formats);
    Profile.bAllowUncompressedFallback = bAllowFallback;
    return Profile;
}

FRHIDeviceCapabilities MakeCapabilities(
    TArray<ERHIFormat> Formats,
    ERHIFormatCapability Usage =
        ERHIFormatCapability::SampledImage |
        ERHIFormatCapability::CopyDestination)
{
    FRHIDeviceCapabilities Capabilities;
    for (ERHIFormat Format : Formats)
    {
        Capabilities.Formats.push_back({Format, Usage});
    }
    return Capabilities;
}

TArray<uint8> ReadBytes(const std::filesystem::path& Path)
{
    std::ifstream Stream(Path, std::ios::binary);
    return {
        std::istreambuf_iterator<char>(Stream),
        std::istreambuf_iterator<char>()};
}

TSharedPtr<const FKTX2TextureArtifact> OpenArtifact(
    const char* Name)
{
    const TArray<uint8> Bytes = ReadBytes(
        std::filesystem::path(
            "Tests/Fixtures/KTX2/Golden") /
        Name);
    FTextureCookLimits Limits;
    FKTX2TextureInfo Info;
    if (FKTX2TextureCodec::Inspect(
            Bytes, Limits, Info, nullptr) !=
        EAssetResult::Success)
    {
        return {};
    }
    FKTX2TextureArtifact Artifact;
    if (FKTX2TextureCodec::Open(
            Info.TextureId,
            Bytes,
            Limits,
            Artifact,
            nullptr) != EAssetResult::Success)
    {
        return {};
    }
    return MakeShared<FKTX2TextureArtifact>(
        std::move(Artifact));
}

class FKTX2RealizationTexture final : public IRHITexture
{
public:
    explicit FKTX2RealizationTexture(FRHITextureDesc InDesc)
        : Desc(std::move(InDesc))
    {
    }

    [[nodiscard]] const FRHITextureDesc&
        GetDesc() const noexcept override { return Desc; }
    [[nodiscard]] ERHITextureDimension
        GetDimension() const noexcept override
    {
        return Desc.Dimension;
    }
    [[nodiscard]] ERHIFormat
        GetFormat() const noexcept override { return Desc.Format; }
    [[nodiscard]] ERHITextureUsage
        GetUsage() const noexcept override { return Desc.Usage; }
    [[nodiscard]] ERHIResourceLifecycleState
        GetLifecycleState() const noexcept override
    {
        return State;
    }
    ERHIResult Invalidate() override
    {
        ++InvalidateCalls;
        State = ERHIResourceLifecycleState::Invalidated;
        return ERHIResult::Success;
    }
    [[nodiscard]] uint32 GetInvalidateCalls() const noexcept
    {
        return InvalidateCalls;
    }

private:
    FRHITextureDesc Desc;
    ERHIResourceLifecycleState State =
        ERHIResourceLifecycleState::Valid;
    uint32 InvalidateCalls = 0;
};

class FKTX2RealizationDevice final : public IRHIDevice
{
public:
    struct FUpload
    {
        FRHITextureUploadDesc Desc;
        TArray<uint8> Bytes;
    };

    explicit FKTX2RealizationDevice(
        FRHIDeviceCapabilities InCapabilities)
        : Capabilities(std::move(InCapabilities))
    {
    }

    [[nodiscard]] ERHIDeviceState
        GetState() const noexcept override { return State; }
    [[nodiscard]] const FRHIDeviceCapabilities&
        GetCapabilities() const noexcept override
    {
        return Capabilities;
    }
    [[nodiscard]] bool IsActive() const noexcept override
    {
        return State == ERHIDeviceState::Active;
    }
    ERHIResult Shutdown() override
    {
        State = ERHIDeviceState::Shutdown;
        return ERHIResult::Success;
    }

    TRHIObjectResult<IRHITexture> CreateTexture(
        const FRHITextureDesc& Desc) override
    {
        ++CreateCalls;
        LastDesc = Desc;
        if (CreateResult != ERHIResult::Success)
        {
            return {CreateResult, nullptr};
        }
        if (!IsActive() ||
            !IsValidRHITextureDesc(Desc) ||
            !Capabilities.SupportsFormatUsage(
                Desc.Format,
                ERHIFormatCapability::SampledImage |
                    ERHIFormatCapability::CopyDestination))
        {
            return {ERHIResult::Unsupported, nullptr};
        }
        LastCreated =
            MakeShared<FKTX2RealizationTexture>(Desc);
        return {ERHIResult::Success, LastCreated};
    }

    ERHIResult UploadTexture(
        const TSharedPtr<IRHITexture>& Texture,
        const FRHITextureUploadDesc& Upload) override
    {
        if (!Texture || Texture != LastCreated ||
            !IsValidRHITextureUploadDesc(
                Texture->GetDesc(), Upload))
        {
            return ERHIResult::InvalidState;
        }
        if (Uploads.size() == FailUploadIndex)
        {
            return ERHIResult::Failed;
        }
        uint64 RequiredBytes = 0;
        if (!TryGetRHITextureUploadRequiredBytes(
                Texture->GetDesc(),
                Upload,
                RequiredBytes))
        {
            return ERHIResult::InvalidState;
        }
        FUpload Record;
        Record.Desc = Upload;
        Record.Desc.Data = nullptr;
        const auto* Begin =
            static_cast<const uint8*>(Upload.Data);
        Record.Bytes.assign(
            Begin,
            Begin + static_cast<usize>(RequiredBytes));
        Uploads.push_back(std::move(Record));
        return ERHIResult::Success;
    }

    void SetCreateResult(ERHIResult Result) noexcept
    {
        CreateResult = Result;
    }
    void SetFailUploadIndex(usize Index) noexcept
    {
        FailUploadIndex = Index;
    }
    [[nodiscard]] uint32 GetCreateCalls() const noexcept
    {
        return CreateCalls;
    }
    [[nodiscard]] const FRHITextureDesc&
        GetLastDesc() const noexcept { return LastDesc; }
    [[nodiscard]] const TArray<FUpload>&
        GetUploads() const noexcept { return Uploads; }
    [[nodiscard]] const TSharedPtr<FKTX2RealizationTexture>&
        GetLastCreated() const noexcept { return LastCreated; }

    TRHIObjectResult<IRHICommandQueue> CreateCommandQueue(
        ERHIQueueType) override { return Unsupported<IRHICommandQueue>(); }
    TRHIObjectResult<IRHICommandBuffer> CreateCommandBuffer(
        ERHIQueueType) override { return Unsupported<IRHICommandBuffer>(); }
    TRHIObjectResult<IRHIFence> CreateFence(
        bool = false) override { return Unsupported<IRHIFence>(); }
    TRHIObjectResult<IRHISemaphore> CreateSemaphore() override
    {
        return Unsupported<IRHISemaphore>();
    }
    TRHIObjectResult<IRHISwapchain> CreateSwapchain(
        uint32) override { return Unsupported<IRHISwapchain>(); }
    TRHIObjectResult<IRHIBuffer> CreateBuffer(
        const FRHIBufferDesc&) override { return Unsupported<IRHIBuffer>(); }
    TRHIObjectResult<IRHISampler> CreateSampler(
        const FRHISamplerDesc&) override { return Unsupported<IRHISampler>(); }
    TRHIObjectResult<IRHIShaderModule> CreateShaderModule(
        const FRHIShaderModuleDesc&) override
    {
        return Unsupported<IRHIShaderModule>();
    }
    TRHIObjectResult<IRHIPipelineLayout> CreatePipelineLayout(
        const FRHIPipelineLayoutDesc&) override
    {
        return Unsupported<IRHIPipelineLayout>();
    }
    TRHIObjectResult<IRHIDescriptorSet> CreateDescriptorSet(
        const TSharedPtr<IRHIPipelineLayout>&,
        uint32) override { return Unsupported<IRHIDescriptorSet>(); }
    TRHIObjectResult<IRHIGraphicsPipeline> CreateGraphicsPipeline(
        const FRHIGraphicsPipelineDesc&) override
    {
        return Unsupported<IRHIGraphicsPipeline>();
    }
    TRHIObjectResult<IRHIComputePipeline> CreateComputePipeline(
        const FRHIComputePipelineDesc&) override
    {
        return Unsupported<IRHIComputePipeline>();
    }
    TRHIObjectResult<IRHIRenderPass> CreateRenderPass(
        const FRHIRenderPassDesc&) override
    {
        return Unsupported<IRHIRenderPass>();
    }
    TRHIObjectResult<IRHIFramebuffer> CreateFramebuffer(
        const FRHIFramebufferDesc&) override
    {
        return Unsupported<IRHIFramebuffer>();
    }

private:
    template <typename T>
    [[nodiscard]] static TRHIObjectResult<T> Unsupported()
    {
        return {ERHIResult::Unsupported, nullptr};
    }

    ERHIDeviceState State = ERHIDeviceState::Active;
    FRHIDeviceCapabilities Capabilities;
    ERHIResult CreateResult = ERHIResult::Success;
    usize FailUploadIndex = std::numeric_limits<usize>::max();
    uint32 CreateCalls = 0;
    FRHITextureDesc LastDesc;
    TSharedPtr<FKTX2RealizationTexture> LastCreated;
    TArray<FUpload> Uploads;
};

void TestTargetAndCapabilityMatrix(
    FRendererKTX2TextureTestResult& Result)
{
    bool bAllTargetsSelected = true;
    for (const FSelectionCase& Case : SelectionCases)
    {
        const FTextureTargetSelection Selection =
            SelectTextureTarget(
                MakeBasisInfo(Case),
                MakeProfile({Case.RHIFormat}),
                MakeCapabilities({Case.RHIFormat}));
        bAllTargetsSelected =
            bAllTargetsSelected &&
            Selection.Result == ERHIResult::Success &&
            Selection.SelectedFormat == Case.RHIFormat &&
            Selection.TranscodeFormat == Case.AssetFormat &&
            Selection.Candidates.size() == 1 &&
            Selection.Candidates[0].bAccepted;
    }
    Record(
        Result,
        bAllTargetsSelected,
        "all 20 Asset targets map and select through RHI capabilities");

    bool bAllCompressedUsageFailuresReject = true;
    for (usize Index = 0; Index < 16; ++Index)
    {
        const FSelectionCase& Case = SelectionCases[Index];
        const FTextureTargetSelection Selection =
            SelectTextureTarget(
                MakeBasisInfo(Case),
                MakeProfile({Case.RHIFormat}),
                MakeCapabilities(
                    {Case.RHIFormat},
                    ERHIFormatCapability::SampledImage));
        bAllCompressedUsageFailuresReject =
            bAllCompressedUsageFailuresReject &&
            Selection.Result == ERHIResult::Unsupported &&
            Selection.SelectedFormat == ERHIFormat::Unknown &&
            Selection.Candidates.size() == 1 &&
            !Selection.Candidates[0].bAccepted &&
            Selection.Candidates[0].Code.View() ==
                "renderer.texture-target.usage";
    }
    Record(
        Result,
        bAllCompressedUsageFailuresReject,
        "all 16 compressed targets reject missing copy-destination usage");
}

void TestOrderingAndCompatibility(
    FRendererKTX2TextureTestResult& Result)
{
    FSelectionCase Opaque = SelectionCases[0];
    const FTextureTargetProfile Ordered = MakeProfile({
        ERHIFormat::BC7_RGBA_UNorm,
        ERHIFormat::BC1_RGBA_UNorm,
        ERHIFormat::ETC2_RGB8_UNorm});
    const FRHIDeviceCapabilities ReverseCapabilities =
        MakeCapabilities({
            ERHIFormat::ETC2_RGB8_UNorm,
            ERHIFormat::BC1_RGBA_UNorm,
            ERHIFormat::BC7_RGBA_UNorm});
    const FTextureTargetSelection OrderedSelection =
        SelectTextureTarget(
            MakeBasisInfo(Opaque),
            Ordered,
            ReverseCapabilities);
    Record(
        Result,
        OrderedSelection.Result == ERHIResult::Success &&
            OrderedSelection.SelectedFormat ==
                ERHIFormat::BC7_RGBA_UNorm &&
            OrderedSelection.Candidates.size() == 1,
        "selection follows profile order independent of capability order");

    FSelectionCase Alpha = SelectionCases[2];
    const FTextureTargetSelection AlphaSelection =
        SelectTextureTarget(
            MakeBasisInfo(Alpha),
            MakeProfile({
                ERHIFormat::BC1_RGBA_UNorm,
                ERHIFormat::BC3_RGBA_UNorm}),
            MakeCapabilities({
                ERHIFormat::BC1_RGBA_UNorm,
                ERHIFormat::BC3_RGBA_UNorm}));
    Record(
        Result,
        AlphaSelection.Result == ERHIResult::Success &&
            AlphaSelection.SelectedFormat ==
                ERHIFormat::BC3_RGBA_UNorm &&
            AlphaSelection.Candidates.size() == 2 &&
            AlphaSelection.Candidates[0].Code.View() ==
                "renderer.texture-target.alpha" &&
            AlphaSelection.Candidates[1].bAccepted,
        "selection rejects alpha-dropping candidates before accepting the next target");

    FSelectionCase Normal = SelectionCases[5];
    const FTextureTargetSelection NormalSelection =
        SelectTextureTarget(
            MakeBasisInfo(Normal),
            MakeProfile({
                ERHIFormat::ASTC_4x4_RGBA_sRGB,
                ERHIFormat::BC4_R_UNorm,
                ERHIFormat::BC5_RG_UNorm}),
            MakeCapabilities({
                ERHIFormat::BC5_RG_UNorm,
                ERHIFormat::BC4_R_UNorm,
                ERHIFormat::ASTC_4x4_RGBA_sRGB}));
    Record(
        Result,
        NormalSelection.Result == ERHIResult::Success &&
            NormalSelection.SelectedFormat ==
                ERHIFormat::BC5_RG_UNorm &&
            NormalSelection.Candidates.size() == 3 &&
            NormalSelection.Candidates[0].Code.View() ==
                "renderer.texture-target.transfer" &&
            NormalSelection.Candidates[1].Code.View() ==
                "renderer.texture-target.channels",
        "selection rejects sRGB and channel-dropping normal targets");

    const FTextureTargetSelection NoFallback =
        SelectTextureTarget(
            MakeBasisInfo(SelectionCases[16]),
            MakeProfile({ERHIFormat::R8_UNorm}, false),
            MakeCapabilities({ERHIFormat::R8_UNorm}));
    Record(
        Result,
        NoFallback.Result == ERHIResult::Unsupported &&
            NoFallback.Candidates.size() == 1 &&
            NoFallback.Candidates[0].Code.View() ==
                "renderer.texture-target.fallback-disabled",
        "selection rejects transient uncompressed fallback when disabled");
}

void TestProfiles(FRendererKTX2TextureTestResult& Result)
{
    FTextureTargetProfile Duplicate = MakeProfile({
        ERHIFormat::BC1_RGBA_UNorm,
        ERHIFormat::BC1_RGBA_UNorm});
    FTextureTargetProfile Unknown =
        MakeProfile({ERHIFormat::Unknown});
    FTextureTargetProfile Depth =
        MakeProfile({ERHIFormat::D32_Float});
    Record(
        Result,
        Duplicate.Validate() == ERHIResult::InvalidState &&
            Unknown.Validate() == ERHIResult::InvalidState &&
            Depth.Validate() == ERHIResult::InvalidState,
        "target profiles reject duplicate unknown and depth formats");

    const FTextureTargetProfile Opaque =
        FTextureTargetProfile::DesktopDefault(
            MakeBasisInfo(SelectionCases[1]));
    const FTextureTargetProfile Alpha =
        FTextureTargetProfile::DesktopDefault(
            MakeBasisInfo(SelectionCases[3]));
    const FTextureTargetProfile TwoChannel =
        FTextureTargetProfile::DesktopDefault(
            MakeBasisInfo(SelectionCases[5]));
    const FTextureTargetProfile OneChannel =
        FTextureTargetProfile::DesktopDefault(
            MakeBasisInfo(SelectionCases[16]));
    Record(
        Result,
        Opaque.PreferredFormats ==
            TArray<ERHIFormat>{
                ERHIFormat::BC1_RGBA_sRGB,
                ERHIFormat::BC7_RGBA_sRGB,
                ERHIFormat::ASTC_4x4_RGBA_sRGB,
                ERHIFormat::ETC2_RGB8_sRGB,
                ERHIFormat::R8G8B8A8_sRGB} &&
            Alpha.PreferredFormats ==
            TArray<ERHIFormat>{
                ERHIFormat::BC7_RGBA_sRGB,
                ERHIFormat::BC3_RGBA_sRGB,
                ERHIFormat::ASTC_4x4_RGBA_sRGB,
                ERHIFormat::ETC2_RGBA8_sRGB,
                ERHIFormat::R8G8B8A8_sRGB} &&
            TwoChannel.PreferredFormats ==
            TArray<ERHIFormat>{
                ERHIFormat::BC5_RG_UNorm,
                ERHIFormat::ASTC_4x4_RGBA_UNorm,
                ERHIFormat::EAC_RG11_UNorm,
                ERHIFormat::R8G8_UNorm} &&
            OneChannel.PreferredFormats ==
            TArray<ERHIFormat>{
                ERHIFormat::BC4_R_UNorm,
                ERHIFormat::EAC_R11_UNorm,
                ERHIFormat::R8_UNorm},
        "desktop profiles preserve documented family and transfer order");
}

void TestFallbackProvenance(
    FRendererKTX2TextureTestResult& Result)
{
    static_assert(
        !std::is_base_of_v<
            FAssetPayload,
            FTranscodedTexturePayload>);

    const TArray<uint8> Bytes = ReadBytes(
        "Tests/Fixtures/KTX2/Golden/"
        "uastc-data-optin-odd.ktx2");
    FTextureCookLimits Limits;
    FKTX2TextureInfo Info;
    FKTX2TextureArtifact Opened;
    const EAssetResult InspectResult =
        FKTX2TextureCodec::Inspect(
            Bytes, Limits, Info, nullptr);
    const EAssetResult OpenResult =
        InspectResult == EAssetResult::Success
        ? FKTX2TextureCodec::Open(
              Info.TextureId,
              Bytes,
              Limits,
              Opened,
              nullptr)
        : InspectResult;
    const auto Artifact =
        OpenResult == EAssetResult::Success
        ? MakeShared<FKTX2TextureArtifact>(std::move(Opened))
        : nullptr;
    if (!Artifact)
    {
        Record(
            Result,
            false,
            "fallback transcodes the authoritative Basis artifact without a second Asset");
        return;
    }

    const FAssetDigest DigestBefore =
        Artifact->GetArtifactDigest();
    const TArray<uint8> BytesBefore(
        Artifact->GetBytes().begin(),
        Artifact->GetBytes().end());
    const FTextureTargetSelection Selection =
        SelectTextureTarget(
            Artifact->GetInfo(),
            MakeProfile({ERHIFormat::R8G8_UNorm}, true),
            MakeCapabilities({ERHIFormat::R8G8_UNorm}));
    FTextureTranscodeRequest Request;
    Request.Artifact = Artifact;
    Request.TargetFormat = Selection.TranscodeFormat;
    Request.Limits = Limits;
    const FTextureTranscodeResult Transcoded =
        Selection.Result == ERHIResult::Success
        ? FTextureTranscoder::Transcode(Request)
        : FTextureTranscodeResult{};

    Record(
        Result,
        Selection.Result == ERHIResult::Success &&
            Selection.SelectedFormat ==
                ERHIFormat::R8G8_UNorm &&
            Transcoded.Result == EAssetResult::Success &&
            Transcoded.Payload &&
            Transcoded.Payload->Format ==
                ETextureTranscodeFormat::R8G8_UNorm &&
            !Transcoded.Payload->Mips.empty() &&
            Artifact->GetArtifactDigest() == DigestBefore &&
            std::equal(
                Artifact->GetBytes().begin(),
                Artifact->GetBytes().end(),
                BytesBefore.begin(),
                BytesBefore.end()),
        "fallback transcodes the authoritative Basis artifact without a second Asset");
}

void TestRealizationAndRollback(
    FRendererKTX2TextureTestResult& Result)
{
    const auto Artifact = OpenArtifact(
        "uastc-color-balanced-full.ktx2");
    if (!Artifact)
    {
        Record(
            Result, false,
            "KTX2 realization uploads every exact mip in ascending order");
        return;
    }
    const FAssetDigest DigestBefore =
        Artifact->GetArtifactDigest();
    FTextureTranscodeRequest ExpectedRequest;
    ExpectedRequest.Artifact = Artifact;
    ExpectedRequest.TargetFormat =
        ETextureTranscodeFormat::BC7_RGBA_SRGB;
    const FTextureTranscodeResult Expected =
        FTextureTranscoder::Transcode(ExpectedRequest);
    const FRHIDeviceCapabilities Capabilities =
        MakeCapabilities({ERHIFormat::BC7_RGBA_sRGB});
    auto Device =
        MakeShared<FKTX2RealizationDevice>(Capabilities);
    FKTX2TextureRealizationRequest Request;
    Request.Device = Device;
    Request.Artifact = Artifact;
    Request.TargetProfile =
        MakeProfile({ERHIFormat::BC7_RGBA_sRGB});
    const FKTX2TextureRealizationResult Realized =
        FKTX2TextureRealizer::Realize(Request);

    bool bUploadsMatch =
        Expected.Result == EAssetResult::Success &&
        Expected.Payload &&
        Device->GetUploads().size() ==
            Expected.Payload->Mips.size();
    if (bUploadsMatch)
    {
        for (usize Index = 0;
             Index < Expected.Payload->Mips.size();
             ++Index)
        {
            const FTranscodedTextureMip& ExpectedMip =
                Expected.Payload->Mips[Index];
            const auto& Actual =
                Device->GetUploads()[Index];
            bUploadsMatch =
                bUploadsMatch &&
                Actual.Desc.MipLevel == Index &&
                Actual.Desc.Width ==
                    ExpectedMip.Extent.Width &&
                Actual.Desc.Height ==
                    ExpectedMip.Extent.Height &&
                Actual.Desc.RowPitchBytes ==
                    ExpectedMip.RowPitchBytes &&
                Actual.Bytes == ExpectedMip.Bytes;
        }
    }
    Record(
        Result,
        Realized.Succeeded() &&
            Realized.Selection.SelectedFormat ==
                ERHIFormat::BC7_RGBA_sRGB &&
            Device->GetCreateCalls() == 1 &&
            Device->GetLastDesc().Width ==
                Artifact->GetInfo().BaseExtent.Width &&
            Device->GetLastDesc().Height ==
                Artifact->GetInfo().BaseExtent.Height &&
            Device->GetLastDesc().MipLevels ==
                Artifact->GetInfo().Levels.size() &&
            bUploadsMatch &&
            Artifact->GetArtifactDigest() == DigestBefore,
        "KTX2 realization uploads every exact mip in ascending order");

    auto MissingUsage =
        MakeShared<FKTX2RealizationDevice>(
            MakeCapabilities(
                {ERHIFormat::BC7_RGBA_sRGB},
                ERHIFormatCapability::SampledImage));
    Request.Device = MissingUsage;
    const auto Unsupported =
        FKTX2TextureRealizer::Realize(Request);
    Record(
        Result,
        !Unsupported.Succeeded() &&
            Unsupported.Result == ERHIResult::Unsupported &&
            Unsupported.Diagnostic.Stage ==
                EKTX2TextureRealizationStage::Select &&
            MissingUsage->GetCreateCalls() == 0,
        "KTX2 realization rejects missing usage before creation");

    auto CreateFailure =
        MakeShared<FKTX2RealizationDevice>(Capabilities);
    CreateFailure->SetCreateResult(ERHIResult::Failed);
    Request.Device = CreateFailure;
    const auto CreateFailed =
        FKTX2TextureRealizer::Realize(Request);
    Record(
        Result,
        !CreateFailed.Succeeded() &&
            CreateFailed.Result == ERHIResult::Failed &&
            CreateFailed.Diagnostic.Stage ==
                EKTX2TextureRealizationStage::Create &&
            CreateFailure->GetCreateCalls() == 1 &&
            !CreateFailure->GetLastCreated(),
        "KTX2 realization exposes no texture after create failure");

    auto UploadFailure =
        MakeShared<FKTX2RealizationDevice>(Capabilities);
    UploadFailure->SetFailUploadIndex(1);
    Request.Device = UploadFailure;
    const auto UploadFailed =
        FKTX2TextureRealizer::Realize(Request);
    Record(
        Result,
        !UploadFailed.Succeeded() &&
            UploadFailed.Result == ERHIResult::Failed &&
            UploadFailed.Diagnostic.Stage ==
                EKTX2TextureRealizationStage::Upload &&
            UploadFailed.Diagnostic.MipLevel ==
                std::optional<uint32>(1) &&
            UploadFailure->GetLastCreated() &&
            UploadFailure->GetLastCreated()
                    ->GetInvalidateCalls() == 1 &&
            UploadFailure->GetLastCreated()
                    ->GetLifecycleState() ==
                ERHIResourceLifecycleState::Invalidated &&
            Artifact->GetArtifactDigest() == DigestBefore,
        "KTX2 realization rolls upload failure back exactly once");
}

} // namespace

FRendererKTX2TextureTestResult RunRendererKTX2TextureTests()
{
    FRendererKTX2TextureTestResult Result;
    std::cout << "[INFO] Running Renderer KTX2 texture tests\n";
    TestTargetAndCapabilityMatrix(Result);
    TestOrderingAndCompatibility(Result);
    TestProfiles(Result);
    TestFallbackProvenance(Result);
    TestRealizationAndRollback(Result);
    std::cout << "[INFO] Renderer KTX2 texture tests passed="
              << Result.Passed << " failed=" << Result.Failed
              << '\n';
    return Result;
}
