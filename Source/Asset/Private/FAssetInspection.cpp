#include "Asset/FAssetInspection.h"

#include "Asset/FAssetExtensionRegistry.h"

#include <algorithm>
#include <string>

namespace Stoner::Asset
{

Core::FString FAssetInspection::Format(const FAssetId& Id)
{
    return Id.ToString();
}

Core::FString FAssetInspection::Format(const FAssetDigest& Digest)
{
    return Digest.IsAvailable() ? Digest.ToLowerHex() : Core::FString("unavailable");
}

Core::FString FAssetInspection::Format(const FAssetVersion& Version)
{
    std::string Text =
        "source=" + Format(Version.SourceDigest).ToStdString() +
        ";content=" + Format(Version.ContentDigest).ToStdString() +
        ";cook=" + Format(Version.CookDigest).ToStdString();
    if (Version.Producer.IsValid())
    {
        Text += ";producer=" + Version.Producer.ToString().ToStdString();
    }
    if (Version.ProducerVersion.IsValid())
    {
        Text += ";producer-version=" + Version.ProducerVersion.ToString().ToStdString();
    }
    if (Version.TargetProfile)
    {
        Text += ";target=" + Version.TargetProfile->ToStdString();
    }
    return Core::FString(std::move(Text));
}

Core::FString FAssetInspection::Format(const FAssetDependency& Dependency)
{
    return Core::FString(
        Dependency.TargetId.ToString().ToStdString() +
        "|role=" + std::to_string(static_cast<int>(Dependency.Role)) +
        "|strength=" + std::to_string(static_cast<int>(Dependency.Strength)) +
        "|resolution=" + std::to_string(static_cast<int>(Dependency.Resolution)));
}

Core::FString FAssetInspection::Format(const FAssetMetadata& Metadata)
{
    std::string Text = Metadata.Id.ToString().ToStdString() +
        "|source=" + Metadata.Source.ToString().ToStdString() +
        "|producer=" + Metadata.Producer.ToString().ToStdString() +
        "@" + Metadata.ProducerVersion.ToString().ToStdString();
    auto Attributes = Metadata.Attributes;
    std::sort(Attributes.begin(), Attributes.end());
    for (const FAssetAttribute& Attribute : Attributes)
    {
        Text += "|attr:" + Attribute.first.ToStdString() + "=" +
            Attribute.second.ToStdString();
    }
    auto Dependencies = Metadata.Dependencies;
    std::sort(
        Dependencies.begin(),
        Dependencies.end(),
        [](const FAssetDependency& Left, const FAssetDependency& Right)
        {
            return Left.TargetId < Right.TargetId;
        });
    for (const FAssetDependency& Dependency : Dependencies)
    {
        Text += "|dep:" + Format(Dependency).ToStdString();
    }
    return Core::FString(std::move(Text));
}

Core::FString FAssetInspection::Format(const FAssetRegistrySnapshot& Snapshot)
{
    std::string Text = "revision=" + std::to_string(Snapshot.Revision);
    for (const FAssetMetadata& Metadata : Snapshot.Records)
    {
        Text += "\n" + Format(Metadata).ToStdString();
    }
    return Core::FString(std::move(Text));
}

Core::FString FAssetInspection::FormatCapabilities(
    const Core::TArray<FAssetExtensionCapability>& Capabilities)
{
    auto Sorted = Capabilities;
    std::sort(
        Sorted.begin(),
        Sorted.end(),
        [](const FAssetExtensionCapability& Left, const FAssetExtensionCapability& Right)
        {
            return Left.Participant < Right.Participant;
        });
    std::string Text;
    for (const FAssetExtensionCapability& Capability : Sorted)
    {
        if (!Text.empty())
        {
            Text.push_back('\n');
        }
        Text += Capability.Participant.ToString().ToStdString();
    }
    return Core::FString(std::move(Text));
}

Core::FString FAssetInspection::FormatAmbiguity(
    EAssetExtensionKind Kind,
    const Core::TArray<FAssetExtensionCapability>& Candidates)
{
    const char* KindText = Kind == EAssetExtensionKind::Resolver
        ? "resolver"
        : Kind == EAssetExtensionKind::Importer
            ? "importer"
            : Kind == EAssetExtensionKind::Loader ? "loader" : "cooker";
    const Core::FString ParticipantText = FormatCapabilities(Candidates);
    return Core::FString(
        std::string("ambiguous-") + KindText + "\n" +
        ParticipantText.ToStdString());
}

} // namespace Stoner::Asset
