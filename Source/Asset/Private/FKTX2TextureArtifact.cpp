#include "Asset/FKTX2TextureArtifact.h"

namespace Stoner::Asset
{

EAssetResult FKTX2TextureArtifact::Create(
    FAssetId Id,
    FKTX2TextureInfo Info,
    Core::TArray<Core::uint8> Bytes,
    FKTX2TextureArtifact& OutArtifact)
{
    OutArtifact = {};
    if (!Id.IsValid() ||
        Id.GetAssetType() != Core::FString("Texture") ||
        Info.TextureId != Id || Bytes.empty() ||
        !Info.SourceDigest.IsAvailable() ||
        !Info.ContentDigest.IsAvailable() ||
        !Info.CookRevision.IsAvailable() ||
        Info.ProducerVersion.IsEmpty() ||
        Info.PortableProfile.IsEmpty() ||
        !Info.BaseExtent.IsValid() ||
        Info.Levels.empty())
    {
        return EAssetResult::InvalidInput;
    }

    const FAssetDigest Digest = FAssetDigest::FromBytes(Bytes);
    if (Info.ArtifactDigest.IsAvailable() &&
        Info.ArtifactDigest != Digest)
    {
        return EAssetResult::Conflict;
    }
    Info.ArtifactDigest = Digest;

    FKTX2TextureArtifact Artifact;
    Artifact.Id_ = std::move(Id);
    Artifact.Info_ = std::move(Info);
    Artifact.Bytes_ = std::move(Bytes);
    OutArtifact = std::move(Artifact);
    return EAssetResult::Success;
}

Core::FString FKTX2TextureArtifact::GetAssetType() const
{
    return Core::FString("Texture");
}

const FAssetId& FKTX2TextureArtifact::GetId() const noexcept
{
    return Id_;
}

const FKTX2TextureInfo& FKTX2TextureArtifact::GetInfo() const noexcept
{
    return Info_;
}

std::span<const Core::uint8>
FKTX2TextureArtifact::GetBytes() const noexcept
{
    return Bytes_;
}

const FAssetDigest&
FKTX2TextureArtifact::GetArtifactDigest() const noexcept
{
    return Info_.ArtifactDigest;
}

} // namespace Stoner::Asset
