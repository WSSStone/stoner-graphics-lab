#include "FMaterialShaderJsonCodec.h"

#include "Asset/FAssetDigest.h"
#include "Core/FUnicode.h"

#include "../../../ThirdParty/yyjson/yyjson.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <set>
#include <string>
#include <string_view>
#include <vector>

namespace Stoner::Asset::Private
{
namespace
{

bool ParseRawNumber(const char* Raw, std::size_t Length, double& Out)
{
    const std::string Text(Raw, Length);
    yyjson_val Value{};
    const char* ParsedEnd = yyjson_read_number(
        Text.c_str(), &Value, YYJSON_READ_NOFLAG, nullptr, nullptr);
    if (ParsedEnd != Text.c_str() + Text.size() || !yyjson_is_num(&Value))
    {
        return false;
    }
    Out = yyjson_get_num(&Value);
    return std::isfinite(Out);
}

using FAllowed = std::initializer_list<std::string_view>;

void AddDiagnostic(
    FAssetDiagnosticList* Diagnostics,
    EAssetStage Stage,
    EAssetResult Result,
    const char* Field,
    const char* Reason)
{
    if (!Diagnostics)
    {
        return;
    }
    Diagnostics->push_back({
        Stage,
        Result,
        EAssetDiagnosticSeverity::Error,
        Core::FString("asset.definition"),
        {},
        {},
        Core::FString(Field),
        {},
        {},
        Core::FString(Reason),
        {}});
}

bool HasBomOrNul(std::span<const Core::uint8> Bytes)
{
    return (Bytes.size() >= 3 &&
            Bytes[0] == 0xef && Bytes[1] == 0xbb && Bytes[2] == 0xbf) ||
        std::find(Bytes.begin(), Bytes.end(), Core::uint8{0}) != Bytes.end();
}

EAssetResult Preflight(
    yyjson_val* Root,
    const FMaterialShaderAssetLimits& Limits)
{
    if (!yyjson_is_obj(Root))
    {
        return EAssetResult::InvalidDefinition;
    }
    struct FNode
    {
        yyjson_val* Value = nullptr;
        Core::usize Depth = 0;
    };
    std::vector<FNode> Stack{{Root, 1}};
    Core::usize Values = 0;
    while (!Stack.empty())
    {
        const FNode Node = Stack.back();
        Stack.pop_back();
        if (++Values > Limits.MaxJsonValues ||
            Node.Depth > Limits.MaxJsonDepth)
        {
            return EAssetResult::DefinitionLimitExceeded;
        }
        if (yyjson_is_obj(Node.Value))
        {
            const std::size_t Count = yyjson_obj_size(Node.Value);
            if (Count > Limits.MaxObjectMembers)
            {
                return EAssetResult::DefinitionLimitExceeded;
            }
            std::set<std::string> Keys;
            yyjson_obj_iter Iterator =
                yyjson_obj_iter_with(Node.Value);
            yyjson_val* Key = nullptr;
            while ((Key = yyjson_obj_iter_next(&Iterator)) != nullptr)
            {
                const char* Text = yyjson_get_str(Key);
                const std::size_t Length = yyjson_get_len(Key);
                yyjson_val* Child = yyjson_obj_iter_get_val(Key);
                if (!Text || Length > Limits.MaxTextBytes ||
                    !Keys.emplace(Text, Length).second)
                {
                    return EAssetResult::InvalidDefinition;
                }
                const std::string_view Name(Text, Length);
                if (yyjson_is_str(Child))
                {
                    const std::size_t ChildLength = yyjson_get_len(Child);
                    if ((Name == "locator" &&
                         ChildLength > Limits.MaxLocatorBytes) ||
                        ((Name == "name" || Name == "type" ||
                          Name == "stage" || Name == "entryPoint" ||
                          Name == "language" || Name == "backend" ||
                          Name == "profile" || Name == "format" ||
                          Name == "producer" ||
                          Name == "producerVersion" ||
                          Name == "programKind" || Name == "domain" ||
                          Name == "blendMode" || Name == "kind") &&
                         ChildLength > Limits.MaxTokenBytes))
                    {
                        return EAssetResult::DefinitionLimitExceeded;
                    }
                }
                Stack.push_back({Child, Node.Depth + 1});
            }
        }
        else if (yyjson_is_arr(Node.Value))
        {
            const std::size_t Count = yyjson_arr_size(Node.Value);
            if (Count > Limits.MaxArrayElements)
            {
                return EAssetResult::DefinitionLimitExceeded;
            }
            std::size_t Index = 0;
            std::size_t Maximum = 0;
            yyjson_val* Value = nullptr;
            yyjson_arr_foreach(Node.Value, Index, Maximum, Value)
            {
                Stack.push_back({Value, Node.Depth + 1});
            }
        }
        else if (yyjson_is_str(Node.Value) &&
                 yyjson_get_len(Node.Value) > Limits.MaxTextBytes)
        {
            return EAssetResult::DefinitionLimitExceeded;
        }
        else if (yyjson_is_raw(Node.Value) &&
                 yyjson_get_len(Node.Value) > Limits.MaxNumberTokenBytes)
        {
            return EAssetResult::DefinitionLimitExceeded;
        }
    }
    return EAssetResult::Success;
}

std::size_t ArraySize(yyjson_val* Object, const char* Name)
{
    yyjson_val* Value = yyjson_obj_get(Object, Name);
    return yyjson_is_arr(Value) ? yyjson_arr_size(Value) : 0;
}

std::size_t TextureParameterCount(yyjson_val* Root, const char* Name)
{
    yyjson_val* Parameters = yyjson_obj_get(Root, Name);
    if (!yyjson_is_arr(Parameters))
    {
        return 0;
    }
    std::size_t Count = 0;
    std::size_t Index = 0;
    std::size_t Maximum = 0;
    yyjson_val* Parameter = nullptr;
    yyjson_arr_foreach(
        Parameters, Index, Maximum, Parameter)
    {
        yyjson_val* Type = yyjson_obj_get(Parameter, "type");
        if (yyjson_is_str(Type) &&
            (std::string_view(
                 yyjson_get_str(Type), yyjson_get_len(Type)) == "texture" ||
             std::string_view(
                 yyjson_get_str(Type), yyjson_get_len(Type)) ==
                 "textureBinding"))
        {
            ++Count;
        }
    }
    return Count;
}

EAssetResult SemanticPreflight(
    yyjson_val* Root,
    std::string_view Schema,
    const FMaterialShaderAssetLimits& Limits)
{
    const std::size_t RequiredExtensions =
        ArraySize(Root, "requiredExtensions");
    yyjson_val* Extensions = yyjson_obj_get(Root, "extensions");
    if (RequiredExtensions > Limits.MaxExtensions ||
        (yyjson_is_obj(Extensions) &&
         yyjson_obj_size(Extensions) > Limits.MaxExtensions))
    {
        return EAssetResult::DefinitionLimitExceeded;
    }
    if (Schema == "stoner.shader-program")
    {
        const std::size_t Stages = ArraySize(Root, "stages");
        const std::size_t Flags = ArraySize(Root, "allowedPermutationFlags");
        const std::size_t Required = ArraySize(Root, "requiredParameters");
        const std::size_t Variants = ArraySize(Root, "variants");
        if (Stages > Limits.MaxStages ||
            Stages > Limits.MaxSourceRecords ||
            Flags > Limits.MaxPermutationFlags ||
            Required > Limits.MaxParameters ||
            Variants > Limits.MaxVariants)
        {
            return EAssetResult::DefinitionLimitExceeded;
        }
        Core::uint64 Payloads = 0;
        Core::uint64 Dependencies = Stages;
        yyjson_val* VariantArray = yyjson_obj_get(Root, "variants");
        if (yyjson_is_arr(VariantArray))
        {
            std::size_t Index = 0;
            std::size_t Maximum = 0;
            yyjson_val* Variant = nullptr;
            yyjson_arr_foreach(
                VariantArray, Index, Maximum, Variant)
            {
                const std::size_t VariantFlags =
                    ArraySize(Variant, "flags");
                const std::size_t VariantPayloads =
                    ArraySize(Variant, "payloads");
                if (VariantFlags > Limits.MaxPermutationFlags ||
                    !CheckedMaterialShaderAdd(
                        Payloads, VariantPayloads, Payloads) ||
                    !CheckedMaterialShaderAdd(
                        Dependencies, VariantPayloads, Dependencies))
                {
                    return EAssetResult::DefinitionLimitExceeded;
                }
            }
        }
        yyjson_val* Interface = yyjson_obj_get(Root, "interface");
        if (yyjson_is_obj(Interface) &&
            ArraySize(Interface, "bindings") >
                Limits.MaxInterfaceBindingsPerStage)
        {
            return EAssetResult::DefinitionLimitExceeded;
        }
        if (Payloads > Limits.MaxPayloadRecords ||
            Dependencies > Limits.MaxDependencies)
        {
            return EAssetResult::DefinitionLimitExceeded;
        }
    }
    else if (Schema == "stoner.material")
    {
        if (ArraySize(Root, "permutationFlags") >
                Limits.MaxPermutationFlags ||
            ArraySize(Root, "parameters") > Limits.MaxParameters ||
            TextureParameterCount(Root, "parameters") + 1 >
                Limits.MaxDependencies)
        {
            return EAssetResult::DefinitionLimitExceeded;
        }
    }
    else if (Schema == "stoner.material-instance")
    {
        if (ArraySize(Root, "overrides") > Limits.MaxParameters ||
            TextureParameterCount(Root, "overrides") + 1 >
                Limits.MaxDependencies)
        {
            return EAssetResult::DefinitionLimitExceeded;
        }
    }
    return EAssetResult::Success;
}

bool ClosedObject(yyjson_val* Object, FAllowed Allowed)
{
    if (!yyjson_is_obj(Object))
    {
        return false;
    }
    yyjson_obj_iter Iterator = yyjson_obj_iter_with(Object);
    yyjson_val* Key = nullptr;
    while ((Key = yyjson_obj_iter_next(&Iterator)) != nullptr)
    {
        const std::string_view Name(
            yyjson_get_str(Key), yyjson_get_len(Key));
        if (std::find(Allowed.begin(), Allowed.end(), Name) == Allowed.end())
        {
            return false;
        }
    }
    return true;
}

bool String(yyjson_val* Object, const char* Name, Core::FString& Out)
{
    yyjson_val* Value = yyjson_obj_get(Object, Name);
    if (!yyjson_is_str(Value))
    {
        return false;
    }
    Out = Core::FString(std::string(
        yyjson_get_str(Value), yyjson_get_len(Value)));
    return true;
}

bool Boolean(yyjson_val* Object, const char* Name, bool& Out)
{
    yyjson_val* Value = yyjson_obj_get(Object, Name);
    if (!yyjson_is_bool(Value))
    {
        return false;
    }
    Out = yyjson_get_bool(Value);
    return true;
}

bool Unsigned(yyjson_val* Object, const char* Name, Core::uint32& Out)
{
    yyjson_val* Value = yyjson_obj_get(Object, Name);
    if (!yyjson_is_raw(Value))
    {
        return false;
    }
    const char* Raw = yyjson_get_raw(Value);
    const std::size_t Length = yyjson_get_len(Value);
    Core::uint32 Parsed = 0;
    const auto Result = std::from_chars(Raw, Raw + Length, Parsed);
    if (Result.ec != std::errc{} || Result.ptr != Raw + Length)
    {
        return false;
    }
    Out = Parsed;
    return true;
}

bool FloatValue(yyjson_val* Value, float& Out)
{
    if (!yyjson_is_raw(Value))
    {
        return false;
    }
    const char* Raw = yyjson_get_raw(Value);
    const std::size_t Length = yyjson_get_len(Value);
    double ParsedDouble = 0.0;
    if (!ParseRawNumber(Raw, Length, ParsedDouble))
    {
        return false;
    }
    const float Parsed = static_cast<float>(ParsedDouble);
    if (!std::isfinite(Parsed))
    {
        return false;
    }
    const bool bNonZeroLexeme =
        std::find_if(
            Raw,
            Raw + Length,
            [](char Character)
            {
                return Character >= '1' && Character <= '9';
            }) != Raw + Length;
    if (Parsed == 0.0f && bNonZeroLexeme)
    {
        return false;
    }
    Out = Parsed == 0.0f ? 0.0f : Parsed;
    return true;
}

bool AssetId(yyjson_val* Value, const char* ExpectedType, FAssetId& Out)
{
    if (!ClosedObject(Value, {"type", "path", "subresource"}))
    {
        return false;
    }
    Core::FString Type;
    Core::FString Path;
    if (!String(Value, "type", Type) ||
        !String(Value, "path", Path) ||
        Type != ExpectedType)
    {
        return false;
    }
    std::optional<Core::FString> Subresource;
    if (yyjson_val* RawSubresource = yyjson_obj_get(Value, "subresource"))
    {
        if (!yyjson_is_str(RawSubresource))
        {
            return false;
        }
        Subresource = Core::FString(std::string(
            yyjson_get_str(RawSubresource),
            yyjson_get_len(RawSubresource)));
    }
    return FAssetId::Create(Type, Path, Subresource, Out) ==
        EAssetResult::Success;
}

bool AssetIdText(yyjson_val* Value, const char* ExpectedType, FAssetId& Out)
{
    if (!yyjson_is_str(Value))
    {
        return false;
    }
    const std::string_view Text(yyjson_get_str(Value), yyjson_get_len(Value));
    const std::string Prefix = std::string(ExpectedType) + ":";
    if (!Text.starts_with(Prefix))
    {
        return false;
    }
    const std::string_view Remainder = Text.substr(Prefix.size());
    const std::size_t Hash = Remainder.find('#');
    const std::string_view Path = Remainder.substr(0, Hash);
    const std::optional<Core::FString> Subresource = Hash == std::string_view::npos
        ? std::nullopt
        : std::optional<Core::FString>(Core::FString(
              std::string(Remainder.substr(Hash + 1))));
    return FAssetId::Create(
               Core::FString(ExpectedType),
               Core::FString(std::string(Path)),
               Subresource,
               Out) == EAssetResult::Success;
}

bool Digest(yyjson_val* Object, FAssetDigest& Out)
{
    Core::FString Text;
    if (!String(Object, "digest", Text) ||
        !Text.View().starts_with("sha256:"))
    {
        return false;
    }
    return FAssetDigest::ParseLowerHex(
        Core::FString(std::string(Text.View().substr(7))),
        Out) == EAssetResult::Success;
}

bool StringArray(
    yyjson_val* Value,
    Core::TArray<Core::FString>& Out)
{
    Out.clear();
    if (!yyjson_is_arr(Value))
    {
        return false;
    }
    std::size_t Index = 0;
    std::size_t Maximum = 0;
    yyjson_val* Element = nullptr;
    yyjson_arr_foreach(Value, Index, Maximum, Element)
    {
        if (!yyjson_is_str(Element))
        {
            return false;
        }
        Out.emplace_back(std::string(
            yyjson_get_str(Element), yyjson_get_len(Element)));
    }
    return true;
}

bool IsExtensionName(
    std::string_view Name,
    const FMaterialShaderAssetLimits& Limits)
{
    if (Name.empty() || Name.size() > Limits.MaxTokenBytes ||
        Name.front() == '.' || Name.back() == '.' ||
        Name.find('.') == std::string_view::npos)
    {
        return false;
    }
    return std::all_of(
        Name.begin(),
        Name.end(),
        [](char Character)
        {
            return (Character >= 'A' && Character <= 'Z') ||
                (Character >= 'a' && Character <= 'z') ||
                (Character >= '0' && Character <= '9') ||
                Character == '_' || Character == '-' ||
                Character == '.';
        });
}

EAssetResult ValidateExtensions(
    yyjson_val* Root,
    const FMaterialShaderAssetLimits& Limits)
{
    Core::TArray<Core::FString> Required;
    if (!StringArray(
            yyjson_obj_get(Root, "requiredExtensions"),
            Required))
    {
        return EAssetResult::InvalidDefinition;
    }
    std::sort(Required.begin(), Required.end());
    if (std::adjacent_find(Required.begin(), Required.end()) !=
            Required.end() ||
        std::any_of(
            Required.begin(),
            Required.end(),
            [&Limits](const Core::FString& Name)
            {
                return !IsExtensionName(Name.View(), Limits);
            }))
    {
        return EAssetResult::InvalidDefinition;
    }
    yyjson_val* Extensions = yyjson_obj_get(Root, "extensions");
    if (!yyjson_is_obj(Extensions))
    {
        return EAssetResult::InvalidDefinition;
    }
    yyjson_obj_iter Iterator = yyjson_obj_iter_with(Extensions);
    yyjson_val* Key = nullptr;
    while ((Key = yyjson_obj_iter_next(&Iterator)) != nullptr)
    {
        const std::string_view Name(
            yyjson_get_str(Key), yyjson_get_len(Key));
        if (!IsExtensionName(Name, Limits) ||
            !yyjson_is_obj(yyjson_obj_iter_get_val(Key)))
        {
            return EAssetResult::InvalidDefinition;
        }
    }
    for (const Core::FString& Name : Required)
    {
        if (!yyjson_obj_getn(
                Extensions, Name.View().data(), Name.View().size()))
        {
            return EAssetResult::InvalidDefinition;
        }
    }
    return Required.empty()
        ? EAssetResult::Success
        : EAssetResult::UnknownRequiredExtension;
}

bool ParseDependency(
    yyjson_val* Object,
    const char* Type,
    FAssetId& Id,
    Core::FString& Locator,
    FAssetDigest& Expected)
{
    return ClosedObject(Object, {"asset", "locator", "digest"}) &&
        AssetId(yyjson_obj_get(Object, "asset"), Type, Id) &&
        String(Object, "locator", Locator) &&
        Digest(Object, Expected);
}

std::optional<EShaderStage> ShaderStage(std::string_view Text)
{
    if (Text == "vertex") return EShaderStage::Vertex;
    if (Text == "fragment") return EShaderStage::Fragment;
    if (Text == "compute") return EShaderStage::Compute;
    return std::nullopt;
}

std::optional<EShaderBackendFamily> Backend(std::string_view Text)
{
    if (Text == "vulkan") return EShaderBackendFamily::Vulkan;
    if (Text == "metal") return EShaderBackendFamily::Metal;
    if (Text == "directx12") return EShaderBackendFamily::DirectX12;
    if (Text == "opengl") return EShaderBackendFamily::OpenGL;
    if (Text == "gles") return EShaderBackendFamily::GLES;
    return std::nullopt;
}

std::optional<EShaderPayloadFormat> PayloadFormat(std::string_view Text)
{
    if (Text == "spirv") return EShaderPayloadFormat::SPIRV;
    if (Text == "msl") return EShaderPayloadFormat::MSL;
    if (Text == "dxil") return EShaderPayloadFormat::DXIL;
    if (Text == "glsl") return EShaderPayloadFormat::GLSL;
    if (Text == "metal-library") return EShaderPayloadFormat::MetalLibrary;
    return std::nullopt;
}

std::optional<EShaderResourceKind> ResourceKind(std::string_view Text)
{
    if (Text == "uniformBuffer") return EShaderResourceKind::UniformBuffer;
    if (Text == "sampledTexture") return EShaderResourceKind::SampledTexture;
    if (Text == "sampler") return EShaderResourceKind::Sampler;
    if (Text == "storageBuffer") return EShaderResourceKind::StorageBuffer;
    if (Text == "storageTexture") return EShaderResourceKind::StorageTexture;
    if (Text == "combinedTextureSampler")
        return EShaderResourceKind::CombinedTextureSampler;
    return std::nullopt;
}

bool ParseVisibility(
    yyjson_val* Value,
    Core::TArray<EShaderStage>& Out)
{
    Out.clear();
    if (!yyjson_is_arr(Value))
    {
        return false;
    }
    std::size_t Index = 0;
    std::size_t Maximum = 0;
    yyjson_val* Element = nullptr;
    yyjson_arr_foreach(Value, Index, Maximum, Element)
    {
        if (!yyjson_is_str(Element))
        {
            return false;
        }
        const auto Stage = ShaderStage(std::string_view(
            yyjson_get_str(Element), yyjson_get_len(Element)));
        if (!Stage)
        {
            return false;
        }
        Out.push_back(*Stage);
    }
    return true;
}

std::optional<EMaterialAssetParameterType> ParameterType(
    std::string_view Text)
{
    if (Text == "scalar") return EMaterialAssetParameterType::Scalar;
    if (Text == "vector") return EMaterialAssetParameterType::Vector;
    if (Text == "color") return EMaterialAssetParameterType::Color;
    if (Text == "texture")
        return EMaterialAssetParameterType::TextureReference;
    if (Text == "textureBinding")
        return EMaterialAssetParameterType::TextureBinding;
    return std::nullopt;
}

std::optional<EAssetSamplerFilter> SamplerFilter(std::string_view Text)
{
    if (Text == "nearest") return EAssetSamplerFilter::Nearest;
    if (Text == "linear") return EAssetSamplerFilter::Linear;
    if (Text == "automatic") return EAssetSamplerFilter::Automatic;
    return std::nullopt;
}

std::optional<EAssetSamplerMipFilter> SamplerMipFilter(std::string_view Text)
{
    if (Text == "none") return EAssetSamplerMipFilter::None;
    if (Text == "nearest") return EAssetSamplerMipFilter::Nearest;
    if (Text == "linear") return EAssetSamplerMipFilter::Linear;
    if (Text == "automatic") return EAssetSamplerMipFilter::Automatic;
    return std::nullopt;
}

std::optional<EAssetSamplerAddressMode> SamplerAddressMode(
    std::string_view Text)
{
    if (Text == "repeat") return EAssetSamplerAddressMode::Repeat;
    if (Text == "mirroredRepeat") return EAssetSamplerAddressMode::MirroredRepeat;
    if (Text == "clampToEdge") return EAssetSamplerAddressMode::ClampToEdge;
    return std::nullopt;
}

bool ParseTextureBinding(yyjson_val* Value, FMaterialTextureBinding& Out)
{
    if (!ClosedObject(Value, {"texture", "texCoord", "sampler"}))
    {
        return false;
    }
    FAssetId Texture;
    Core::uint32 TexCoord = 0;
    yyjson_val* Sampler = yyjson_obj_get(Value, "sampler");
    Core::FString Min;
    Core::FString Mag;
    Core::FString Mip;
    Core::FString AddressU;
    Core::FString AddressV;
    if (!AssetIdText(yyjson_obj_get(Value, "texture"), "Texture", Texture) ||
        !Unsigned(Value, "texCoord", TexCoord) ||
        !ClosedObject(Sampler, {"min", "mag", "mip", "addressU", "addressV"}) ||
        !String(Sampler, "min", Min) ||
        !String(Sampler, "mag", Mag) ||
        !String(Sampler, "mip", Mip) ||
        !String(Sampler, "addressU", AddressU) ||
        !String(Sampler, "addressV", AddressV))
    {
        return false;
    }
    const auto MinFilter = SamplerFilter(Min.View());
    const auto MagFilter = SamplerFilter(Mag.View());
    const auto MipFilter = SamplerMipFilter(Mip.View());
    const auto U = SamplerAddressMode(AddressU.View());
    const auto V = SamplerAddressMode(AddressV.View());
    return MinFilter && MagFilter && MipFilter && U && V &&
        FMaterialTextureBinding::Create(
            Texture,
            TexCoord,
            {*MinFilter, *MagFilter, *MipFilter, *U, *V},
            Out) == EAssetResult::Success;
}

bool ParseParameter(
    yyjson_val* Object,
    Core::uint32 SchemaVersion,
    FMaterialAssetParameter& Out)
{
    if (!ClosedObject(Object, {"name", "type", "value"}) ||
        !String(Object, "name", Out.Name))
    {
        return false;
    }
    Core::FString TypeText;
    if (!String(Object, "type", TypeText))
    {
        return false;
    }
    const auto Type = ParameterType(TypeText.View());
    yyjson_val* Value = yyjson_obj_get(Object, "value");
    if (!Type)
    {
        return false;
    }
    switch (*Type)
    {
    case EMaterialAssetParameterType::Scalar:
    {
        float Scalar = 0.0f;
        if (!FloatValue(Value, Scalar)) return false;
        Out.Value = FMaterialAssetParameterValue::FromScalar(Scalar);
        return true;
    }
    case EMaterialAssetParameterType::Vector:
    case EMaterialAssetParameterType::Color:
    {
        if (!yyjson_is_arr(Value) || yyjson_arr_size(Value) != 4)
            return false;
        float Components[4]{};
        for (std::size_t Index = 0; Index < 4; ++Index)
        {
            if (!FloatValue(yyjson_arr_get(Value, Index), Components[Index]))
                return false;
        }
        Out.Value = *Type == EMaterialAssetParameterType::Vector
            ? FMaterialAssetParameterValue::FromVector({
                  Components[0], Components[1], Components[2], Components[3]})
            : FMaterialAssetParameterValue::FromColor({
                  Components[0], Components[1], Components[2], Components[3]});
        return true;
    }
    case EMaterialAssetParameterType::TextureReference:
    {
        if (SchemaVersion != 1) return false;
        FAssetId Texture;
        if (!AssetId(Value, "Texture", Texture)) return false;
        FMaterialTextureBinding Binding;
        if (FMaterialTextureBinding::Create(
                Texture, 0, {}, Binding) != EAssetResult::Success)
            return false;
        Out.Value = FMaterialAssetParameterValue::FromTextureBinding(
            std::move(Binding));
        return true;
    }
    case EMaterialAssetParameterType::TextureBinding:
    {
        if (SchemaVersion != 2) return false;
        FMaterialTextureBinding Binding;
        if (!ParseTextureBinding(Value, Binding)) return false;
        Out.Value = FMaterialAssetParameterValue::FromTextureBinding(
            std::move(Binding));
        return true;
    }
    }
    return false;
}

bool ParseParameters(
    yyjson_val* Value,
    Core::uint32 SchemaVersion,
    Core::TArray<FMaterialAssetParameter>& Out)
{
    Out.clear();
    if (!yyjson_is_arr(Value))
    {
        return false;
    }
    std::size_t Index = 0;
    std::size_t Maximum = 0;
    yyjson_val* Element = nullptr;
    yyjson_arr_foreach(Value, Index, Maximum, Element)
    {
        FMaterialAssetParameter Parameter;
        if (!ParseParameter(Element, SchemaVersion, Parameter))
        {
            return false;
        }
        Out.push_back(std::move(Parameter));
    }
    return true;
}

bool ParseShader(yyjson_val* Root, FShaderAssetDesc& Out)
{
    if (!ClosedObject(
            Root,
            {"schema", "version", "id", "requiredExtensions",
             "programKind", "stages", "allowedPermutationFlags",
             "requiredParameters", "interface", "variants", "extensions"}) ||
        !AssetId(yyjson_obj_get(Root, "id"), "ShaderProgram", Out.Id) ||
        !Unsigned(Root, "version", Out.SchemaVersion) ||
        (Out.SchemaVersion != 1 && Out.SchemaVersion != 2))
    {
        return false;
    }
    Core::FString Kind;
    if (!String(Root, "programKind", Kind) ||
        (Kind != "graphics" && Kind != "compute"))
    {
        return false;
    }
    Out.ProgramKind = Kind == "graphics"
        ? EShaderProgramKind::Graphics
        : EShaderProgramKind::Compute;
    if (!StringArray(
            yyjson_obj_get(Root, "allowedPermutationFlags"),
            Out.AllowedPermutationFlags))
    {
        return false;
    }

    yyjson_val* Stages = yyjson_obj_get(Root, "stages");
    if (!yyjson_is_arr(Stages))
    {
        return false;
    }
    std::size_t Index = 0;
    std::size_t Maximum = 0;
    yyjson_val* Element = nullptr;
    yyjson_arr_foreach(Stages, Index, Maximum, Element)
    {
        if (!ClosedObject(
                Element,
                {"stage", "entryPoint", "language", "source"}))
            return false;
        Core::FString StageText;
        Core::FString Language;
        FShaderSourceReference Stage;
        FAssetId SourceId;
        if (!String(Element, "stage", StageText) ||
            !String(Element, "entryPoint", Stage.EntryPoint) ||
            !String(Element, "language", Language) ||
            Language != "glsl" ||
            !ShaderStage(StageText.View()) ||
            !ParseDependency(
                yyjson_obj_get(Element, "source"),
                "ShaderSource",
                SourceId,
                Stage.Locator,
                Stage.ExpectedDigest) ||
            TSoftAssetRef<FShaderSourceAsset>::Create(
                SourceId, Stage.Source) != EAssetResult::Success)
        {
            return false;
        }
        Stage.Stage = *ShaderStage(StageText.View());
        Out.Stages.push_back(std::move(Stage));
    }

    yyjson_val* Required = yyjson_obj_get(Root, "requiredParameters");
    if (!yyjson_is_arr(Required)) return false;
    yyjson_arr_foreach(Required, Index, Maximum, Element)
    {
        if (!ClosedObject(Element, {"name", "type"})) return false;
        FShaderRequiredParameter Parameter;
        Core::FString Type;
        if (!String(Element, "name", Parameter.Name) ||
            !String(Element, "type", Type) ||
            !ParameterType(Type.View()))
            return false;
        Parameter.Type = *ParameterType(Type.View());
        Out.RequiredParameters.push_back(std::move(Parameter));
    }

    yyjson_val* Interface = yyjson_obj_get(Root, "interface");
    if (!ClosedObject(Interface, {"bindings", "constantRanges"}))
        return false;
    yyjson_val* Bindings = yyjson_obj_get(Interface, "bindings");
    yyjson_val* Ranges = yyjson_obj_get(Interface, "constantRanges");
    if (!yyjson_is_arr(Bindings) || !yyjson_is_arr(Ranges)) return false;
    yyjson_arr_foreach(Bindings, Index, Maximum, Element)
    {
        if (!ClosedObject(
                Element,
                {"set", "binding", "kind", "arrayCount", "visibility",
                 "name"}))
        {
            return false;
        }
        FShaderInterfaceBinding Binding;
        Core::FString ResourceKindText;
        if (!Unsigned(Element, "set", Binding.SetIndex) ||
            !Unsigned(Element, "binding", Binding.BindingIndex) ||
            !String(Element, "kind", ResourceKindText) ||
            !ResourceKind(ResourceKindText.View()) ||
            !Unsigned(Element, "arrayCount", Binding.ArrayCount) ||
            !ParseVisibility(
                yyjson_obj_get(Element, "visibility"),
                Binding.Visibility) ||
            !String(Element, "name", Binding.Name))
        {
            return false;
        }
        Binding.Kind = *ResourceKind(ResourceKindText.View());
        Out.InterfaceBindings.push_back(std::move(Binding));
    }
    yyjson_arr_foreach(Ranges, Index, Maximum, Element)
    {
        if (!ClosedObject(
                Element,
                {"offset", "size", "visibility"}))
        {
            return false;
        }
        FShaderConstantRange Range;
        if (!Unsigned(Element, "offset", Range.OffsetBytes) ||
            !Unsigned(Element, "size", Range.SizeBytes) ||
            !ParseVisibility(
                yyjson_obj_get(Element, "visibility"),
                Range.Visibility))
        {
            return false;
        }
        Out.ConstantRanges.push_back(std::move(Range));
    }

    yyjson_val* Variants = yyjson_obj_get(Root, "variants");
    if (!yyjson_is_arr(Variants)) return false;
    yyjson_arr_foreach(Variants, Index, Maximum, Element)
    {
        if (!ClosedObject(Element, {"name", "flags", "payloads"}))
            return false;
        FShaderVariantDefinition Variant;
        if (!String(Element, "name", Variant.VariantName) ||
            !StringArray(
                yyjson_obj_get(Element, "flags"),
                Variant.Permutation.Flags))
            return false;
        yyjson_val* Payloads = yyjson_obj_get(Element, "payloads");
        if (!yyjson_is_arr(Payloads)) return false;
        std::size_t PayloadIndex = 0;
        std::size_t PayloadMaximum = 0;
        yyjson_val* PayloadValue = nullptr;
        yyjson_arr_foreach(
            Payloads,
            PayloadIndex,
            PayloadMaximum,
            PayloadValue)
        {
            if (!ClosedObject(
                    PayloadValue,
                    {"backend", "profile", "format", "stage", "entryPoint",
                     "asset", "locator", "digest", "producer",
                     "producerVersion"}))
                return false;
            FShaderPayloadReference Payload;
            Core::FString BackendText;
            Core::FString FormatText;
            Core::FString StageText;
            FAssetId PayloadId;
            if (!String(PayloadValue, "backend", BackendText) ||
                !String(PayloadValue, "profile", Payload.Profile) ||
                !String(PayloadValue, "format", FormatText) ||
                !String(PayloadValue, "stage", StageText) ||
                !String(PayloadValue, "entryPoint", Payload.EntryPoint) ||
                !String(PayloadValue, "locator", Payload.Locator) ||
                !String(PayloadValue, "producer", Payload.Producer) ||
                !String(
                    PayloadValue,
                    "producerVersion",
                    Payload.ProducerVersion) ||
                !Digest(PayloadValue, Payload.ExpectedDigest) ||
                !AssetId(
                    yyjson_obj_get(PayloadValue, "asset"),
                    "ShaderPayload",
                    PayloadId) ||
                !Backend(BackendText.View()) ||
                !PayloadFormat(FormatText.View()) ||
                !ShaderStage(StageText.View()) ||
                TSoftAssetRef<FShaderPayloadAsset>::Create(
                    PayloadId, Payload.Payload) != EAssetResult::Success)
                return false;
            Payload.Backend = *Backend(BackendText.View());
            Payload.Format = *PayloadFormat(FormatText.View());
            Payload.Stage = *ShaderStage(StageText.View());
            Payload.Permutation = Variant.Permutation;
            Variant.Payloads.push_back(std::move(Payload));
        }
        Out.Variants.push_back(std::move(Variant));
    }
    return true;
}

bool ParseMaterial(yyjson_val* Root, FMaterialAssetDesc& Out)
{
    if (!ClosedObject(
            Root,
            {"schema", "version", "id", "requiredExtensions", "domain",
             "blendMode", "renderState", "shader", "permutationFlags",
             "parameters", "extensions"}) ||
        !AssetId(yyjson_obj_get(Root, "id"), "Material", Out.Id) ||
        !Unsigned(Root, "version", Out.SchemaVersion) ||
        (Out.SchemaVersion != 1 && Out.SchemaVersion != 2))
    {
        return false;
    }
    Core::FString Domain;
    Core::FString Blend;
    if (!String(Root, "domain", Domain) ||
        !String(Root, "blendMode", Blend))
    {
        return false;
    }
    if (Domain == "surface") Out.Domain = EMaterialAssetDomain::Surface;
    else if (Domain == "postProcess") Out.Domain = EMaterialAssetDomain::PostProcess;
    else if (Domain == "ui") Out.Domain = EMaterialAssetDomain::UI;
    else if (Domain == "decal") Out.Domain = EMaterialAssetDomain::Decal;
    else return false;
    if (Blend == "opaque") Out.BlendMode = EMaterialAssetBlendMode::Opaque;
    else if (Blend == "translucent") Out.BlendMode = EMaterialAssetBlendMode::Translucent;
    else if (Blend == "additive") Out.BlendMode = EMaterialAssetBlendMode::Additive;
    else if (Blend == "masked") Out.BlendMode = EMaterialAssetBlendMode::Masked;
    else return false;

    yyjson_val* State = yyjson_obj_get(Root, "renderState");
    FAssetId ShaderId;
    if (!ClosedObject(State, {"depthTest", "depthWrite", "twoSided"}) ||
        !Boolean(State, "depthTest", Out.RenderState.bDepthTest) ||
        !Boolean(State, "depthWrite", Out.RenderState.bDepthWrite) ||
        !Boolean(State, "twoSided", Out.RenderState.bTwoSided) ||
        !AssetId(yyjson_obj_get(Root, "shader"), "ShaderProgram", ShaderId) ||
        TSoftAssetRef<FShaderAsset>::Create(ShaderId, Out.Shader) !=
            EAssetResult::Success ||
        !StringArray(
            yyjson_obj_get(Root, "permutationFlags"),
            Out.PermutationRequest.Flags) ||
        !ParseParameters(
            yyjson_obj_get(Root, "parameters"),
            Out.SchemaVersion,
            Out.Parameters))
        return false;
    return true;
}

bool ParseMaterialInstance(
    yyjson_val* Root,
    FMaterialInstanceAssetDesc& Out)
{
    if (!ClosedObject(
            Root,
            {"schema", "version", "id", "requiredExtensions", "parent",
             "overrides", "extensions"}) ||
        !AssetId(
            yyjson_obj_get(Root, "id"), "MaterialInstance", Out.Id) ||
        !Unsigned(Root, "version", Out.SchemaVersion) ||
        (Out.SchemaVersion != 1 && Out.SchemaVersion != 2) ||
        !ParseParameters(
            yyjson_obj_get(Root, "overrides"),
            Out.SchemaVersion,
            Out.Overrides))
        return false;
    yyjson_val* Parent = yyjson_obj_get(Root, "parent");
    Core::FString Type;
    if (!yyjson_is_obj(Parent) || !String(Parent, "type", Type))
        return false;
    FAssetId ParentId;
    if (Type == "Material" &&
        AssetId(Parent, "Material", ParentId))
    {
        TSoftAssetRef<FMaterialAsset> Reference;
        if (TSoftAssetRef<FMaterialAsset>::Create(
                ParentId, Reference) != EAssetResult::Success)
            return false;
        Out.Parent.Reference = std::move(Reference);
        return true;
    }
    if (Type == "MaterialInstance" &&
        AssetId(Parent, "MaterialInstance", ParentId))
    {
        TSoftAssetRef<FMaterialInstanceAsset> Reference;
        if (TSoftAssetRef<FMaterialInstanceAsset>::Create(
                ParentId, Reference) != EAssetResult::Success)
            return false;
        Out.Parent.Reference = std::move(Reference);
        return true;
    }
    return false;
}

void Indent(std::string& Out, int Depth)
{
    Out.append(static_cast<std::size_t>(Depth) * 2, ' ');
}

void JsonString(std::string& Out, std::string_view Text)
{
    Out.push_back('"');
    for (unsigned char Character : Text)
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
                constexpr char Hex[] = "0123456789abcdef";
                Out += "\\u00";
                Out.push_back(Hex[Character >> 4]);
                Out.push_back(Hex[Character & 0xf]);
            }
            else
            {
                Out.push_back(static_cast<char>(Character));
            }
        }
    }
    Out.push_back('"');
}

void Key(std::string& Out, int Depth, const char* Name)
{
    Indent(Out, Depth);
    JsonString(Out, Name);
    Out += ": ";
}

void AssetIdJson(std::string& Out, const FAssetId& Id, int Depth)
{
    Out += "{\n";
    Key(Out, Depth + 1, "type");
    JsonString(Out, Id.GetAssetType().View());
    Out += ",\n";
    Key(Out, Depth + 1, "path");
    JsonString(Out, Id.GetLogicalPath().View());
    if (Id.GetSubresource())
    {
        Out += ",\n";
        Key(Out, Depth + 1, "subresource");
        JsonString(Out, Id.GetSubresource()->View());
    }
    Out += "\n";
    Indent(Out, Depth);
    Out += "}";
}

const char* StageText(EShaderStage Stage)
{
    switch (Stage)
    {
    case EShaderStage::Vertex: return "vertex";
    case EShaderStage::Fragment: return "fragment";
    case EShaderStage::Compute: return "compute";
    }
    return "unknown";
}

const char* BackendText(EShaderBackendFamily Value)
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

const char* FormatText(EShaderPayloadFormat Value)
{
    switch (Value)
    {
    case EShaderPayloadFormat::SPIRV: return "spirv";
    case EShaderPayloadFormat::MSL: return "msl";
    case EShaderPayloadFormat::DXIL: return "dxil";
    case EShaderPayloadFormat::GLSL: return "glsl";
    case EShaderPayloadFormat::MetalLibrary: return "metal-library";
    }
    return "unknown";
}

const char* ResourceKindText(EShaderResourceKind Value)
{
    switch (Value)
    {
    case EShaderResourceKind::UniformBuffer: return "uniformBuffer";
    case EShaderResourceKind::SampledTexture: return "sampledTexture";
    case EShaderResourceKind::Sampler: return "sampler";
    case EShaderResourceKind::StorageBuffer: return "storageBuffer";
    case EShaderResourceKind::StorageTexture: return "storageTexture";
    case EShaderResourceKind::CombinedTextureSampler:
        return "combinedTextureSampler";
    }
    return "unknown";
}

void VisibilityJson(
    std::string& Out,
    const Core::TArray<EShaderStage>& Visibility)
{
    Out += "[";
    for (std::size_t Index = 0; Index < Visibility.size(); ++Index)
    {
        if (Index) Out += ", ";
        JsonString(Out, StageText(Visibility[Index]));
    }
    Out += "]";
}

void StringArrayJson(
    std::string& Out,
    const Core::TArray<Core::FString>& Values)
{
    Out += "[";
    for (std::size_t Index = 0; Index < Values.size(); ++Index)
    {
        if (Index) Out += ", ";
        JsonString(Out, Values[Index].View());
    }
    Out += "]";
}

void DependencyJson(
    std::string& Out,
    const FAssetId& Id,
    const Core::FString& Locator,
    const FAssetDigest& DigestValue,
    int Depth)
{
    Out += "{\n";
    Key(Out, Depth + 1, "asset");
    AssetIdJson(Out, Id, Depth + 1);
    Out += ",\n";
    Key(Out, Depth + 1, "locator");
    JsonString(Out, Locator.View());
    Out += ",\n";
    Key(Out, Depth + 1, "digest");
    JsonString(
        Out,
        std::string("sha256:") + DigestValue.ToLowerHex().ToStdString());
    Out += "\n";
    Indent(Out, Depth);
    Out += "}";
}

void FloatJson(std::string& Out, float Value)
{
    if (Value == 0.0f)
    {
        Out += "0";
        return;
    }
    char Buffer[64]{};
    yyjson_mut_doc* Document = yyjson_mut_doc_new(nullptr);
    yyjson_mut_val* JsonNumber = yyjson_mut_float(Document, Value);
    const char* End = yyjson_mut_write_number(JsonNumber, Buffer);
    std::size_t Length = static_cast<std::size_t>(End - Buffer);
    if (Length >= 2 && Buffer[Length - 2] == '.' && Buffer[Length - 1] == '0')
    {
        Length -= 2;
    }
    Out.append(Buffer, Length);
    yyjson_mut_doc_free(Document);
}

const char* ParameterTypeText(
    EMaterialAssetParameterType Type,
    Core::uint32 SchemaVersion)
{
    switch (Type)
    {
    case EMaterialAssetParameterType::Scalar: return "scalar";
    case EMaterialAssetParameterType::Vector: return "vector";
    case EMaterialAssetParameterType::Color: return "color";
    case EMaterialAssetParameterType::TextureReference: return "texture";
    case EMaterialAssetParameterType::TextureBinding:
        return SchemaVersion == 1 ? "texture" : "textureBinding";
    }
    return "unknown";
}

const char* SamplerFilterText(EAssetSamplerFilter Value)
{
    switch (Value)
    {
    case EAssetSamplerFilter::Nearest: return "nearest";
    case EAssetSamplerFilter::Linear: return "linear";
    case EAssetSamplerFilter::Automatic: return "automatic";
    }
    return "unknown";
}

const char* SamplerMipFilterText(EAssetSamplerMipFilter Value)
{
    switch (Value)
    {
    case EAssetSamplerMipFilter::None: return "none";
    case EAssetSamplerMipFilter::Nearest: return "nearest";
    case EAssetSamplerMipFilter::Linear: return "linear";
    case EAssetSamplerMipFilter::Automatic: return "automatic";
    }
    return "unknown";
}

const char* SamplerAddressModeText(EAssetSamplerAddressMode Value)
{
    switch (Value)
    {
    case EAssetSamplerAddressMode::Repeat: return "repeat";
    case EAssetSamplerAddressMode::MirroredRepeat: return "mirroredRepeat";
    case EAssetSamplerAddressMode::ClampToEdge: return "clampToEdge";
    }
    return "unknown";
}

bool IsDefaultTextureBinding(const FMaterialTextureBinding& Binding)
{
    return Binding.TexCoordSet == 0 &&
        Binding.Sampler == FMaterialSamplerIntent{};
}

bool CanWriteParameters(
    Core::uint32 SchemaVersion,
    const Core::TArray<FMaterialAssetParameter>& Parameters)
{
    if (SchemaVersion != 1 && SchemaVersion != 2)
    {
        return false;
    }
    for (const FMaterialAssetParameter& Parameter : Parameters)
    {
        if (Parameter.Value.Type == EMaterialAssetParameterType::TextureBinding)
        {
            if (!std::holds_alternative<FMaterialTextureBinding>(
                    Parameter.Value.Value))
            {
                return false;
            }
            if (SchemaVersion == 1 &&
                !IsDefaultTextureBinding(
                    std::get<FMaterialTextureBinding>(Parameter.Value.Value)))
            {
                return false;
            }
        }
        else if (SchemaVersion == 2 &&
                 Parameter.Value.Type ==
                     EMaterialAssetParameterType::TextureReference)
        {
            return false;
        }
    }
    return true;
}

void ParameterJson(
    std::string& Out,
    const FMaterialAssetParameter& Parameter,
    Core::uint32 SchemaVersion,
    int Depth)
{
    Out += "{\n";
    Key(Out, Depth + 1, "name");
    JsonString(Out, Parameter.Name.View());
    Out += ",\n";
    Key(Out, Depth + 1, "type");
    JsonString(Out, ParameterTypeText(Parameter.Value.Type, SchemaVersion));
    Out += ",\n";
    Key(Out, Depth + 1, "value");
    if (Parameter.Value.Type == EMaterialAssetParameterType::Scalar)
    {
        FloatJson(Out, std::get<float>(Parameter.Value.Value));
    }
    else if (Parameter.Value.Type == EMaterialAssetParameterType::Vector)
    {
        const auto V = std::get<Core::FVector4>(Parameter.Value.Value);
        Out += "[";
        FloatJson(Out, V.X); Out += ", ";
        FloatJson(Out, V.Y); Out += ", ";
        FloatJson(Out, V.Z); Out += ", ";
        FloatJson(Out, V.W); Out += "]";
    }
    else if (Parameter.Value.Type == EMaterialAssetParameterType::Color)
    {
        const auto C = std::get<Core::FColor>(Parameter.Value.Value);
        Out += "[";
        FloatJson(Out, C.R); Out += ", ";
        FloatJson(Out, C.G); Out += ", ";
        FloatJson(Out, C.B); Out += ", ";
        FloatJson(Out, C.A); Out += "]";
    }
    else if (Parameter.Value.Type ==
             EMaterialAssetParameterType::TextureReference)
    {
        AssetIdJson(
            Out,
            std::get<FAssetId>(Parameter.Value.Value),
            Depth + 1);
    }
    else
    {
        const auto& Binding = std::get<FMaterialTextureBinding>(
            Parameter.Value.Value);
        if (SchemaVersion == 1)
        {
            AssetIdJson(Out, *Binding.Texture.GetId(), Depth + 1);
        }
        else
        {
            Out += "{\n";
            Key(Out, Depth + 2, "texture");
            JsonString(Out, Binding.Texture.GetId()->ToString().View());
            Out += ",\n";
            Key(Out, Depth + 2, "texCoord");
            Out += std::to_string(Binding.TexCoordSet);
            Out += ",\n";
            Key(Out, Depth + 2, "sampler");
            Out += "{\n";
            Key(Out, Depth + 3, "min");
            JsonString(Out, SamplerFilterText(Binding.Sampler.MinFilter));
            Out += ",\n";
            Key(Out, Depth + 3, "mag");
            JsonString(Out, SamplerFilterText(Binding.Sampler.MagFilter));
            Out += ",\n";
            Key(Out, Depth + 3, "mip");
            JsonString(Out, SamplerMipFilterText(Binding.Sampler.MipFilter));
            Out += ",\n";
            Key(Out, Depth + 3, "addressU");
            JsonString(
                Out,
                SamplerAddressModeText(Binding.Sampler.AddressU));
            Out += ",\n";
            Key(Out, Depth + 3, "addressV");
            JsonString(
                Out,
                SamplerAddressModeText(Binding.Sampler.AddressV));
            Out += "\n";
            Indent(Out, Depth + 2);
            Out += "}\n";
            Indent(Out, Depth + 1);
            Out += "}";
        }
    }
    Out += "\n";
    Indent(Out, Depth);
    Out += "}";
}

void ParametersJson(
    std::string& Out,
    const Core::TArray<FMaterialAssetParameter>& Parameters,
    Core::uint32 SchemaVersion,
    int Depth)
{
    Out += "[";
    if (!Parameters.empty()) Out += "\n";
    for (std::size_t Index = 0; Index < Parameters.size(); ++Index)
    {
        Indent(Out, Depth + 1);
        ParameterJson(Out, Parameters[Index], SchemaVersion, Depth + 1);
        Out += Index + 1 == Parameters.size() ? "\n" : ",\n";
    }
    if (!Parameters.empty()) Indent(Out, Depth);
    Out += "]";
}

void WriteCommonStart(
    std::string& Out,
    const char* Schema,
    const FAssetId& Id,
    Core::uint32 SchemaVersion)
{
    Out += "{\n";
    Key(Out, 1, "schema"); JsonString(Out, Schema); Out += ",\n";
    Key(Out, 1, "version"); Out += std::to_string(SchemaVersion); Out += ",\n";
    Key(Out, 1, "id"); AssetIdJson(Out, Id, 1); Out += ",\n";
    Key(Out, 1, "requiredExtensions"); Out += "[],\n";
}

std::string WriteMaterial(const FMaterialAssetDesc& Desc)
{
    std::string Out;
    WriteCommonStart(Out, "stoner.material", Desc.Id, Desc.SchemaVersion);
    Key(Out, 1, "domain");
    const char* Domain = Desc.Domain == EMaterialAssetDomain::Surface
        ? "surface"
        : Desc.Domain == EMaterialAssetDomain::PostProcess
            ? "postProcess"
            : Desc.Domain == EMaterialAssetDomain::UI ? "ui" : "decal";
    JsonString(Out, Domain); Out += ",\n";
    Key(Out, 1, "blendMode");
    const char* Blend = Desc.BlendMode == EMaterialAssetBlendMode::Opaque
        ? "opaque"
        : Desc.BlendMode == EMaterialAssetBlendMode::Translucent
            ? "translucent"
            : Desc.BlendMode == EMaterialAssetBlendMode::Additive
                ? "additive" : "masked";
    JsonString(Out, Blend); Out += ",\n";
    Key(Out, 1, "renderState"); Out += "{\n";
    Key(Out, 2, "depthTest"); Out += Desc.RenderState.bDepthTest ? "true,\n" : "false,\n";
    Key(Out, 2, "depthWrite"); Out += Desc.RenderState.bDepthWrite ? "true,\n" : "false,\n";
    Key(Out, 2, "twoSided"); Out += Desc.RenderState.bTwoSided ? "true\n" : "false\n";
    Indent(Out, 1); Out += "},\n";
    Key(Out, 1, "shader");
    AssetIdJson(Out, *Desc.Shader.GetId(), 1); Out += ",\n";
    Key(Out, 1, "permutationFlags");
    StringArrayJson(Out, Desc.PermutationRequest.Flags); Out += ",\n";
    Key(Out, 1, "parameters");
    ParametersJson(Out, Desc.Parameters, Desc.SchemaVersion, 1);
    Out += ",\n";
    Key(Out, 1, "extensions"); Out += "{}\n}\n";
    return Out;
}

std::string WriteInstance(const FMaterialInstanceAssetDesc& Desc)
{
    std::string Out;
    WriteCommonStart(
        Out,
        "stoner.material-instance",
        Desc.Id,
        Desc.SchemaVersion);
    Key(Out, 1, "parent");
    std::visit(
        [&Out](const auto& Parent)
        {
            AssetIdJson(Out, *Parent.GetId(), 1);
        },
        Desc.Parent.Reference);
    Out += ",\n";
    Key(Out, 1, "overrides");
    ParametersJson(Out, Desc.Overrides, Desc.SchemaVersion, 1);
    Out += ",\n";
    Key(Out, 1, "extensions"); Out += "{}\n}\n";
    return Out;
}

std::string WriteShader(const FShaderAssetDesc& Desc)
{
    std::string Out;
    WriteCommonStart(
        Out,
        "stoner.shader-program",
        Desc.Id,
        Desc.SchemaVersion);
    Key(Out, 1, "programKind");
    JsonString(
        Out,
        Desc.ProgramKind == EShaderProgramKind::Graphics
            ? "graphics" : "compute");
    Out += ",\n";
    Key(Out, 1, "stages"); Out += "[\n";
    for (std::size_t Index = 0; Index < Desc.Stages.size(); ++Index)
    {
        const auto& Stage = Desc.Stages[Index];
        Indent(Out, 2); Out += "{\n";
        Key(Out, 3, "stage"); JsonString(Out, StageText(Stage.Stage)); Out += ",\n";
        Key(Out, 3, "entryPoint"); JsonString(Out, Stage.EntryPoint.View()); Out += ",\n";
        Key(Out, 3, "language"); JsonString(Out, "glsl"); Out += ",\n";
        Key(Out, 3, "source");
        DependencyJson(
            Out, *Stage.Source.GetId(), Stage.Locator,
            Stage.ExpectedDigest, 3);
        Out += "\n";
        Indent(Out, 2); Out += Index + 1 == Desc.Stages.size() ? "}\n" : "},\n";
    }
    Indent(Out, 1); Out += "],\n";
    Key(Out, 1, "allowedPermutationFlags");
    StringArrayJson(Out, Desc.AllowedPermutationFlags); Out += ",\n";
    Key(Out, 1, "requiredParameters"); Out += "[";
    if (!Desc.RequiredParameters.empty()) Out += "\n";
    for (std::size_t Index = 0;
         Index < Desc.RequiredParameters.size();
         ++Index)
    {
        const auto& Parameter = Desc.RequiredParameters[Index];
        Indent(Out, 2); Out += "{\n";
        Key(Out, 3, "name"); JsonString(Out, Parameter.Name.View()); Out += ",\n";
        Key(Out, 3, "type");
        JsonString(Out, ParameterTypeText(Parameter.Type, 2));
        Out += "\n";
        Indent(Out, 2);
        Out += Index + 1 == Desc.RequiredParameters.size() ? "}\n" : "},\n";
    }
    if (!Desc.RequiredParameters.empty()) Indent(Out, 1);
    Out += "],\n";
    Key(Out, 1, "interface"); Out += "{\n";
    Key(Out, 2, "bindings"); Out += "[";
    if (!Desc.InterfaceBindings.empty()) Out += "\n";
    for (std::size_t Index = 0;
         Index < Desc.InterfaceBindings.size();
         ++Index)
    {
        const auto& Binding = Desc.InterfaceBindings[Index];
        Indent(Out, 3); Out += "{\n";
        Key(Out, 4, "set"); Out += std::to_string(Binding.SetIndex); Out += ",\n";
        Key(Out, 4, "binding"); Out += std::to_string(Binding.BindingIndex); Out += ",\n";
        Key(Out, 4, "kind"); JsonString(Out, ResourceKindText(Binding.Kind)); Out += ",\n";
        Key(Out, 4, "arrayCount"); Out += std::to_string(Binding.ArrayCount); Out += ",\n";
        Key(Out, 4, "visibility"); VisibilityJson(Out, Binding.Visibility); Out += ",\n";
        Key(Out, 4, "name"); JsonString(Out, Binding.Name.View()); Out += "\n";
        Indent(Out, 3);
        Out += Index + 1 == Desc.InterfaceBindings.size() ? "}\n" : "},\n";
    }
    if (!Desc.InterfaceBindings.empty()) Indent(Out, 2);
    Out += "],\n";
    Key(Out, 2, "constantRanges"); Out += "[";
    if (!Desc.ConstantRanges.empty()) Out += "\n";
    for (std::size_t Index = 0;
         Index < Desc.ConstantRanges.size();
         ++Index)
    {
        const auto& Range = Desc.ConstantRanges[Index];
        Indent(Out, 3); Out += "{\n";
        Key(Out, 4, "offset"); Out += std::to_string(Range.OffsetBytes); Out += ",\n";
        Key(Out, 4, "size"); Out += std::to_string(Range.SizeBytes); Out += ",\n";
        Key(Out, 4, "visibility"); VisibilityJson(Out, Range.Visibility); Out += "\n";
        Indent(Out, 3);
        Out += Index + 1 == Desc.ConstantRanges.size() ? "}\n" : "},\n";
    }
    if (!Desc.ConstantRanges.empty()) Indent(Out, 2);
    Out += "]\n";
    Indent(Out, 1); Out += "},\n";
    Key(Out, 1, "variants"); Out += "[\n";
    for (std::size_t V = 0; V < Desc.Variants.size(); ++V)
    {
        const auto& Variant = Desc.Variants[V];
        Indent(Out, 2); Out += "{\n";
        Key(Out, 3, "name"); JsonString(Out, Variant.VariantName.View()); Out += ",\n";
        Key(Out, 3, "flags"); StringArrayJson(Out, Variant.Permutation.Flags); Out += ",\n";
        Key(Out, 3, "payloads"); Out += "[\n";
        for (std::size_t P = 0; P < Variant.Payloads.size(); ++P)
        {
            const auto& Payload = Variant.Payloads[P];
            Indent(Out, 4); Out += "{\n";
            Key(Out, 5, "backend"); JsonString(Out, BackendText(Payload.Backend)); Out += ",\n";
            Key(Out, 5, "profile"); JsonString(Out, Payload.Profile.View()); Out += ",\n";
            Key(Out, 5, "format"); JsonString(Out, FormatText(Payload.Format)); Out += ",\n";
            Key(Out, 5, "stage"); JsonString(Out, StageText(Payload.Stage)); Out += ",\n";
            Key(Out, 5, "entryPoint"); JsonString(Out, Payload.EntryPoint.View()); Out += ",\n";
            Key(Out, 5, "asset"); AssetIdJson(Out, *Payload.Payload.GetId(), 5); Out += ",\n";
            Key(Out, 5, "locator"); JsonString(Out, Payload.Locator.View()); Out += ",\n";
            Key(Out, 5, "digest");
            JsonString(Out, std::string("sha256:") + Payload.ExpectedDigest.ToLowerHex().ToStdString());
            Out += ",\n";
            Key(Out, 5, "producer"); JsonString(Out, Payload.Producer.View()); Out += ",\n";
            Key(Out, 5, "producerVersion"); JsonString(Out, Payload.ProducerVersion.View()); Out += "\n";
            Indent(Out, 4); Out += P + 1 == Variant.Payloads.size() ? "}\n" : "},\n";
        }
        Indent(Out, 3); Out += "]\n";
        Indent(Out, 2); Out += V + 1 == Desc.Variants.size() ? "}\n" : "},\n";
    }
    Indent(Out, 1); Out += "],\n";
    Key(Out, 1, "extensions"); Out += "{}\n}\n";
    return Out;
}

FAssetVersion VersionFromCanonical(const Core::FString& Canonical)
{
    const auto* Begin = reinterpret_cast<const Core::uint8*>(
        Canonical.View().data());
    FAssetVersion Version;
    Version.SourceDigest = FAssetDigest::FromBytes(
        std::span<const Core::uint8>(Begin, Canonical.Len()));
    return Version;
}

} // namespace

EAssetResult ParseMaterialShaderDefinition(
    std::span<const Core::uint8> Bytes,
    const FMaterialShaderAssetLimits& Limits,
    FMaterialShaderDefinition& OutDefinition,
    FAssetDiagnosticList* Diagnostics)
{
    OutDefinition = {};
    if (Limits.Validate() != EAssetResult::Success ||
        Bytes.empty() ||
        Bytes.size() > Limits.MaxDefinitionBytes)
    {
        return EAssetResult::DefinitionLimitExceeded;
    }
    if (HasBomOrNul(Bytes))
    {
        return EAssetResult::InvalidDefinition;
    }

    constexpr yyjson_read_flag Flags = YYJSON_READ_NUMBER_AS_RAW;
    const std::size_t Required =
        yyjson_read_max_memory_usage(Bytes.size(), Flags);
    if (Required == 0)
    {
        return EAssetResult::DefinitionLimitExceeded;
    }
    std::vector<Core::uint8> Pool(Required);
    yyjson_alc Allocator{};
    if (!yyjson_alc_pool_init(&Allocator, Pool.data(), Pool.size()))
    {
        return EAssetResult::DefinitionLimitExceeded;
    }
    yyjson_read_err Error{};
    yyjson_doc* Document = yyjson_read_opts(
        const_cast<char*>(
            reinterpret_cast<const char*>(Bytes.data())),
        Bytes.size(),
        Flags,
        &Allocator,
        &Error);
    if (!Document)
    {
        AddDiagnostic(
            Diagnostics,
            EAssetStage::Parse,
            EAssetResult::InvalidDefinition,
            "/",
            "invalid-json");
        return EAssetResult::InvalidDefinition;
    }
    yyjson_val* Root = yyjson_doc_get_root(Document);
    const EAssetResult PreflightResult = Preflight(Root, Limits);
    if (PreflightResult != EAssetResult::Success)
    {
        return PreflightResult;
    }
    Core::FString Schema;
    if (!String(Root, "schema", Schema))
    {
        return EAssetResult::InvalidDefinition;
    }
    const EAssetResult SemanticResult =
        SemanticPreflight(Root, Schema.View(), Limits);
    if (SemanticResult != EAssetResult::Success)
    {
        return SemanticResult;
    }
    Core::uint32 SchemaVersion = 0;
    if (!Unsigned(Root, "version", SchemaVersion))
    {
        return EAssetResult::InvalidDefinition;
    }
    const bool bShaderSchema = Schema == "stoner.shader-program";
    const bool bMaterialSchema =
        Schema == "stoner.material" ||
        Schema == "stoner.material-instance";
    if ((bShaderSchema && SchemaVersion != 1) ||
        (bMaterialSchema && SchemaVersion != 1 && SchemaVersion != 2) ||
        (!bShaderSchema && !bMaterialSchema))
    {
        return EAssetResult::UnsupportedSchema;
    }
    const EAssetResult ExtensionResult =
        ValidateExtensions(Root, Limits);
    if (ExtensionResult != EAssetResult::Success)
    {
        return ExtensionResult;
    }
    bool bParsed = false;
    if (Schema == "stoner.shader-program")
    {
        OutDefinition.Kind = EMaterialShaderDefinitionKind::Shader;
        FShaderAssetDesc Desc;
        bParsed = ParseShader(Root, Desc);
        OutDefinition.Value = std::move(Desc);
    }
    else if (Schema == "stoner.material")
    {
        OutDefinition.Kind = EMaterialShaderDefinitionKind::Material;
        FMaterialAssetDesc Desc;
        bParsed = ParseMaterial(Root, Desc);
        OutDefinition.Value = std::move(Desc);
    }
    else if (Schema == "stoner.material-instance")
    {
        OutDefinition.Kind = EMaterialShaderDefinitionKind::MaterialInstance;
        FMaterialInstanceAssetDesc Desc;
        bParsed = ParseMaterialInstance(Root, Desc);
        OutDefinition.Value = std::move(Desc);
    }
    else
    {
        return EAssetResult::UnsupportedSchema;
    }
    return bParsed
        ? EAssetResult::Success
        : EAssetResult::InvalidDefinition;
}

EAssetResult WriteMaterialShaderDefinition(
    FMaterialShaderDefinition& Definition,
    Core::FString& OutCanonical,
    FAssetDiagnosticList*)
{
    std::string Text;
    switch (Definition.Kind)
    {
    case EMaterialShaderDefinitionKind::Shader:
        Text = WriteShader(std::get<FShaderAssetDesc>(Definition.Value));
        break;
    case EMaterialShaderDefinitionKind::Material:
        if (!CanWriteParameters(
                std::get<FMaterialAssetDesc>(Definition.Value).SchemaVersion,
                std::get<FMaterialAssetDesc>(Definition.Value).Parameters))
        {
            return EAssetResult::InvalidDefinition;
        }
        Text = WriteMaterial(std::get<FMaterialAssetDesc>(Definition.Value));
        break;
    case EMaterialShaderDefinitionKind::MaterialInstance:
        if (!CanWriteParameters(
                std::get<FMaterialInstanceAssetDesc>(Definition.Value)
                    .SchemaVersion,
                std::get<FMaterialInstanceAssetDesc>(Definition.Value)
                    .Overrides))
        {
            return EAssetResult::InvalidDefinition;
        }
        Text = WriteInstance(
            std::get<FMaterialInstanceAssetDesc>(Definition.Value));
        break;
    }
    OutCanonical = Core::FString(std::move(Text));
    const FAssetVersion Version = VersionFromCanonical(OutCanonical);
    std::visit(
        [&Version, &OutCanonical](auto& Desc)
        {
            Desc.Version = Version;
            Desc.CanonicalDefinition = OutCanonical;
        },
        Definition.Value);
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
