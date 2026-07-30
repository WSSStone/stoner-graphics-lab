#include "RendererTextureAssetTests.h"

#include "Asset/AssetMinimal.h"
#include "Renderer/FTextureAssetRealization.h"
#include "RHI/RHIMinimal.h"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <limits>
#include <optional>
#include <utility>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Core;
using namespace Stoner::Renderer;
using namespace Stoner::RHI;

void Record(
    FRendererTextureAssetTestResult& Result,
    bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ")
              << Name << '\n';
}

class FRealizationTexture final : public IRHITexture
{
public:
    explicit FRealizationTexture(FRHITextureDesc InDesc)
        : Desc(std::move(InDesc))
    {
    }

    [[nodiscard]] const FRHITextureDesc& GetDesc() const noexcept override
    {
        return Desc;
    }
    [[nodiscard]] ERHITextureDimension GetDimension() const noexcept override
    {
        return Desc.Dimension;
    }
    [[nodiscard]] ERHIFormat GetFormat() const noexcept override
    {
        return Desc.Format;
    }
    [[nodiscard]] ERHITextureUsage GetUsage() const noexcept override
    {
        return Desc.Usage;
    }
    [[nodiscard]] ERHIResourceLifecycleState
        GetLifecycleState() const noexcept override
    {
        return State;
    }
    ERHIResult Invalidate() override
    {
        State = ERHIResourceLifecycleState::Invalidated;
        return ERHIResult::Success;
    }

private:
    FRHITextureDesc Desc;
    ERHIResourceLifecycleState State =
        ERHIResourceLifecycleState::Valid;
};

class FRealizationDevice final : public IRHIDevice
{
public:
    struct FUpload
    {
        uint32 MipLevel = 0;
        uint64 RowPitchBytes = 0;
        TArray<uint8> Bytes;
    };

    FRealizationDevice()
    {
        Capabilities.Formats = {
            MakeRHIFormatCapabilities(ERHIFormat::R8_UNorm),
            MakeRHIFormatCapabilities(ERHIFormat::R8G8_UNorm),
            MakeRHIFormatCapabilities(
                ERHIFormat::R8G8B8A8_UNorm),
            MakeRHIFormatCapabilities(
                ERHIFormat::R8G8B8A8_sRGB),
            MakeRHIFormatCapabilities(
                ERHIFormat::R16G16B16A16_Float),
            MakeRHIFormatCapabilities(
                ERHIFormat::R32G32B32A32_Float)};
    }

    [[nodiscard]] ERHIDeviceState GetState() const noexcept override
    {
        return State;
    }
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
        if (!IsActive())
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!IsValidRHITextureDesc(Desc))
        {
            return {ERHIResult::InvalidState, nullptr};
        }
        if (!Capabilities.SupportsFormat(Desc.Format))
        {
            return {ERHIResult::Unsupported, nullptr};
        }
        LastCreated = MakeShared<FRealizationTexture>(Desc);
        return {ERHIResult::Success, LastCreated};
    }

    ERHIResult UploadTexture(
        const TSharedPtr<IRHITexture>& Texture,
        const FRHITextureUploadDesc& Upload) override
    {
        if (!IsActive() || !Texture ||
            Texture != LastCreated ||
            !IsValidRHITextureUploadDesc(
                Texture->GetDesc(), Upload))
        {
            return ERHIResult::InvalidState;
        }
        if (Uploads.size() == FailUploadIndex)
        {
            return ERHIResult::Failed;
        }
        FRHITextureFootprint Footprint;
        if (!TryGetRHITextureFootprint(
                Texture->GetFormat(),
                Upload.Width,
                Upload.Height,
                Upload.Depth,
                Footprint))
        {
            return ERHIResult::InvalidState;
        }
        const uint64 TightRow = Footprint.TightRowBytes;
        const uint64 RowCount =
            Footprint.BlockCountY * Footprint.BlockCountZ;
        FUpload RecordValue;
        RecordValue.MipLevel = Upload.MipLevel;
        RecordValue.RowPitchBytes = Upload.RowPitchBytes;
        RecordValue.Bytes.resize(
            static_cast<usize>(TightRow * RowCount));
        const auto* Source =
            static_cast<const uint8*>(Upload.Data);
        for (uint64 Row = 0; Row < RowCount; ++Row)
        {
            std::memcpy(
                RecordValue.Bytes.data() +
                    static_cast<usize>(Row * TightRow),
                Source +
                    static_cast<usize>(
                        Row * Upload.RowPitchBytes),
                static_cast<usize>(TightRow));
        }
        Uploads.push_back(std::move(RecordValue));
        return ERHIResult::Success;
    }

    void SetFailUploadIndex(usize Index) noexcept
    {
        FailUploadIndex = Index;
    }
    void RemoveFormat(ERHIFormat Format)
    {
        std::erase_if(
            Capabilities.Formats,
            [Format](const FRHIFormatCapabilities& Record) {
                return Record.Format == Format;
            });
    }
    [[nodiscard]] const TArray<FUpload>&
        GetUploads() const noexcept
    {
        return Uploads;
    }
    [[nodiscard]] const TSharedPtr<FRealizationTexture>&
        GetLastCreated() const noexcept
    {
        return LastCreated;
    }

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
    TSharedPtr<FRealizationTexture> LastCreated;
    TArray<FUpload> Uploads;
    usize FailUploadIndex = std::numeric_limits<usize>::max();
};

FAssetId MakeAssetId(const char* Type, const char* Subresource)
{
    FAssetId Id;
    (void)FAssetId::Create(
        FString(Type),
        FString("Tests/RendererTexture"),
        std::optional<FString>(FString(Subresource)),
        Id);
    return Id;
}

TSharedPtr<const FTextureAsset> MakeTextureAsset(
    EImageTexelFormat Format,
    ETextureSemantic Semantic,
    EImageColorSpace ColorSpace,
    FImageExtent2D Extent,
    TArray<uint8> BaseBytes,
    std::optional<TArray<uint8>> LastMipBytes = std::nullopt)
{
    FImageMip BaseMip;
    if (FImageMip::Create(
            Extent, Format, std::move(BaseBytes), BaseMip) !=
        EAssetResult::Success)
    {
        return {};
    }
    FAssetSourceLocator Source;
    if (FAssetSourceLocator::Create(
            FString("memory"),
            FString("renderer-texture"),
            Source) != EAssetResult::Success)
    {
        return {};
    }
    FImageAsset ImageValue;
    if (FImageAsset::Create(
            MakeAssetId("Image", "image"),
            Source,
            BaseMip,
            ColorSpace,
            ImageFormatHasAlpha(Format)
                ? EImageAlphaMode::Straight
                : EImageAlphaMode::None,
            FAssetDigest::FromBytes(BaseMip.GetBytes()),
            ImageValue) != EAssetResult::Success)
    {
        return {};
    }
    auto Image = MakeShared<FImageAsset>(std::move(ImageValue));

    TArray<FImageMip> Mips{Image->GetBaseMip()};
    if (LastMipBytes)
    {
        FImageMip LastMip;
        if (FImageMip::Create(
                {1, 1},
                Format,
                std::move(*LastMipBytes),
                LastMip) != EAssetResult::Success)
        {
            return {};
        }
        Mips.push_back(std::move(LastMip));
    }
    FImageImportSettings Settings;
    Settings.Semantic = Semantic;
    Settings.ColorSpace = ColorSpace;
    Settings.MipPolicy = LastMipBytes
        ? EImageMipPolicy::FullChain
        : EImageMipPolicy::BaseOnly;
    FTextureAsset TextureValue;
    if (FTextureAsset::Create(
            MakeAssetId("Texture", "texture"),
            Image,
            Settings,
            std::move(Mips),
            TextureValue) != EAssetResult::Success)
    {
        return {};
    }
    return MakeShared<FTextureAsset>(std::move(TextureValue));
}

void TestPortableMappingAndOrientation(
    FRendererTextureAssetTestResult& Result)
{
    const TArray<uint8> SourceBytes{1, 2, 3, 4, 5, 6};
    const auto Asset = MakeTextureAsset(
        EImageTexelFormat::R8G8B8_UNorm,
        ETextureSemantic::Color,
        EImageColorSpace::SRGB,
        {2, 1},
        SourceBytes);
    auto Device = MakeShared<FRealizationDevice>();
    const auto Realized = FTextureAssetRealizer::Realize(
        {Device, Asset});
    const TArray<uint8> Expected{
        1, 2, 3, 255,
        4, 5, 6, 255};
    Record(
        Result,
        Realized.Succeeded() &&
            Realized.Texture->GetFormat() ==
                ERHIFormat::R8G8B8A8_sRGB &&
            Device->GetUploads().size() == 1 &&
            Device->GetUploads()[0].Bytes == Expected &&
            Asset->GetMips()[0].GetBytes().size() ==
                SourceBytes.size() &&
            std::equal(
                Asset->GetMips()[0].GetBytes().begin(),
                Asset->GetMips()[0].GetBytes().end(),
                SourceBytes.begin()),
        "Renderer expands RGB sRGB in top-left order without mutating Asset bytes");

    const auto GrayAlpha = MakeTextureAsset(
        EImageTexelFormat::R8G8_UNorm,
        ETextureSemantic::Color,
        EImageColorSpace::Linear,
        {1, 1},
        {10, 20});
    auto GrayDevice = MakeShared<FRealizationDevice>();
    const auto GrayResult = FTextureAssetRealizer::Realize(
        {GrayDevice, GrayAlpha});
    const TArray<uint8> GrayExpected{10, 10, 10, 20};
    Record(
        Result,
        GrayResult.Succeeded() &&
            GrayResult.Texture->GetFormat() ==
                ERHIFormat::R8G8B8A8_UNorm &&
            GrayDevice->GetUploads()[0].Bytes == GrayExpected,
        "Renderer expands gray-plus-straight-alpha to portable RGBA");
}

void TestAscendingUploadsAndRollback(
    FRendererTextureAssetTestResult& Result)
{
    const auto Asset = MakeTextureAsset(
        EImageTexelFormat::R8G8B8A8_UNorm,
        ETextureSemantic::Color,
        EImageColorSpace::Linear,
        {2, 2},
        {
            1, 2, 3, 4, 5, 6, 7, 8,
            9, 10, 11, 12, 13, 14, 15, 16},
        TArray<uint8>{17, 18, 19, 20});
    auto Device = MakeShared<FRealizationDevice>();
    const auto Success = FTextureAssetRealizer::Realize(
        {Device, Asset});
    Record(
        Result,
        Success.Succeeded() &&
            Device->GetUploads().size() == 2 &&
            Device->GetUploads()[0].MipLevel == 0 &&
            Device->GetUploads()[1].MipLevel == 1,
        "Renderer uploads complete mip chains in ascending order");

    auto FailingDevice = MakeShared<FRealizationDevice>();
    FailingDevice->SetFailUploadIndex(1);
    const auto Failed = FTextureAssetRealizer::Realize(
        {FailingDevice, Asset});
    Record(
        Result,
        !Failed.Succeeded() && !Failed.Texture &&
            Failed.Result == ERHIResult::Failed &&
            Failed.Diagnostic.Stage ==
                ETextureAssetRealizationStage::Upload &&
            Failed.Diagnostic.MipLevel ==
                std::optional<uint32>(1) &&
            FailingDevice->GetLastCreated() &&
            FailingDevice->GetLastCreated()
                    ->GetLifecycleState() ==
                ERHIResourceLifecycleState::Invalidated &&
            Asset->GetMips()[1].GetBytes().size() == 4,
        "Renderer upload failure rolls back GPU state and preserves CPU payload");
}

void TestUnsupportedAndInactive(
    FRendererTextureAssetTestResult& Result)
{
    const auto Asset = MakeTextureAsset(
        EImageTexelFormat::R8G8B8_UNorm,
        ETextureSemantic::Color,
        EImageColorSpace::SRGB,
        {1, 1},
        {1, 2, 3});
    auto UnsupportedDevice = MakeShared<FRealizationDevice>();
    UnsupportedDevice->RemoveFormat(
        ERHIFormat::R8G8B8A8_sRGB);
    const auto Unsupported = FTextureAssetRealizer::Realize(
        {UnsupportedDevice, Asset});
    Record(
        Result,
        !Unsupported.Succeeded() &&
            Unsupported.Result == ERHIResult::Unsupported &&
            Unsupported.Diagnostic.Stage ==
                ETextureAssetRealizationStage::Plan &&
            !UnsupportedDevice->GetLastCreated(),
        "Renderer rejects unsupported format before resource creation");

    auto InactiveDevice = MakeShared<FRealizationDevice>();
    (void)InactiveDevice->Shutdown();
    const auto Inactive = FTextureAssetRealizer::Realize(
        {InactiveDevice, Asset});
    Record(
        Result,
        !Inactive.Succeeded() &&
            Inactive.Result == ERHIResult::InvalidState &&
            Inactive.Diagnostic.Stage ==
                ETextureAssetRealizationStage::ValidateAsset,
        "Renderer rejects inactive devices with stable validation diagnostics");
}

} // namespace

FRendererTextureAssetTestResult RunRendererTextureAssetTests()
{
    FRendererTextureAssetTestResult Result;
    std::cout << "[INFO] Running Renderer texture asset tests\n";
    TestPortableMappingAndOrientation(Result);
    TestAscendingUploadsAndRollback(Result);
    TestUnsupportedAndInactive(Result);
    std::cout << "[INFO] Renderer texture asset tests passed="
              << Result.Passed << " failed=" << Result.Failed << '\n';
    return Result;
}
