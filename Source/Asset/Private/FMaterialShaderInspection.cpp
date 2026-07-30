#include "Asset/FMaterialShaderInspection.h"

#include "Asset/FMaterialAsset.h"
#include "Asset/FMaterialInstanceAsset.h"
#include "Asset/FShaderAsset.h"
#include "Asset/FShaderPayloadAsset.h"
#include "Asset/FShaderSourceAsset.h"

#include <sstream>

namespace Stoner::Asset
{
namespace
{

const char* Stage(EShaderStage Value)
{
    switch (Value)
    {
    case EShaderStage::Vertex: return "vertex";
    case EShaderStage::Fragment: return "fragment";
    case EShaderStage::Compute: return "compute";
    }
    return "unknown";
}

const char* Backend(EShaderBackendFamily Value)
{
    switch (Value)
    {
    case EShaderBackendFamily::Vulkan: return "vulkan";
    case EShaderBackendFamily::Metal: return "metal";
    case EShaderBackendFamily::DirectX12: return "directx12";
    case EShaderBackendFamily::OpenGL: return "opengl";
    case EShaderBackendFamily::GLES: return "gles";
    }
    return "unknown";
}

void WriteParameters(
    std::ostringstream& Stream,
    const Core::TArray<FMaterialAssetParameter>& Parameters)
{
    for (const FMaterialAssetParameter& Parameter : Parameters)
    {
        Stream << "  parameter=" << Parameter.Name.CStr()
               << " type=" << static_cast<unsigned>(Parameter.Value.Type)
               << '\n';
    }
}

} // namespace

Core::FString InspectShaderAsset(const FShaderAsset& Asset)
{
    const auto& Desc = Asset.GetDesc();
    std::ostringstream Stream;
    Stream << "ShaderProgram id=" << Desc.Id.ToString().CStr()
           << " version=" << Desc.Version.SourceDigest.ToLowerHex().CStr()
           << " kind="
           << (Desc.ProgramKind == EShaderProgramKind::Graphics
                   ? "graphics" : "compute")
           << '\n';
    for (const FShaderSourceReference& Source : Desc.Stages)
    {
        Stream << "  source stage=" << Stage(Source.Stage)
               << " entry=" << Source.EntryPoint.CStr()
               << " id=" << Source.Source.GetId()->ToString().CStr()
               << " digest=" << Source.ExpectedDigest.ToLowerHex().CStr()
               << '\n';
    }
    for (const FShaderVariantDefinition& Variant : Desc.Variants)
    {
        Stream << "  variant=" << Variant.VariantName.CStr()
               << " permutation=" << Variant.Permutation.ToString().CStr()
               << " payloads=" << Variant.Payloads.size() << '\n';
    }
    Stream << "  bindings=" << Desc.InterfaceBindings.size()
           << " constantRanges=" << Desc.ConstantRanges.size()
           << " dependencies=" << Desc.Dependencies.size() << '\n';
    return Core::FString(Stream.str());
}

Core::FString InspectShaderSourceAsset(const FShaderSourceAsset& Asset)
{
    std::ostringstream Stream;
    Stream << "ShaderSource id=" << Asset.GetId().ToString().CStr()
           << " version=" << Asset.GetVersion().SourceDigest.ToLowerHex().CStr()
           << " language=glsl bytes=" << Asset.GetBytes().size() << '\n';
    return Core::FString(Stream.str());
}

Core::FString InspectShaderPayloadAsset(const FShaderPayloadAsset& Asset)
{
    std::ostringstream Stream;
    Stream << "ShaderPayload id=" << Asset.GetId().ToString().CStr()
           << " version=" << Asset.GetVersion().ContentDigest.ToLowerHex().CStr()
           << " backend=" << Backend(Asset.GetBackend())
           << " profile=" << Asset.GetProfile().CStr()
           << " stage=" << Stage(Asset.GetStage())
           << " entry=" << Asset.GetEntryPoint().CStr()
           << " bytes=" << Asset.GetBytes().size() << '\n';
    return Core::FString(Stream.str());
}

Core::FString InspectMaterialAsset(const FMaterialAsset& Asset)
{
    const auto& Desc = Asset.GetDesc();
    std::ostringstream Stream;
    Stream << "Material id=" << Desc.Id.ToString().CStr()
           << " version=" << Desc.Version.SourceDigest.ToLowerHex().CStr()
           << " shader=" << Desc.Shader.GetId()->ToString().CStr()
           << " parameters=" << Desc.Parameters.size()
           << " dependencies=" << Desc.Dependencies.size() << '\n';
    WriteParameters(Stream, Desc.Parameters);
    return Core::FString(Stream.str());
}

Core::FString InspectMaterialInstanceAsset(
    const FMaterialInstanceAsset& Asset)
{
    const auto& Desc = Asset.GetDesc();
    const FAssetId* Parent = std::visit(
        [](const auto& Reference) -> const FAssetId*
        {
            return Reference.GetId() ? &*Reference.GetId() : nullptr;
        },
        Desc.Parent.Reference);
    std::ostringstream Stream;
    Stream << "MaterialInstance id=" << Desc.Id.ToString().CStr()
           << " version=" << Desc.Version.SourceDigest.ToLowerHex().CStr()
           << " parent=" << (Parent ? Parent->ToString().CStr() : "<empty>")
           << " overrides=" << Desc.Overrides.size() << '\n';
    WriteParameters(Stream, Desc.Overrides);
    return Core::FString(Stream.str());
}

Core::FString InspectResolvedMaterial(
    const FResolvedMaterialAsset& Material)
{
    std::ostringstream Stream;
    Stream << "ResolvedMaterial leaf=" << Material.LeafId.ToString().CStr()
           << " root=" << Material.RootMaterialId.ToString().CStr()
           << " shader=" << Material.Shader.GetId()->ToString().CStr()
           << " parameters=" << Material.EffectiveParameters.size()
           << " sources=" << Material.SourceManifest.size() << '\n';
    WriteParameters(Stream, Material.EffectiveParameters);
    return Core::FString(Stream.str());
}

Core::FString InspectSelectedShader(
    const FSelectedShaderProgram& Selection)
{
    std::ostringstream Stream;
    Stream << "SelectedShader id=" << Selection.ShaderId.ToString().CStr()
           << " backend=" << Backend(Selection.Backend)
           << " profile=" << Selection.SelectedProfile.CStr()
           << " permutation=" << Selection.Permutation.ToString().CStr()
           << " stages=" << Selection.Stages.size()
           << " sources=" << Selection.SourceManifest.size() << '\n';
    for (const FSelectedShaderStage& Selected : Selection.Stages)
    {
        Stream << "  stage=" << Stage(Selected.Stage)
               << " payload="
               << Selected.Payload->GetId().ToString().CStr() << '\n';
    }
    return Core::FString(Stream.str());
}

} // namespace Stoner::Asset
