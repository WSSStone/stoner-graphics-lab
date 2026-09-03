#include "Renderer/FTextureTargetProfile.h"

#include "Asset/FKTX2TextureArtifact.h"
#include "Asset/FTextureTranscode.h"
#include "RHI/ERHIResult.h"
#include "RHI/FRHIDeviceCapabilities.h"
#include "RHI/FRHIFormatInfo.h"

#include <algorithm>

namespace Stoner::Renderer
{
namespace
{

using Stoner::Asset::EImageAlphaMode;
using Stoner::Asset::EImageColorSpace;
using Stoner::Asset::EImageTexelFormat;
using Stoner::Asset::EKTX2BasisModel;
using Stoner::Asset::ETextureCompressionPolicy;
using Stoner::Asset::ETextureSemantic;
using Stoner::Asset::ETextureTranscodeFormat;
using Stoner::Asset::FKTX2TextureInfo;
using Stoner::RHI::ERHIFormat;
using Stoner::RHI::ERHIResult;
using Stoner::RHI::FRHIFormatInfo;

struct FTargetFacts
{
    Stoner::Core::uint32 Channels = 0;
    bool bPreservesAlpha = false;
};

[[nodiscard]] FTargetFacts GetTargetFacts(
    ERHIFormat Format) noexcept
{
    switch (Format)
    {
    case ERHIFormat::R8_UNorm:
    case ERHIFormat::BC4_R_UNorm:
    case ERHIFormat::EAC_R11_UNorm:
        return {1, false};
    case ERHIFormat::R8G8_UNorm:
    case ERHIFormat::BC5_RG_UNorm:
    case ERHIFormat::EAC_RG11_UNorm:
        return {2, false};
    case ERHIFormat::BC1_RGBA_UNorm:
    case ERHIFormat::BC1_RGBA_sRGB:
    case ERHIFormat::ETC2_RGB8_UNorm:
    case ERHIFormat::ETC2_RGB8_sRGB:
    case ERHIFormat::R32G32B32_Float:
        return {3, false};
    case ERHIFormat::R8G8B8A8_UNorm:
    case ERHIFormat::R8G8B8A8_sRGB:
    case ERHIFormat::B8G8R8A8_UNorm:
    case ERHIFormat::R10G10B10A2_UNorm:
    case ERHIFormat::R16G16B16A16_Float:
    case ERHIFormat::R32G32B32A32_Float:
    case ERHIFormat::BC3_RGBA_UNorm:
    case ERHIFormat::BC3_RGBA_sRGB:
    case ERHIFormat::BC7_RGBA_UNorm:
    case ERHIFormat::BC7_RGBA_sRGB:
    case ERHIFormat::ETC2_RGBA8_UNorm:
    case ERHIFormat::ETC2_RGBA8_sRGB:
    case ERHIFormat::ASTC_4x4_RGBA_UNorm:
    case ERHIFormat::ASTC_4x4_RGBA_sRGB:
        return {4, true};
    case ERHIFormat::R32_Float:
        return {1, false};
    case ERHIFormat::R32G32_Float:
        return {2, false};
    case ERHIFormat::D24_UNorm_S8_UInt:
    case ERHIFormat::D32_Float:
    case ERHIFormat::S8_UInt:
    case ERHIFormat::Unknown:
    case ERHIFormat::Count:
        return {};
    }
    return {};
}

[[nodiscard]] ERHIFormat GetStoredRHIFormat(
    const FKTX2TextureInfo& Info) noexcept
{
    if (!Info.StoredTexelFormat)
    {
        return ERHIFormat::Unknown;
    }
    switch (*Info.StoredTexelFormat)
    {
    case EImageTexelFormat::R8_UNorm:
        return ERHIFormat::R8_UNorm;
    case EImageTexelFormat::R8G8_UNorm:
        return ERHIFormat::R8G8_UNorm;
    case EImageTexelFormat::R8G8B8_UNorm:
        return ERHIFormat::Unknown;
    case EImageTexelFormat::R8G8B8A8_UNorm:
        return Info.ColorSpace == EImageColorSpace::SRGB
            ? ERHIFormat::R8G8B8A8_sRGB
            : ERHIFormat::R8G8B8A8_UNorm;
    case EImageTexelFormat::R32G32B32_Float:
        return ERHIFormat::R32G32B32_Float;
    case EImageTexelFormat::R16G16B16A16_Float:
        return ERHIFormat::R16G16B16A16_Float;
    case EImageTexelFormat::R32G32B32A32_Float:
        return ERHIFormat::R32G32B32A32_Float;
    case EImageTexelFormat::Unknown:
        return ERHIFormat::Unknown;
    }
    return ERHIFormat::Unknown;
}

[[nodiscard]] bool CanExpandStoredFormat(
    const FKTX2TextureInfo& Info,
    ERHIFormat Format) noexcept
{
    if (!Info.StoredTexelFormat) return false;
    if (*Info.StoredTexelFormat == EImageTexelFormat::R8G8B8_UNorm)
        return Format == ERHIFormat::R8G8B8A8_UNorm ||
            Format == ERHIFormat::R8G8B8A8_sRGB;
    return *Info.StoredTexelFormat == EImageTexelFormat::R32G32B32_Float &&
        Format == ERHIFormat::R32G32B32A32_Float;
}

[[nodiscard]] bool IsBasisArtifact(
    const FKTX2TextureInfo& Info) noexcept
{
    return Info.CompressionPolicy !=
            ETextureCompressionPolicy::Uncompressed &&
        (Info.BasisModel == EKTX2BasisModel::ETC1S ||
         Info.BasisModel == EKTX2BasisModel::UASTC);
}

[[nodiscard]] bool HasRequiredChannels(
    const FKTX2TextureInfo& Info,
    const FTargetFacts& Facts) noexcept
{
    switch (Info.Semantic)
    {
    case ETextureSemantic::Color:
        return Facts.Channels >= 3;
    case ETextureSemantic::Normal:
        return Facts.Channels >= 2;
    case ETextureSemantic::Data:
        return Info.SourceChannelCount > 0 &&
            Facts.Channels >= Info.SourceChannelCount;
    case ETextureSemantic::Unspecified:
        return false;
    }
    return false;
}

FTextureTargetCandidateDiagnostic Reject(
    ERHIFormat Format,
    ETextureTranscodeFormat TranscodeFormat,
    const char* Code,
    const char* Reason)
{
    return {
        Format,
        TranscodeFormat,
        false,
        Stoner::Core::FString(Code),
        Stoner::Core::FString(Reason)};
}

} // namespace

ERHIResult FTextureTargetProfile::Validate() const noexcept
{
    if (Name.IsEmpty() || PreferredFormats.empty())
    {
        return ERHIResult::InvalidState;
    }
    for (Stoner::Core::usize Index = 0;
         Index < PreferredFormats.size();
         ++Index)
    {
        const ERHIFormat Format = PreferredFormats[Index];
        if (!Stoner::RHI::IsValidRHIFormat(Format) ||
            Stoner::RHI::IsDepthStencilFormat(Format))
        {
            return ERHIResult::InvalidState;
        }
        if (std::find(
                PreferredFormats.begin() + Index + 1,
                PreferredFormats.end(),
                Format) != PreferredFormats.end())
        {
            return ERHIResult::InvalidState;
        }
    }
    return ERHIResult::Success;
}

FTextureTargetProfile FTextureTargetProfile::DesktopDefault(
    const FKTX2TextureInfo& Info)
{
    FTextureTargetProfile Profile;
    Profile.bAllowUncompressedFallback = true;

    if (Info.CompressionPolicy ==
        ETextureCompressionPolicy::Uncompressed)
    {
        Profile.Name =
            Stoner::Core::FString("desktop.uncompressed");
        const ERHIFormat Stored = GetStoredRHIFormat(Info);
        if (Stored != ERHIFormat::Unknown)
        {
            Profile.PreferredFormats.push_back(Stored);
        }
        else if (Info.StoredTexelFormat ==
                 EImageTexelFormat::R8G8B8_UNorm)
        {
            Profile.PreferredFormats.push_back(
                Info.ColorSpace == EImageColorSpace::SRGB
                    ? ERHIFormat::R8G8B8A8_sRGB
                    : ERHIFormat::R8G8B8A8_UNorm);
        }
        else if (Info.StoredTexelFormat ==
                 EImageTexelFormat::R32G32B32_Float)
        {
            Profile.PreferredFormats.push_back(
                ERHIFormat::R32G32B32A32_Float);
        }
        return Profile;
    }

    const bool bSRGB =
        Info.ColorSpace == EImageColorSpace::SRGB;
    if (Info.Semantic == ETextureSemantic::Color)
    {
        if (Info.AlphaMode == EImageAlphaMode::Straight)
        {
            Profile.Name =
                Stoner::Core::FString("desktop.alpha-color");
            Profile.PreferredFormats = bSRGB
                ? Stoner::Core::TArray<ERHIFormat>{
                      ERHIFormat::BC7_RGBA_sRGB,
                      ERHIFormat::BC3_RGBA_sRGB,
                      ERHIFormat::ASTC_4x4_RGBA_sRGB,
                      ERHIFormat::ETC2_RGBA8_sRGB,
                      ERHIFormat::R8G8B8A8_sRGB}
                : Stoner::Core::TArray<ERHIFormat>{
                      ERHIFormat::BC7_RGBA_UNorm,
                      ERHIFormat::BC3_RGBA_UNorm,
                      ERHIFormat::ASTC_4x4_RGBA_UNorm,
                      ERHIFormat::ETC2_RGBA8_UNorm,
                      ERHIFormat::R8G8B8A8_UNorm};
        }
        else
        {
            Profile.Name =
                Stoner::Core::FString("desktop.opaque-color");
            Profile.PreferredFormats = bSRGB
                ? Stoner::Core::TArray<ERHIFormat>{
                      ERHIFormat::BC1_RGBA_sRGB,
                      ERHIFormat::BC7_RGBA_sRGB,
                      ERHIFormat::ASTC_4x4_RGBA_sRGB,
                      ERHIFormat::ETC2_RGB8_sRGB,
                      ERHIFormat::R8G8B8A8_sRGB}
                : Stoner::Core::TArray<ERHIFormat>{
                      ERHIFormat::BC1_RGBA_UNorm,
                      ERHIFormat::BC7_RGBA_UNorm,
                      ERHIFormat::ASTC_4x4_RGBA_UNorm,
                      ERHIFormat::ETC2_RGB8_UNorm,
                      ERHIFormat::R8G8B8A8_UNorm};
        }
        return Profile;
    }

    if (Info.ColorSpace != EImageColorSpace::Linear)
    {
        return Profile;
    }
    if (Info.Semantic == ETextureSemantic::Normal ||
        (Info.Semantic == ETextureSemantic::Data &&
         Info.SourceChannelCount == 2))
    {
        Profile.Name =
            Stoner::Core::FString("desktop.two-channel");
        Profile.PreferredFormats = {
            ERHIFormat::BC5_RG_UNorm,
            ERHIFormat::ASTC_4x4_RGBA_UNorm,
            ERHIFormat::EAC_RG11_UNorm,
            ERHIFormat::R8G8_UNorm};
    }
    else if (Info.Semantic == ETextureSemantic::Data &&
             Info.SourceChannelCount == 1)
    {
        Profile.Name =
            Stoner::Core::FString("desktop.one-channel");
        Profile.PreferredFormats = {
            ERHIFormat::BC4_R_UNorm,
            ERHIFormat::EAC_R11_UNorm,
            ERHIFormat::R8_UNorm};
    }
    else if (Info.Semantic == ETextureSemantic::Data &&
             (Info.SourceChannelCount == 3 ||
              Info.SourceChannelCount == 4))
    {
        Profile.Name =
            Stoner::Core::FString("desktop.rgba-data");
        Profile.PreferredFormats = {
            ERHIFormat::ASTC_4x4_RGBA_UNorm,
            ERHIFormat::R8G8B8A8_UNorm};
    }
    return Profile;
}

bool TryMapTextureTranscodeFormat(
    ERHIFormat Format,
    ETextureTranscodeFormat& OutTranscodeFormat) noexcept
{
    switch (Format)
    {
    case ERHIFormat::BC1_RGBA_UNorm:
        OutTranscodeFormat =
            ETextureTranscodeFormat::BC1_RGBA_UNorm;
        return true;
    case ERHIFormat::BC1_RGBA_sRGB:
        OutTranscodeFormat =
            ETextureTranscodeFormat::BC1_RGBA_SRGB;
        return true;
    case ERHIFormat::BC3_RGBA_UNorm:
        OutTranscodeFormat =
            ETextureTranscodeFormat::BC3_RGBA_UNorm;
        return true;
    case ERHIFormat::BC3_RGBA_sRGB:
        OutTranscodeFormat =
            ETextureTranscodeFormat::BC3_RGBA_SRGB;
        return true;
    case ERHIFormat::BC4_R_UNorm:
        OutTranscodeFormat =
            ETextureTranscodeFormat::BC4_R_UNorm;
        return true;
    case ERHIFormat::BC5_RG_UNorm:
        OutTranscodeFormat =
            ETextureTranscodeFormat::BC5_RG_UNorm;
        return true;
    case ERHIFormat::BC7_RGBA_UNorm:
        OutTranscodeFormat =
            ETextureTranscodeFormat::BC7_RGBA_UNorm;
        return true;
    case ERHIFormat::BC7_RGBA_sRGB:
        OutTranscodeFormat =
            ETextureTranscodeFormat::BC7_RGBA_SRGB;
        return true;
    case ERHIFormat::ETC2_RGB8_UNorm:
        OutTranscodeFormat =
            ETextureTranscodeFormat::ETC2_RGB8_UNorm;
        return true;
    case ERHIFormat::ETC2_RGB8_sRGB:
        OutTranscodeFormat =
            ETextureTranscodeFormat::ETC2_RGB8_SRGB;
        return true;
    case ERHIFormat::ETC2_RGBA8_UNorm:
        OutTranscodeFormat =
            ETextureTranscodeFormat::ETC2_RGBA8_UNorm;
        return true;
    case ERHIFormat::ETC2_RGBA8_sRGB:
        OutTranscodeFormat =
            ETextureTranscodeFormat::ETC2_RGBA8_SRGB;
        return true;
    case ERHIFormat::EAC_R11_UNorm:
        OutTranscodeFormat =
            ETextureTranscodeFormat::EAC_R11_UNorm;
        return true;
    case ERHIFormat::EAC_RG11_UNorm:
        OutTranscodeFormat =
            ETextureTranscodeFormat::EAC_RG11_UNorm;
        return true;
    case ERHIFormat::ASTC_4x4_RGBA_UNorm:
        OutTranscodeFormat =
            ETextureTranscodeFormat::ASTC_4x4_RGBA_UNorm;
        return true;
    case ERHIFormat::ASTC_4x4_RGBA_sRGB:
        OutTranscodeFormat =
            ETextureTranscodeFormat::ASTC_4x4_RGBA_SRGB;
        return true;
    case ERHIFormat::R8_UNorm:
        OutTranscodeFormat =
            ETextureTranscodeFormat::R8_UNorm;
        return true;
    case ERHIFormat::R8G8_UNorm:
        OutTranscodeFormat =
            ETextureTranscodeFormat::R8G8_UNorm;
        return true;
    case ERHIFormat::R8G8B8A8_UNorm:
        OutTranscodeFormat =
            ETextureTranscodeFormat::R8G8B8A8_UNorm;
        return true;
    case ERHIFormat::R8G8B8A8_sRGB:
        OutTranscodeFormat =
            ETextureTranscodeFormat::R8G8B8A8_SRGB;
        return true;
    case ERHIFormat::B8G8R8A8_UNorm:
    case ERHIFormat::R10G10B10A2_UNorm:
    case ERHIFormat::R16G16B16A16_Float:
    case ERHIFormat::R32_Float:
    case ERHIFormat::R32G32_Float:
    case ERHIFormat::R32G32B32_Float:
    case ERHIFormat::R32G32B32A32_Float:
    case ERHIFormat::D24_UNorm_S8_UInt:
    case ERHIFormat::D32_Float:
    case ERHIFormat::S8_UInt:
    case ERHIFormat::Unknown:
    case ERHIFormat::Count:
        OutTranscodeFormat = ETextureTranscodeFormat::Unknown;
        return false;
    }
    OutTranscodeFormat = ETextureTranscodeFormat::Unknown;
    return false;
}

FTextureTargetSelection SelectTextureTarget(
    const FKTX2TextureInfo& Info,
    const FTextureTargetProfile& Profile,
    const Stoner::RHI::FRHIDeviceCapabilities& Capabilities)
{
    FTextureTargetSelection Selection;
    if (Profile.Validate() != ERHIResult::Success ||
        !Capabilities.HasValidFormatCapabilities() ||
        Info.SourceChannelCount == 0 ||
        Info.SourceChannelCount > 4 ||
        Info.Semantic == ETextureSemantic::Unspecified)
    {
        Selection.Result = ERHIResult::InvalidState;
        return Selection;
    }

    const bool bBasis = IsBasisArtifact(Info);
    const ERHIFormat StoredFormat = GetStoredRHIFormat(Info);
    const auto RequiredUsage =
        Stoner::RHI::ERHIFormatCapability::SampledImage |
        Stoner::RHI::ERHIFormatCapability::CopyDestination;

    for (ERHIFormat Format : Profile.PreferredFormats)
    {
        ETextureTranscodeFormat TranscodeFormat =
            ETextureTranscodeFormat::Unknown;
        const bool bHasTranscodeMapping =
            TryMapTextureTranscodeFormat(
                Format, TranscodeFormat);
        const FRHIFormatInfo FormatInfo =
            Stoner::RHI::GetRHIFormatInfo(Format);
        const FTargetFacts Facts = GetTargetFacts(Format);

        if (bBasis && !bHasTranscodeMapping)
        {
            Selection.Candidates.push_back(Reject(
                Format, TranscodeFormat,
                "renderer.texture-target.codec",
                "artifact codec cannot produce candidate format"));
            continue;
        }
        if (!bBasis && Format != StoredFormat &&
            !CanExpandStoredFormat(Info, Format))
        {
            Selection.Candidates.push_back(Reject(
                Format, TranscodeFormat,
                "renderer.texture-target.stored-format",
                "uncompressed artifact does not store candidate format"));
            continue;
        }
        if (bBasis && !FormatInfo.bCompressed &&
            !Profile.bAllowUncompressedFallback)
        {
            Selection.Candidates.push_back(Reject(
                Format, TranscodeFormat,
                "renderer.texture-target.fallback-disabled",
                "profile disables transient uncompressed fallback"));
            continue;
        }
        if (FormatInfo.bSRGB !=
            (Info.ColorSpace == EImageColorSpace::SRGB))
        {
            Selection.Candidates.push_back(Reject(
                Format, TranscodeFormat,
                "renderer.texture-target.transfer",
                "candidate does not preserve artifact transfer"));
            continue;
        }
        if (Info.Semantic != ETextureSemantic::Color &&
            FormatInfo.bSRGB)
        {
            Selection.Candidates.push_back(Reject(
                Format, TranscodeFormat,
                "renderer.texture-target.transfer",
                "normal and data targets must remain linear"));
            continue;
        }
        if (Info.AlphaMode == EImageAlphaMode::Straight &&
            !Facts.bPreservesAlpha)
        {
            Selection.Candidates.push_back(Reject(
                Format, TranscodeFormat,
                "renderer.texture-target.alpha",
                "candidate drops required straight alpha"));
            continue;
        }
        if (!HasRequiredChannels(Info, Facts))
        {
            Selection.Candidates.push_back(Reject(
                Format, TranscodeFormat,
                "renderer.texture-target.channels",
                "candidate drops semantic-required channels"));
            continue;
        }
        if (!Capabilities.SupportsFormatUsage(
                Format, RequiredUsage))
        {
            Selection.Candidates.push_back(Reject(
                Format, TranscodeFormat,
                "renderer.texture-target.usage",
                "candidate lacks sampled-image or copy-destination usage"));
            continue;
        }

        Selection.Candidates.push_back({
            Format,
            TranscodeFormat,
            true,
            Stoner::Core::FString(
                "renderer.texture-target.selected"),
            Stoner::Core::FString(
                "candidate selected by explicit profile order")});
        Selection.Result = ERHIResult::Success;
        Selection.SelectedFormat = Format;
        Selection.TranscodeFormat = TranscodeFormat;
        return Selection;
    }
    Selection.Result = ERHIResult::Unsupported;
    return Selection;
}

} // namespace Stoner::Renderer
