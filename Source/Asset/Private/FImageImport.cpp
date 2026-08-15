#include "Asset/FImageImport.h"

#include "FImageContainerInspector.h"
#include "FImageDecode.h"
#include "FImageMipGenerator.h"

#include <string>

namespace Stoner::Asset
{
namespace
{

FAssetParticipantId ImageImporterId()
{
    FAssetParticipantId Id;
    (void)FAssetParticipantId::Create(
        Core::FString("stoner.image.stb"),
        Id);
    return Id;
}

FAssetProducerVersion ImageImporterVersion()
{
    FAssetProducerVersion Version;
    (void)FAssetProducerVersion::Create(
        Core::FString("stb-2.30+image-1"),
        Version);
    return Version;
}

Core::FString ToString(Core::uint64 Value)
{
    return Core::FString(std::to_string(Value));
}

void AddImageDiagnostic(
    FAssetDiagnosticList* Diagnostics,
    FAssetDiagnostic Diagnostic,
    EAssetStage Stage,
    EAssetResult Result,
    const FAssetImportRequest& Request,
    const char* Code,
    const char* Field = "",
    const char* Limit = "")
{
    if (Diagnostics == nullptr)
    {
        return;
    }
    if (Diagnostic.Result == EAssetResult::Success)
    {
        Diagnostic.Stage = Stage;
        Diagnostic.Result = Result;
        Diagnostic.Severity = EAssetDiagnosticSeverity::Error;
        Diagnostic.Code = Core::FString(Code);
        Diagnostic.Field = Core::FString(Field);
        Diagnostic.Limit = Core::FString(Limit);
    }
    if (Diagnostic.Subject.IsEmpty())
    {
        Diagnostic.Subject = Request.Descriptor.Location.ToString();
    }
    if (Diagnostic.Participant.IsEmpty())
    {
        Diagnostic.Participant = ImageImporterId().ToString();
    }
    if (Diagnostic.Field.IsEmpty() && Field[0] != '\0')
    {
        Diagnostic.Field = Core::FString(Field);
    }
    if (Diagnostic.Limit.IsEmpty() && Limit[0] != '\0')
    {
        Diagnostic.Limit = Core::FString(Limit);
    }
    Diagnostics->push_back(std::move(Diagnostic));
}

FAssetMetadata MakeImageMetadata(
    const FImageAsset& Image,
    EImageSourceFormat SourceFormat,
    const FImageContainerInspection& Inspection)
{
    FAssetMetadata Metadata;
    Metadata.Id = Image.GetId();
    Metadata.Source = Image.GetSource();
    Metadata.Producer = ImageImporterId();
    Metadata.ProducerVersion = ImageImporterVersion();
    Metadata.Version.SourceDigest = Image.GetSourceDigest();
    Metadata.Version.ContentDigest = Image.GetContentDigest();
    Metadata.Version.Producer = Metadata.Producer;
    Metadata.Version.ProducerVersion = Metadata.ProducerVersion;
    Metadata.Attributes = {
        {Core::FString("image.width"), ToString(Image.GetBaseMip().GetExtent().Width)},
        {Core::FString("image.height"), ToString(Image.GetBaseMip().GetExtent().Height)},
        {Core::FString("image.format"), ToString(static_cast<Core::uint8>(Image.GetBaseMip().GetFormat()))},
        {Core::FString("image.color-space"), ToString(static_cast<Core::uint8>(Image.GetColorSpace()))},
        {Core::FString("image.alpha"), ToString(static_cast<Core::uint8>(Image.GetAlphaMode()))},
        {Core::FString("image.origin"), Core::FString("top-left")},
        {Core::FString("image.source-format"), ToString(static_cast<Core::uint8>(SourceFormat))},
        {Core::FString("image.orientation-transform"), ToString(static_cast<Core::uint8>(Inspection.Orientation))},
    };
    return Metadata;
}

FAssetMetadata MakeTextureMetadata(const FTextureAsset& Texture)
{
    FAssetMetadata Metadata;
    Metadata.Id = Texture.GetId();
    Metadata.Source = Texture.GetImage()->GetSource();
    Metadata.Producer = ImageImporterId();
    Metadata.ProducerVersion = ImageImporterVersion();
    Metadata.Version.SourceDigest = Texture.GetImage()->GetSourceDigest();
    Metadata.Version.ContentDigest = Texture.GetContentDigest();
    Metadata.Version.Producer = Metadata.Producer;
    Metadata.Version.ProducerVersion = Metadata.ProducerVersion;
    Metadata.Attributes = {
        {Core::FString("texture.semantic"), ToString(static_cast<Core::uint8>(Texture.GetSemantic()))},
        {Core::FString("texture.color-space"), ToString(static_cast<Core::uint8>(Texture.GetColorSpace()))},
        {Core::FString("texture.alpha"), ToString(static_cast<Core::uint8>(Texture.GetAlphaMode()))},
        {Core::FString("texture.mip-policy"), ToString(static_cast<Core::uint8>(Texture.GetMipPolicy()))},
        {Core::FString("texture.mip-count"), ToString(Texture.GetMips().size())},
        {Core::FString("texture.origin"), Core::FString("top-left")},
    };
    FAssetDependency Dependency;
    Dependency.TargetId = Texture.GetImage()->GetId();
    Dependency.Role = EAssetDependencyRole::Source;
    Dependency.Strength = EAssetDependencyStrength::Required;
    Metadata.Dependencies.push_back(std::move(Dependency));
    return Metadata;
}

EAssetResult BuildImageOutputs(
    const FAssetImportRequest& Request,
    Core::TArray<FAssetImportOutput>& OutOutputs,
    FAssetDiagnosticList* Diagnostics)
{
    OutOutputs.clear();
    const auto Parameters =
        std::dynamic_pointer_cast<const FImageImportParameters>(
            Request.Parameters);
    if (!Parameters ||
        !Parameters->ImageId.IsValid() ||
        !Parameters->TextureId.IsValid() ||
        Parameters->ImageId == Parameters->TextureId ||
        Parameters->ImageId.GetAssetType() !=
            TAssetTypeTraits<FImageAsset>::GetAssetType() ||
        Parameters->TextureId.GetAssetType() !=
            TAssetTypeTraits<FTextureAsset>::GetAssetType() ||
        Parameters->Settings.Validate() != EAssetResult::Success ||
        !Request.Descriptor.Location.IsValid() ||
        !Request.Source.IsValid())
    {
        AddImageDiagnostic(
            Diagnostics,
            {},
            EAssetStage::Validate,
            EAssetResult::InvalidInput,
            Request,
            "image.request.invalid",
            "settings-or-identity");
        return EAssetResult::InvalidInput;
    }

    Core::TArray<Core::uint8> SourceBytes;
    EAssetResult Result = Request.Source.ReadBounded(
        Parameters->Settings.Limits.MaxSourceBytes,
        Request.Descriptor.Size,
        SourceBytes);
    if (Result != EAssetResult::Success)
    {
        AddImageDiagnostic(
            Diagnostics,
            {},
            EAssetStage::Import,
            Result,
            Request,
            "image.source.read",
            "source-bytes",
            "max-source-bytes");
        return Result;
    }
    FImageContainerInspection Inspection;
    FAssetDiagnostic OperationDiagnostic;
    Result = Private::FImageContainerInspector::Inspect(
        SourceBytes,
        Parameters->Settings.Limits,
        Inspection,
        &OperationDiagnostic);
    if (Result != EAssetResult::Success)
    {
        AddImageDiagnostic(
            Diagnostics,
            std::move(OperationDiagnostic),
            EAssetStage::Inspect,
            Result,
            Request,
            "image.inspect.failed",
            "container");
        return Result;
    }

    FImageImportSettings ResolvedSettings = Parameters->Settings;
    const EImageColorSpace DefaultColorSpace =
        Inspection.SourceFormat == EImageSourceFormat::RadianceHDR
        ? EImageColorSpace::Linear
        : EImageColorSpace::SRGB;
    const EImageColorSpace ResolvedColorSpace =
        ResolvedSettings.ColorSpace.value_or(
            Inspection.DeclaredColorSpace.value_or(DefaultColorSpace));
    ResolvedSettings.ColorSpace = ResolvedColorSpace;
    if (ResolvedSettings.Validate() != EAssetResult::Success)
    {
        AddImageDiagnostic(
            Diagnostics,
            {},
            EAssetStage::Validate,
            EAssetResult::InvalidInput,
            Request,
            "image.settings.invalid",
            "semantic-or-color-space");
        return EAssetResult::InvalidInput;
    }

    FImageMip BaseMip;
    OperationDiagnostic = {};
    Result = Private::DecodeCanonicalImage(
        SourceBytes,
        Inspection,
        ResolvedSettings,
        BaseMip,
        &OperationDiagnostic);
    if (Result != EAssetResult::Success)
    {
        AddImageDiagnostic(
            Diagnostics,
            std::move(OperationDiagnostic),
            EAssetStage::Decode,
            Result,
            Request,
            "image.decode.failed",
            "pixels");
        return Result;
    }
    const EImageAlphaMode AlphaMode =
        ImageFormatHasAlpha(BaseMip.GetFormat())
        ? EImageAlphaMode::Straight
        : EImageAlphaMode::None;
    FImageAsset ImageValue;
    Result = FImageAsset::Create(
        Parameters->ImageId,
        Request.Descriptor.Location,
        BaseMip,
        ResolvedColorSpace,
        AlphaMode,
        FAssetDigest::FromBytes(SourceBytes),
        ImageValue);
    if (Result != EAssetResult::Success)
    {
        AddImageDiagnostic(
            Diagnostics,
            {},
            EAssetStage::Validate,
            Result,
            Request,
            "image.asset.invalid",
            "base-payload");
        return Result;
    }
    auto Image = Core::MakeShared<const FImageAsset>(std::move(ImageValue));

    Core::TArray<FImageMip> Mips;
    OperationDiagnostic = {};
    Result = Private::GenerateImageMips(
        Image->GetBaseMip(),
        ResolvedSettings,
        Mips,
        &OperationDiagnostic);
    if (Result != EAssetResult::Success)
    {
        AddImageDiagnostic(
            Diagnostics,
            std::move(OperationDiagnostic),
            EAssetStage::Mip,
            Result,
            Request,
            "image.mip.failed",
            "mip-chain",
            "max-mip-or-chain-bytes");
        return Result;
    }
    FTextureAsset TextureValue;
    Result = FTextureAsset::Create(
        Parameters->TextureId,
        Image,
        ResolvedSettings,
        std::move(Mips),
        TextureValue);
    if (Result != EAssetResult::Success)
    {
        AddImageDiagnostic(
            Diagnostics,
            {},
            EAssetStage::Validate,
            Result,
            Request,
            "texture.asset.invalid",
            "mip-chain");
        return Result;
    }
    auto Texture =
        Core::MakeShared<const FTextureAsset>(std::move(TextureValue));

    OutOutputs.push_back({
        MakeImageMetadata(*Image, Inspection.SourceFormat, Inspection),
        Image});
    OutOutputs.push_back({MakeTextureMetadata(*Texture), Texture});
    return EAssetResult::Success;
}

} // namespace

FAssetExtensionCapability FImageAssetImporter::GetCapability() const
{
    FAssetExtensionCapability Capability;
    Capability.Kind = EAssetExtensionKind::Importer;
    Capability.Participant = ImageImporterId();
    Capability.ProducerVersion = ImageImporterVersion();
    Capability.FormatHints = {
        Core::FString("png"),
        Core::FString("jpg"),
        Core::FString("jpeg"),
        Core::FString("hdr")};
    Capability.ProbeByteLimit = 64U * 1024U;
    Capability.bRuntimeCompatible = true;
    return Capability;
}

FAssetProbeResult FImageAssetImporter::Probe(
    const FAssetSourceDescriptor& Descriptor,
    std::span<const Core::uint8> Prefix)
{
    (void)Descriptor;
    const EImageSourceFormat Format =
        Private::FImageContainerInspector::Detect(Prefix);
    return Format == EImageSourceFormat::Unknown
        ? FAssetProbeResult{
              EAssetResult::Success,
              0,
              Core::FString("image signature not recognized")}
        : FAssetProbeResult{
              EAssetResult::Success,
              100,
              Core::FString("image signature recognized")};
}

EAssetResult FImageAssetImporter::Import(
    const FAssetSourceDescriptor& Descriptor,
    const FAssetSourceLease& Source,
    Core::TArray<FAssetImportOutput>& OutOutputs)
{
    (void)Descriptor;
    (void)Source;
    OutOutputs.clear();
    return EAssetResult::InvalidInput;
}

EAssetResult FImageAssetImporter::Import(
    const FAssetImportRequest& Request,
    Core::TArray<FAssetImportOutput>& OutOutputs)
{
    return BuildImageOutputs(Request, OutOutputs, nullptr);
}

EAssetResult FImageAssetImporter::Import(
    const FAssetImportRequest& Request,
    Core::TArray<FAssetImportOutput>& OutOutputs,
    FAssetDiagnosticList* Diagnostics)
{
    if (Request.RuntimeContext && Request.RuntimeContext->ShouldStop())
        return EAssetResult::Cancelled;
    return BuildImageOutputs(Request, OutOutputs, Diagnostics);
}

FImageImportResult FAssetImportService::ImportAndRegister(
    const FAssetExtensionRegistry& Extensions,
    FAssetRegistry& Registry,
    const FAssetImportRequest& Request)
{
    FImageImportResult Result;
    Core::TArray<FAssetImportOutput> Outputs;
    Result.Result = FAssetDispatch::Import(
        Extensions,
        Request,
        Outputs,
        &Result.Diagnostics);
    if (Result.Result != EAssetResult::Success || Outputs.size() != 2)
    {
        return Result;
    }
    for (const FAssetImportOutput& Output : Outputs)
    {
        if (const auto Image =
                std::dynamic_pointer_cast<const FImageAsset>(Output.Payload))
        {
            Result.Image = Image;
        }
        else if (const auto Texture =
                     std::dynamic_pointer_cast<const FTextureAsset>(
                         Output.Payload))
        {
            Result.Texture = Texture;
        }
    }
    if (!Result.Image || !Result.Texture)
    {
        Result.Image.reset();
        Result.Texture.reset();
        Result.Result = EAssetResult::InvalidInput;
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
        AddImageDiagnostic(
            &Result.Diagnostics,
            {},
            EAssetStage::Registry,
            Result.Result,
            Request,
            "image.registry.publication",
            "asset-records");
        Result.Image.reset();
        Result.Texture.reset();
        return Result;
    }
    Result.RegistryRevision = Registry.Snapshot().Revision;
    return Result;
}

EAssetResult RegisterImageAssetImporter(
    FAssetExtensionRegistry& Registry,
    FAssetRegistrationToken& OutToken)
{
    return Registry.Register(
        Core::MakeShared<FImageAssetImporter>(),
        OutToken);
}

} // namespace Stoner::Asset
