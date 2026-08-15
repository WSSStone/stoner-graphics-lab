#include "FDevelopmentAssetLoadingStrategy.h"

#include "Asset/FAssetDispatch.h"
#include "Asset/FImageImport.h"
#include "Asset/FMaterialShaderSourceLoader.h"
#include "Asset/FStaticModelImport.h"

#include <span>
#include <string>
#include <utility>

namespace Stoner::Asset::Private
{
namespace
{
EAssetResult MakeLocation(
    const FAssetManagerConfig& Config,
    const FAssetId& Id,
    FAssetSourceLocator& Out)
{
    std::string Locator = Config.SourceRoot.ToStdString();
    if (!Locator.empty() && Locator.back() != '/') Locator.push_back('/');
    Locator += Id.GetLogicalPath().ToStdString();
    return FAssetSourceLocator::Create(
        Core::FString("asset"), Core::FString(std::move(Locator)), Out);
}

EAssetResult ReadPinned(
    const FAssetSourceLease& Source,
    const FAssetSourceDescriptor& Descriptor,
    Core::uint64 Limit,
    Core::TArray<Core::uint8>& Out,
    FAssetDigest& OutDigest)
{
    const EAssetResult Result = Source.ReadBounded(Limit, Descriptor.Size, Out);
    if (Result != EAssetResult::Success) return Result;
    OutDigest = FAssetDigest::FromBytes(Out);
    return EAssetResult::Success;
}

Core::TSharedPtr<const FAssetImportParameters> MakeParameters(
    const FAssetManagerConfig& Config,
    const FAssetLoadKey& Key,
    const FAssetSourceDescriptor& Descriptor)
{
    if (Descriptor.FormatHint &&
        (*Descriptor.FormatHint == Core::FString("gltf") ||
         *Descriptor.FormatHint == Core::FString("glb")))
        return Core::MakeShared<FStaticModelImportProfile>();
    if (Key.ExpectedType == Core::FString("Image") ||
        Key.ExpectedType == Core::FString("Texture"))
    {
        auto Parameters = Core::MakeShared<FImageImportParameters>();
        (void)FAssetId::Create(Core::FString("Image"),
            Key.AssetId.GetLogicalPath(), Core::FString("image"),
            Parameters->ImageId);
        (void)FAssetId::Create(Core::FString("Texture"),
            Key.AssetId.GetLogicalPath(), Core::FString("texture"),
            Parameters->TextureId);
        Parameters->Settings.Semantic = ETextureSemantic::Color;
        Parameters->Settings.ColorSpace = EImageColorSpace::SRGB;
        Parameters->Settings.MipPolicy = EImageMipPolicy::FullChain;
        return Parameters;
    }
    if (Key.ExpectedType == Core::FString("ShaderProgram") ||
        Key.ExpectedType == Core::FString("Material") ||
        Key.ExpectedType == Core::FString("MaterialInstance"))
    {
        auto Parameters = Core::MakeShared<FMaterialShaderImportParameters>();
        Parameters->ExpectedId = Key.AssetId;
        Parameters->Extensions = Config.ExtensionRegistry;
        return Parameters;
    }
    if (Key.ExpectedType == Core::FString("StaticModel") ||
        Key.ExpectedType == Core::FString("StaticMesh"))
        return Core::MakeShared<FStaticModelImportProfile>();
    return {};
}
} // namespace

FDevelopmentAssetLoadingStrategy::FDevelopmentAssetLoadingStrategy(
    FAssetManagerConfig Config)
    : Config_(std::move(Config))
{
}

FAssetLoadScratchResult FDevelopmentAssetLoadingStrategy::Load(
    const FAssetLoadKey& Key,
    const FAssetRuntimeExecutionContext& Context)
{
    FAssetLoadScratchResult Out;
    if (Context.ShouldStop())
    {
        Out.Result = EAssetResult::Cancelled;
        return Out;
    }
    FAssetSourceLocator Location;
    Out.Result = MakeLocation(Config_, Key.AssetId, Location);
    if (Out.Result != EAssetResult::Success) return Out;

    auto RuntimeContext = Core::MakeShared<FAssetRuntimeExecutionContext>(Context);
    const FAssetResolveResult Resolved = FAssetDispatch::Resolve(
        *Config_.ExtensionRegistry, {Location, RuntimeContext}, &Out.Diagnostics);
    if (Resolved.Result != EAssetResult::Success || !Resolved.Source.IsValid())
    {
        Out.Result = Resolved.Result;
        return Out;
    }
    Core::TArray<Core::uint8> PinnedBytes;
    FAssetDigest PinnedDigest;
    Out.Result = ReadPinned(
        Resolved.Source, Resolved.Descriptor, Config_.Limits.MaxPayloadBytes,
        PinnedBytes, PinnedDigest);
    if (Out.Result != EAssetResult::Success) return Out;

    Core::TArray<FAssetImportOutput> Outputs;
    const FAssetImportRequest ImportRequest{
        Resolved.Descriptor, Resolved.Source,
        MakeParameters(Config_, Key, Resolved.Descriptor),
        RuntimeContext};
    Out.Result = FAssetDispatch::Import(
        *Config_.ExtensionRegistry, ImportRequest, Outputs, &Out.Diagnostics);
    if (Out.Result != EAssetResult::Success) return Out;
    if (Context.ShouldStop())
    {
        Out.bExtensionContractViolation = true;
        Out.Result = EAssetResult::Cancelled;
        return Out;
    }

    const FAssetResolveResult ReResolved = FAssetDispatch::Resolve(
        *Config_.ExtensionRegistry, {Location, RuntimeContext}, &Out.Diagnostics);
    Core::TArray<Core::uint8> CurrentBytes;
    FAssetDigest CurrentDigest;
    if (ReResolved.Result != EAssetResult::Success ||
        ReadPinned(ReResolved.Source, ReResolved.Descriptor,
            Config_.Limits.MaxPayloadBytes, CurrentBytes, CurrentDigest) !=
            EAssetResult::Success ||
        ReResolved.Descriptor != Resolved.Descriptor ||
        CurrentDigest != PinnedDigest)
    {
        Out.Result = EAssetResult::SourceChanged;
        return Out;
    }

    bool Found = false;
    for (auto& Output : Outputs)
    {
        if (Output.Metadata.Id == Key.AssetId)
        {
            if (!Output.Payload ||
                Output.Payload->GetAssetType() != Key.ExpectedType)
            {
                Out.Result = EAssetResult::TypeMismatch;
                return Out;
            }
            Found = true;
        }
        Out.Metadata.push_back(std::move(Output.Metadata));
        Out.Payloads.push_back(std::move(Output.Payload));
        Out.PayloadBytes.push_back(
            static_cast<Core::uint64>(PinnedBytes.size()));
    }
    Out.Result = Found ? EAssetResult::Success : EAssetResult::NotFound;
    if (!Found)
    {
        Out.Metadata.clear();
        Out.Payloads.clear();
        Out.PayloadBytes.clear();
    }
    return Out;
}

} // namespace Stoner::Asset::Private
