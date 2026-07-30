#include "Renderer/FTextureAssetRealization.h"

#include "Asset/FTextureAsset.h"
#include "RHI/FRHITextureUploadDesc.h"
#include "RHI/IRHIDevice.h"
#include "RHI/IRHITexture.h"

#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>

namespace Stoner::Renderer
{
namespace
{

enum class ETextureExpansion
{
    None,
    GrayToRGBA8,
    GrayAlphaToRGBA8,
    RGBToRGBA8,
    RGBToRGBA32F
};

struct FTextureFormatPlan
{
    Stoner::RHI::ERHIFormat Format = Stoner::RHI::ERHIFormat::Unknown;
    ETextureExpansion Expansion = ETextureExpansion::None;
};

FTextureAssetRealizationResult Failure(
    ETextureAssetRealizationStage Stage,
    Stoner::RHI::ERHIResult Result,
    const Stoner::Core::FString& Identity,
    const char* Code,
    const char* Reason,
    std::optional<Stoner::Core::uint32> MipLevel = std::nullopt)
{
    FTextureAssetRealizationResult FailureResult;
    FailureResult.Result = Result;
    FailureResult.Diagnostic = {
        Stage,
        Result,
        Identity,
        MipLevel,
        Stoner::Core::FString(Code),
        Stoner::Core::FString(Reason)};
    return FailureResult;
}

[[nodiscard]] bool PlanFormat(
    const Stoner::Asset::FTextureAsset& Asset,
    FTextureFormatPlan& OutPlan) noexcept
{
    using namespace Stoner::Asset;
    using namespace Stoner::RHI;
    OutPlan = {};
    if (Asset.GetMips().empty())
    {
        return false;
    }

    const EImageTexelFormat SourceFormat =
        Asset.GetMips().front().GetFormat();
    const bool bSRGB =
        Asset.GetColorSpace() == EImageColorSpace::SRGB;
    const bool bColor = Asset.GetSemantic() == ETextureSemantic::Color;
    if (!bColor && bSRGB)
    {
        return false;
    }

    switch (SourceFormat)
    {
    case EImageTexelFormat::R8_UNorm:
        OutPlan = bSRGB
            ? FTextureFormatPlan{
                  ERHIFormat::R8G8B8A8_sRGB,
                  ETextureExpansion::GrayToRGBA8}
            : FTextureFormatPlan{
                  ERHIFormat::R8_UNorm,
                  ETextureExpansion::None};
        return true;
    case EImageTexelFormat::R8G8_UNorm:
        OutPlan = bColor
            ? FTextureFormatPlan{
                  bSRGB ? ERHIFormat::R8G8B8A8_sRGB
                        : ERHIFormat::R8G8B8A8_UNorm,
                  ETextureExpansion::GrayAlphaToRGBA8}
            : FTextureFormatPlan{
                  ERHIFormat::R8G8_UNorm,
                  ETextureExpansion::None};
        return true;
    case EImageTexelFormat::R8G8B8_UNorm:
        OutPlan = {
            bSRGB ? ERHIFormat::R8G8B8A8_sRGB
                  : ERHIFormat::R8G8B8A8_UNorm,
            ETextureExpansion::RGBToRGBA8};
        return true;
    case EImageTexelFormat::R8G8B8A8_UNorm:
        OutPlan = {
            bSRGB ? ERHIFormat::R8G8B8A8_sRGB
                  : ERHIFormat::R8G8B8A8_UNorm,
            ETextureExpansion::None};
        return true;
    case EImageTexelFormat::R32G32B32_Float:
        if (bSRGB)
        {
            return false;
        }
        OutPlan = {
            ERHIFormat::R32G32B32A32_Float,
            ETextureExpansion::RGBToRGBA32F};
        return true;
    case EImageTexelFormat::R16G16B16A16_Float:
        if (bSRGB)
        {
            return false;
        }
        OutPlan = {
            ERHIFormat::R16G16B16A16_Float,
            ETextureExpansion::None};
        return true;
    case EImageTexelFormat::R32G32B32A32_Float:
        if (bSRGB)
        {
            return false;
        }
        OutPlan = {
            ERHIFormat::R32G32B32A32_Float,
            ETextureExpansion::None};
        return true;
    case EImageTexelFormat::Unknown:
        return false;
    }
    return false;
}

[[nodiscard]] Stoner::RHI::ERHIResult ExpandMip(
    const Stoner::Asset::FImageMip& Mip,
    ETextureExpansion Expansion,
    Stoner::Core::TArray<Stoner::Core::uint8>& OutBytes)
{
    OutBytes.clear();
    if (Expansion == ETextureExpansion::None)
    {
        return Stoner::RHI::ERHIResult::Success;
    }

    const auto Source = Mip.GetBytes();
    const Stoner::Core::uint64 PixelCount =
        static_cast<Stoner::Core::uint64>(Mip.GetExtent().Width) *
        Mip.GetExtent().Height;
    const Stoner::Core::uint64 OutputBytesPerPixel =
        Expansion == ETextureExpansion::RGBToRGBA32F ? 16 : 4;
    if (PixelCount >
        static_cast<Stoner::Core::uint64>(
            std::numeric_limits<Stoner::Core::usize>::max()) /
            OutputBytesPerPixel)
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }

    try
    {
        OutBytes.resize(static_cast<Stoner::Core::usize>(
            PixelCount * OutputBytesPerPixel));
        for (Stoner::Core::uint64 Pixel = 0;
             Pixel < PixelCount;
             ++Pixel)
        {
            if (Expansion == ETextureExpansion::GrayToRGBA8)
            {
                const Stoner::Core::uint8 Gray = Source[Pixel];
                const Stoner::Core::usize Destination =
                    static_cast<Stoner::Core::usize>(Pixel * 4);
                OutBytes[Destination] = Gray;
                OutBytes[Destination + 1] = Gray;
                OutBytes[Destination + 2] = Gray;
                OutBytes[Destination + 3] = 255;
            }
            else if (
                Expansion == ETextureExpansion::GrayAlphaToRGBA8)
            {
                const Stoner::Core::usize SourceOffset =
                    static_cast<Stoner::Core::usize>(Pixel * 2);
                const Stoner::Core::usize Destination =
                    static_cast<Stoner::Core::usize>(Pixel * 4);
                const Stoner::Core::uint8 Gray =
                    Source[SourceOffset];
                OutBytes[Destination] = Gray;
                OutBytes[Destination + 1] = Gray;
                OutBytes[Destination + 2] = Gray;
                OutBytes[Destination + 3] =
                    Source[SourceOffset + 1];
            }
            else if (Expansion == ETextureExpansion::RGBToRGBA8)
            {
                const Stoner::Core::usize SourceOffset =
                    static_cast<Stoner::Core::usize>(Pixel * 3);
                const Stoner::Core::usize Destination =
                    static_cast<Stoner::Core::usize>(Pixel * 4);
                std::memcpy(
                    OutBytes.data() + Destination,
                    Source.data() + SourceOffset,
                    3);
                OutBytes[Destination + 3] = 255;
            }
            else
            {
                const Stoner::Core::usize SourceOffset =
                    static_cast<Stoner::Core::usize>(Pixel * 12);
                const Stoner::Core::usize Destination =
                    static_cast<Stoner::Core::usize>(Pixel * 16);
                std::memcpy(
                    OutBytes.data() + Destination,
                    Source.data() + SourceOffset,
                    12);
                constexpr float Alpha = 1.0f;
                std::memcpy(
                    OutBytes.data() + Destination + 12,
                    &Alpha,
                    sizeof(Alpha));
            }
        }
    }
    catch (const std::bad_alloc&)
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    catch (const std::length_error&)
    {
        return Stoner::RHI::ERHIResult::Unavailable;
    }
    return Stoner::RHI::ERHIResult::Success;
}

} // namespace

FTextureAssetRealizationResult FTextureAssetRealizer::Realize(
    const FTextureAssetRealizationRequest& Request)
{
    using namespace Stoner::RHI;
    const Stoner::Core::FString EmptyIdentity("<missing>");
    const Stoner::Core::FString Identity =
        Request.Asset ? Request.Asset->GetId().ToString()
                      : EmptyIdentity;
    if (!Request.Device || !Request.Device->IsActive())
    {
        return Failure(
            ETextureAssetRealizationStage::ValidateAsset,
            ERHIResult::InvalidState,
            Identity,
            "TextureRealization.DeviceInactive",
            "device is missing or inactive");
    }
    if (!Request.Asset || Request.Asset->GetMips().empty() ||
        Request.Asset->GetOrigin() !=
            Stoner::Asset::EImageOrigin::TopLeft)
    {
        return Failure(
            ETextureAssetRealizationStage::ValidateAsset,
            ERHIResult::InvalidState,
            Identity,
            "TextureRealization.AssetInvalid",
            "texture asset payload is missing or invalid");
    }

    FTextureFormatPlan FormatPlan;
    if (!PlanFormat(*Request.Asset, FormatPlan))
    {
        return Failure(
            ETextureAssetRealizationStage::Plan,
            ERHIResult::Unsupported,
            Identity,
            "TextureRealization.LayoutUnsupported",
            "asset layout and semantic combination is unsupported");
    }
    if (!Request.Device->GetCapabilities().SupportsFormat(
            FormatPlan.Format))
    {
        return Failure(
            ETextureAssetRealizationStage::Plan,
            ERHIResult::Unsupported,
            Identity,
            "TextureRealization.FormatUnsupported",
            "device does not report the planned texture format");
    }

    const auto& Mips = Request.Asset->GetMips();
    FRHITextureDesc TextureDesc;
    TextureDesc.Dimension = ERHITextureDimension::Texture2D;
    TextureDesc.Width = Mips.front().GetExtent().Width;
    TextureDesc.Height = Mips.front().GetExtent().Height;
    TextureDesc.Depth = 1;
    TextureDesc.MipLevels =
        static_cast<Stoner::Core::uint32>(Mips.size());
    TextureDesc.ArrayLayers = 1;
    TextureDesc.SampleCount = ERHISampleCount::One;
    TextureDesc.Format = FormatPlan.Format;
    TextureDesc.Usage =
        ERHITextureUsage::Sampled |
        ERHITextureUsage::CopyDestination;

    auto Created = Request.Device->CreateTexture(TextureDesc);
    if (!Created.Succeeded())
    {
        return Failure(
            ETextureAssetRealizationStage::Create,
            Created.Result,
            Identity,
            "TextureRealization.CreateFailed",
            "RHI texture creation failed");
    }

    for (Stoner::Core::uint32 MipLevel = 0;
         MipLevel < Mips.size();
         ++MipLevel)
    {
        const Stoner::Asset::FImageMip& Mip = Mips[MipLevel];
        Stoner::Core::TArray<Stoner::Core::uint8> ExpandedBytes;
        const ERHIResult ExpandResult =
            ExpandMip(Mip, FormatPlan.Expansion, ExpandedBytes);
        if (ExpandResult != ERHIResult::Success)
        {
            (void)Created.Object->Invalidate();
            return Failure(
                ETextureAssetRealizationStage::Expand,
                ExpandResult,
                Identity,
                "TextureRealization.ExpandFailed",
                "temporary portable-format expansion failed",
                MipLevel);
        }

        const auto SourceBytes = FormatPlan.Expansion ==
                ETextureExpansion::None
            ? Mip.GetBytes()
            : std::span<const Stoner::Core::uint8>(ExpandedBytes);
        FRHITextureUploadDesc Upload;
        Upload.MipLevel = MipLevel;
        Upload.Width = Mip.GetExtent().Width;
        Upload.Height = Mip.GetExtent().Height;
        Upload.Depth = 1;
        FRHITextureFootprint Footprint;
        if (!TryGetRHITextureFootprint(
                FormatPlan.Format,
                Upload.Width,
                Upload.Height,
                Upload.Depth,
                Footprint))
        {
            (void)Created.Object->Invalidate();
            return Failure(
                ETextureAssetRealizationStage::Upload,
                ERHIResult::Unavailable,
                Identity,
                "TextureRealization.FootprintFailed",
                "RHI upload footprint is not representable",
                MipLevel);
        }
        Upload.RowPitchBytes = Footprint.TightRowBytes;
        Upload.Data = SourceBytes.data();
        Upload.DataSizeBytes = SourceBytes.size();
        const ERHIResult UploadResult =
            Request.Device->UploadTexture(Created.Object, Upload);
        if (UploadResult != ERHIResult::Success)
        {
            (void)Created.Object->Invalidate();
            return Failure(
                ETextureAssetRealizationStage::Upload,
                UploadResult,
                Identity,
                "TextureRealization.UploadFailed",
                "synchronous mip upload failed",
                MipLevel);
        }
    }

    if (Created.Object->GetLifecycleState() !=
        ERHIResourceLifecycleState::Valid)
    {
        (void)Created.Object->Invalidate();
        return Failure(
            ETextureAssetRealizationStage::Finalize,
            ERHIResult::Failed,
            Identity,
            "TextureRealization.FinalizeFailed",
            "uploaded texture is not sample-ready");
    }

    FTextureAssetRealizationResult Result;
    Result.Result = ERHIResult::Success;
    Result.Texture = std::move(Created.Object);
    Result.Diagnostic = {
        ETextureAssetRealizationStage::Finalize,
        ERHIResult::Success,
        Identity,
        std::nullopt,
        Stoner::Core::FString("TextureRealization.Success"),
        Stoner::Core::FString("all mips are sample-ready")};
    return Result;
}

bool FTextureAssetRealizationResult::Succeeded() const noexcept
{
    return Result == Stoner::RHI::ERHIResult::Success &&
        Texture != nullptr;
}

const char* ToString(ETextureAssetRealizationStage Stage) noexcept
{
    switch (Stage)
    {
    case ETextureAssetRealizationStage::ValidateAsset:
        return "ValidateAsset";
    case ETextureAssetRealizationStage::Plan:
        return "Plan";
    case ETextureAssetRealizationStage::Create:
        return "Create";
    case ETextureAssetRealizationStage::Expand:
        return "Expand";
    case ETextureAssetRealizationStage::Upload:
        return "Upload";
    case ETextureAssetRealizationStage::Finalize:
        return "Finalize";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
