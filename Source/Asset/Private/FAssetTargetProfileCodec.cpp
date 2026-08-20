#include "FAssetTargetProfileCodec.h"

#include "Asset/FAssetDigest.h"

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

constexpr Core::usize MaxProfileBytes = 1024U * 1024U;
constexpr Core::usize MaxJsonDepth = 16;
constexpr Core::usize MaxJsonValues = 8192;

using FAllowed = std::initializer_list<std::string_view>;

bool ParseRawNumber(const char* Begin, const char* End, double& Out)
{
    const std::string Text(Begin, End);
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
        const std::string_view Name(yyjson_get_str(Key), yyjson_get_len(Key));
        if (std::find(Allowed.begin(), Allowed.end(), Name) == Allowed.end())
        {
            return false;
        }
    }
    return true;
}

EAssetResult Preflight(yyjson_val* Root)
{
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
        if (++Values > MaxJsonValues || Node.Depth > MaxJsonDepth)
        {
            return EAssetResult::DefinitionLimitExceeded;
        }
        if (yyjson_is_obj(Node.Value))
        {
            std::set<std::string> Keys;
            yyjson_obj_iter Iterator = yyjson_obj_iter_with(Node.Value);
            yyjson_val* Key = nullptr;
            while ((Key = yyjson_obj_iter_next(&Iterator)) != nullptr)
            {
                if (!Keys.emplace(yyjson_get_str(Key), yyjson_get_len(Key)).second)
                {
                    return EAssetResult::InvalidDefinition;
                }
                Stack.push_back({yyjson_obj_iter_get_val(Key), Node.Depth + 1});
            }
        }
        else if (yyjson_is_arr(Node.Value))
        {
            std::size_t Index = 0;
            std::size_t Maximum = 0;
            yyjson_val* Value = nullptr;
            yyjson_arr_foreach(Node.Value, Index, Maximum, Value)
            {
                Stack.push_back({Value, Node.Depth + 1});
            }
        }
        else if (yyjson_is_str(Node.Value) && yyjson_get_len(Node.Value) > 1024)
        {
            return EAssetResult::DefinitionLimitExceeded;
        }
    }
    return EAssetResult::Success;
}

bool String(yyjson_val* Object, const char* Name, Core::FString& Out)
{
    yyjson_val* Value = yyjson_obj_get(Object, Name);
    if (!yyjson_is_str(Value))
    {
        return false;
    }
    Out = Core::FString(std::string(yyjson_get_str(Value), yyjson_get_len(Value)));
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

template <typename T>
bool Unsigned(yyjson_val* Object, const char* Name, T& Out)
{
    yyjson_val* Value = yyjson_obj_get(Object, Name);
    if (!yyjson_is_raw(Value))
    {
        return false;
    }
    const char* Begin = yyjson_get_raw(Value);
    const char* End = Begin + yyjson_get_len(Value);
    T Parsed = 0;
    const auto Result = std::from_chars(Begin, End, Parsed);
    if (Result.ec != std::errc{} || Result.ptr != End)
    {
        return false;
    }
    Out = Parsed;
    return true;
}

template <typename T>
bool ParseEnumToken(
    yyjson_val* Object,
    const char* Name,
    std::initializer_list<std::pair<std::string_view, T>> Choices,
    T& Out)
{
    Core::FString Text;
    if (!String(Object, Name, Text))
    {
        return false;
    }
    for (const auto& [Token, Value] : Choices)
    {
        if (Text.View() == Token)
        {
            Out = Value;
            return true;
        }
    }
    return false;
}

bool ParseStringArray(
    yyjson_val* Object,
    const char* Name,
    Core::TArray<Core::FString>& Out,
    Core::usize Maximum,
    bool Required)
{
    Out.clear();
    yyjson_val* Array = yyjson_obj_get(Object, Name);
    if (Array == nullptr)
    {
        return !Required;
    }
    if (!yyjson_is_arr(Array) || yyjson_arr_size(Array) > Maximum)
    {
        return false;
    }
    std::size_t Index = 0;
    std::size_t Count = 0;
    yyjson_val* Value = nullptr;
    yyjson_arr_foreach(Array, Index, Count, Value)
    {
        if (!yyjson_is_str(Value))
        {
            return false;
        }
        Out.emplace_back(std::string(yyjson_get_str(Value), yyjson_get_len(Value)));
    }
    return !Required || !Out.empty();
}

bool ParseShaderChoices(
    yyjson_val* Root,
    Core::TArray<FAssetShaderPayloadChoice>& Out)
{
    yyjson_val* Array = yyjson_obj_get(Root, "shaderPayloadChoices");
    if (!yyjson_is_arr(Array) || yyjson_arr_size(Array) == 0 ||
        yyjson_arr_size(Array) > 32)
    {
        return false;
    }
    std::size_t Index = 0;
    std::size_t Count = 0;
    yyjson_val* Value = nullptr;
    yyjson_arr_foreach(Array, Index, Count, Value)
    {
        FAssetShaderPayloadChoice Choice;
        if (!ClosedObject(Value, {"backend", "profile", "format"}) ||
            !ParseEnumToken(
                Value, "backend",
                {{"vulkan", EAssetGraphicsBackend::Vulkan},
                 {"metal", EAssetGraphicsBackend::Metal},
                 {"dx12", EAssetGraphicsBackend::DirectX12},
                 {"opengl", EAssetGraphicsBackend::OpenGL},
                 {"gles", EAssetGraphicsBackend::GLES}},
                Choice.Backend) ||
            !String(Value, "profile", Choice.Profile) ||
            !ParseEnumToken(
                Value, "format",
                {{"spirv", EAssetShaderPayloadFormat::SpirV},
                 {"msl", EAssetShaderPayloadFormat::MSL},
                 {"dxil", EAssetShaderPayloadFormat::DXIL},
                 {"glsl", EAssetShaderPayloadFormat::GLSL},
                 {"essl", EAssetShaderPayloadFormat::ESSL},
                 {"metal-library", EAssetShaderPayloadFormat::MetalLibrary}},
                Choice.Format))
        {
            return false;
        }
        Out.push_back(std::move(Choice));
    }
    return true;
}

bool ParseSettingValue(yyjson_val* Value, FAssetProducerSettingValue& Out)
{
    if (yyjson_is_bool(Value))
    {
        Out = yyjson_get_bool(Value);
        return true;
    }
    if (yyjson_is_str(Value))
    {
        Out = Core::FString(std::string(yyjson_get_str(Value), yyjson_get_len(Value)));
        return true;
    }
    if (!yyjson_is_raw(Value))
    {
        return false;
    }
    const char* Begin = yyjson_get_raw(Value);
    const char* End = Begin + yyjson_get_len(Value);
    Core::int64 Integer = 0;
    const auto IntegerResult = std::from_chars(Begin, End, Integer);
    if (IntegerResult.ec == std::errc{} && IntegerResult.ptr == End)
    {
        Out = Integer;
        return true;
    }
    double Number = 0.0;
    if (!ParseRawNumber(Begin, End, Number))
    {
        return false;
    }
    Out = Number;
    return true;
}

bool ParseProducerSettings(
    yyjson_val* BuildPolicy,
    Core::TArray<FAssetProducerSettingsRecord>& Out)
{
    yyjson_val* Array = yyjson_obj_get(BuildPolicy, "producerSettings");
    if (!yyjson_is_arr(Array) || yyjson_arr_size(Array) == 0 ||
        yyjson_arr_size(Array) > 256)
    {
        return false;
    }
    std::size_t Index = 0;
    std::size_t Count = 0;
    yyjson_val* Value = nullptr;
    yyjson_arr_foreach(Array, Index, Count, Value)
    {
        Core::FString ProducerText;
        FAssetProducerSettingsRecord Record;
        yyjson_val* Settings = yyjson_obj_get(Value, "settings");
        if (!ClosedObject(Value, {"producer", "schemaVersion", "settings"}) ||
            !String(Value, "producer", ProducerText) ||
            FAssetParticipantId::Create(ProducerText, Record.Producer) !=
                EAssetResult::Success ||
            !Unsigned(Value, "schemaVersion", Record.SchemaVersion) ||
            !yyjson_is_obj(Settings) || yyjson_obj_size(Settings) > 64)
        {
            return false;
        }
        yyjson_obj_iter Iterator = yyjson_obj_iter_with(Settings);
        yyjson_val* Key = nullptr;
        while ((Key = yyjson_obj_iter_next(&Iterator)) != nullptr)
        {
            FAssetProducerSetting Setting;
            Setting.Name = Core::FString(std::string(
                yyjson_get_str(Key), yyjson_get_len(Key)));
            if (!ParseSettingValue(yyjson_obj_iter_get_val(Key), Setting.Value))
            {
                return false;
            }
            Record.Settings.push_back(std::move(Setting));
        }
        Out.push_back(std::move(Record));
    }
    return true;
}

bool ParseBuildPolicy(yyjson_val* Root, FAssetTargetBuildPolicy& Out)
{
    yyjson_val* Object = yyjson_obj_get(Root, "buildPolicy");
    return ClosedObject(
               Object,
               {"optimization", "includeDebugSymbols", "validation",
                "producerSettings"}) &&
        ParseEnumToken(
            Object, "optimization",
            {{"development", EAssetBuildOptimization::Development},
             {"shipping", EAssetBuildOptimization::Shipping}},
            Out.Optimization) &&
        Boolean(Object, "includeDebugSymbols", Out.bIncludeDebugSymbols) &&
        ParseEnumToken(
            Object, "validation",
            {{"standard", EAssetBuildValidation::Standard},
             {"strict", EAssetBuildValidation::Strict}},
            Out.Validation) &&
        ParseProducerSettings(Object, Out.ProducerSettings);
}

bool ParseLimits(yyjson_val* Root, FAssetTargetLimits& Out)
{
    yyjson_val* Object = yyjson_obj_get(Root, "limits");
    return ClosedObject(
               Object,
               {"maxDiscoveredSources", "maxAssets", "maxDependencyEdges",
                "maxDependencyDepth", "maxSourceBytes", "maxPayloadBytes",
                "maxAggregateBytes", "maxManifestBytes", "maxDiagnostics"}) &&
        Unsigned(Object, "maxDiscoveredSources", Out.MaxDiscoveredSources) &&
        Unsigned(Object, "maxAssets", Out.MaxAssets) &&
        Unsigned(Object, "maxDependencyEdges", Out.MaxDependencyEdges) &&
        Unsigned(Object, "maxDependencyDepth", Out.MaxDependencyDepth) &&
        Unsigned(Object, "maxSourceBytes", Out.MaxSourceBytes) &&
        Unsigned(Object, "maxPayloadBytes", Out.MaxPayloadBytes) &&
        Unsigned(Object, "maxAggregateBytes", Out.MaxAggregateBytes) &&
        Unsigned(Object, "maxManifestBytes", Out.MaxManifestBytes) &&
        Unsigned(Object, "maxDiagnostics", Out.MaxDiagnostics);
}

void EscapeJson(std::string_view Text, std::string& Out)
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
            else
            {
                Out.push_back(static_cast<char>(Character));
            }
        }
    }
    Out.push_back('"');
}

void Indent(std::string& Out, int Depth)
{
    Out.append(static_cast<std::size_t>(Depth) * 2U, ' ');
}

void Key(std::string& Out, int Depth, const char* Name)
{
    Indent(Out, Depth);
    EscapeJson(Name, Out);
    Out += ": ";
}

const char* PlatformToken(EAssetTargetPlatform Value)
{
    switch (Value)
    {
    case EAssetTargetPlatform::Windows: return "windows";
    case EAssetTargetPlatform::MacOS: return "macos";
    case EAssetTargetPlatform::Linux: return "linux";
    case EAssetTargetPlatform::Android: return "android";
    case EAssetTargetPlatform::IOS: return "ios";
    }
    return "linux";
}

const char* ArchitectureToken(EAssetTargetCpuArchitecture Value)
{
    return Value == EAssetTargetCpuArchitecture::X86_64 ? "x86_64" : "arm64";
}

const char* BackendToken(EAssetGraphicsBackend Value)
{
    switch (Value)
    {
    case EAssetGraphicsBackend::Vulkan: return "vulkan";
    case EAssetGraphicsBackend::Metal: return "metal";
    case EAssetGraphicsBackend::DirectX12: return "dx12";
    case EAssetGraphicsBackend::OpenGL: return "opengl";
    case EAssetGraphicsBackend::GLES: return "gles";
    }
    return "vulkan";
}

const char* ShaderFormatToken(EAssetShaderPayloadFormat Value)
{
    switch (Value)
    {
    case EAssetShaderPayloadFormat::SpirV: return "spirv";
    case EAssetShaderPayloadFormat::MSL: return "msl";
    case EAssetShaderPayloadFormat::DXIL: return "dxil";
    case EAssetShaderPayloadFormat::GLSL: return "glsl";
    case EAssetShaderPayloadFormat::ESSL: return "essl";
    case EAssetShaderPayloadFormat::MetalLibrary: return "metal-library";
    }
    return "spirv";
}

void WriteStringArray(
    std::string& Out,
    const Core::TArray<Core::FString>& Values)
{
    Out.push_back('[');
    for (Core::usize Index = 0; Index < Values.size(); ++Index)
    {
        if (Index != 0) Out += ", ";
        EscapeJson(Values[Index].View(), Out);
    }
    Out.push_back(']');
}

void WriteSettingValue(std::string& Out, const FAssetProducerSettingValue& Value)
{
    if (const auto* BooleanValue = std::get_if<bool>(&Value))
    {
        Out += *BooleanValue ? "true" : "false";
    }
    else if (const auto* Integer = std::get_if<Core::int64>(&Value))
    {
        Out += std::to_string(*Integer);
    }
    else if (const auto* Number = std::get_if<double>(&Value))
    {
        char Buffer[64]{};
        yyjson_mut_doc* Document = yyjson_mut_doc_new(nullptr);
        yyjson_mut_val* JsonNumber = yyjson_mut_double(Document, *Number);
        const char* End = yyjson_mut_write_number(JsonNumber, Buffer);
        std::size_t Length = static_cast<std::size_t>(End - Buffer);
        if (Length >= 2 && Buffer[Length - 2] == '.' &&
            Buffer[Length - 1] == '0')
        {
            Length -= 2;
        }
        Out.append(Buffer, Length);
        yyjson_mut_doc_free(Document);
    }
    else
    {
        EscapeJson(std::get<Core::FString>(Value).View(), Out);
    }
}

std::string WriteProfile(const FAssetTargetProfile& Profile, bool IncludeDisplay)
{
    std::string Out = "{\n";
    Key(Out, 1, "schema"); EscapeJson(Profile.Schema.View(), Out); Out += ",\n";
    Key(Out, 1, "schemaVersion"); Out += std::to_string(Profile.SchemaVersion); Out += ",\n";
    if (IncludeDisplay)
    {
        Key(Out, 1, "displayName"); EscapeJson(Profile.DisplayName.View(), Out); Out += ",\n";
    }
    Key(Out, 1, "platform"); EscapeJson(PlatformToken(Profile.Platform), Out); Out += ",\n";
    Key(Out, 1, "cpuArchitecture"); EscapeJson(ArchitectureToken(Profile.CpuArchitecture), Out); Out += ",\n";
    Key(Out, 1, "graphicsBackend"); EscapeJson(BackendToken(Profile.GraphicsBackend), Out); Out += ",\n";
    Key(Out, 1, "shaderPayloadChoices"); Out += "[\n";
    for (Core::usize Index = 0; Index < Profile.ShaderPayloadChoices.size(); ++Index)
    {
        const auto& Choice = Profile.ShaderPayloadChoices[Index];
        Indent(Out, 2); Out += "{\n";
        Key(Out, 3, "backend"); EscapeJson(BackendToken(Choice.Backend), Out); Out += ",\n";
        Key(Out, 3, "profile"); EscapeJson(Choice.Profile.View(), Out); Out += ",\n";
        Key(Out, 3, "format"); EscapeJson(ShaderFormatToken(Choice.Format), Out); Out += "\n";
        Indent(Out, 2); Out += Index + 1 == Profile.ShaderPayloadChoices.size() ? "}\n" : "},\n";
    }
    Indent(Out, 1); Out += "],\n";
    if (Profile.MetalShaderTarget.has_value())
    {
        const auto& Metal = *Profile.MetalShaderTarget;
        Key(Out, 1, "metalShaderTarget"); Out += "{\n";
        Key(Out, 2, "deploymentTarget"); EscapeJson(Metal.DeploymentTarget.View(), Out); Out += ",\n";
        Key(Out, 2, "mslVersion"); EscapeJson(Metal.MslVersion.View(), Out); Out += ",\n";
        Key(Out, 2, "bindingPolicy"); EscapeJson(Metal.BindingPolicy.View(), Out); Out += ",\n";
        Key(Out, 2, "nativeEvidenceSchemaVersion"); Out += std::to_string(Metal.NativeEvidenceSchemaVersion); Out += "\n";
        Indent(Out, 1); Out += "},\n";
    }
    Key(Out, 1, "textureCapabilities"); WriteStringArray(Out, Profile.TextureCapabilities); Out += ",\n";
    Key(Out, 1, "textureFallback");
    EscapeJson(
        Profile.TextureFallback == EAssetTextureFallback::Fail
            ? "fail"
            : Profile.TextureFallback == EAssetTextureFallback::Uncompressed
                ? "uncompressed" : "portable-ktx2",
        Out);
    Out += ",\n";
    Key(Out, 1, "buildPolicy"); Out += "{\n";
    Key(Out, 2, "optimization"); EscapeJson(Profile.BuildPolicy.Optimization == EAssetBuildOptimization::Development ? "development" : "shipping", Out); Out += ",\n";
    Key(Out, 2, "includeDebugSymbols"); Out += Profile.BuildPolicy.bIncludeDebugSymbols ? "true" : "false"; Out += ",\n";
    Key(Out, 2, "validation"); EscapeJson(Profile.BuildPolicy.Validation == EAssetBuildValidation::Standard ? "standard" : "strict", Out); Out += ",\n";
    Key(Out, 2, "producerSettings"); Out += "[\n";
    for (Core::usize RecordIndex = 0; RecordIndex < Profile.BuildPolicy.ProducerSettings.size(); ++RecordIndex)
    {
        const auto& Record = Profile.BuildPolicy.ProducerSettings[RecordIndex];
        Indent(Out, 3); Out += "{\n";
        Key(Out, 4, "producer"); EscapeJson(Record.Producer.ToString().View(), Out); Out += ",\n";
        Key(Out, 4, "schemaVersion"); Out += std::to_string(Record.SchemaVersion); Out += ",\n";
        Key(Out, 4, "settings"); Out += "{";
        if (!Record.Settings.empty()) Out += "\n";
        for (Core::usize SettingIndex = 0; SettingIndex < Record.Settings.size(); ++SettingIndex)
        {
            const auto& Setting = Record.Settings[SettingIndex];
            Key(Out, 5, Setting.Name.CStr());
            WriteSettingValue(Out, Setting.Value);
            Out += SettingIndex + 1 == Record.Settings.size() ? "\n" : ",\n";
        }
        if (!Record.Settings.empty()) Indent(Out, 4);
        Out += "}\n";
        Indent(Out, 3); Out += RecordIndex + 1 == Profile.BuildPolicy.ProducerSettings.size() ? "}\n" : "},\n";
    }
    Indent(Out, 2); Out += "]\n";
    Indent(Out, 1); Out += "},\n";
    Key(Out, 1, "limits"); Out += "{\n";
    Key(Out, 2, "maxDiscoveredSources"); Out += std::to_string(Profile.Limits.MaxDiscoveredSources); Out += ",\n";
    Key(Out, 2, "maxAssets"); Out += std::to_string(Profile.Limits.MaxAssets); Out += ",\n";
    Key(Out, 2, "maxDependencyEdges"); Out += std::to_string(Profile.Limits.MaxDependencyEdges); Out += ",\n";
    Key(Out, 2, "maxDependencyDepth"); Out += std::to_string(Profile.Limits.MaxDependencyDepth); Out += ",\n";
    Key(Out, 2, "maxSourceBytes"); Out += std::to_string(Profile.Limits.MaxSourceBytes); Out += ",\n";
    Key(Out, 2, "maxPayloadBytes"); Out += std::to_string(Profile.Limits.MaxPayloadBytes); Out += ",\n";
    Key(Out, 2, "maxAggregateBytes"); Out += std::to_string(Profile.Limits.MaxAggregateBytes); Out += ",\n";
    Key(Out, 2, "maxManifestBytes"); Out += std::to_string(Profile.Limits.MaxManifestBytes); Out += ",\n";
    Key(Out, 2, "maxDiagnostics"); Out += std::to_string(Profile.Limits.MaxDiagnostics); Out += "\n";
    Indent(Out, 1); Out += "}";
    if (!Profile.RequiredExtensions.empty() || !Profile.OptionalExtensions.empty())
    {
        Out += ",\n";
        Key(Out, 1, "requiredExtensions"); WriteStringArray(Out, Profile.RequiredExtensions); Out += ",\n";
        Key(Out, 1, "optionalExtensions"); WriteStringArray(Out, Profile.OptionalExtensions);
    }
    Out += "\n}\n";
    return Out;
}

std::string CanonicalSettings(const FAssetProducerSettingsRecord& Record)
{
    std::string Out = "{\n";
    Key(Out, 1, "producer"); EscapeJson(Record.Producer.ToString().View(), Out); Out += ",\n";
    Key(Out, 1, "schemaVersion"); Out += std::to_string(Record.SchemaVersion); Out += ",\n";
    Key(Out, 1, "settings"); Out += "{";
    if (!Record.Settings.empty()) Out += "\n";
    for (Core::usize Index = 0; Index < Record.Settings.size(); ++Index)
    {
        Key(Out, 2, Record.Settings[Index].Name.CStr());
        WriteSettingValue(Out, Record.Settings[Index].Value);
        Out += Index + 1 == Record.Settings.size() ? "\n" : ",\n";
    }
    if (!Record.Settings.empty()) Indent(Out, 1);
    Out += "}\n}\n";
    return Out;
}

void AppendRelevantField(
    std::string& Out,
    const FAssetTargetProfile& Profile,
    const Core::FString& Field)
{
    EscapeJson(Field.View(), Out);
    Out.push_back('=');
    if (Field == Core::FString("platform"))
        EscapeJson(PlatformToken(Profile.Platform), Out);
    else if (Field == Core::FString("cpuArchitecture"))
        EscapeJson(ArchitectureToken(Profile.CpuArchitecture), Out);
    else if (Field == Core::FString("graphicsBackend"))
        EscapeJson(BackendToken(Profile.GraphicsBackend), Out);
    else if (Field == Core::FString("textureFallback"))
        Out += std::to_string(static_cast<int>(Profile.TextureFallback));
    else if (Field == Core::FString("textureCapabilities"))
        WriteStringArray(Out, Profile.TextureCapabilities);
    else if (Field == Core::FString("shaderPayloadChoices"))
    {
        Out.push_back('[');
        for (const auto& Choice : Profile.ShaderPayloadChoices)
        {
            EscapeJson(BackendToken(Choice.Backend), Out);
            EscapeJson(Choice.Profile.View(), Out);
            EscapeJson(ShaderFormatToken(Choice.Format), Out);
        }
        Out.push_back(']');
    }
    else if (Field == Core::FString("metalShaderTarget"))
    {
        if (!Profile.MetalShaderTarget)
        {
            Out += "null";
        }
        else
        {
            const auto& Metal = *Profile.MetalShaderTarget;
            Out.push_back('{');
            EscapeJson(Metal.DeploymentTarget.View(), Out);
            EscapeJson(Metal.MslVersion.View(), Out);
            EscapeJson(Metal.BindingPolicy.View(), Out);
            Out += std::to_string(Metal.NativeEvidenceSchemaVersion);
            Out.push_back('}');
        }
    }
    else if (Field == Core::FString("optimization"))
        Out += std::to_string(static_cast<int>(Profile.BuildPolicy.Optimization));
    else if (Field == Core::FString("includeDebugSymbols"))
        Out += Profile.BuildPolicy.bIncludeDebugSymbols ? "true" : "false";
    else if (Field == Core::FString("validation"))
        Out += std::to_string(static_cast<int>(Profile.BuildPolicy.Validation));
}

} // namespace

EAssetResult ParseAssetTargetProfile(
    std::span<const Core::uint8> Bytes,
    FAssetTargetProfileEvidence& OutEvidence)
{
    OutEvidence = {};
    if (Bytes.empty() || Bytes.size() > MaxProfileBytes ||
        std::find(Bytes.begin(), Bytes.end(), Core::uint8{0}) != Bytes.end())
    {
        return EAssetResult::DefinitionLimitExceeded;
    }
    constexpr yyjson_read_flag Flags = YYJSON_READ_NUMBER_AS_RAW;
    const std::size_t Required = yyjson_read_max_memory_usage(Bytes.size(), Flags);
    if (Required == 0)
    {
        return EAssetResult::InvalidDefinition;
    }
    std::vector<Core::uint8> Pool(Required);
    yyjson_alc Allocator{};
    if (!yyjson_alc_pool_init(&Allocator, Pool.data(), Pool.size()))
    {
        return EAssetResult::DefinitionLimitExceeded;
    }
    yyjson_doc* Document = yyjson_read_opts(
        const_cast<char*>(reinterpret_cast<const char*>(Bytes.data())),
        Bytes.size(), Flags, &Allocator, nullptr);
    if (!Document)
    {
        return EAssetResult::InvalidDefinition;
    }
    yyjson_val* Root = yyjson_doc_get_root(Document);
    if (Preflight(Root) != EAssetResult::Success ||
        !ClosedObject(
            Root,
            {"schema", "schemaVersion", "displayName", "platform",
             "cpuArchitecture", "graphicsBackend", "shaderPayloadChoices",
             "metalShaderTarget",
             "textureCapabilities", "textureFallback", "buildPolicy", "limits",
             "requiredExtensions", "optionalExtensions", "extensions"}))
    {
        return EAssetResult::InvalidDefinition;
    }
    yyjson_val* Extensions = yyjson_obj_get(Root, "extensions");
    if (Extensions != nullptr &&
        (!yyjson_is_obj(Extensions) || yyjson_obj_size(Extensions) != 0))
    {
        return EAssetResult::UnknownRequiredExtension;
    }

    FAssetTargetProfile Profile;
    if (!String(Root, "schema", Profile.Schema) ||
        !Unsigned(Root, "schemaVersion", Profile.SchemaVersion) ||
        !String(Root, "displayName", Profile.DisplayName) ||
        !ParseEnumToken(
            Root, "platform",
            {{"windows", EAssetTargetPlatform::Windows},
             {"macos", EAssetTargetPlatform::MacOS},
             {"linux", EAssetTargetPlatform::Linux},
             {"android", EAssetTargetPlatform::Android},
             {"ios", EAssetTargetPlatform::IOS}},
            Profile.Platform) ||
        !ParseEnumToken(
            Root, "cpuArchitecture",
            {{"x86_64", EAssetTargetCpuArchitecture::X86_64},
             {"arm64", EAssetTargetCpuArchitecture::Arm64}},
            Profile.CpuArchitecture) ||
        !ParseEnumToken(
            Root, "graphicsBackend",
            {{"vulkan", EAssetGraphicsBackend::Vulkan},
             {"metal", EAssetGraphicsBackend::Metal},
             {"dx12", EAssetGraphicsBackend::DirectX12},
             {"opengl", EAssetGraphicsBackend::OpenGL},
             {"gles", EAssetGraphicsBackend::GLES}},
            Profile.GraphicsBackend) ||
        !ParseShaderChoices(Root, Profile.ShaderPayloadChoices) ||
        !ParseStringArray(Root, "textureCapabilities", Profile.TextureCapabilities, 64, true) ||
        !ParseEnumToken(
            Root, "textureFallback",
            {{"fail", EAssetTextureFallback::Fail},
             {"uncompressed", EAssetTextureFallback::Uncompressed},
             {"portable-ktx2", EAssetTextureFallback::PortableKTX2}},
            Profile.TextureFallback) ||
        !ParseBuildPolicy(Root, Profile.BuildPolicy) ||
        !ParseLimits(Root, Profile.Limits) ||
        !ParseStringArray(Root, "requiredExtensions", Profile.RequiredExtensions, 64, false) ||
        !ParseStringArray(Root, "optionalExtensions", Profile.OptionalExtensions, 64, false))
    {
        return EAssetResult::InvalidDefinition;
    }
    if (Profile.SchemaVersion != 1 &&
        Profile.SchemaVersion != FAssetTargetProfile::CurrentSchemaVersion)
    {
        return EAssetResult::UnsupportedSchema;
    }
    yyjson_val* Metal = yyjson_obj_get(Root, "metalShaderTarget");
    if (Metal != nullptr)
    {
        FAssetMetalShaderTarget Target;
        if (!ClosedObject(
                Metal,
                {"deploymentTarget", "mslVersion", "bindingPolicy",
                 "nativeEvidenceSchemaVersion"}) ||
            !String(Metal, "deploymentTarget", Target.DeploymentTarget) ||
            !String(Metal, "mslVersion", Target.MslVersion) ||
            !String(Metal, "bindingPolicy", Target.BindingPolicy) ||
            !Unsigned(
                Metal, "nativeEvidenceSchemaVersion",
                Target.NativeEvidenceSchemaVersion))
        {
            return EAssetResult::InvalidDefinition;
        }
        Profile.MetalShaderTarget = std::move(Target);
    }
    const EAssetResult Validation = Profile.Validate();
    if (Validation != EAssetResult::Success)
    {
        return Validation;
    }
    Core::FString Canonical;
    return WriteAssetTargetProfile(Profile, Canonical, &OutEvidence);
}

EAssetResult WriteAssetTargetProfile(
    const FAssetTargetProfile& Profile,
    Core::FString& OutCanonical,
    FAssetTargetProfileEvidence* OutEvidence)
{
    OutCanonical.Clear();
    if (OutEvidence) *OutEvidence = {};
    const EAssetResult Validation = Profile.Validate();
    if (Validation != EAssetResult::Success)
    {
        return Validation;
    }
    OutCanonical = Core::FString(WriteProfile(Profile, true));
    const Core::FString Effective(WriteProfile(Profile, false));
    const auto* Begin = reinterpret_cast<const Core::uint8*>(Effective.View().data());
    const FAssetDigest Digest = FAssetDigest::FromBytes(
        std::span<const Core::uint8>(Begin, Effective.Len()));
    if (OutEvidence)
    {
        OutEvidence->Profile = Profile;
        OutEvidence->EffectiveProfileDigest = Digest;
        OutEvidence->CanonicalEffectiveConfiguration = Effective;
    }
    return EAssetResult::Success;
}

EAssetResult BuildAssetProfileProjection(
    const FAssetTargetProfileEvidence& Profile,
    const FAssetParticipantId& Producer,
    Core::uint32 ExpectedSchemaVersion,
    std::span<const Core::FString> RelevantTargetFields,
    FAssetProfileProjectionEvidence& OutProjection)
{
    OutProjection = {};
    if (Profile.Validate() != EAssetResult::Success || !Producer.IsValid() ||
        ExpectedSchemaVersion == 0)
    {
        return EAssetResult::InvalidInput;
    }
    const FAssetProducerSettingsRecord* Record =
        Profile.Profile.BuildPolicy.FindProducer(Producer);
    if (!Record || Record->SchemaVersion != ExpectedSchemaVersion)
    {
        return EAssetResult::InvalidInput;
    }
    Core::TArray<Core::FString> SortedFields(
        RelevantTargetFields.begin(), RelevantTargetFields.end());
    std::sort(SortedFields.begin(), SortedFields.end());
    if (std::adjacent_find(SortedFields.begin(), SortedFields.end()) !=
        SortedFields.end())
    {
        return EAssetResult::InvalidInput;
    }
    static const std::set<std::string_view> AllowedFields = {
        "platform", "cpuArchitecture", "graphicsBackend",
        "shaderPayloadChoices", "metalShaderTarget",
        "textureCapabilities", "textureFallback",
        "optimization", "includeDebugSymbols", "validation"};
    std::string Relevant = "{\n";
    for (Core::usize Index = 0; Index < SortedFields.size(); ++Index)
    {
        if (!AllowedFields.contains(SortedFields[Index].View()))
        {
            return EAssetResult::InvalidInput;
        }
        Indent(Relevant, 1);
        AppendRelevantField(Relevant, Profile.Profile, SortedFields[Index]);
        Relevant += Index + 1 == SortedFields.size() ? "\n" : ",\n";
    }
    Relevant += "}\n";
    const Core::FString Settings(CanonicalSettings(*Record));
    const Core::FString RelevantText(std::move(Relevant));
    const auto* SettingsBegin = reinterpret_cast<const Core::uint8*>(Settings.View().data());
    const auto* RelevantBegin = reinterpret_cast<const Core::uint8*>(RelevantText.View().data());
    OutProjection.Producer = Producer;
    OutProjection.ProducerSettingsSchemaVersion = Record->SchemaVersion;
    OutProjection.CanonicalProducerSettings = Settings;
    OutProjection.EffectiveSettingsDigest = FAssetDigest::FromBytes(
        std::span<const Core::uint8>(SettingsBegin, Settings.Len()));
    OutProjection.CanonicalRelevantProfile = RelevantText;
    OutProjection.RelevantProfileDigest = FAssetDigest::FromBytes(
        std::span<const Core::uint8>(RelevantBegin, RelevantText.Len()));
    return OutProjection.Validate();
}

} // namespace Stoner::Asset::Private
