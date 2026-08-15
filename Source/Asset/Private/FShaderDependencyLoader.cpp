#include "FShaderDependencyLoader.h"

#include "Asset/FAssetDigest.h"
#include "Asset/FAssetDispatch.h"
#include "Asset/FShaderPayloadAsset.h"
#include "Asset/FShaderSourceAsset.h"
#include "FShaderPayloadValidation.h"

#include <algorithm>
#include <map>
#include <string>

namespace Stoner::Asset::Private
{
namespace
{

bool IsRelativeLocator(const Core::FString& Locator)
{
    const std::string_view Value = Locator.View();
    if (Value.empty() || Value.front() == '/' || Value.front() == '\\' ||
        (Value.size() > 1 && Value[1] == ':'))
    {
        return false;
    }
    std::size_t Begin = 0;
    while (Begin <= Value.size())
    {
        const std::size_t End = Value.find_first_of("/\\", Begin);
        const std::string_view Segment = Value.substr(
            Begin,
            End == std::string_view::npos
                ? Value.size() - Begin
                : End - Begin);
        if (Segment.empty() || Segment == "." || Segment == "..")
        {
            return false;
        }
        if (End == std::string_view::npos)
        {
            break;
        }
        Begin = End + 1;
    }
    return true;
}

EAssetResult MakeDependencyLocation(
    const FAssetSourceLocator& Definition,
    const Core::FString& Relative,
    FAssetSourceLocator& Out)
{
    if (!IsRelativeLocator(Relative))
    {
        return EAssetResult::DependencyMismatch;
    }
    std::string Base = Definition.GetLocator().ToStdString();
    const std::size_t Slash = Base.find_last_of("/\\");
    Base = Slash == std::string::npos ? std::string{} : Base.substr(0, Slash + 1);
    std::string Child = Relative.ToStdString();
    std::replace(Child.begin(), Child.end(), '\\', '/');
    return FAssetSourceLocator::Create(
        Definition.GetScheme(),
        Core::FString(Base + Child),
        Out);
}

FAssetParticipantId Participant()
{
    FAssetParticipantId Value;
    (void)FAssetParticipantId::Create(
        Core::FString("stoner.material-shader.dependency"),
        Value);
    return Value;
}

FAssetProducerVersion ProducerVersion()
{
    FAssetProducerVersion Value;
    (void)FAssetProducerVersion::Create(Core::FString("023-v1"), Value);
    return Value;
}

FAssetMetadata Metadata(
    const FAssetId& Id,
    const FAssetVersion& Version,
    const FAssetSourceLocator& Source)
{
    FAssetMetadata Value;
    Value.Id = Id;
    Value.Version = Version;
    Value.Source = Source;
    Value.Producer = Participant();
    Value.ProducerVersion = ProducerVersion();
    return Value;
}

EAssetResult ResolveBytes(
    const FMaterialShaderLoadRequest& Request,
    const Core::FString& Relative,
    Core::uint64 MaximumBytes,
    Core::TArray<Core::uint8>& OutBytes,
    FAssetSourceLocator& OutLocation,
    FAssetDiagnosticList* Diagnostics)
{
    OutBytes.clear();
    if (!Request.Extensions ||
        MakeDependencyLocation(
            Request.Descriptor.Location, Relative, OutLocation) !=
            EAssetResult::Success)
    {
        return EAssetResult::DependencyMismatch;
    }
    FAssetResolveResult Resolved = FAssetDispatch::Resolve(
        *Request.Extensions,
        {OutLocation, {}},
        Diagnostics);
    if (Resolved.Result != EAssetResult::Success)
    {
        return Resolved.Result;
    }
    EAssetResult Result = Resolved.Source.ReadBounded(
        MaximumBytes,
        Resolved.Descriptor.Size,
        OutBytes);
    if (Result == EAssetResult::ImageLimitExceeded)
    {
        Result = EAssetResult::DefinitionLimitExceeded;
    }
    return Result;
}

} // namespace

EAssetResult LoadShaderDependencies(
    const FMaterialShaderLoadRequest& Request,
    FShaderAssetDesc& Desc,
    Core::TArray<Core::TSharedPtr<const FAssetPayload>>& OutPayloads,
    Core::TArray<FAssetMetadata>& OutMetadata,
    FAssetDiagnosticList* Diagnostics)
{
    OutPayloads.clear();
    OutMetadata.clear();
    if (!Request.bLoadDependencies)
    {
        return EAssetResult::Success;
    }

    Core::uint64 AggregateBytes = 0;
    std::map<FAssetId, FAssetDigest> Seen;
    for (const FShaderSourceReference& Reference : Desc.Stages)
    {
        const FAssetId* Id = Reference.Source.GetId()
            ? &*Reference.Source.GetId()
            : nullptr;
        if (!Id)
        {
            return EAssetResult::DependencyMismatch;
        }
        const auto Existing = Seen.find(*Id);
        if (Existing != Seen.end())
        {
            if (Existing->second != Reference.ExpectedDigest)
            {
                return EAssetResult::DependencyMismatch;
            }
            continue;
        }
        Core::TArray<Core::uint8> Bytes;
        FAssetSourceLocator Location;
        EAssetResult Result = ResolveBytes(
            Request,
            Reference.Locator,
            Request.Limits.MaxShaderSourceBytes,
            Bytes,
            Location,
            Diagnostics);
        if (Result != EAssetResult::Success)
        {
            return Result;
        }
        const FAssetDigest Digest = FAssetDigest::FromBytes(Bytes);
        if (Digest != Reference.ExpectedDigest ||
            !CheckedMaterialShaderAdd(
                AggregateBytes, Bytes.size(), AggregateBytes) ||
            AggregateBytes > Request.Limits.MaxProgramDependencyBytes)
        {
            return EAssetResult::DependencyMismatch;
        }
        FAssetVersion Version;
        Version.SourceDigest = Digest;
        Version.ContentDigest = Digest;
        FShaderSourceAsset Source;
        Result = FShaderSourceAsset::Create(
            *Id,
            Version,
            Reference.Language,
            std::move(Bytes),
            Source);
        if (Result != EAssetResult::Success)
        {
            return EAssetResult::DependencyMismatch;
        }
        Seen.emplace(*Id, Digest);
        OutMetadata.push_back(Metadata(*Id, Version, Location));
        OutPayloads.push_back(
            Core::MakeShared<FShaderSourceAsset>(std::move(Source)));
    }

    for (const FShaderVariantDefinition& Variant : Desc.Variants)
    {
        for (const FShaderPayloadReference& Reference : Variant.Payloads)
        {
            const FAssetId* Id = Reference.Payload.GetId()
                ? &*Reference.Payload.GetId()
                : nullptr;
            if (!Id)
            {
                return EAssetResult::DependencyMismatch;
            }
            const auto Existing = Seen.find(*Id);
            if (Existing != Seen.end())
            {
                if (Existing->second != Reference.ExpectedDigest)
                {
                    return EAssetResult::DependencyMismatch;
                }
                continue;
            }
            Core::TArray<Core::uint8> Bytes;
            FAssetSourceLocator Location;
            EAssetResult Result = ResolveBytes(
                Request,
                Reference.Locator,
                Request.Limits.MaxShaderPayloadBytes,
                Bytes,
                Location,
                Diagnostics);
            if (Result != EAssetResult::Success)
            {
                return Result;
            }
            const FAssetDigest Digest = FAssetDigest::FromBytes(Bytes);
            if (Digest != Reference.ExpectedDigest ||
                !CheckedMaterialShaderAdd(
                    AggregateBytes, Bytes.size(), AggregateBytes) ||
                AggregateBytes > Request.Limits.MaxProgramDependencyBytes ||
                ValidateShaderPayloadBytes(
                    Bytes,
                    Reference.Format,
                    Reference.Stage,
                    Reference.EntryPoint) != EAssetResult::Success)
            {
                return EAssetResult::DependencyMismatch;
            }
            FAssetVersion Version;
            Version.SourceDigest = Digest;
            Version.ContentDigest = Digest;
            FShaderPayloadAsset Payload;
            Result = FShaderPayloadAsset::Create(
                *Id,
                Version,
                Reference.Backend,
                Reference.Profile,
                Reference.Format,
                Reference.Stage,
                Reference.EntryPoint,
                Reference.Permutation,
                std::move(Bytes),
                Payload);
            if (Result != EAssetResult::Success)
            {
                return EAssetResult::DependencyMismatch;
            }
            Seen.emplace(*Id, Digest);
            OutMetadata.push_back(Metadata(*Id, Version, Location));
            OutPayloads.push_back(
                Core::MakeShared<FShaderPayloadAsset>(std::move(Payload)));
        }
    }
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
