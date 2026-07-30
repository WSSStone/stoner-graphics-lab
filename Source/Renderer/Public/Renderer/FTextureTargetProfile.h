#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::Asset
{
enum class ETextureTranscodeFormat : Core::uint8;
struct FKTX2TextureInfo;
}

namespace Stoner::RHI
{
enum class ERHIFormat;
enum class ERHIResult;
struct FRHIDeviceCapabilities;
}

namespace Stoner::Renderer
{

struct FTextureTargetProfile
{
    Stoner::Core::FString Name;
    Stoner::Core::TArray<Stoner::RHI::ERHIFormat>
        PreferredFormats;
    bool bAllowUncompressedFallback = true;

    [[nodiscard]] Stoner::RHI::ERHIResult
        Validate() const noexcept;

    [[nodiscard]] static FTextureTargetProfile DesktopDefault(
        const Stoner::Asset::FKTX2TextureInfo& Info);
};

struct FTextureTargetCandidateDiagnostic
{
    Stoner::RHI::ERHIFormat Format =
        {};
    Stoner::Asset::ETextureTranscodeFormat TranscodeFormat =
        {};
    bool bAccepted = false;
    Stoner::Core::FString Code;
    Stoner::Core::FString Reason;
};

struct FTextureTargetSelection
{
    Stoner::RHI::ERHIResult Result =
        {};
    Stoner::RHI::ERHIFormat SelectedFormat =
        {};
    Stoner::Asset::ETextureTranscodeFormat TranscodeFormat =
        {};
    Stoner::Core::TArray<FTextureTargetCandidateDiagnostic>
        Candidates;
};

[[nodiscard]] bool TryMapTextureTranscodeFormat(
    Stoner::RHI::ERHIFormat Format,
    Stoner::Asset::ETextureTranscodeFormat&
        OutTranscodeFormat) noexcept;

[[nodiscard]] FTextureTargetSelection SelectTextureTarget(
    const Stoner::Asset::FKTX2TextureInfo& Info,
    const FTextureTargetProfile& Profile,
    const Stoner::RHI::FRHIDeviceCapabilities&
        Capabilities);

} // namespace Stoner::Renderer
