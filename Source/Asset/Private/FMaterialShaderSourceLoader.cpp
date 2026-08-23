#include "Asset/FMaterialShaderSourceLoader.h"

#include "Asset/FAssetDispatch.h"
#include "Asset/FMaterialAsset.h"
#include "Asset/FMaterialInstanceAsset.h"
#include "Asset/FShaderAsset.h"
#include "FMaterialDependencyExtractor.h"
#include "FMaterialShaderJsonCodec.h"
#include "FMaterialShaderSchemaValidator.h"
#include "FShaderDependencyLoader.h"

#include <algorithm>

namespace Stoner::Asset
{
namespace
{

FAssetParticipantId MaterialShaderParticipant()
{
    FAssetParticipantId Participant;
    (void)FAssetParticipantId::Create(
        Core::FString("stoner.material-shader.source"),
        Participant);
    return Participant;
}

FAssetProducerVersion MaterialShaderProducerVersion()
{
    FAssetProducerVersion Version;
    (void)FAssetProducerVersion::Create(
        Core::FString("024-material-v2"),
        Version);
    return Version;
}

template <typename T>
FAssetMetadata BuildMetadata(
    const T& Asset,
    const FAssetSourceDescriptor& Descriptor,
    const Core::TArray<FAssetDependency>& Dependencies)
{
    FAssetMetadata Metadata;
    Metadata.Id = Asset.GetDesc().Id;
    Metadata.Version = Asset.GetDesc().Version;
    Metadata.Source = Descriptor.Location;
    Metadata.Producer = MaterialShaderParticipant();
    Metadata.ProducerVersion = MaterialShaderProducerVersion();
    Metadata.Dependencies = Dependencies;
    return Metadata;
}

} // namespace

FMaterialShaderLoadResult FMaterialShaderSourceLoader::Load(
    const FMaterialShaderLoadRequest& Request)
{
    FMaterialShaderLoadResult Result;
    if (!Request.Source.IsValid() ||
        !Request.Descriptor.Location.IsValid() ||
        Request.Limits.Validate() != EAssetResult::Success)
    {
        Result.Result = EAssetResult::InvalidInput;
        return Result;
    }
    Core::TArray<Core::uint8> Bytes;
    Result.Result = Request.Source.ReadBounded(
        Request.Limits.MaxDefinitionBytes,
        Request.Descriptor.Size,
        Bytes);
    if (Result.Result == EAssetResult::ImageLimitExceeded)
    {
        Result.Result = EAssetResult::DefinitionLimitExceeded;
    }
    if (Result.Result != EAssetResult::Success)
    {
        return Result;
    }

    Private::FMaterialShaderDefinition Definition;
    Result.Result = Private::ParseMaterialShaderDefinition(
        Bytes,
        Request.Limits,
        Definition,
        &Result.Diagnostics);
    if (Result.Result != EAssetResult::Success)
    {
        return Result;
    }
    Result.Result = Private::WriteMaterialShaderDefinition(
        Definition,
        Result.CanonicalDefinition,
        &Result.Diagnostics);
    if (Result.Result != EAssetResult::Success)
    {
        return Result;
    }
    if (Definition.Kind ==
        Private::EMaterialShaderDefinitionKind::Shader)
    {
        auto& ShaderDesc = std::get<FShaderAssetDesc>(Definition.Value);
        (void)Private::ExtractShaderDependencies(ShaderDesc);
        Core::TArray<Core::TSharedPtr<const FAssetPayload>>
            DependencyPayloads;
        Core::TArray<FAssetMetadata> DependencyMetadata;
        Result.Result = Private::LoadShaderDependencies(
            Request,
            ShaderDesc,
            DependencyPayloads,
            DependencyMetadata,
            &Result.Diagnostics);
        if (Result.Result != EAssetResult::Success)
        {
            Result.CanonicalDefinition = {};
            return Result;
        }
        Result.Payloads = std::move(DependencyPayloads);
        Result.Metadata = std::move(DependencyMetadata);
    }
    Result.Result = Private::ValidateMaterialShaderDefinition(
        Definition,
        &Result.Diagnostics);
    if (Result.Result != EAssetResult::Success)
    {
        Result.CanonicalDefinition = {};
        return Result;
    }

    const FAssetId* ParsedId = nullptr;
    std::visit(
        [&ParsedId](const auto& Desc) { ParsedId = &Desc.Id; },
        Definition.Value);
    if (!ParsedId ||
        (Request.ExpectedId.IsValid() &&
         Request.ExpectedId != *ParsedId))
    {
        Result.Result = EAssetResult::DependencyMismatch;
        Result.CanonicalDefinition = {};
        return Result;
    }

    if (Definition.Kind ==
        Private::EMaterialShaderDefinitionKind::Shader)
    {
        FShaderAsset Asset;
        auto Desc = std::get<FShaderAssetDesc>(std::move(Definition.Value));
        Result.Result = FShaderAsset::CreateValidated(
            std::move(Desc),
            Asset,
            &Result.Diagnostics);
        if (Result.Result != EAssetResult::Success) return Result;
        auto Payload = Core::MakeShared<FShaderAsset>(std::move(Asset));
        Result.Dependencies = Payload->GetDesc().Dependencies;
        Result.Metadata.push_back(BuildMetadata(
            *Payload,
            Request.Descriptor,
            Result.Dependencies));
        Result.Payloads.insert(Result.Payloads.begin(), std::move(Payload));
        std::rotate(
            Result.Metadata.rbegin(),
            Result.Metadata.rbegin() + 1,
            Result.Metadata.rend());
    }
    else if (Definition.Kind ==
        Private::EMaterialShaderDefinitionKind::Material)
    {
        FMaterialAsset Asset;
        auto Desc = std::get<FMaterialAssetDesc>(std::move(Definition.Value));
        Result.Result = FMaterialAsset::CreateValidated(
            std::move(Desc),
            Asset,
            &Result.Diagnostics);
        if (Result.Result != EAssetResult::Success) return Result;
        auto Payload = Core::MakeShared<FMaterialAsset>(std::move(Asset));
        Result.Dependencies = Payload->GetDesc().Dependencies;
        Result.Metadata.push_back(BuildMetadata(
            *Payload,
            Request.Descriptor,
            Result.Dependencies));
        Result.Payloads.push_back(std::move(Payload));
    }
    else
    {
        FMaterialInstanceAsset Asset;
        auto Desc = std::get<FMaterialInstanceAssetDesc>(
            std::move(Definition.Value));
        Result.Result = FMaterialInstanceAsset::CreateValidated(
            std::move(Desc),
            Asset,
            &Result.Diagnostics);
        if (Result.Result != EAssetResult::Success) return Result;
        auto Payload =
            Core::MakeShared<FMaterialInstanceAsset>(std::move(Asset));
        Result.Dependencies = Payload->GetDesc().Dependencies;
        Result.Metadata.push_back(BuildMetadata(
            *Payload,
            Request.Descriptor,
            Result.Dependencies));
        Result.Payloads.push_back(std::move(Payload));
    }
    Result.Result = EAssetResult::Success;
    return Result;
}

FMaterialShaderLoadResult FMaterialShaderImportService::ImportAndRegister(
    const FAssetExtensionRegistry& Extensions,
    FAssetRegistry& Registry,
    const FAssetImportRequest& Request)
{
    FMaterialShaderLoadResult Result;
    Core::TArray<FAssetImportOutput> Outputs;
    Result.Result = FAssetDispatch::Import(
        Extensions,
        Request,
        Outputs,
        &Result.Diagnostics);
    if (Result.Result != EAssetResult::Success)
    {
        return Result;
    }
    FAssetMutationBatch Batch;
    for (const FAssetImportOutput& Output : Outputs)
    {
        Batch.Register(Output.Metadata);
    }
    Result.Result = Registry.Apply(Batch);
    if (Result.Result != EAssetResult::Success)
    {
        return Result;
    }
    for (const FAssetImportOutput& Output : Outputs)
    {
        Result.Payloads.push_back(Output.Payload);
        Result.Metadata.push_back(Output.Metadata);
        Result.Dependencies.insert(
            Result.Dependencies.end(),
            Output.Metadata.Dependencies.begin(),
            Output.Metadata.Dependencies.end());
    }
    Result.RegistryRevision = Registry.Snapshot().Revision;
    return Result;
}

} // namespace Stoner::Asset
