#include "AssetMaterialShaderTests.h"

#include "Asset/AssetMinimal.h"
#include "FMaterialShaderJsonCodec.h"
#include "FShaderPayloadValidation.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>

namespace
{

using namespace Stoner::Asset;
using namespace Stoner::Core;

class FMemoryDefinitionSource final : public IAssetSource
{
public:
    explicit FMemoryDefinitionSource(std::string Text)
        : Bytes_(Text.begin(), Text.end())
    {
    }

    EAssetResult Read(
        uint64 Offset,
        usize MaximumBytes,
        TArray<uint8>& OutBytes) const override
    {
        OutBytes.clear();
        if (Offset > Bytes_.size())
        {
            return EAssetResult::MalformedSource;
        }
        const usize Count = std::min(
            MaximumBytes,
            Bytes_.size() - static_cast<usize>(Offset));
        OutBytes.assign(
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset),
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset + Count));
        return EAssetResult::Success;
    }

private:
    TArray<uint8> Bytes_;
};

class FFileDefinitionSource final : public IAssetSource
{
public:
    explicit FFileDefinitionSource(TArray<uint8> Bytes)
        : Bytes_(std::move(Bytes))
    {
    }

    EAssetResult Read(
        uint64 Offset,
        usize MaximumBytes,
        TArray<uint8>& OutBytes) const override
    {
        OutBytes.clear();
        if (Offset > Bytes_.size())
        {
            return EAssetResult::MalformedSource;
        }
        const usize Count = std::min(
            MaximumBytes,
            Bytes_.size() - static_cast<usize>(Offset));
        OutBytes.assign(
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset),
            Bytes_.begin() + static_cast<std::ptrdiff_t>(Offset + Count));
        return EAssetResult::Success;
    }

    [[nodiscard]] usize Size() const noexcept { return Bytes_.size(); }

private:
    TArray<uint8> Bytes_;
};

class FContentResolver final : public IAssetResolver
{
public:
    explicit FContentResolver(std::filesystem::path Root)
        : Root_(std::move(Root))
    {
    }

    FAssetExtensionCapability GetCapability() const override
    {
        FAssetParticipantId Participant;
        FAssetProducerVersion Version;
        (void)FAssetParticipantId::Create(
            FString("stoner.tests.content-resolver"), Participant);
        (void)FAssetProducerVersion::Create(FString("023-v1"), Version);
        return {
            EAssetExtensionKind::Resolver,
            Participant,
            Version,
            100,
            {FString("content")},
            {},
            0};
    }

    FAssetResolveResult Resolve(
        const FAssetResolveRequest& Request) override
    {
        FAssetResolveResult Result;
        Result.Descriptor.Location = Request.Location;
        if (Request.Location.GetScheme() != FString("content"))
        {
            return Result;
        }
        const std::filesystem::path Relative(
            Request.Location.GetLocator().ToStdString());
        if (Relative.is_absolute() ||
            std::find(Relative.begin(), Relative.end(), "..") !=
                Relative.end())
        {
            Result.Result = EAssetResult::AccessDenied;
            return Result;
        }
        std::ifstream Input(Root_ / Relative, std::ios::binary);
        if (!Input)
        {
            Result.Result = EAssetResult::NotFound;
            return Result;
        }
        TArray<uint8> Bytes{
            std::istreambuf_iterator<char>(Input),
            std::istreambuf_iterator<char>()};
        auto Source = MakeShared<FFileDefinitionSource>(std::move(Bytes));
        Result.Descriptor.Size = Source->Size();
        Result.Source = FAssetSourceLease(std::move(Source));
        Result.Result = EAssetResult::Success;
        return Result;
    }

private:
    std::filesystem::path Root_;
};

class FLoadedPayloadLookup final : public IShaderPayloadLookup
{
public:
    explicit FLoadedPayloadLookup(
        const TArray<TSharedPtr<const FAssetPayload>>& Payloads)
        : Payloads_(Payloads)
    {
    }

    TSharedPtr<const FShaderPayloadAsset> Find(
        const FAssetId& Id) const override
    {
        for (const auto& Payload : Payloads_)
        {
            const auto ShaderPayload =
                std::dynamic_pointer_cast<const FShaderPayloadAsset>(
                    Payload);
            if (ShaderPayload && ShaderPayload->GetId() == Id)
            {
                return ShaderPayload;
            }
        }
        return nullptr;
    }

private:
    const TArray<TSharedPtr<const FAssetPayload>>& Payloads_;
};

class FMaterialLookup final : public IMaterialAssetLookup
{
public:
    void Add(const TSharedPtr<const FAssetPayload>& Payload)
    {
        if (const auto Material =
                std::dynamic_pointer_cast<const FMaterialAsset>(Payload))
        {
            Materials_[Material->GetDesc().Id] = Material;
        }
        else if (const auto Instance =
                     std::dynamic_pointer_cast<
                         const FMaterialInstanceAsset>(Payload))
        {
            Instances_[Instance->GetDesc().Id] = Instance;
        }
    }

    void AddDependency(
        const FAssetId& Id,
        const FAssetVersion& Version)
    {
        Versions_[Id] = Version;
    }

    TSharedPtr<const FMaterialAsset> FindMaterial(
        const FAssetId& Id) const override
    {
        const auto Found = Materials_.find(Id);
        return Found == Materials_.end() ? nullptr : Found->second;
    }

    TSharedPtr<const FMaterialInstanceAsset> FindInstance(
        const FAssetId& Id) const override
    {
        const auto Found = Instances_.find(Id);
        return Found == Instances_.end() ? nullptr : Found->second;
    }

    std::optional<FAssetVersion> FindDependencyVersion(
        const FAssetId& Id) const override
    {
        const auto Found = Versions_.find(Id);
        return Found == Versions_.end()
            ? std::nullopt
            : std::optional<FAssetVersion>(Found->second);
    }

private:
    std::map<FAssetId, TSharedPtr<const FMaterialAsset>> Materials_;
    std::map<FAssetId, TSharedPtr<const FMaterialInstanceAsset>> Instances_;
    std::map<FAssetId, FAssetVersion> Versions_;
};

void Record(
    FAssetMaterialShaderTestResult& Result,
    bool Passed,
    const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FAssetSourceLocator MakeSource()
{
    FAssetSourceLocator Source;
    (void)FAssetSourceLocator::Create(
        FString("memory"),
        FString("Fixtures/Material.json"),
        Source);
    return Source;
}

FAssetId MakeId(const char* Type, const char* Path)
{
    FAssetId Id;
    (void)FAssetId::Create(
        FString(Type),
        FString(Path),
        std::nullopt,
        Id);
    return Id;
}

FAssetVersion MakeVersion(std::string_view Text)
{
    const auto* Bytes =
        reinterpret_cast<const uint8*>(Text.data());
    FAssetVersion Version;
    Version.SourceDigest =
        FAssetDigest::FromBytes(
            std::span<const uint8>(Bytes, Text.size()));
    Version.ContentDigest = Version.SourceDigest;
    return Version;
}

std::string ValidMaterial()
{
    return R"({
  "schema": "stoner.material",
  "version": 1,
  "id": {
    "type": "Material",
    "path": "Tests/Materials/Foundation"
  },
  "requiredExtensions": [],
  "domain": "surface",
  "blendMode": "opaque",
  "renderState": {
    "depthTest": true,
    "depthWrite": true,
    "twoSided": false
  },
  "shader": {
    "type": "ShaderProgram",
    "path": "Tests/Shaders/Foundation"
  },
  "permutationFlags": [],
  "parameters": [
    {
      "name": "Roughness",
      "type": "scalar",
      "value": 0.5
    }
  ],
  "extensions": {}
}
)";
}

FMaterialShaderLoadRequest MakeRequest(const std::string& Text)
{
    FMaterialShaderLoadRequest Request;
    Request.ExpectedId = MakeId(
        "Material",
        "Tests/Materials/Foundation");
    Request.Descriptor.Location = MakeSource();
    Request.Descriptor.Size = Text.size();
    Request.Descriptor.FormatHint = FString("material.json");
    Request.Source = FAssetSourceLease(
        MakeShared<FMemoryDefinitionSource>(Text));
    Request.bLoadDependencies = false;
    return Request;
}

FMaterialShaderLoadRequest MakeCorpusRequest(
    const std::string& Text,
    const std::filesystem::path& Path)
{
    FMaterialShaderLoadRequest Request;
    FAssetSourceLocator Source;
    (void)FAssetSourceLocator::Create(
        FString("fixture"),
        FString(Path.generic_string()),
        Source);
    Request.Descriptor.Location = Source;
    Request.Descriptor.Size = Text.size();
    Request.Descriptor.FormatHint =
        FString(Path.filename().string());
    Request.Source = FAssetSourceLease(
        MakeShared<FMemoryDefinitionSource>(Text));
    Request.bLoadDependencies = false;
    return Request;
}

FMaterialShaderLoadRequest MakeRepositoryRequest(
    const std::filesystem::path& Root,
    const std::filesystem::path& Relative,
    const FAssetExtensionRegistry& Extensions)
{
    const std::filesystem::path Path = Root / Relative;
    std::ifstream Input(Path, std::ios::binary);
    TArray<uint8> Bytes{
        std::istreambuf_iterator<char>(Input),
        std::istreambuf_iterator<char>()};
    FAssetSourceLocator Location;
    (void)FAssetSourceLocator::Create(
        FString("content"),
        FString(Relative.generic_string()),
        Location);
    auto Source = MakeShared<FFileDefinitionSource>(std::move(Bytes));
    FMaterialShaderLoadRequest Request;
    Request.Extensions = &Extensions;
    Request.Descriptor.Location = Location;
    Request.Descriptor.Size = Source->Size();
    Request.Descriptor.FormatHint =
        FString(Relative.filename().string());
    Request.Source = FAssetSourceLease(std::move(Source));
    Request.bLoadDependencies = true;
    return Request;
}

std::string ReadText(const std::filesystem::path& Path)
{
    std::ifstream Input(Path, std::ios::binary);
    return std::string(
        std::istreambuf_iterator<char>(Input),
        std::istreambuf_iterator<char>());
}

TArray<uint8> ReadBytes(const std::filesystem::path& Path)
{
    std::ifstream Input(Path, std::ios::binary);
    return TArray<uint8>{
        std::istreambuf_iterator<char>(Input),
        std::istreambuf_iterator<char>()};
}

FString DigestText(const std::string& Text)
{
    return FAssetDigest::FromBytes(std::span<const uint8>(
        reinterpret_cast<const uint8*>(Text.data()),
        Text.size())).ToLowerHex();
}

void TestLimitsAndStrictParsing(FAssetMaterialShaderTestResult& Result)
{
    auto Request = MakeRequest(ValidMaterial());
    Request.Limits.MaxDefinitionBytes =
        static_cast<uint64>(ValidMaterial().size() - 1);
    const EAssetResult LimitResult =
        FMaterialShaderSourceLoader::Load(Request).Result;
    Record(
        Result,
        LimitResult == EAssetResult::TruncatedSource ||
            LimitResult == EAssetResult::DefinitionLimitExceeded,
        "Material/shader loader enforces definition byte limit before publication");

    std::string Duplicate = ValidMaterial();
    const std::string Needle = R"("schema": "stoner.material")";
    Duplicate.replace(
        Duplicate.find(Needle),
        Needle.size(),
        R"("schema": "stoner.material", "\u0073chema": "stoner.material")");
    Record(
        Result,
        FMaterialShaderSourceLoader::Load(MakeRequest(Duplicate)).Result ==
            EAssetResult::InvalidDefinition,
        "Strict parser rejects escaped-equivalent duplicate object keys");

    std::string Unknown = ValidMaterial();
    Unknown.replace(
        Unknown.find(R"("extensions": {})"),
        std::string(R"("extensions": {})").size(),
        R"("misspelledField": 1, "extensions": {})");
    Record(
        Result,
        FMaterialShaderSourceLoader::Load(MakeRequest(Unknown)).Result ==
            EAssetResult::InvalidDefinition,
        "Closed schema rejects unknown ordinary fields");

    std::string Optional = ValidMaterial();
    Optional.replace(
        Optional.find(R"("extensions": {})"),
        std::string(R"("extensions": {})").size(),
        R"("extensions": {"example.optional": {"ignored": true}})");
    const auto OptionalResult =
        FMaterialShaderSourceLoader::Load(MakeRequest(Optional));
    Record(
        Result,
        OptionalResult.Succeeded() &&
            OptionalResult.CanonicalDefinition.View().find(
                "example.optional") == std::string_view::npos,
        "Unknown optional namespaced extension is omitted canonically");

    std::string InvalidOptional = ValidMaterial();
    InvalidOptional.replace(
        InvalidOptional.find(R"("extensions": {})"),
        std::string(R"("extensions": {})").size(),
        R"("extensions": {"plain": {"ignored": true}})");
    std::string InvalidBody = ValidMaterial();
    InvalidBody.replace(
        InvalidBody.find(R"("extensions": {})"),
        std::string(R"("extensions": {})").size(),
        R"("extensions": {"vendor.optional": 1})");
    Record(
        Result,
        FMaterialShaderSourceLoader::Load(
            MakeRequest(InvalidOptional)).Result ==
                EAssetResult::InvalidDefinition &&
        FMaterialShaderSourceLoader::Load(
            MakeRequest(InvalidBody)).Result ==
                EAssetResult::InvalidDefinition,
        "Optional extensions require namespaced names and object bodies");
}

void TestCanonicalOwnershipAndRollback(
    FAssetMaterialShaderTestResult& Result)
{
    const auto Loaded =
        FMaterialShaderSourceLoader::Load(MakeRequest(ValidMaterial()));
    Record(
        Result,
        Loaded.Succeeded() &&
            Loaded.Payloads.size() == 1 &&
            Loaded.Metadata.size() == 1 &&
            Loaded.CanonicalDefinition.View().ends_with("\n"),
        "Valid definition produces one immutable payload and canonical source");

    if (Loaded.Succeeded())
    {
        const auto Reparsed = FMaterialShaderSourceLoader::Load(
            MakeRequest(Loaded.CanonicalDefinition.ToStdString()));
        Record(
            Result,
            Reparsed.Succeeded() &&
                Reparsed.CanonicalDefinition == Loaded.CanonicalDefinition &&
                Reparsed.Metadata.front().Version.SourceDigest ==
                    Loaded.Metadata.front().Version.SourceDigest,
            "Canonical parse-write cycle is byte and version idempotent");
    }
    else
    {
        Record(
            Result,
            false,
            "Canonical parse-write cycle is byte and version idempotent");
    }

    FAssetRegistry Registry;
    const uint64 Before = Registry.Snapshot().Revision;
    FAssetExtensionRegistry Extensions;
    FAssetRegistrationToken Token;
    const bool bRegistered =
        RegisterMaterialShaderDefinitionImporter(Extensions, Token) ==
        EAssetResult::Success;
    const auto LoadRequest = MakeRequest(ValidMaterial());
    auto Parameters = MakeShared<FMaterialShaderImportParameters>();
    Parameters->ExpectedId = MakeId("Material", "Wrong/Identity");
    Parameters->bLoadDependencies = false;
    FAssetImportRequest ImportRequest{
        LoadRequest.Descriptor,
        LoadRequest.Source,
        Parameters};
    const auto Failed = bRegistered
        ? FMaterialShaderImportService::ImportAndRegister(
              Extensions, Registry, ImportRequest)
        : FMaterialShaderLoadResult{};
    Record(
        Result,
        bRegistered &&
            Failed.Result == EAssetResult::DependencyMismatch &&
            Failed.Payloads.empty() &&
            Registry.Snapshot().Revision == Before,
        "Failed import publishes no payload and preserves Registry revision");
}

void TestDiagnosticsAndLimits(FAssetMaterialShaderTestResult& Result)
{
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = EAssetStage::Parse;
    Diagnostic.Result = EAssetResult::InvalidDefinition;
    Diagnostic.Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic.Code = "asset.definition";
    Diagnostic.Subject = "/Users/example/private.material.json";
    Diagnostic.Reason = "yyjson internal parser error";
    const FString Formatted = FAssetDiagnostics::Format(Diagnostic);
    Record(
        Result,
        Formatted.View().find("/Users/") == std::string_view::npos &&
            Formatted.View().find("yyjson") == std::string_view::npos &&
            Formatted.View().find("[redacted]") != std::string_view::npos,
        "Normalized diagnostics redact paths and third-party parser text");

    FMaterialShaderAssetLimits Limits;
    Limits.MaxJsonDepth = 0;
    Record(
        Result,
        Limits.Validate() == EAssetResult::InvalidInput,
        "Material/shader limits reject zero unbounded sentinels");
    uint64 Checked = 0;
    Record(
        Result,
        !CheckedMaterialShaderAdd(
            std::numeric_limits<uint64>::max(), 1, Checked) &&
            !CheckedMaterialShaderMultiply(
                std::numeric_limits<uint64>::max(), 2, Checked),
        "Material/shader aggregate arithmetic rejects overflow");
}

void TestSchemaCategoriesAndInterfaceRoundTrip(
    FAssetMaterialShaderTestResult& Result)
{
    std::string Required = ValidMaterial();
    Required.replace(
        Required.find(R"("requiredExtensions": [])"),
        std::string(R"("requiredExtensions": [])").size(),
        R"("requiredExtensions": ["vendor.required"])");
    Required.replace(
        Required.find(R"("extensions": {})"),
        std::string(R"("extensions": {})").size(),
        R"("extensions": {"vendor.required": {}})");
    Record(
        Result,
        FMaterialShaderSourceLoader::Load(MakeRequest(Required)).Result ==
            EAssetResult::UnknownRequiredExtension,
        "Unknown required extension reports its stable result category");

    std::string Version = ValidMaterial();
    Version.replace(
        Version.find(R"("version": 1)"),
        std::string(R"("version": 1)").size(),
        R"("version": 3)");
    Record(
        Result,
        FMaterialShaderSourceLoader::Load(MakeRequest(Version)).Result ==
            EAssetResult::UnsupportedSchema,
        "Unknown future schema version reports its stable result category");

    const std::filesystem::path InterfacePath =
        "Tests/Fixtures/MaterialShader/Valid/shader-08.json";
    const auto Loaded = FMaterialShaderSourceLoader::Load(
        MakeCorpusRequest(ReadText(InterfacePath), InterfacePath));
    const auto Reparsed = Loaded.Succeeded()
        ? FMaterialShaderSourceLoader::Load(
              MakeCorpusRequest(
                  Loaded.CanonicalDefinition.ToStdString(),
                  InterfacePath))
        : FMaterialShaderLoadResult{};
    Record(
        Result,
        Loaded.Succeeded() &&
            Loaded.CanonicalDefinition.View().find(
                "\"bindings\": [") != std::string_view::npos &&
            Loaded.CanonicalDefinition.View().find(
                "\"uniformBuffer\"") != std::string_view::npos &&
            Reparsed.Succeeded() &&
            Reparsed.CanonicalDefinition == Loaded.CanonicalDefinition,
        "Shader interface survives canonical parse-write-parse");
}

void TestMaterialSchemaV2(FAssetMaterialShaderTestResult& Result)
{
    const std::filesystem::path V1Path =
        "Tests/Fixtures/MaterialShader/Valid/material-00.json";
    const auto V1 = FMaterialShaderSourceLoader::Load(
        MakeCorpusRequest(ReadText(V1Path), V1Path));
    const auto V1Material = V1.Succeeded()
        ? std::dynamic_pointer_cast<const FMaterialAsset>(V1.Payloads.front())
        : nullptr;
    bool bV1Defaults = false;
    if (V1Material)
    {
        const auto V1Texture = std::find_if(
            V1Material->GetDesc().Parameters.begin(),
            V1Material->GetDesc().Parameters.end(),
            [](const FMaterialAssetParameter& Parameter)
            {
                return Parameter.Name == FString("Albedo");
            });
        bV1Defaults =
            V1Texture != V1Material->GetDesc().Parameters.end() &&
            V1Texture->Value.Type ==
                EMaterialAssetParameterType::TextureBinding &&
            std::holds_alternative<FMaterialTextureBinding>(
                V1Texture->Value.Value) &&
            std::get<FMaterialTextureBinding>(V1Texture->Value.Value)
                    .TexCoordSet == 0 &&
            std::get<FMaterialTextureBinding>(V1Texture->Value.Value)
                    .Sampler == FMaterialSamplerIntent{} &&
            FMaterialShaderSourceLoader::Load(
                MakeCorpusRequest(
                    V1.CanonicalDefinition.ToStdString(),
                    V1Path)).CanonicalDefinition == V1.CanonicalDefinition;
    }
    Record(
        Result,
        bV1Defaults,
        "Schema v1 texture references upgrade to default structured bindings without changing canonical bytes");

    const std::filesystem::path V2Path =
        "Tests/Fixtures/MaterialShader/Valid/material-v2.json";
    const auto V2Loaded = FMaterialShaderSourceLoader::Load(
        MakeCorpusRequest(ReadText(V2Path), V2Path));
    const auto V2Material = V2Loaded.Succeeded()
        ? std::dynamic_pointer_cast<const FMaterialAsset>(
              V2Loaded.Payloads.front())
        : nullptr;
    const bool bV2Binding = V2Material &&
        V2Material->GetDesc().SchemaVersion == 2 &&
        V2Material->GetDesc().Parameters.size() == 1 &&
        V2Material->GetDesc().Parameters.front().Value.Type ==
            EMaterialAssetParameterType::TextureBinding &&
        V2Loaded.Dependencies.size() == 2 &&
        V2Loaded.CanonicalDefinition.View().find(
            "\"textureBinding\"") != std::string_view::npos &&
        V2Loaded.CanonicalDefinition.View().find(
            "\"texCoord\": 1") != std::string_view::npos &&
        FMaterialShaderSourceLoader::Load(
            MakeCorpusRequest(
                V2Loaded.CanonicalDefinition.ToStdString(),
                V2Path)).CanonicalDefinition == V2Loaded.CanonicalDefinition;
    Record(
        Result,
        bV2Binding,
        "Schema v2 texture bindings preserve typed texture UV sampler dependencies and canonical JSON");

    const std::string V2Instance = R"({
  "schema": "stoner.material-instance",
  "version": 2,
  "id": { "type": "MaterialInstance", "path": "Tests/Instances/V2" },
  "requiredExtensions": [],
  "parent": { "type": "Material", "path": "Tests/Materials/V2" },
  "overrides": [{
    "name": "BaseColorTexture",
    "type": "textureBinding",
    "value": {
      "texture": "Texture:Tests/Textures/V2#base-color",
      "texCoord": 0,
      "sampler": {
        "min": "automatic",
        "mag": "linear",
        "mip": "automatic",
        "addressU": "repeat",
        "addressV": "repeat"
      }
    }
  }],
  "extensions": {}
}
)";
    const std::filesystem::path V2InstancePath =
        "Tests/Fixtures/MaterialShader/Valid/instance-v2.json";
    const auto V2InstanceLoaded = FMaterialShaderSourceLoader::Load(
        MakeCorpusRequest(V2Instance, V2InstancePath));
    FMaterialLookup V2Lookup;
    if (V2Loaded.Succeeded()) V2Lookup.Add(V2Loaded.Payloads.front());
    if (V2InstanceLoaded.Succeeded())
        V2Lookup.Add(V2InstanceLoaded.Payloads.front());
    V2Lookup.AddDependency(
        MakeId("ShaderProgram", "Tests/Shaders/Foundation"),
        MakeVersion("v2-shader"));
    FAssetId V2Texture;
    (void)FAssetId::Create(
        FString("Texture"),
        FString("Tests/Textures/V2"),
        std::optional<FString>(FString("base-color")),
        V2Texture);
    V2Lookup.AddDependency(V2Texture, MakeVersion("v2-texture"));
    FResolvedMaterialAsset ResolvedV2;
    const EAssetResult ResolveV2 = ResolveMaterial(
        MakeId("MaterialInstance", "Tests/Instances/V2"),
        V2Lookup,
        FMaterialShaderAssetLimits{},
        ResolvedV2);
    const bool bInstanceBinding =
        ResolveV2 == EAssetResult::Success &&
        V2InstanceLoaded.Dependencies.size() == 2 &&
        ResolvedV2.EffectiveParameters.size() == 1 &&
        ResolvedV2.EffectiveParameters.front().Value.Type ==
            EMaterialAssetParameterType::TextureBinding &&
        std::get<FMaterialTextureBinding>(
            ResolvedV2.EffectiveParameters.front().Value.Value).TexCoordSet ==
            0;
    Record(
        Result,
        bInstanceBinding,
        "Schema v2 MaterialInstance overrides retain complete texture bindings through resolution");

    const std::filesystem::path InvalidV2Path =
        "Tests/Fixtures/MaterialShader/Invalid/40-v2-texcoord.json";
    std::string LegacyInV2 = ReadText(V2Path);
    LegacyInV2.replace(
        LegacyInV2.find("\"textureBinding\""),
        std::string("\"textureBinding\"").size(),
        "\"texture\"");
    Record(
        Result,
        FMaterialShaderSourceLoader::Load(
            MakeCorpusRequest(ReadText(InvalidV2Path), InvalidV2Path)).Result ==
            EAssetResult::InvalidDefinition &&
        FMaterialShaderSourceLoader::Load(
            MakeCorpusRequest(LegacyInV2, V2Path)).Result ==
            EAssetResult::InvalidDefinition,
        "Schema v2 rejects out-of-range UV sets and legacy unstructured texture values");

    FMaterialTextureBinding LossyBinding;
    const FAssetId Texture = MakeId("Texture", "Tests/Textures/Lossy");
    const bool bMadeBinding = FMaterialTextureBinding::Create(
        Texture,
        1,
        {EAssetSamplerFilter::Nearest,
         EAssetSamplerFilter::Linear,
         EAssetSamplerMipFilter::None,
         EAssetSamplerAddressMode::MirroredRepeat,
         EAssetSamplerAddressMode::ClampToEdge},
        LossyBinding) == EAssetResult::Success;
    FMaterialAssetDesc LossyDesc;
    LossyDesc.SchemaVersion = 1;
    LossyDesc.Parameters.push_back({
        FString("Albedo"),
        FMaterialAssetParameterValue::FromTextureBinding(LossyBinding)});
    Private::FMaterialShaderDefinition LossyDefinition;
    LossyDefinition.Kind = Private::EMaterialShaderDefinitionKind::Material;
    LossyDefinition.Value = std::move(LossyDesc);
    FString Ignored;
    FMaterialAssetDesc ProgrammaticV2;
    ProgrammaticV2.Id = MakeId("Material", "Tests/Materials/LegacyV2");
    ProgrammaticV2.Version = MakeVersion("legacy-v2");
    ProgrammaticV2.SchemaVersion = 2;
    (void)TSoftAssetRef<FShaderAsset>::Create(
        MakeId("ShaderProgram", "Tests/Shaders/Foundation"),
        ProgrammaticV2.Shader);
    ProgrammaticV2.Parameters.push_back({
        FString("Albedo"),
        FMaterialAssetParameterValue::FromTexture(Texture)});
    FMaterialAsset RejectedProgrammaticV2;
    Record(
        Result,
        bMadeBinding &&
        Private::WriteMaterialShaderDefinition(
            LossyDefinition, Ignored, nullptr) == EAssetResult::InvalidDefinition &&
        FMaterialAsset::CreateValidated(
            std::move(ProgrammaticV2),
            RejectedProgrammaticV2) == EAssetResult::InvalidMaterialAsset,
        "Schema v1 rejects lossy downgrade and schema v2 rejects legacy texture values");
}

void TestSchemaSpecificLimits(FAssetMaterialShaderTestResult& Result)
{
    const std::filesystem::path ShaderPath =
        "Content/Shaders/Triangle/Triangle.shader.json";
    const std::string Shader = ReadText(ShaderPath);
    auto ShaderRequest = MakeCorpusRequest(Shader, ShaderPath);
    ShaderRequest.Limits.MaxStages = 2;
    ShaderRequest.Limits.MaxSourceRecords = 2;
    ShaderRequest.Limits.MaxPayloadRecords = 2;
    ShaderRequest.Limits.MaxDependencies = 4;
    const bool bExactShaderLimits =
        FMaterialShaderSourceLoader::Load(ShaderRequest).Succeeded();
    ShaderRequest.Limits.MaxStages = 1;
    const bool bStageAboveRejected =
        FMaterialShaderSourceLoader::Load(ShaderRequest).Result ==
        EAssetResult::DefinitionLimitExceeded;
    ShaderRequest.Limits.MaxStages = 2;
    ShaderRequest.Limits.MaxPayloadRecords = 1;
    const bool bPayloadAboveRejected =
        FMaterialShaderSourceLoader::Load(ShaderRequest).Result ==
        EAssetResult::DefinitionLimitExceeded;
    Record(
        Result,
        bExactShaderLimits && bStageAboveRejected &&
            bPayloadAboveRejected,
        "Shader schema limits accept exact counts and reject first values above");

    const std::filesystem::path MaterialPath =
        "Tests/Fixtures/MaterialShader/Valid/material-00.json";
    const std::string Material = ReadText(MaterialPath);
    auto MaterialRequest =
        MakeCorpusRequest(Material, MaterialPath);
    MaterialRequest.Limits.MaxParameters = 3;
    MaterialRequest.Limits.MaxDependencies = 2;
    const bool bExactMaterialLimits =
        FMaterialShaderSourceLoader::Load(MaterialRequest).Succeeded();
    MaterialRequest.Limits.MaxParameters = 2;
    const bool bParameterAboveRejected =
        FMaterialShaderSourceLoader::Load(MaterialRequest).Result ==
        EAssetResult::DefinitionLimitExceeded;
    Record(
        Result,
        bExactMaterialLimits && bParameterAboveRejected,
        "Material parameter and dependency limits are enforced before model allocation");

    std::string LongLocator = Shader;
    const std::string Locator = "Triangle.vert.spv";
    ShaderRequest = MakeCorpusRequest(LongLocator, ShaderPath);
    ShaderRequest.Limits.MaxLocatorBytes = Locator.size();
    const bool bExactLocator =
        FMaterialShaderSourceLoader::Load(ShaderRequest).Succeeded();
    ShaderRequest.Limits.MaxLocatorBytes = Locator.size() - 1;
    const bool bLocatorAboveRejected =
        FMaterialShaderSourceLoader::Load(ShaderRequest).Result ==
        EAssetResult::DefinitionLimitExceeded;
    Record(
        Result,
        bExactLocator && bLocatorAboveRejected,
        "Relative locator byte limit accepts exact length and rejects overflow");
}

void TestFixtureCorpus(
    FAssetMaterialShaderTestResult& Result,
    const FAssetMaterialShaderTestOptions& Options)
{
    const auto Start = std::chrono::steady_clock::now();
    const std::filesystem::path Root =
        "Tests/Fixtures/MaterialShader";
    int ValidCount = 0;
    int InvalidCount = 0;
    bool bValid = true;
    bool bInvalid = true;
    FString FirstCanonical;
    std::vector<std::pair<std::string, FString>> CanonicalRecords;
    std::vector<std::pair<std::string, EAssetResult>> FailureRecords;
    std::vector<std::filesystem::path> ValidPaths;
    std::vector<std::filesystem::path> InvalidPaths;
    for (const auto& Entry :
         std::filesystem::directory_iterator(Root / "Valid"))
    {
        if (Entry.is_regular_file())
            ValidPaths.push_back(Entry.path());
    }
    for (const auto& Entry :
         std::filesystem::directory_iterator(Root / "Invalid"))
    {
        if (Entry.is_regular_file())
            InvalidPaths.push_back(Entry.path());
    }
    std::sort(ValidPaths.begin(), ValidPaths.end());
    std::sort(InvalidPaths.begin(), InvalidPaths.end());
    std::string CanonicalCorpus;
    std::string FailureCorpus;
    for (const auto& Path : ValidPaths)
    {
        const auto Loaded = FMaterialShaderSourceLoader::Load(
            MakeCorpusRequest(ReadText(Path), Path));
        bValid = bValid && Loaded.Succeeded();
        if (FirstCanonical.IsEmpty() && Loaded.Succeeded())
        {
            FirstCanonical = Loaded.CanonicalDefinition;
        }
        if (Loaded.Succeeded())
        {
            CanonicalRecords.emplace_back(
                Path.filename().generic_string(),
                DigestText(Loaded.CanonicalDefinition.ToStdString()));
            CanonicalCorpus += Path.filename().generic_string();
            CanonicalCorpus.push_back('\n');
            CanonicalCorpus +=
                Loaded.CanonicalDefinition.ToStdString();
        }
        ++ValidCount;
    }
    for (const auto& Path : InvalidPaths)
    {
        const auto Loaded = FMaterialShaderSourceLoader::Load(
            MakeCorpusRequest(ReadText(Path), Path));
        bInvalid = bInvalid && !Loaded.Succeeded() &&
            Loaded.Payloads.empty() && Loaded.Metadata.empty();
        FailureCorpus += Path.filename().generic_string();
        FailureCorpus.push_back('=');
        FailureCorpus +=
            std::to_string(static_cast<unsigned>(Loaded.Result));
        FailureCorpus.push_back('\n');
        FailureRecords.emplace_back(
            Path.filename().generic_string(),
            Loaded.Result);
        ++InvalidCount;
    }
    Record(
        Result,
        bValid && ValidCount == 41,
        "All 41 representative valid definitions load transactionally");
    Record(
        Result,
        bInvalid && InvalidCount == 41,
        "All 41 malformed and boundary definitions fail atomically");
    std::string CanonicalGolden;
    for (const auto& [Name, Digest] : CanonicalRecords)
    {
        CanonicalGolden += Name + "=" + Digest.ToStdString() + "\n";
    }
    std::string FailureGolden;
    for (const auto& [Name, Failure] : FailureRecords)
    {
        FailureGolden += Name + "=" +
            std::to_string(static_cast<unsigned>(Failure)) + "\n";
    }
    Record(
        Result,
        CanonicalGolden ==
            ReadText(Root / "Golden/canonical-digests.txt") &&
        FailureGolden ==
            ReadText(Root / "Golden/failure-results.txt"),
        "Corpus canonical digests and first failure categories match golden evidence");

    bool bDeterministic = !FirstCanonical.IsEmpty();
    for (int Run = 0;
         Run < Options.DeterminismRuns && bDeterministic;
         ++Run)
    {
        const auto Loaded = FMaterialShaderSourceLoader::Load(
            MakeCorpusRequest(
                FirstCanonical.ToStdString(),
                Root / "Valid/material-00.json"));
        bDeterministic =
            Loaded.Succeeded() &&
            Loaded.CanonicalDefinition == FirstCanonical;
    }
    Record(
        Result,
        bDeterministic,
        "Canonical definition remains byte-identical across requested repetitions");

    TArray<uint8> ThreadResults(8, 0);
    TArray<std::thread> Threads;
    for (std::size_t Index = 0; Index < ThreadResults.size(); ++Index)
    {
        Threads.emplace_back(
            [Index, &ThreadResults, &FirstCanonical, &Root]()
            {
                const auto Loaded = FMaterialShaderSourceLoader::Load(
                    MakeCorpusRequest(
                        FirstCanonical.ToStdString(),
                        Root / "Valid/material-00.json"));
                ThreadResults[Index] = static_cast<uint8>(
                    Loaded.Succeeded() &&
                    Loaded.CanonicalDefinition == FirstCanonical);
            });
    }
    for (std::thread& Thread : Threads) Thread.join();
    const bool bReadersAgree = std::all_of(
        ThreadResults.begin(),
        ThreadResults.end(),
        [](uint8 Passed) { return Passed != 0; });
    Record(
        Result,
        bReadersAgree,
        "Eight concurrent immutable parser readers agree");

    if (!Options.ReportPath.IsEmpty())
    {
        const auto Elapsed = std::chrono::duration_cast<
            std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - Start).count();
        const std::filesystem::path ReportPath =
            Options.ReportPath.ToStdString();
        std::error_code Error;
        if (ReportPath.has_parent_path())
        {
            std::filesystem::create_directories(
                ReportPath.parent_path(), Error);
        }
        std::ofstream Report(
            ReportPath,
            std::ios::binary | std::ios::trunc);
        Report << "feature=023\n"
               << "valid=" << ValidCount << '\n'
               << "invalid=" << InvalidCount << '\n'
               << "determinism_runs=" << Options.DeterminismRuns << '\n'
               << "reader_count=8\n"
               << "reader_outcome="
               << (bReadersAgree ? "pass" : "fail") << '\n'
               << "canonical_corpus_sha256="
               << DigestText(CanonicalCorpus).CStr()
               << '\n'
               << "failure_corpus_sha256="
               << DigestText(FailureCorpus).CStr()
               << '\n'
               << "elapsed_ms_non_gating=" << Elapsed << '\n'
#if defined(_WIN32)
               << "os=windows\n"
#elif defined(__APPLE__)
               << "os=macos\n"
#else
               << "os=linux\n"
#endif
#if defined(__aarch64__) || defined(_M_ARM64)
               << "cpu=arm64\n"
#elif defined(__x86_64__) || defined(_M_X64)
               << "cpu=x86_64\n"
#else
               << "cpu=other\n"
#endif
#if defined(_DEBUG)
               << "build=debug\n"
#else
               << "build=release\n"
#endif
#if defined(__clang__)
               << "compiler=clang\n";
#elif defined(_MSC_VER)
               << "compiler=msvc\n";
#else
               << "compiler=gcc\n";
#endif
        for (const auto& [Name, Digest] : CanonicalRecords)
        {
            Report << "canonical." << Name << '='
                   << Digest.CStr() << '\n';
        }
        for (const auto& [Name, Failure] : FailureRecords)
        {
            Report << "failure." << Name << '='
                   << static_cast<unsigned>(Failure) << '\n';
        }
        Record(
            Result,
            !Error && Report.good(),
            "Feature 023 normalized report is written");
    }
}

void TestRepositoryAssets(FAssetMaterialShaderTestResult& Result)
{
    const std::filesystem::path Root = "Content/Shaders";
    const std::filesystem::path Programs[] = {
        "Triangle/Triangle.shader.json",
        "Deferred/Surface.shader.json",
        "Deferred/Composition.shader.json",
        "Deferred/DirectionalLight.shader.json",
        "Deferred/PointLight.shader.json",
        "Deferred/SpotLight.shader.json"};
    FAssetExtensionRegistry Extensions;
    FAssetRegistrationToken ResolverToken;
    const bool bRegistered =
        Extensions.Register(
            MakeShared<FContentResolver>(Root),
            ResolverToken) == EAssetResult::Success;
    std::set<FAssetId> SourceIds;
    std::set<FAssetId> PayloadIds;
    std::map<FAssetId, FAssetVersion> PayloadVersions;
    bool bLoaded = bRegistered;
    bool bSelected = bRegistered;
    for (const auto& ProgramPath : Programs)
    {
        const auto Loaded = FMaterialShaderSourceLoader::Load(
            MakeRepositoryRequest(Root, ProgramPath, Extensions));
        bLoaded = bLoaded && Loaded.Succeeded() &&
            Loaded.Payloads.size() == 5 &&
            Loaded.Metadata.size() == 5;
        TSharedPtr<const FShaderAsset> Program;
        for (const auto& Payload : Loaded.Payloads)
        {
            if (const auto Source =
                    std::dynamic_pointer_cast<const FShaderSourceAsset>(
                        Payload))
            {
                SourceIds.insert(Source->GetId());
            }
            else if (const auto Compiled =
                         std::dynamic_pointer_cast<
                             const FShaderPayloadAsset>(Payload))
            {
                PayloadIds.insert(Compiled->GetId());
                PayloadVersions.emplace(
                    Compiled->GetId(),
                    Compiled->GetVersion());
            }
            else if (const auto Shader =
                         std::dynamic_pointer_cast<const FShaderAsset>(
                             Payload))
            {
                Program = Shader;
            }
        }
        if (Program)
        {
            FShaderTargetRequest Target;
            Target.Backend = EShaderBackendFamily::Vulkan;
            Target.AcceptableProfiles = {FString("vulkan-1.3")};
            FSelectedShaderProgram Selection;
            const FLoadedPayloadLookup Lookup(Loaded.Payloads);
            bSelected = bSelected &&
                SelectShaderProgram(
                    *Program,
                    Target,
                    Lookup,
                    Selection) == EAssetResult::Success &&
                Selection.Stages.size() == 2 &&
                Selection.SourceManifest.size() == 5;
        }
        else
        {
            bSelected = false;
        }
    }
    Record(
        Result,
        bLoaded && SourceIds.size() == 11 && PayloadIds.size() == 11,
        "All six repository programs load 11 source and 11 payload identities");
    Record(
        Result,
        bSelected,
        "Repository programs select complete Vulkan manifests");

    const FAssetId Point = []()
    {
        FAssetId Value;
        (void)FAssetId::Create(
            FString("ShaderPayload"),
            FString("Engine/Shaders/Deferred/PointLight"),
            FString("payload.vulkan.vertex"),
            Value);
        return Value;
    }();
    const FAssetId Spot = []()
    {
        FAssetId Value;
        (void)FAssetId::Create(
            FString("ShaderPayload"),
            FString("Engine/Shaders/Deferred/SpotLight"),
            FString("payload.vulkan.vertex"),
            Value);
        return Value;
    }();
    Record(
        Result,
        Point != Spot &&
            PayloadVersions.contains(Point) &&
            PayloadVersions.contains(Spot) &&
            PayloadVersions.at(Point) == PayloadVersions.at(Spot),
        "Equal Point and Spot payload bytes retain distinct stable identities");

    const auto Triangle = FMaterialShaderSourceLoader::Load(
        MakeRepositoryRequest(
            Root,
            Programs[0],
            Extensions));
    TSharedPtr<const FShaderAsset> TriangleProgram;
    for (const auto& Payload : Triangle.Payloads)
    {
        if (const auto Shader =
                std::dynamic_pointer_cast<const FShaderAsset>(Payload))
        {
            TriangleProgram = Shader;
            break;
        }
    }
    FSelectedShaderProgram FallbackSelection;
    FShaderTargetRequest FallbackTarget;
    FallbackTarget.Backend = EShaderBackendFamily::Vulkan;
    FallbackTarget.AcceptableProfiles = {
        FString("missing"),
        FString("vulkan-1.3")};
    const FLoadedPayloadLookup TriangleLookup(Triangle.Payloads);
    const bool bOrderedFallback =
        TriangleProgram &&
        SelectShaderProgram(
            *TriangleProgram,
            FallbackTarget,
            TriangleLookup,
            FallbackSelection) == EAssetResult::Success &&
        FallbackSelection.SelectedProfile == FString("vulkan-1.3");
    FallbackTarget.AcceptableProfiles = {
        FString("vulkan-1.3"),
        FString("vulkan-1.3")};
    FSelectedShaderProgram InvalidSelection = FallbackSelection;
    const bool bDuplicateProfilesRejected =
        TriangleProgram &&
        SelectShaderProgram(
            *TriangleProgram,
            FallbackTarget,
            TriangleLookup,
            InvalidSelection) == EAssetResult::InvalidInput &&
        InvalidSelection.Stages.empty();
    FallbackTarget.Backend = EShaderBackendFamily::Metal;
    FallbackTarget.AcceptableProfiles = {FString("vulkan-1.3")};
    const bool bCrossBackendRejected =
        TriangleProgram &&
        SelectShaderProgram(
            *TriangleProgram,
            FallbackTarget,
            TriangleLookup,
            InvalidSelection) == EAssetResult::TargetUnavailable;
    Record(
        Result,
        bOrderedFallback && bDuplicateProfilesRejected &&
            bCrossBackendRejected,
        "Shader target selection honors profile order without backend fallback");

    const TArray<uint8> VertexSpirv =
        ReadBytes(Root / "Triangle/Triangle.vert.spv");
    TArray<uint8> CorruptSpirv = VertexSpirv;
    if (!CorruptSpirv.empty()) CorruptSpirv[0] = 0;
    TArray<uint8> TruncatedSpirv = VertexSpirv;
    if (!TruncatedSpirv.empty()) TruncatedSpirv.pop_back();
    Record(
        Result,
        Private::ValidateShaderPayloadBytes(
            VertexSpirv,
            EShaderPayloadFormat::SPIRV,
            EShaderStage::Vertex,
            FString("main")) == EAssetResult::Success &&
        Private::ValidateShaderPayloadBytes(
            VertexSpirv,
            EShaderPayloadFormat::SPIRV,
            EShaderStage::Fragment,
            FString("main")) == EAssetResult::DependencyMismatch &&
        Private::ValidateShaderPayloadBytes(
            VertexSpirv,
            EShaderPayloadFormat::SPIRV,
            EShaderStage::Vertex,
            FString("other")) == EAssetResult::DependencyMismatch &&
        Private::ValidateShaderPayloadBytes(
            CorruptSpirv,
            EShaderPayloadFormat::SPIRV,
            EShaderStage::Vertex,
            FString("main")) == EAssetResult::DependencyMismatch &&
        Private::ValidateShaderPayloadBytes(
            TruncatedSpirv,
            EShaderPayloadFormat::SPIRV,
            EShaderStage::Vertex,
            FString("main")) == EAssetResult::DependencyMismatch,
        "SPIR-V validation checks header alignment stage and entry point");

    std::string InvalidLocator =
        ReadText(Root / Programs[0]);
    InvalidLocator.replace(
        InvalidLocator.find("Triangle.vert"),
        std::string("Triangle.vert").size(),
        "../Triangle.vert");
    auto InvalidLocatorRequest =
        MakeRepositoryRequest(Root, Programs[0], Extensions);
    InvalidLocatorRequest.Descriptor.Size = InvalidLocator.size();
    InvalidLocatorRequest.Source = FAssetSourceLease(
        MakeShared<FMemoryDefinitionSource>(InvalidLocator));
    std::string InvalidDigest =
        ReadText(Root / Programs[0]);
    const std::string DigestPrefix = "sha256:";
    const std::size_t DigestAt =
        InvalidDigest.find(DigestPrefix) + DigestPrefix.size();
    InvalidDigest.replace(DigestAt, 64, std::string(64, '0'));
    auto InvalidDigestRequest =
        MakeRepositoryRequest(Root, Programs[0], Extensions);
    InvalidDigestRequest.Descriptor.Size = InvalidDigest.size();
    InvalidDigestRequest.Source = FAssetSourceLease(
        MakeShared<FMemoryDefinitionSource>(InvalidDigest));
    Record(
        Result,
        FMaterialShaderSourceLoader::Load(InvalidLocatorRequest).Result ==
            EAssetResult::DependencyMismatch &&
        FMaterialShaderSourceLoader::Load(InvalidDigestRequest).Result ==
            EAssetResult::DependencyMismatch,
        "Shader dependency loading rejects traversal and digest mismatch");

    bool bMaterials = true;
    for (const std::filesystem::path& MaterialPath : {
             std::filesystem::path(
                 "Content/Materials/DeferredSurface.material.json"),
             std::filesystem::path(
                 "Content/Materials/DeferredSurfacePolished.material-instance.json")})
    {
        const auto Loaded = FMaterialShaderSourceLoader::Load(
            MakeCorpusRequest(ReadText(MaterialPath), MaterialPath));
        bMaterials = bMaterials && Loaded.Succeeded() &&
            Loaded.Payloads.size() == 1 &&
            !Loaded.Dependencies.empty();
    }
    Record(
        Result,
        bMaterials,
        "Representative repository material and instance definitions load");
}

void TestMaterialResolution(FAssetMaterialShaderTestResult& Result)
{
    const std::filesystem::path BasePath =
        "Content/Materials/DeferredSurface.material.json";
    const std::filesystem::path InstancePath =
        "Content/Materials/DeferredSurfacePolished.material-instance.json";
    const auto Base = FMaterialShaderSourceLoader::Load(
        MakeCorpusRequest(ReadText(BasePath), BasePath));
    const auto Instance = FMaterialShaderSourceLoader::Load(
        MakeCorpusRequest(ReadText(InstancePath), InstancePath));
    FMaterialLookup Lookup;
    if (Base.Succeeded()) Lookup.Add(Base.Payloads.front());
    if (Instance.Succeeded()) Lookup.Add(Instance.Payloads.front());
    const FAssetId Shader =
        MakeId("ShaderProgram", "Engine/Shaders/Deferred/Surface");
    const FAssetId Texture =
        MakeId("Texture", "Engine/Textures/DefaultWhite");
    Lookup.AddDependency(Shader, MakeVersion("surface-shader"));
    Lookup.AddDependency(Texture, MakeVersion("default-white"));

    FResolvedMaterialAsset Resolved;
    const FAssetId InstanceId = MakeId(
        "MaterialInstance",
        "Engine/Materials/DeferredSurfacePolished");
    const EAssetResult Resolution =
        ResolveMaterial(
            InstanceId,
            Lookup,
            FMaterialShaderAssetLimits{},
            Resolved);
    const auto Roughness = std::find_if(
        Resolved.EffectiveParameters.begin(),
        Resolved.EffectiveParameters.end(),
        [](const FMaterialAssetParameter& Parameter)
        {
            return Parameter.Name == FString("Roughness");
        });
    Record(
        Result,
        Resolution == EAssetResult::Success &&
            Roughness != Resolved.EffectiveParameters.end() &&
            std::get<float>(Roughness->Value.Value) == 0.125f &&
            Resolved.SourceManifest.size() == 4,
        "Material instance resolution applies nearest override and complete versions");

    FMaterialLookup MissingDependency;
    if (Base.Succeeded()) MissingDependency.Add(Base.Payloads.front());
    if (Instance.Succeeded())
        MissingDependency.Add(Instance.Payloads.front());
    MissingDependency.AddDependency(
        Shader, MakeVersion("surface-shader"));
    FResolvedMaterialAsset Unchanged = Resolved;
    Record(
        Result,
        ResolveMaterial(
            InstanceId,
            MissingDependency,
            FMaterialShaderAssetLimits{},
            Unchanged) == EAssetResult::UnresolvedDependency,
        "Material resolution rejects a missing texture version");

    std::string CycleText = ReadText(InstancePath);
    const std::string ParentType = R"("type": "Material")";
    CycleText.replace(
        CycleText.find(ParentType),
        ParentType.size(),
        R"("type": "MaterialInstance")");
    const std::string ParentPath =
        "Engine/Materials/DeferredSurface";
    CycleText.replace(
        CycleText.rfind(ParentPath),
        ParentPath.size(),
        "Engine/Materials/DeferredSurfacePolished");
    const auto Cycle = FMaterialShaderSourceLoader::Load(
        MakeCorpusRequest(CycleText, InstancePath));
    FMaterialLookup CycleLookup;
    if (Cycle.Succeeded()) CycleLookup.Add(Cycle.Payloads.front());
    FResolvedMaterialAsset CycleOutput;
    Record(
        Result,
        Cycle.Succeeded() &&
            ResolveMaterial(
                InstanceId,
                CycleLookup,
                FMaterialShaderAssetLimits{},
                CycleOutput) == EAssetResult::InvalidInstanceChain,
        "Material instance resolution detects a self-parent cycle");
}

} // namespace

FAssetMaterialShaderTestResult RunAssetMaterialShaderTests(
    const FAssetMaterialShaderTestOptions& Options)
{
    FAssetMaterialShaderTestResult Result;
    TestLimitsAndStrictParsing(Result);
    TestCanonicalOwnershipAndRollback(Result);
    TestDiagnosticsAndLimits(Result);
    TestSchemaCategoriesAndInterfaceRoundTrip(Result);
    TestMaterialSchemaV2(Result);
    TestSchemaSpecificLimits(Result);
    TestFixtureCorpus(Result, Options);
    TestRepositoryAssets(Result);
    TestMaterialResolution(Result);
    return Result;
}
