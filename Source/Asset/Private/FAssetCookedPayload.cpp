#include "Asset/FAssetCookedPayload.h"

#include "Asset/FAssetParticipant.h"

#include <array>

namespace Stoner::Asset
{
namespace
{

bool IsKnownCodec(const Core::FString& Codec)
{
    static constexpr std::array<const char*, 10> Codecs = {
        "stoner.image",
        "stoner.texture",
        "stoner.ktx2",
        "stoner.shader-source",
        "stoner.shader-payload",
        "stoner.shader-program",
        "stoner.material",
        "stoner.material-instance",
        "stoner.static-mesh",
        "stoner.static-model"};
    for (const char* Candidate : Codecs)
    {
        if (Codec == Core::FString(Candidate))
        {
            return true;
        }
    }
    return false;
}

} // namespace

EAssetResult FAssetCookedPayloadLimits::Validate() const noexcept
{
    return MaxEnvelopeBytes > 0 &&
            MaxEnvelopeBytes <= CompiledMaxEnvelopeBytes &&
            MaxBodyBytes > 0 && MaxBodyBytes <= CompiledMaxEnvelopeBytes &&
            MaxHeaderBytes >= 64 &&
            MaxHeaderBytes <= CompiledMaxHeaderBytes
        ? EAssetResult::Success
        : EAssetResult::InvalidInput;
}

EAssetResult FAssetCookedPayloadHeader::Validate() const noexcept
{
    const bool bLegacySchema = CodecVersion == 1 && PayloadSchemaVersion == 1;
    const bool bMetalShaderSchema =
        CodecId == Core::FString("stoner.shader-payload") &&
        CodecVersion == 2 && PayloadSchemaVersion == 2;
    if (ContainerVersion != CurrentContainerVersion ||
        (!bLegacySchema && !bMetalShaderSchema))
    {
        return EAssetResult::UnsupportedSchema;
    }
    if (Flags != 0 || !IsKnownCodec(CodecId))
    {
        return EAssetResult::Unsupported;
    }
    if (!AssetId.IsValid() || AssetType != AssetId.GetAssetType() ||
        AssetType.IsEmpty() || AssetType.Len() > 255 ||
        CodecId.IsEmpty() || CodecId.Len() > 127 ||
        CodecVersion == 0 || BodyBytes == 0 ||
        !BodyDigest.IsAvailable())
    {
        return EAssetResult::InvalidInput;
    }
    FAssetParticipantId CodecToken;
    return FAssetParticipantId::Create(CodecId, CodecToken) ==
            EAssetResult::Success
        ? EAssetResult::Success
        : EAssetResult::InvalidInput;
}

} // namespace Stoner::Asset
