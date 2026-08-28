#include "ProductionImageBaselineRegistry.h"

#include "../ThirdParty/yyjson/yyjson.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <string>

namespace
{
using namespace Stoner::Core;

using FSignature = FProductionCapabilitySignature;

bool HasExactKeys(yyjson_val* Object,
    std::initializer_list<const char*> Keys)
{
    if (!yyjson_is_obj(Object) || yyjson_obj_size(Object) != Keys.size())
        return false;
    for (const char* Key : Keys)
        if (!yyjson_obj_get(Object, Key)) return false;
    return true;
}

bool String(yyjson_val* Object, const char* Key, FString& Out)
{
    yyjson_val* Value = yyjson_obj_get(Object, Key);
    if (!yyjson_is_str(Value)) return false;
    Out = FString(std::string_view(yyjson_get_str(Value), yyjson_get_len(Value)));
    return !Out.IsEmpty();
}

bool UInt(yyjson_val* Object, const char* Key, uint32& Out)
{
    yyjson_val* Value = yyjson_obj_get(Object, Key);
    if (!yyjson_is_uint(Value) || yyjson_get_uint(Value) > UINT32_MAX)
        return false;
    Out = static_cast<uint32>(yyjson_get_uint(Value));
    return true;
}

bool Number(yyjson_val* Object, const char* Key, float& Out)
{
    yyjson_val* Value = yyjson_obj_get(Object, Key);
    if (!yyjson_is_num(Value)) return false;
    Out = static_cast<float>(yyjson_get_num(Value));
    return std::isfinite(Out);
}

bool IsToken(const FString& Value)
{
    if (Value.IsEmpty() || Value.Len() > 128) return false;
    return std::all_of(Value.View().begin(), Value.View().end(), [](char Character) {
        return (Character >= 'a' && Character <= 'z') ||
            (Character >= '0' && Character <= '9') || Character == '.' ||
            Character == '-';
    });
}

bool IsSignatureToken(const FString& Value)
{
    if (Value.IsEmpty() || Value.Len() > 128) return false;
    return std::all_of(Value.View().begin(), Value.View().end(), [](char Character) {
        return (Character >= 'a' && Character <= 'z') ||
            (Character >= '0' && Character <= '9') || Character == '.' ||
            Character == '-' || Character == '_';
    });
}

bool IsDigest(const FString& Value)
{
    return Value.Len() == 64 &&
        std::all_of(Value.View().begin(), Value.View().end(), [](char Character) {
            return (Character >= '0' && Character <= '9') ||
                (Character >= 'a' && Character <= 'f');
        });
}

bool ParseSignature(yyjson_val* Object, FSignature& Out)
{
    if (!HasExactKeys(Object,
            {"registryVersion", "backendImplementation", "cpuArchitecture",
             "adapterFamily", "shaderProfile", "colorFormat", "depthFormat",
             "sampleCount", "textureFormatFamily"}) ||
        !UInt(Object, "registryVersion", Out.RegistryVersion) ||
        !String(Object, "backendImplementation", Out.BackendImplementation) ||
        !String(Object, "cpuArchitecture", Out.CpuArchitecture) ||
        !String(Object, "adapterFamily", Out.AdapterFamily) ||
        !String(Object, "shaderProfile", Out.ShaderProfile) ||
        !String(Object, "colorFormat", Out.ColorFormat) ||
        !String(Object, "depthFormat", Out.DepthFormat) ||
        !UInt(Object, "sampleCount", Out.SampleCount) ||
        !String(Object, "textureFormatFamily", Out.TextureFormatFamily))
        return false;
    return Out.IsValid();
}

struct FDocument
{
    yyjson_doc* Value = nullptr;
    ~FDocument() { yyjson_doc_free(Value); }
};

bool ReadJson(const std::filesystem::path& Path, FDocument& Out)
{
    std::ifstream Input(Path, std::ios::binary);
    if (!Input) return false;
    std::string Bytes{std::istreambuf_iterator<char>(Input), {}};
    yyjson_read_err Error{};
    Out.Value = yyjson_read_opts(
        const_cast<char*>(Bytes.data()), Bytes.size(), YYJSON_READ_NOFLAG,
        nullptr, &Error);
    return Out.Value != nullptr && yyjson_doc_get_root(Out.Value) != nullptr;
}

bool ParsePolicy(yyjson_val* Object, FProductionFlipPolicy& Out)
{
    if (!HasExactKeys(Object,
            {"meanMax", "p95Max", "maximumMax", "badPixelThreshold",
             "badPixelFractionMax"}) ||
        !Number(Object, "meanMax", Out.MeanMax) ||
        !Number(Object, "p95Max", Out.P95Max) ||
        !Number(Object, "maximumMax", Out.MaximumMax) ||
        !Number(Object, "badPixelThreshold", Out.BadPixelThreshold) ||
        !Number(Object, "badPixelFractionMax", Out.BadPixelFractionMax))
        return false;
    return Out.MeanMax >= 0.0f && Out.MeanMax <= 1.0f &&
        Out.P95Max >= 0.0f && Out.P95Max <= 1.0f &&
        Out.MaximumMax >= 0.0f && Out.MaximumMax <= 1.0f &&
        Out.BadPixelThreshold > 0.0f && Out.BadPixelThreshold <= 1.0f &&
        Out.BadPixelFractionMax >= 0.0f && Out.BadPixelFractionMax <= 1.0f;
}

bool ParseBaseline(yyjson_val* Root, FProductionImageBaseline& Out)
{
    uint32 SchemaVersion = 0;
    FString Schema;
    FString Transfer;
    if (!HasExactKeys(Root,
            {"schema", "schemaVersion", "baselineId", "state",
             "workloadRevision", "backend", "deviceClass",
             "capabilitySignature", "width", "height", "colorTransfer",
             "referencePath", "referenceSha256", "flipPolicy",
             "calibrationEvidenceSha256"}) ||
        !String(Root, "schema", Schema) ||
        Schema != FString("stoner.production-image-baseline") ||
        !UInt(Root, "schemaVersion", SchemaVersion) || SchemaVersion != 1 ||
        !String(Root, "baselineId", Out.BaselineId) ||
        !String(Root, "state", Out.State) ||
        !String(Root, "workloadRevision", Out.WorkloadRevision) ||
        !String(Root, "backend", Out.Backend) ||
        !String(Root, "deviceClass", Out.DeviceClass) ||
        !ParseSignature(yyjson_obj_get(Root, "capabilitySignature"), Out.Signature) ||
        !UInt(Root, "width", Out.Width) || !UInt(Root, "height", Out.Height) ||
        !String(Root, "colorTransfer", Transfer) ||
        !String(Root, "referencePath", Out.ReferencePath) ||
        !String(Root, "referenceSha256", Out.ReferenceSha256) ||
        !ParsePolicy(yyjson_obj_get(Root, "flipPolicy"), Out.FlipPolicy) ||
        !String(Root, "calibrationEvidenceSha256",
            Out.CalibrationEvidenceSha256))
        return false;
    static const std::set<std::string> States = {
        "candidate", "calibrated", "reviewed", "accepted", "superseded"};
    if (!IsToken(Out.BaselineId) || !IsToken(Out.WorkloadRevision) ||
        !IsToken(Out.DeviceClass) ||
        States.find(Out.State.ToStdString()) == States.end() ||
        (Out.Backend != FString("vulkan") && Out.Backend != FString("metal")) ||
        Out.Width != 512 || Out.Height != 512 ||
        Out.ReferencePath.View().starts_with('/') ||
        Out.ReferencePath.View().find('\\') != std::string_view::npos ||
        !IsDigest(Out.ReferenceSha256) ||
        !IsDigest(Out.CalibrationEvidenceSha256))
        return false;
    if (Transfer == FString("srgb"))
        Out.ColorTransfer = EProductionColorTransfer::SRGB;
    else if (Transfer == FString("linear-to-srgb-v1"))
        Out.ColorTransfer = EProductionColorTransfer::Linear;
    else
        return false;
    return true;
}

} // namespace

bool FProductionCapabilitySignature::IsValid() const noexcept
{
    static const std::set<std::string> Implementations = {
        "native-vulkan", "moltenvk", "lavapipe", "native-metal"};
    return RegistryVersion == 1 &&
        Implementations.contains(BackendImplementation.ToStdString()) &&
        (CpuArchitecture == Stoner::Core::FString("x86_64") ||
         CpuArchitecture == Stoner::Core::FString("arm64")) &&
        IsSignatureToken(AdapterFamily) &&
        IsSignatureToken(ShaderProfile) &&
        IsSignatureToken(ColorFormat) &&
        IsSignatureToken(DepthFormat) &&
        IsSignatureToken(TextureFormatFamily) &&
        (SampleCount == 1 || SampleCount == 2 || SampleCount == 4 ||
         SampleCount == 8);
}

Stoner::Core::FString FProductionCapabilitySignature::CanonicalKey() const
{
    return Stoner::Core::FString(
        std::to_string(RegistryVersion) + "|" +
        BackendImplementation.ToStdString() + "|" +
        CpuArchitecture.ToStdString() + "|" + AdapterFamily.ToStdString() +
        "|" + ShaderProfile.ToStdString() + "|" + ColorFormat.ToStdString() +
        "|" + DepthFormat.ToStdString() + "|" +
        std::to_string(SampleCount) + "|" +
        TextureFormatFamily.ToStdString());
}

bool FProductionImageBaselineRegistry::LoadDeviceClasses(
    const Stoner::Core::FString& Path,
    Stoner::Core::FString& OutFailure)
{
    DeviceClasses.clear();
    OutFailure = {};
    FDocument Document;
    if (!ReadJson(Path.ToStdString(), Document))
    {
        OutFailure = "device-class-json";
        return false;
    }
    yyjson_val* Root = yyjson_doc_get_root(Document.Value);
    uint32 SchemaVersion = 0;
    uint32 RegistryVersion = 0;
    FString Schema;
    yyjson_val* Classes = yyjson_obj_get(Root, "classes");
    if (!HasExactKeys(Root,
            {"schema", "schemaVersion", "registryVersion", "classes"}) ||
        !String(Root, "schema", Schema) ||
        Schema != FString("stoner.production-device-class-registry") ||
        !UInt(Root, "schemaVersion", SchemaVersion) || SchemaVersion != 1 ||
        !UInt(Root, "registryVersion", RegistryVersion) ||
        RegistryVersion != 1 || !yyjson_is_arr(Classes) ||
        yyjson_arr_size(Classes) == 0)
    {
        OutFailure = "device-class-schema";
        return false;
    }
    std::set<std::string> ClassIds;
    std::set<std::string> SignatureKeys;
    FString Previous;
    size_t Index = 0;
    size_t Maximum = 0;
    yyjson_val* Entry = nullptr;
    yyjson_arr_foreach(Classes, Index, Maximum, Entry)
    {
        FDeviceClassRecord Record;
        if (!HasExactKeys(Entry, {"deviceClass", "capabilitySignature"}) ||
            !String(Entry, "deviceClass", Record.DeviceClass) ||
            !IsToken(Record.DeviceClass) ||
            !ParseSignature(yyjson_obj_get(Entry, "capabilitySignature"),
                Record.Signature) ||
            (!Previous.IsEmpty() && !(Previous < Record.DeviceClass)) ||
            !ClassIds.insert(Record.DeviceClass.ToStdString()).second ||
            !SignatureKeys.insert(
                Record.Signature.CanonicalKey().ToStdString()).second)
        {
            DeviceClasses.clear();
            OutFailure = "device-class-order-or-uniqueness";
            return false;
        }
        Previous = Record.DeviceClass;
        DeviceClasses.push_back(std::move(Record));
    }
    return true;
}

bool FProductionImageBaselineRegistry::LoadBaselines(
    const Stoner::Core::FString& Root,
    Stoner::Core::FString& OutFailure)
{
    Baselines.clear();
    OutFailure = {};
    std::error_code Error;
    std::vector<std::filesystem::path> Paths;
    for (std::filesystem::recursive_directory_iterator Iterator(
             Root.ToStdString(), Error), End;
         !Error && Iterator != End; Iterator.increment(Error))
    {
        if (Iterator->is_regular_file() && Iterator->path().extension() == ".json" &&
            Iterator->path().filename() != "DeviceClasses.json")
            Paths.push_back(Iterator->path());
    }
    if (Error)
    {
        OutFailure = "baseline-enumeration";
        return false;
    }
    std::sort(Paths.begin(), Paths.end());
    std::set<std::string> BaselineIds;
    for (const auto& Path : Paths)
    {
        FDocument Document;
        FProductionImageBaseline Baseline;
        if (!ReadJson(Path, Document) ||
            !ParseBaseline(yyjson_doc_get_root(Document.Value), Baseline) ||
            !BaselineIds.insert(Baseline.BaselineId.ToStdString()).second)
        {
            Baselines.clear();
            OutFailure = "baseline-schema-or-identity";
            return false;
        }
        Baselines.push_back(std::move(Baseline));
    }
    return true;
}

bool FProductionImageBaselineRegistry::SelectAccepted(
    const FProductionCapabilitySignature& Signature,
    const Stoner::Core::FString& WorkloadRevision,
    const Stoner::Core::FString& Backend,
    FProductionImageBaseline& OutBaseline,
    Stoner::Core::FString& OutFailure) const
{
    OutBaseline = {};
    OutFailure = {};
    if (!Signature.IsValid() || !IsToken(WorkloadRevision) ||
        (Backend != Stoner::Core::FString("vulkan") &&
         Backend != Stoner::Core::FString("metal")))
    {
        OutFailure = "selection-contract";
        return false;
    }
    const auto SignatureKey = Signature.CanonicalKey();
    const FDeviceClassRecord* MatchedClass = nullptr;
    for (const auto& Candidate : DeviceClasses)
    {
        if (Candidate.Signature.CanonicalKey() == SignatureKey)
        {
            if (MatchedClass)
            {
                OutFailure = "device-class-ambiguous";
                return false;
            }
            MatchedClass = &Candidate;
        }
    }
    if (!MatchedClass)
    {
        OutFailure = "device-class-missing";
        return false;
    }
    OutBaseline.DeviceClass = MatchedClass->DeviceClass;

    const FProductionImageBaseline* Match = nullptr;
    bool bFoundNonAccepted = false;
    for (const auto& Candidate : Baselines)
    {
        if (Candidate.WorkloadRevision != WorkloadRevision ||
            Candidate.Backend != Backend ||
            Candidate.DeviceClass != MatchedClass->DeviceClass)
            continue;
        if (Candidate.Signature.CanonicalKey() != SignatureKey)
        {
            OutFailure = "baseline-signature-mismatch";
            return false;
        }
        if (Candidate.State != Stoner::Core::FString("accepted"))
        {
            bFoundNonAccepted = true;
            continue;
        }
        if (Match)
        {
            OutFailure = "baseline-ambiguous";
            return false;
        }
        Match = &Candidate;
    }
    if (!Match)
    {
        OutFailure = bFoundNonAccepted
            ? Stoner::Core::FString("baseline-state-not-accepted")
            : Stoner::Core::FString("baseline-missing");
        return false;
    }
    OutBaseline = *Match;
    return true;
}
