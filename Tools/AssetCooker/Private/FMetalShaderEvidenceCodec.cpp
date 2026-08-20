#include "FMetalShaderEvidenceCodec.h"

#include <algorithm>
#include <new>
#include <string>
#include <string_view>

namespace Stoner::AssetCooker::Private
{
namespace
{

void Escape(std::string_view Text, std::string& Out)
{
    Out.push_back('"');
    constexpr char Hex[] = "0123456789abcdef";
    for (const unsigned char Character : Text)
    {
        switch (Character)
        {
        case '"': Out += "\\\""; break;
        case '\\': Out += "\\\\"; break;
        case '\b': Out += "\\b"; break;
        case '\f': Out += "\\f"; break;
        case '\n': Out += "\\n"; break;
        case '\r': Out += "\\r"; break;
        case '\t': Out += "\\t"; break;
        default:
            if (Character < 0x20)
            {
                Out += "\\u00";
                Out.push_back(Hex[Character >> 4U]);
                Out.push_back(Hex[Character & 0x0fU]);
            }
            else Out.push_back(static_cast<char>(Character));
        }
    }
    Out.push_back('"');
}

const char* StageToken(Asset::EShaderStage Stage)
{
    switch (Stage)
    {
    case Asset::EShaderStage::Vertex: return "vertex";
    case Asset::EShaderStage::Fragment: return "fragment";
    case Asset::EShaderStage::Compute: return "compute";
    }
    return "unknown";
}

const char* DescriptorToken(Asset::EShaderResourceKind Kind)
{
    switch (Kind)
    {
    case Asset::EShaderResourceKind::UniformBuffer: return "uniform-buffer";
    case Asset::EShaderResourceKind::SampledTexture: return "sampled-texture";
    case Asset::EShaderResourceKind::Sampler: return "sampler";
    case Asset::EShaderResourceKind::StorageBuffer: return "storage-buffer";
    case Asset::EShaderResourceKind::StorageTexture: return "storage-texture";
    case Asset::EShaderResourceKind::CombinedTextureSampler:
        return "combined-texture-sampler";
    }
    return "unknown";
}

const char* NativeClassToken(Asset::EShaderNativeResourceClass Class)
{
    switch (Class)
    {
    case Asset::EShaderNativeResourceClass::Buffer: return "buffer";
    case Asset::EShaderNativeResourceClass::Texture: return "texture";
    case Asset::EShaderNativeResourceClass::Sampler: return "sampler";
    }
    return "unknown";
}

void StringField(
    std::string& Out,
    int Depth,
    const char* Name,
    std::string_view Value,
    bool Comma = true)
{
    Out.append(static_cast<std::size_t>(Depth) * 2U, ' ');
    Escape(Name, Out);
    Out += ": ";
    Escape(Value, Out);
    Out += Comma ? ",\n" : "\n";
}

void NumberField(
    std::string& Out,
    int Depth,
    const char* Name,
    Core::uint64 Value,
    bool Comma = true)
{
    Out.append(static_cast<std::size_t>(Depth) * 2U, ' ');
    Escape(Name, Out);
    Out += ": " + std::to_string(Value) + (Comma ? ",\n" : "\n");
}

std::string Canonical(
    const FMetalShaderEvidence& Evidence,
    bool IncludeEvidenceDigest)
{
    std::string Out = "{\n";
    NumberField(Out, 1, "schemaVersion", Evidence.SchemaVersion);
    StringField(
        Out, 1, "kind",
        Evidence.Kind == EMetalShaderEvidenceKind::Derivation
            ? "derivation" : "native-library");
    StringField(
        Out, 1, "shaderAssetId", Evidence.ShaderAssetId.ToString().View());
    StringField(
        Out, 1, "shaderAssetVersion",
        Evidence.ShaderAssetVersion.ToLowerHex().View());
    if (Evidence.GlslDigest)
        StringField(
            Out, 1, "glslDigest", Evidence.GlslDigest->ToLowerHex().View());
    StringField(
        Out, 1, "spirvDigest", Evidence.SpirvDigest.ToLowerHex().View());
    StringField(Out, 1, "stage", StageToken(Evidence.Stage));
    StringField(Out, 1, "entryPoint", Evidence.EntryPoint.View());
    StringField(
        Out, 1, "interfaceDigest",
        Evidence.InterfaceDigest.ToLowerHex().View());
    Out += "  \"spirvCross\": {\n";
    StringField(Out, 2, "commit", Evidence.SpirvCrossCommit.View());
    StringField(
        Out, 2, "optionsDigest",
        Evidence.SpirvCrossOptionsDigest.ToLowerHex().View(), false);
    Out += "  },\n";
    Out += "  \"bindingPolicy\": {\n";
    StringField(
        Out, 2, "version", Evidence.BindingEvidence.PolicyVersion.View());
    Out += "    \"entries\": [";
    if (!Evidence.BindingEvidence.Entries.empty()) Out += "\n";
    for (Core::usize Index = 0;
         Index < Evidence.BindingEvidence.Entries.size(); ++Index)
    {
        const auto& Entry = Evidence.BindingEvidence.Entries[Index];
        Out += "      {\n";
        StringField(Out, 4, "stage", StageToken(Entry.Stage));
        NumberField(Out, 4, "set", Entry.SetIndex);
        NumberField(Out, 4, "binding", Entry.BindingIndex);
        StringField(
            Out, 4, "descriptorType", DescriptorToken(Entry.DescriptorType));
        NumberField(Out, 4, "arrayElement", Entry.ArrayElement);
        StringField(
            Out, 4, "nativeClass", NativeClassToken(Entry.NativeClass));
        NumberField(Out, 4, "nativeIndex", Entry.NativeIndex, false);
        Out += Index + 1 == Evidence.BindingEvidence.Entries.size()
            ? "      }\n" : "      },\n";
    }
    Out += Evidence.BindingEvidence.Entries.empty()
        ? "],\n" : "    ],\n";
    StringField(
        Out, 2, "digest",
        Evidence.BindingEvidence.CanonicalDigest.ToLowerHex().View(), false);
    Out += "  },\n";
    Out += "  \"target\": {\n";
    StringField(Out, 2, "profile", Evidence.TargetProfile.View());
    if (Evidence.NativeLibrary)
        StringField(
            Out, 2, "architecture",
            Evidence.NativeLibrary->Architecture.View());
    StringField(
        Out, 2, "deploymentTarget", Evidence.DeploymentTarget.View());
    StringField(Out, 2, "mslVersion", Evidence.MslVersion.View(), false);
    Out += "  },\n";
    StringField(
        Out, 1, "normalizedMslDigest",
        Evidence.NormalizedMslDigest.ToLowerHex().View());
    if (Evidence.NativeLibrary)
    {
        const auto& Native = *Evidence.NativeLibrary;
        Out += "  \"nativeLibrary\": {\n";
        StringField(Out, 2, "compiler", Native.Compiler.View());
        StringField(Out, 2, "xcodeBuild", Native.XcodeBuild.View());
        StringField(Out, 2, "sdk", Native.Sdk.View());
        StringField(
            Out, 2, "argumentDigest",
            Native.ArgumentDigest.ToLowerHex().View());
        StringField(
            Out, 2, "digest", Native.LibraryDigest.ToLowerHex().View());
        NumberField(Out, 2, "sizeBytes", Native.SizeBytes, false);
        Out += "  },\n";
    }
    if (IncludeEvidenceDigest)
        StringField(
            Out, 1, "evidenceDigest",
            Evidence.EvidenceDigest.ToLowerHex().View(), false);
    else
    {
        if (!Out.empty() && Out.size() >= 2 && Out[Out.size() - 2] == ',')
            Out.erase(Out.size() - 2, 1);
    }
    Out += "}\n";
    return Out;
}

Asset::FAssetDigest IdentityDigest(const FMetalShaderEvidence& Evidence)
{
    const std::string Text = Canonical(Evidence, false);
    return Asset::FAssetDigest::FromBytes(std::span<const Core::uint8>(
        reinterpret_cast<const Core::uint8*>(Text.data()), Text.size()));
}

} // namespace

bool FMetalNativeLibraryEvidence::IsValid() const noexcept
{
    return (Architecture == Core::FString("arm64") ||
            Architecture == Core::FString("x86_64")) &&
        !Compiler.IsEmpty() && !XcodeBuild.IsEmpty() && !Sdk.IsEmpty() &&
        ArgumentDigest.IsAvailable() && LibraryDigest.IsAvailable() &&
        SizeBytes > 0;
}

bool FMetalShaderEvidence::IsValid() const noexcept
{
    if (SchemaVersion != 1 || !ShaderAssetId.IsValid() ||
        !ShaderAssetVersion.IsAvailable() || !SpirvDigest.IsAvailable() ||
        EntryPoint.IsEmpty() || !InterfaceDigest.IsAvailable() ||
        SpirvCrossCommit != Core::FString(
            "a0fba56c34a6700f1724bf9b751da5b488a3775c") ||
        !SpirvCrossOptionsDigest.IsAvailable() ||
        BindingEvidence.Validate() != Asset::EAssetResult::Success ||
        TargetProfile.IsEmpty() || DeploymentTarget != Core::FString("12.0") ||
        MslVersion != Core::FString("2.4") ||
        !NormalizedMslDigest.IsAvailable() || !EvidenceDigest.IsAvailable())
        return false;
    if (Stage != Asset::EShaderStage::Vertex &&
        Stage != Asset::EShaderStage::Fragment &&
        Stage != Asset::EShaderStage::Compute)
        return false;
    if (Kind == EMetalShaderEvidenceKind::Derivation)
        return !NativeLibrary.has_value();
    return Kind == EMetalShaderEvidenceKind::NativeLibrary && NativeLibrary &&
        NativeLibrary->IsValid();
}

Asset::EAssetResult FinalizeMetalShaderEvidence(
    FMetalShaderEvidence& Evidence) noexcept
{
    Evidence.EvidenceDigest = {};
    try
    {
        Evidence.EvidenceDigest = IdentityDigest(Evidence);
        return Evidence.IsValid()
            ? Asset::EAssetResult::Success
            : Asset::EAssetResult::InvalidInput;
    }
    catch (const std::bad_alloc&)
    {
        Evidence.EvidenceDigest = {};
        return Asset::EAssetResult::CapacityExceeded;
    }
}

Asset::EAssetResult WriteMetalShaderEvidence(
    const FMetalShaderEvidence& Evidence,
    Core::FString& OutCanonical) noexcept
{
    OutCanonical.Clear();
    try
    {
        if (!Evidence.IsValid() ||
            IdentityDigest(Evidence) != Evidence.EvidenceDigest)
            return Asset::EAssetResult::InvalidInput;
        OutCanonical = Core::FString(Canonical(Evidence, true));
        return Asset::EAssetResult::Success;
    }
    catch (const std::bad_alloc&)
    {
        OutCanonical.Clear();
        return Asset::EAssetResult::CapacityExceeded;
    }
}

} // namespace Stoner::AssetCooker::Private
