#include "FGLTFImageTextureBridge.h"

#include "Asset/FImageImport.h"
#include "FGLTFDependencyResolver.h"
#include "FImageMipGenerator.h"

#include "cgltf/cgltf.h"

#include <algorithm>
#include <optional>
#include <string>

namespace Stoner::Asset::Private
{
namespace
{

class FMemoryImageSource final : public IAssetSource
{
public:
    explicit FMemoryImageSource(Core::TArray<Core::uint8> Bytes)
        : Bytes_(std::move(Bytes)) {}
    EAssetResult Read(Core::uint64 Offset, Core::usize MaximumBytes,
        Core::TArray<Core::uint8>& OutBytes) const override
    {
        OutBytes.clear();
        if (Offset > Bytes_.size()) return EAssetResult::MalformedSource;
        const Core::usize Count = std::min(
            MaximumBytes, Bytes_.size() - static_cast<Core::usize>(Offset));
        OutBytes.assign(Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset),
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset + Count));
        return EAssetResult::Success;
    }
private:
    Core::TArray<Core::uint8> Bytes_;
};

FImageImportSettings SettingsFor(ETextureSemantic Semantic,
    const FStaticModelImportProfile& Profile)
{
    FImageImportSettings Settings;
    Settings.Semantic = Semantic;
    Settings.ColorSpace = Semantic == ETextureSemantic::Color
        ? EImageColorSpace::SRGB : EImageColorSpace::Linear;
    Settings.Limits.MaxSourceBytes = Profile.Limits.MaxSingleDependencyBytes;
    Settings.Limits.MaxDecodedChainBytes =
        Profile.Limits.MaxAggregateDependencyBytes;
    Settings.Limits.MaxMipBytes = Profile.Limits.MaxSingleDependencyBytes;
    return Settings;
}

FAssetParticipantId Participant()
{
    FAssetParticipantId Id;
    (void)FAssetParticipantId::Create(Core::FString("stoner.gltf.cgltf"), Id);
    return Id;
}

FAssetProducerVersion ProducerVersion()
{
    FAssetProducerVersion Value;
    (void)FAssetProducerVersion::Create(
        Core::FString("cgltf-1.15+static-model-1"), Value);
    return Value;
}

FAssetMetadata MakeTextureMetadata(
    const FTextureAsset& Texture,
    const FAssetSourceDescriptor& Descriptor)
{
    FAssetMetadata Metadata;
    Metadata.Id = Texture.GetId();
    Metadata.Source = Descriptor.Location;
    Metadata.Producer = Participant();
    Metadata.ProducerVersion = ProducerVersion();
    Metadata.Version.SourceDigest = Texture.GetImage()->GetSourceDigest();
    Metadata.Version.ContentDigest = Texture.GetContentDigest();
    Metadata.Version.Producer = Metadata.Producer;
    Metadata.Version.ProducerVersion = Metadata.ProducerVersion;
    Metadata.Dependencies.push_back({Texture.GetImage()->GetId(),
        EAssetDependencyRole::Source, EAssetDependencyStrength::Required,
        EAssetDependencyResolution::Unresolved});
    return Metadata;
}

void AddBridgeDiagnostic(
    FAssetDiagnosticList* Diagnostics,
    EAssetResult Result,
    Core::uint32 ImageIndex,
    const char* Field,
    Core::FString Actual)
{
    if (Diagnostics == nullptr) return;
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = EAssetStage::Dependency;
    Diagnostic.Result = Result;
    Diagnostic.Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic.Code = Core::FString("asset.gltf.image-output");
    Diagnostic.Subject = Core::FString("idx.image." + std::to_string(ImageIndex));
    Diagnostic.Participant = Participant().ToString();
    Diagnostic.Field = Core::FString(Field);
    Diagnostic.Actual = std::move(Actual);
    Diagnostic.Reason = Core::FString("image importer output contract rejected");
    Diagnostics->push_back(std::move(Diagnostic));
}

} // namespace

const FAssetId* FindGLTFTextureVariant(
    const Core::TArray<FGLTFTextureVariant>& Variants,
    const cgltf_data& Data,
    const cgltf_texture* Texture,
    ETextureSemantic Semantic)
{
    if (Texture == nullptr || Texture < Data.textures ||
        Texture >= Data.textures + Data.textures_count)
        return nullptr;
    const Core::uint32 Index = static_cast<Core::uint32>(Texture - Data.textures);
    const auto Found = std::find_if(Variants.begin(), Variants.end(),
        [Index, Semantic](const FGLTFTextureVariant& Variant)
        { return Variant.SourceTextureIndex == Index && Variant.Semantic == Semantic; });
    return Found == Variants.end() ? nullptr : &Found->TextureId;
}

EAssetResult BuildGLTFImageTextureOutputs(
    const cgltf_data& Data,
    const FGLTFPackageIdentityPlan& Identities,
    const Core::TArray<FGLTFTextureVariant>& Variants,
    const FAssetImportRequest& MainRequest,
    const Core::TSharedPtr<IAssetResolver>& Resolver,
    const FStaticModelImportProfile& Profile,
    Core::uint64& InOutAggregateDependencyBytes,
    const FGLTFBufferViewReader& ReadBufferView,
    Core::TArray<FAssetImportOutput>& OutOutputs,
    FAssetDiagnosticList* Diagnostics)
{
    OutOutputs.clear();
    if (Identities.ImageIds.size() != Data.images_count) return EAssetResult::InvalidInput;
    for (Core::uint32 ImageIndex = 0; ImageIndex < Data.images_count; ++ImageIndex)
    {
        const cgltf_image& SourceImage = Data.images[ImageIndex];
        const bool CountsAsDependency = SourceImage.buffer_view == nullptr;
        Core::TArray<Core::uint8> Bytes;
        FAssetSourceDescriptor Descriptor;
        Descriptor.Location = MainRequest.Descriptor.Location;
        EAssetResult Result = EAssetResult::Success;
        if (SourceImage.buffer_view != nullptr)
            Result = ReadBufferView(SourceImage.buffer_view, Bytes);
        else if (SourceImage.uri != nullptr &&
                 std::string_view(SourceImage.uri).starts_with("data:"))
            Result = DecodeGLTFDataUri(SourceImage.uri,
                Profile.Limits.MaxSingleDependencyBytes, Bytes);
        else if (SourceImage.uri != nullptr)
        {
            FGLTFResolvedDependency Dependency;
            Result = ResolveGLTFDependency(MainRequest.Descriptor.Location,
                SourceImage.uri, Resolver,
                Profile.Limits.MaxSingleDependencyBytes, Dependency);
            if (Result == EAssetResult::Success)
            {
                Bytes = std::move(Dependency.Bytes);
                Descriptor = std::move(Dependency.Descriptor);
            }
        }
        else Result = EAssetResult::MalformedSource;
        if (Result != EAssetResult::Success || Bytes.empty()) return Result;
        if (CountsAsDependency)
        {
            if (Bytes.size() > Profile.Limits.MaxAggregateDependencyBytes -
                    InOutAggregateDependencyBytes)
                return EAssetResult::CapacityExceeded;
            InOutAggregateDependencyBytes += Bytes.size();
        }
        Descriptor.Size = Bytes.size();
        if (SourceImage.mime_type != nullptr)
        {
            const std::string_view Mime(SourceImage.mime_type);
            if (Mime == "image/png") Descriptor.FormatHint = Core::FString("png");
            else if (Mime == "image/jpeg") Descriptor.FormatHint = Core::FString("jpeg");
            else return EAssetResult::Unsupported;
        }

        Core::TArray<const FGLTFTextureVariant*> ImageVariants;
        for (const FGLTFTextureVariant& Variant : Variants)
        {
            const cgltf_texture& Texture = Data.textures[Variant.SourceTextureIndex];
            if (Texture.has_basisu || Texture.has_webp) return EAssetResult::Unsupported;
            if (Texture.image == &SourceImage) ImageVariants.push_back(&Variant);
        }
        std::sort(ImageVariants.begin(), ImageVariants.end(),
            [](const auto* Left, const auto* Right)
            {
                if (Left->Semantic != Right->Semantic)
                    return Left->Semantic < Right->Semantic;
                return Left->TextureId < Right->TextureId;
            });
        FAssetId DecodeTextureId;
        if (!ImageVariants.empty()) DecodeTextureId = ImageVariants.front()->TextureId;
        else
        {
            const Core::FString Key("idx.image." + std::to_string(ImageIndex) + ".decode");
            Result = FAssetId::Create(Core::FString("Texture"),
                MainRequest.Descriptor.Location.GetLocator(),
                std::optional<Core::FString>(Key), DecodeTextureId);
            if (Result != EAssetResult::Success) return Result;
        }
        FImageImportParameters Parameters;
        Parameters.ImageId = Identities.ImageIds[ImageIndex];
        Parameters.TextureId = DecodeTextureId;
        Parameters.Settings = SettingsFor(ImageVariants.empty()
            ? ETextureSemantic::Data : ImageVariants.front()->Semantic, Profile);
        FAssetImportRequest ImageRequest;
        ImageRequest.Descriptor = Descriptor;
        ImageRequest.Source = FAssetSourceLease(
            Core::MakeShared<FMemoryImageSource>(std::move(Bytes)));
        ImageRequest.Parameters = Core::MakeShared<FImageImportParameters>(Parameters);
        FImageAssetImporter Importer;
        Core::TArray<FAssetImportOutput> Imported;
        Result = Importer.Import(ImageRequest, Imported, Diagnostics);
        if (Result != EAssetResult::Success) return Result;
        if (Imported.size() != 2)
        {
            AddBridgeDiagnostic(Diagnostics, EAssetResult::ProcessingFailure,
                ImageIndex, "output-count",
                Core::FString(std::to_string(Imported.size())));
            return EAssetResult::ProcessingFailure;
        }
        const auto ImageOutput = std::find_if(Imported.begin(), Imported.end(),
            [](const FAssetImportOutput& Output)
            { return std::dynamic_pointer_cast<const FImageAsset>(Output.Payload) != nullptr; });
        const auto TextureOutput = std::find_if(Imported.begin(), Imported.end(),
            [](const FAssetImportOutput& Output)
            { return std::dynamic_pointer_cast<const FTextureAsset>(Output.Payload) != nullptr; });
        if (ImageOutput == Imported.end() || TextureOutput == Imported.end())
        {
            std::string Actual;
            for (const FAssetImportOutput& Output : Imported)
            {
                if (!Actual.empty()) Actual += ',';
                Actual += Output.Payload
                    ? Output.Payload->GetAssetType().ToStdString()
                    : std::string("null");
            }
            AddBridgeDiagnostic(Diagnostics, EAssetResult::ProcessingFailure,
                ImageIndex, "output-types", Core::FString(std::move(Actual)));
            return EAssetResult::ProcessingFailure;
        }
        const auto Image = std::dynamic_pointer_cast<const FImageAsset>(ImageOutput->Payload);
        OutOutputs.push_back(*ImageOutput);
        if (!ImageVariants.empty()) OutOutputs.push_back(*TextureOutput);
        for (Core::usize VariantIndex = 1; VariantIndex < ImageVariants.size(); ++VariantIndex)
        {
            const FGLTFTextureVariant& Variant = *ImageVariants[VariantIndex];
            const FImageImportSettings Settings = SettingsFor(Variant.Semantic, Profile);
            Core::TArray<FImageMip> Mips;
            Result = GenerateImageMips(Image->GetBaseMip(), Settings, Mips, nullptr);
            if (Result != EAssetResult::Success) return Result;
            FTextureAsset TextureValue;
            Result = FTextureAsset::Create(
                Variant.TextureId, Image, Settings, std::move(Mips), TextureValue);
            if (Result != EAssetResult::Success) return Result;
            auto Texture = Core::MakeShared<const FTextureAsset>(std::move(TextureValue));
            OutOutputs.push_back({MakeTextureMetadata(*Texture, Descriptor), Texture});
        }
    }
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
