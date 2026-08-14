#include "Asset/FAssetCookedExtensions.h"

#include "Asset/FAssetCookContractCodec.h"
#include "Asset/FImageAsset.h"
#include "Asset/FKTX2TextureArtifact.h"
#include "Asset/FTextureAsset.h"
#include "Asset/IAssetCooker.h"
#include "Asset/IAssetLoader.h"
#include "Asset/FShaderPayloadAsset.h"
#include "FAssetTargetProfileCodec.h"

#include <array>
#include <memory>
#include <set>
#include <string>

namespace Stoner::Asset
{
namespace
{

const char* ParticipantName(
    EAssetCookedFamily Family,
    EAssetExtensionKind Kind)
{
    const char* Prefix = Kind == EAssetExtensionKind::Cooker
        ? "cooker.cooked-" : "loader.cooked-";
    switch (Family)
    {
    case EAssetCookedFamily::ImageTexture:
        return Kind == EAssetExtensionKind::Cooker
            ? "cooker.cooked-image-texture" : "loader.cooked-image-texture";
    case EAssetCookedFamily::MaterialShader:
        return Kind == EAssetExtensionKind::Cooker
            ? "cooker.cooked-material-shader" : "loader.cooked-material-shader";
    case EAssetCookedFamily::StaticModel:
        return Kind == EAssetExtensionKind::Cooker
            ? "cooker.cooked-static-model" : "loader.cooked-static-model";
    }
    return Prefix;
}

bool CodecMatches(EAssetCookedFamily Family, const Core::FString& Codec)
{
    switch (Family)
    {
    case EAssetCookedFamily::ImageTexture:
        return Codec == Core::FString("stoner.image") ||
            Codec == Core::FString("stoner.texture") ||
            Codec == Core::FString("stoner.ktx2");
    case EAssetCookedFamily::MaterialShader:
        return Codec == Core::FString("stoner.shader-source") ||
            Codec == Core::FString("stoner.shader-payload") ||
            Codec == Core::FString("stoner.shader-program") ||
            Codec == Core::FString("stoner.material") ||
            Codec == Core::FString("stoner.material-instance");
    case EAssetCookedFamily::StaticModel:
        return Codec == Core::FString("stoner.static-mesh") ||
            Codec == Core::FString("stoner.static-model");
    }
    return false;
}

FAssetExtensionCapability Capability(
    EAssetCookedFamily Family,
    EAssetExtensionKind Kind)
{
    FAssetExtensionCapability Result;
    Result.Kind = Kind;
    (void)FAssetParticipantId::Create(
        Core::FString(ParticipantName(Family, Kind)), Result.Participant);
    (void)FAssetProducerVersion::Create(
        Core::FString("025-v1"), Result.ProducerVersion);
    Result.Priority = 100;
    Result.FormatHints = {Core::FString("sgasset")};
    return Result;
}

Core::TArray<Core::FString> RelevantFields(EAssetCookedFamily Family)
{
    switch (Family)
    {
    case EAssetCookedFamily::ImageTexture:
        return {Core::FString("textureCapabilities"),
                Core::FString("textureFallback")};
    case EAssetCookedFamily::MaterialShader:
        return {Core::FString("graphicsBackend"),
                Core::FString("shaderPayloadChoices")};
    case EAssetCookedFamily::StaticModel:
        return {};
    }
    return {};
}

const char* BackendToken(EAssetGraphicsBackend Backend)
{
    switch (Backend)
    {
    case EAssetGraphicsBackend::Vulkan: return "vulkan";
    case EAssetGraphicsBackend::Metal: return "metal";
    case EAssetGraphicsBackend::DirectX12: return "dx12";
    case EAssetGraphicsBackend::OpenGL: return "opengl";
    case EAssetGraphicsBackend::GLES: return "gles";
    }
    return "unknown";
}

const char* ShaderFormatToken(EAssetShaderPayloadFormat Format)
{
    switch (Format)
    {
    case EAssetShaderPayloadFormat::SpirV: return "spirv";
    case EAssetShaderPayloadFormat::MSL: return "msl";
    case EAssetShaderPayloadFormat::DXIL: return "dxil";
    case EAssetShaderPayloadFormat::GLSL: return "glsl";
    case EAssetShaderPayloadFormat::ESSL: return "essl";
    }
    return "unknown";
}

bool IsKnownTextureCapability(const Core::FString& Capability)
{
    static const std::set<std::string_view> Known = {
        "astc-4x4-rgba", "bc1-rgba", "bc3-rgba", "bc4-r", "bc5-rg",
        "bc7-rgba", "eac-r", "eac-rg", "etc2-rgb", "etc2-rgba",
        "r8-unorm", "rg8-unorm", "rgb32-float", "rgba8-unorm",
        "rgba8-srgb", "rgba16-float", "rgba32-float"};
    return Known.contains(Capability.View());
}

const char* ImageFormatCapability(EImageTexelFormat Format)
{
    switch (Format)
    {
    case EImageTexelFormat::R8_UNorm: return "r8-unorm";
    case EImageTexelFormat::R8G8_UNorm: return "rg8-unorm";
    case EImageTexelFormat::R8G8B8_UNorm:
    case EImageTexelFormat::R8G8B8A8_UNorm: return "rgba8-unorm";
    case EImageTexelFormat::R32G32B32_Float: return "rgb32-float";
    case EImageTexelFormat::R16G16B16A16_Float: return "rgba16-float";
    case EImageTexelFormat::R32G32B32A32_Float: return "rgba32-float";
    case EImageTexelFormat::Unknown: return nullptr;
    }
    return nullptr;
}

bool IsFloatCapability(const Core::FString& Capability)
{
    return Capability == Core::FString("rgb32-float") ||
        Capability == Core::FString("rgba16-float") ||
        Capability == Core::FString("rgba32-float");
}

bool TextureCapabilitySupports(
    const FAssetPayload& Payload,
    const Core::FString& Capability)
{
    if (const auto* Image = dynamic_cast<const FImageAsset*>(&Payload))
        return IsImageFloatFormat(Image->GetBaseMip().GetFormat())
            ? IsFloatCapability(Capability) : !IsFloatCapability(Capability);
    if (const auto* Texture = dynamic_cast<const FTextureAsset*>(&Payload))
        return !Texture->GetMips().empty() &&
            (IsImageFloatFormat(Texture->GetMips().front().GetFormat())
                ? IsFloatCapability(Capability)
                : !IsFloatCapability(Capability));
    if (const auto* KTX = dynamic_cast<const FKTX2TextureArtifact*>(&Payload))
    {
        if (KTX->GetInfo().BasisModel != EKTX2BasisModel::None)
            return !IsFloatCapability(Capability);
        return KTX->GetInfo().StoredTexelFormat.has_value() &&
            ImageFormatCapability(*KTX->GetInfo().StoredTexelFormat) != nullptr &&
            Capability == Core::FString(
                ImageFormatCapability(*KTX->GetInfo().StoredTexelFormat));
    }
    return false;
}

bool ValidateGenericProducerSettings(
    const FAssetTargetProfile& Profile,
    const FAssetParticipantId& Producer)
{
    const FAssetProducerSettingsRecord* Record =
        Profile.BuildPolicy.FindProducer(Producer);
    if (!Record || Record->SchemaVersion != 1 || Record->Settings.size() != 1)
        return false;
    const FAssetProducerSetting* Policy =
        Record->Find(Core::FString("codecPolicy"));
    const auto* Value = Policy
        ? std::get_if<Core::FString>(&Policy->Value) : nullptr;
    return Value && *Value == Core::FString("exact-v1");
}

EAssetResult ResolveMaterialDecision(
    const FAssetPayload& Payload,
    const FAssetTargetProfile& Profile,
    FAssetCookedTargetDecision& Out)
{
    if (const auto* ShaderPayload =
            dynamic_cast<const FShaderPayloadAsset*>(&Payload))
    {
        for (Core::usize Index = 0;
             Index < Profile.ShaderPayloadChoices.size(); ++Index)
        {
            const auto& Choice = Profile.ShaderPayloadChoices[Index];
            if (static_cast<Core::uint8>(Choice.Backend) ==
                    static_cast<Core::uint8>(ShaderPayload->GetBackend()) &&
                Choice.Profile == ShaderPayload->GetProfile() &&
                static_cast<Core::uint8>(Choice.Format) ==
                    static_cast<Core::uint8>(ShaderPayload->GetFormat()))
            {
                Out.Selection = Core::FString(
                    std::string("shader:") + BackendToken(Choice.Backend) +
                    "/" + Choice.Profile.ToStdString() + "/" +
                    ShaderFormatToken(Choice.Format));
                Out.bUsedFallback = Index != 0;
                return EAssetResult::Success;
            }
        }
        return EAssetResult::Unsupported;
    }
    for (Core::usize Index = 0;
         Index < Profile.ShaderPayloadChoices.size(); ++Index)
    {
        const auto& Choice = Profile.ShaderPayloadChoices[Index];
        if (Choice.Backend == Profile.GraphicsBackend)
        {
            Out.Selection = Core::FString(
                std::string("shader-family:") + BackendToken(Choice.Backend) +
                "/" + Choice.Profile.ToStdString() + "/" +
                ShaderFormatToken(Choice.Format));
            Out.bUsedFallback = Index != 0;
            return EAssetResult::Success;
        }
    }
    return EAssetResult::Unsupported;
}

class FCookedAssetCooker final : public IAssetCooker
{
public:
    explicit FCookedAssetCooker(EAssetCookedFamily Family)
        : Family_(Family)
    {
    }

    [[nodiscard]] FAssetExtensionCapability GetCapability() const override
    {
        return Capability(Family_, EAssetExtensionKind::Cooker);
    }

    [[nodiscard]] EAssetResult GetRelevantProfileEvidence(
        const FAssetTargetProfileEvidence& Profile,
        FAssetProfileProjectionEvidence& OutEvidence) const override
    {
        const FAssetParticipantId Producer = GetCapability().Participant;
        if (!ValidateGenericProducerSettings(Profile.Profile, Producer))
        {
            OutEvidence = {};
            return EAssetResult::InvalidInput;
        }
        return Private::BuildAssetProfileProjection(
            Profile, Producer, 1, RelevantFields(Family_), OutEvidence);
    }

    [[nodiscard]] FAssetCookResult Cook(
        const FAssetCookRequest& Request) override
    {
        FAssetCookResult Result;
        Result.TargetProfile = Request.TargetProfileEvidence
            ? Request.TargetProfileEvidence->Profile.DisplayName
            : Request.TargetProfile;
        Result.TargetProfileEvidence = Request.TargetProfileEvidence;
        if (!Request.Payload || !Request.TargetProfileEvidence ||
            Request.Metadata.Validate() != EAssetResult::Success ||
            GetRelevantProfileEvidence(
                *Request.TargetProfileEvidence,
                Result.ProfileProjection) != EAssetResult::Success)
        {
            Result.Result = EAssetResult::InvalidInput;
            return Result;
        }
        FAssetCookedTargetDecision TargetDecision;
        if (ResolveAssetCookedTargetDecision(
                Family_, *Request.Payload,
                Request.TargetProfileEvidence->Profile,
                TargetDecision) != EAssetResult::Success)
        {
            Result.Result = EAssetResult::Unsupported;
            return Result;
        }
        FAssetCookedPayloadEnvelope Envelope;
        Result.Result = FAssetCookContractCodec::WriteTypedPayload(
            *Request.Payload, {}, Result.Artifact, &Envelope);
        if (Result.Result != EAssetResult::Success ||
            Envelope.Header.AssetId != Request.Metadata.Id ||
            !CodecMatches(Family_, Envelope.Header.CodecId))
        {
            Result.Artifact.clear();
            Result.Result = Result.Result == EAssetResult::Success
                ? EAssetResult::TypeMismatch : Result.Result;
            return Result;
        }
        Result.CookDigest = Envelope.EnvelopeDigest;
        Result.Payload = Request.Payload;
        return Result;
    }

private:
    EAssetCookedFamily Family_;
};

class FCookedAssetLoader final : public IAssetLoader
{
public:
    explicit FCookedAssetLoader(EAssetCookedFamily Family)
        : Family_(Family)
    {
    }

    [[nodiscard]] FAssetExtensionCapability GetCapability() const override
    {
        return Capability(Family_, EAssetExtensionKind::Loader);
    }

    [[nodiscard]] FAssetLoadResult Load(
        const FAssetLoadRequest& Request) override
    {
        FAssetLoadResult Result;
        Result.TargetProfileEvidence = Request.TargetProfileEvidence;
        if (Request.Metadata.Validate() != EAssetResult::Success ||
            !Request.Source.IsValid())
        {
            Result.Result = EAssetResult::InvalidInput;
            return Result;
        }
        Core::TArray<Core::uint8> Bytes;
        Result.Result = Request.Source.ReadBounded(
            FAssetCookedPayloadLimits::CompiledMaxEnvelopeBytes,
            std::nullopt, Bytes);
        if (Result.Result != EAssetResult::Success) return Result;
        FAssetCookedPayloadEnvelope Envelope;
        Result.Result = FAssetCookContractCodec::LoadTypedPayload(
            Bytes, {}, Result.Payload, &Envelope);
        if (Result.Result != EAssetResult::Success ||
            Envelope.Header.AssetId != Request.Metadata.Id ||
            !CodecMatches(Family_, Envelope.Header.CodecId))
        {
            Result.Payload.reset();
            Result.Result = Result.Result == EAssetResult::Success
                ? EAssetResult::TypeMismatch : Result.Result;
        }
        return Result;
    }

private:
    EAssetCookedFamily Family_;
};

} // namespace

EAssetResult FAssetCookedTargetDecision::Validate() const noexcept
{
    return Selection.IsEmpty()
        ? EAssetResult::InvalidInput : EAssetResult::Success;
}

EAssetResult ResolveAssetCookedTargetDecision(
    EAssetCookedFamily Family,
    const FAssetPayload& Payload,
    const FAssetTargetProfile& Profile,
    FAssetCookedTargetDecision& OutDecision)
{
    OutDecision = {};
    if (Profile.Validate() != EAssetResult::Success)
        return EAssetResult::InvalidInput;
    switch (Family)
    {
    case EAssetCookedFamily::ImageTexture:
        for (const Core::FString& Capability : Profile.TextureCapabilities)
            if (!IsKnownTextureCapability(Capability))
                return EAssetResult::Unsupported;
        for (Core::usize Index = 0;
             Index < Profile.TextureCapabilities.size(); ++Index)
        {
            const auto& Capability = Profile.TextureCapabilities[Index];
            if (!TextureCapabilitySupports(Payload, Capability)) continue;
            OutDecision.Selection = Core::FString(
                "texture:" + Capability.ToStdString());
            OutDecision.bUsedFallback = Index != 0;
            return EAssetResult::Success;
        }
        if (Profile.TextureFallback == EAssetTextureFallback::Uncompressed &&
            (dynamic_cast<const FImageAsset*>(&Payload) ||
             dynamic_cast<const FTextureAsset*>(&Payload)))
        {
            OutDecision.Selection = Core::FString("texture-fallback:uncompressed");
            OutDecision.bUsedFallback = true;
            return EAssetResult::Success;
        }
        if (Profile.TextureFallback == EAssetTextureFallback::PortableKTX2 &&
            dynamic_cast<const FKTX2TextureArtifact*>(&Payload))
        {
            OutDecision.Selection = Core::FString("texture-fallback:portable-ktx2");
            OutDecision.bUsedFallback = true;
            return EAssetResult::Success;
        }
        return EAssetResult::Unsupported;
    case EAssetCookedFamily::MaterialShader:
        return ResolveMaterialDecision(Payload, Profile, OutDecision);
    case EAssetCookedFamily::StaticModel:
        OutDecision.Selection = Core::FString("target-independent");
        return EAssetResult::Success;
    }
    return EAssetResult::Unsupported;
}

void FAssetCookedExtensionRegistrations::Reset() noexcept
{
    Tokens_.clear();
}

bool FAssetCookedExtensionRegistrations::IsComplete() const noexcept
{
    return Tokens_.size() == 6;
}

EAssetResult GetAssetCookedFamily(
    const Core::FString& AssetType,
    EAssetCookedFamily& OutFamily) noexcept
{
    if (AssetType == Core::FString("Image") ||
        AssetType == Core::FString("Texture"))
        OutFamily = EAssetCookedFamily::ImageTexture;
    else if (AssetType == Core::FString("ShaderSource") ||
             AssetType == Core::FString("ShaderPayload") ||
             AssetType == Core::FString("ShaderProgram") ||
             AssetType == Core::FString("Material") ||
             AssetType == Core::FString("MaterialInstance"))
        OutFamily = EAssetCookedFamily::MaterialShader;
    else if (AssetType == Core::FString("StaticMesh") ||
             AssetType == Core::FString("StaticModel"))
        OutFamily = EAssetCookedFamily::StaticModel;
    else return EAssetResult::Unsupported;
    return EAssetResult::Success;
}

EAssetResult GetAssetCookedParticipant(
    EAssetCookedFamily Family,
    EAssetExtensionKind Kind,
    FAssetParticipantId& OutParticipant) noexcept
{
    OutParticipant = {};
    if (Kind != EAssetExtensionKind::Cooker &&
        Kind != EAssetExtensionKind::Loader)
        return EAssetResult::InvalidInput;
    return FAssetParticipantId::Create(
        Core::FString(ParticipantName(Family, Kind)), OutParticipant);
}

EAssetResult RegisterCookedAssetExtensions(
    FAssetExtensionRegistry& Registry,
    FAssetCookedExtensionRegistrations& OutRegistrations)
{
    OutRegistrations.Reset();
    try
    {
        for (const EAssetCookedFamily Family : {
                 EAssetCookedFamily::ImageTexture,
                 EAssetCookedFamily::MaterialShader,
                 EAssetCookedFamily::StaticModel})
        {
            FAssetRegistrationToken CookerToken;
            EAssetResult Result = Registry.Register(
                Core::MakeShared<FCookedAssetCooker>(Family), CookerToken);
            if (Result != EAssetResult::Success)
            {
                OutRegistrations.Reset();
                return Result;
            }
            OutRegistrations.Tokens_.push_back(std::move(CookerToken));
            FAssetRegistrationToken LoaderToken;
            Result = Registry.Register(
                Core::MakeShared<FCookedAssetLoader>(Family), LoaderToken);
            if (Result != EAssetResult::Success)
            {
                OutRegistrations.Reset();
                return Result;
            }
            OutRegistrations.Tokens_.push_back(std::move(LoaderToken));
        }
        return EAssetResult::Success;
    }
    catch (const std::bad_alloc&)
    {
        OutRegistrations.Reset();
        return EAssetResult::CapacityExceeded;
    }
}

} // namespace Stoner::Asset
