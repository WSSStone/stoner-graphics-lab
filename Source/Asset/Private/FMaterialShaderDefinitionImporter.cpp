#include "Asset/FMaterialShaderSourceLoader.h"

#include "Asset/IAssetImporter.h"

#include <algorithm>
#include <string_view>

namespace Stoner::Asset
{
namespace
{

class FMaterialShaderDefinitionImporter final : public IAssetImporter
{
public:
    [[nodiscard]] FAssetExtensionCapability GetCapability() const override
    {
        FAssetExtensionCapability Result{
            EAssetExtensionKind::Importer,
            Participant(),
            ProducerVersion(),
            50,
            {},
            {
                Core::FString("shader.json"),
                Core::FString("material.json"),
                Core::FString("material-instance.json")},
            4096};
        Result.bRuntimeCompatible = true;
        return Result;
    }

    [[nodiscard]] FAssetProbeResult Probe(
        const FAssetSourceDescriptor& Descriptor,
        std::span<const Core::uint8> Prefix) override
    {
        const std::string_view Text(
            reinterpret_cast<const char*>(Prefix.data()),
            Prefix.size());
        const bool bSchema =
            Text.find("\"schema\"") != std::string_view::npos &&
            Text.find("stoner.") != std::string_view::npos;
        bool bHint = false;
        if (Descriptor.FormatHint)
        {
            const auto Hint = Descriptor.FormatHint->View();
            bHint = Hint == "shader.json" ||
                Hint == "material.json" ||
                Hint == "material-instance.json";
        }
        return {EAssetResult::Success, bSchema ? 100 : bHint ? 80 : 0, {}};
    }

    [[nodiscard]] EAssetResult Import(
        const FAssetSourceDescriptor& Descriptor,
        const FAssetSourceLease& Source,
        Core::TArray<FAssetImportOutput>& OutOutputs) override
    {
        return Import(
            FAssetImportRequest{Descriptor, Source, {}, {}},
            OutOutputs,
            nullptr);
    }

    [[nodiscard]] EAssetResult Import(
        const FAssetImportRequest& Request,
        Core::TArray<FAssetImportOutput>& OutOutputs,
        FAssetDiagnosticList* Diagnostics) override
    {
        OutOutputs.clear();
        if (Request.RuntimeContext && Request.RuntimeContext->ShouldStop())
            return EAssetResult::Cancelled;
        FMaterialShaderLoadRequest LoadRequest;
        LoadRequest.Descriptor = Request.Descriptor;
        LoadRequest.Source = Request.Source;
        if (const auto Parameters =
                std::dynamic_pointer_cast<
                    const FMaterialShaderImportParameters>(
                    Request.Parameters))
        {
            LoadRequest.ExpectedId = Parameters->ExpectedId;
            LoadRequest.Extensions = Parameters->Extensions.get();
            LoadRequest.Limits = Parameters->Limits;
            LoadRequest.bLoadDependencies =
                Parameters->bLoadDependencies;
        }
        FMaterialShaderLoadResult Loaded =
            FMaterialShaderSourceLoader::Load(LoadRequest);
        if (Request.RuntimeContext && Request.RuntimeContext->ShouldStop())
            return EAssetResult::Cancelled;
        if (Diagnostics)
        {
            Diagnostics->insert(
                Diagnostics->end(),
                Loaded.Diagnostics.begin(),
                Loaded.Diagnostics.end());
        }
        if (!Loaded.Succeeded() ||
            Loaded.Payloads.size() != Loaded.Metadata.size())
        {
            return Loaded.Result;
        }
        for (std::size_t Index = 0;
             Index < Loaded.Payloads.size();
             ++Index)
        {
            OutOutputs.push_back({
                Loaded.Metadata[Index],
                Loaded.Payloads[Index]});
        }
        return EAssetResult::Success;
    }

private:
    static FAssetParticipantId Participant()
    {
        FAssetParticipantId Value;
        (void)FAssetParticipantId::Create(
            Core::FString("stoner.material-shader.importer"),
            Value);
        return Value;
    }

    static FAssetProducerVersion ProducerVersion()
    {
        FAssetProducerVersion Value;
        (void)FAssetProducerVersion::Create(
            Core::FString("023-v1"),
            Value);
        return Value;
    }
};

} // namespace

EAssetResult RegisterMaterialShaderDefinitionImporter(
    FAssetExtensionRegistry& Registry,
    FAssetRegistrationToken& OutToken)
{
    return Registry.Register(
        Core::MakeShared<FMaterialShaderDefinitionImporter>(),
        OutToken);
}

} // namespace Stoner::Asset
