#include "ProductionImageAcceptance.h"

#include "../ThirdParty/yyjson/yyjson.h"

#if defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4201)
#endif
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#pragma GCC diagnostic ignored "-Wextra"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "FLIP.h"
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif
#if defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <memory>
#include <set>
#include <string>

namespace
{
using namespace Stoner::Core;

float SrgbToLinear(float Value)
{
    return Value <= 0.04045f
        ? Value / 12.92f
        : std::pow((Value + 0.055f) / 1.055f, 2.4f);
}

float HalfToFloat(uint16 Value)
{
    const uint32 Sign = static_cast<uint32>(Value & 0x8000u) << 16u;
    uint32 Exponent = (Value >> 10u) & 0x1fu;
    uint32 Mantissa = Value & 0x03ffu;
    uint32 Bits = 0;
    if (Exponent == 0)
    {
        if (Mantissa == 0)
        {
            Bits = Sign;
        }
        else
        {
            Exponent = 127u - 15u + 1u;
            while ((Mantissa & 0x0400u) == 0)
            {
                Mantissa <<= 1u;
                --Exponent;
            }
            Bits = Sign | (Exponent << 23u) |
                ((Mantissa & 0x03ffu) << 13u);
        }
    }
    else if (Exponent == 0x1fu)
    {
        Bits = Sign | 0x7f800000u | (Mantissa << 13u);
    }
    else
    {
        Bits = Sign | ((Exponent + 127u - 15u) << 23u) |
            (Mantissa << 13u);
    }
    float Result = 0.0f;
    std::memcpy(&Result, &Bits, sizeof(Result));
    return Result;
}

uint32 BytesPerPixel(EProductionReadbackPixelFormat Format)
{
    switch (Format)
    {
    case EProductionReadbackPixelFormat::RGBA8UNorm:
    case EProductionReadbackPixelFormat::BGRA8UNorm:
        return 4;
    case EProductionReadbackPixelFormat::RGBA16Float:
        return 8;
    case EProductionReadbackPixelFormat::RGBA32Float:
        return 16;
    case EProductionReadbackPixelFormat::R32Float:
        return 4;
    }
    return 0;
}

bool ReadPixel(const uint8* Bytes, EProductionReadbackPixelFormat Format,
    float& R, float& G, float& B)
{
    switch (Format)
    {
    case EProductionReadbackPixelFormat::RGBA8UNorm:
        R = Bytes[0] / 255.0f;
        G = Bytes[1] / 255.0f;
        B = Bytes[2] / 255.0f;
        return true;
    case EProductionReadbackPixelFormat::BGRA8UNorm:
        R = Bytes[2] / 255.0f;
        G = Bytes[1] / 255.0f;
        B = Bytes[0] / 255.0f;
        return true;
    case EProductionReadbackPixelFormat::RGBA16Float:
    {
        uint16 Channels[3]{};
        std::memcpy(Channels, Bytes, sizeof(Channels));
        R = HalfToFloat(Channels[0]);
        G = HalfToFloat(Channels[1]);
        B = HalfToFloat(Channels[2]);
        return true;
    }
    case EProductionReadbackPixelFormat::RGBA32Float:
    {
        float Channels[3]{};
        std::memcpy(Channels, Bytes, sizeof(Channels));
        R = Channels[0];
        G = Channels[1];
        B = Channels[2];
        return true;
    }
    case EProductionReadbackPixelFormat::R32Float:
        std::memcpy(&R, Bytes, sizeof(R));
        G = R;
        B = R;
        return true;
    }
    return false;
}

bool IsUnit(float Value)
{
    return std::isfinite(Value) && Value >= 0.0f && Value <= 1.0f;
}

bool HasExactJsonKeys(yyjson_val* Object,
    std::initializer_list<const char*> Keys)
{
    if (!yyjson_is_obj(Object) || yyjson_obj_size(Object) != Keys.size())
        return false;
    for (const char* Key : Keys)
        if (!yyjson_obj_get(Object, Key)) return false;
    return true;
}

bool ReadJsonString(yyjson_val* Object, const char* Key, FString& Out)
{
    yyjson_val* Value = yyjson_obj_get(Object, Key);
    if (!yyjson_is_str(Value)) return false;
    Out = FString(std::string_view(yyjson_get_str(Value), yyjson_get_len(Value)));
    return !Out.IsEmpty();
}

bool ReadJsonUInt(yyjson_val* Object, const char* Key, uint32& Out)
{
    yyjson_val* Value = yyjson_obj_get(Object, Key);
    if (!yyjson_is_uint(Value) || yyjson_get_uint(Value) > UINT32_MAX)
        return false;
    Out = static_cast<uint32>(yyjson_get_uint(Value));
    return true;
}

bool ReadJsonNumber(yyjson_val* Object, const char* Key, double& Out)
{
    yyjson_val* Value = yyjson_obj_get(Object, Key);
    if (!yyjson_is_num(Value)) return false;
    Out = yyjson_get_num(Value);
    return std::isfinite(Out);
}

bool IsBaselineV3Token(const FString& Value)
{
    if (Value.IsEmpty() || Value.Len() > 128) return false;
    return std::all_of(Value.View().begin(), Value.View().end(), [](char Character) {
        return (Character >= 'A' && Character <= 'Z') ||
            (Character >= 'a' && Character <= 'z') ||
            (Character >= '0' && Character <= '9') || Character == '.' ||
            Character == '-' || Character == '_';
    });
}

bool IsBaselineV3Digest(const FString& Value)
{
    return Value.Len() == 64 &&
        std::all_of(Value.View().begin(), Value.View().end(), [](char Character) {
            return (Character >= '0' && Character <= '9') ||
                (Character >= 'a' && Character <= 'f');
        });
}

bool IsFreshV3Workload(const FString& Value)
{
    const auto View = Value.View();
    constexpr std::string_view Lantern = "production-content-lantern-v3";
    constexpr std::string_view Sponza = "production-content-sponza-v3";
    const bool bPrefix = View == Lantern || View == Sponza ||
        View.starts_with(std::string(Lantern) + ".") ||
        View.starts_with(std::string(Lantern) + "-") ||
        View.starts_with(std::string(Sponza) + ".") ||
        View.starts_with(std::string(Sponza) + "-");
    if (!bPrefix) return false;
    return std::all_of(View.begin(), View.end(), [](char Character) {
        return (Character >= 'a' && Character <= 'z') ||
            (Character >= '0' && Character <= '9') || Character == '.' ||
            Character == '-';
    });
}

bool IsSafePngPath(const FString& Value)
{
    const auto View = Value.View();
    if (View.empty() || View.size() > 240 || View.front() == '/' ||
        View.find('\\') != std::string_view::npos ||
        !View.ends_with(".png"))
        return false;
    const std::filesystem::path Path(Value.ToStdString());
    return std::none_of(Path.begin(), Path.end(), [](const auto& Part) {
        return Part == "..";
    });
}

bool ParseBaselineV3Policy(yyjson_val* Object, FProductionFlipPolicy& Out)
{
    double Mean = 0.0;
    double P95 = 0.0;
    double Maximum = 0.0;
    double BadThreshold = 0.0;
    double BadFraction = 0.0;
    if (!HasExactJsonKeys(Object,
            {"meanMax", "p95Max", "maximumMax", "badPixelThreshold",
             "badPixelFractionMax"}) ||
        !ReadJsonNumber(Object, "meanMax", Mean) ||
        !ReadJsonNumber(Object, "p95Max", P95) ||
        !ReadJsonNumber(Object, "maximumMax", Maximum) ||
        !ReadJsonNumber(Object, "badPixelThreshold", BadThreshold) ||
        !ReadJsonNumber(Object, "badPixelFractionMax", BadFraction) ||
        Mean < 0.0 || Mean > 1.0 || P95 < 0.0 || P95 > 1.0 ||
        Maximum < 0.0 || Maximum > 1.0 || BadThreshold <= 0.0 ||
        BadThreshold > 1.0 || BadFraction < 0.0 || BadFraction > 1.0)
        return false;
    Out = {static_cast<float>(Mean), static_cast<float>(P95),
        static_cast<float>(Maximum), static_cast<float>(BadThreshold),
        static_cast<float>(BadFraction)};
    return true;
}

bool ParseBaselineV3Acceptance(
    yyjson_val* Value,
    std::optional<FOutputTransformSdrAcceptanceV3>& Out)
{
    Out.reset();
    if (yyjson_is_null(Value)) return true;
    FOutputTransformSdrAcceptanceV3 Acceptance;
    if (!HasExactJsonKeys(Value,
            {"maintainerId", "reviewedAt", "candidateSha256", "decision"}) ||
        !ReadJsonString(Value, "maintainerId", Acceptance.MaintainerId) ||
        !ReadJsonString(Value, "reviewedAt", Acceptance.ReviewedAt) ||
        !ReadJsonString(Value, "candidateSha256", Acceptance.CandidateSha256) ||
        !ReadJsonString(Value, "decision", Acceptance.Decision) ||
        !IsBaselineV3Token(Acceptance.MaintainerId) ||
        Acceptance.ReviewedAt.Len() > 64 ||
        !IsBaselineV3Digest(Acceptance.CandidateSha256) ||
        Acceptance.Decision != FString("accepted"))
        return false;
    Out = std::move(Acceptance);
    return true;
}

bool ParseBaselineV3Record(yyjson_val* Value,
    FOutputTransformSdrBaselineV3& Out)
{
    FString Schema;
    uint32 SchemaVersion = 0;
    if (!HasExactJsonKeys(Value,
            {"schema", "schemaVersion", "baselineId", "state",
             "workloadRevision", "backend", "deviceClass",
             "capabilityDigest", "outputDeviceProfileId", "transformVersion",
             "exposureStops", "settingsDigest", "width", "height",
             "sampleCount", "referencePath", "compressedSha256",
             "decodedSha256", "calibrationEvidenceSha256", "flipPolicy",
             "acceptance"}) ||
        !ReadJsonString(Value, "schema", Schema) ||
        Schema != FString("stoner.sdr-image-baseline") ||
        !ReadJsonUInt(Value, "schemaVersion", SchemaVersion) ||
        SchemaVersion != 3 ||
        !ReadJsonString(Value, "baselineId", Out.BaselineId) ||
        !ReadJsonString(Value, "state", Out.State) ||
        !ReadJsonString(Value, "workloadRevision", Out.WorkloadRevision) ||
        !ReadJsonString(Value, "backend", Out.Backend) ||
        !ReadJsonString(Value, "deviceClass", Out.DeviceClass) ||
        !ReadJsonString(Value, "capabilityDigest", Out.CapabilityDigest) ||
        !ReadJsonString(Value, "outputDeviceProfileId",
            Out.OutputDeviceProfileId) ||
        !ReadJsonString(Value, "transformVersion", Out.TransformVersion) ||
        !ReadJsonNumber(Value, "exposureStops", Out.ExposureStops) ||
        !ReadJsonString(Value, "settingsDigest", Out.SettingsDigest) ||
        !ReadJsonUInt(Value, "width", Out.Width) ||
        !ReadJsonUInt(Value, "height", Out.Height) ||
        !ReadJsonUInt(Value, "sampleCount", Out.SampleCount) ||
        !ReadJsonString(Value, "referencePath", Out.ReferencePath) ||
        !ReadJsonString(Value, "compressedSha256", Out.CompressedSha256) ||
        !ReadJsonString(Value, "decodedSha256", Out.DecodedSha256) ||
        !ReadJsonString(Value, "calibrationEvidenceSha256",
            Out.CalibrationEvidenceSha256) ||
        !ParseBaselineV3Policy(yyjson_obj_get(Value, "flipPolicy"),
            Out.FlipPolicy) ||
        !ParseBaselineV3Acceptance(yyjson_obj_get(Value, "acceptance"),
            Out.Acceptance))
        return false;

    static const std::set<std::string> States = {
        "candidate", "calibrated", "reviewed", "accepted", "superseded"};
    static const std::set<std::string> Profiles = {
        "Sdr.sRGB.v1", "Sdr.BT709.v1", "Sdr.ExplicitGamma22.v1"};
    static const std::set<std::string> Transforms = {
        "Sdr.KhronosPbrNeutral.v1", "Sdr.NarkowiczAcesFit.v1",
        "Sdr.ExtendedReinhardRec709.v1"};
    if (!IsBaselineV3Token(Out.BaselineId) ||
        States.find(Out.State.ToStdString()) == States.end() ||
        !IsFreshV3Workload(Out.WorkloadRevision) ||
        (Out.Backend != FString("vulkan") && Out.Backend != FString("metal")) ||
        !IsBaselineV3Token(Out.DeviceClass) ||
        !IsBaselineV3Digest(Out.CapabilityDigest) ||
        Profiles.find(Out.OutputDeviceProfileId.ToStdString()) == Profiles.end() ||
        Transforms.find(Out.TransformVersion.ToStdString()) == Transforms.end() ||
        Out.ExposureStops < -16.0 || Out.ExposureStops > 16.0 ||
        !IsBaselineV3Digest(Out.SettingsDigest) || Out.Width != 512 ||
        Out.Height != 512 || Out.SampleCount != 1 ||
        !IsSafePngPath(Out.ReferencePath) ||
        !IsBaselineV3Digest(Out.CompressedSha256) ||
        !IsBaselineV3Digest(Out.DecodedSha256) ||
        !IsBaselineV3Digest(Out.CalibrationEvidenceSha256))
        return false;
    if (Out.Acceptance.has_value() &&
        Out.Acceptance->CandidateSha256 != Out.CompressedSha256)
        return false;
    if (Out.State == FString("accepted"))
        return Out.Acceptance.has_value();
    if (Out.State == FString("candidate") ||
        Out.State == FString("calibrated") || Out.State == FString("reviewed"))
        return !Out.Acceptance.has_value();
    return true;
}

bool HasSameBaselineV3AuthorityKey(
    const FOutputTransformSdrBaselineV3& Left,
    const FOutputTransformSdrBaselineV3& Right)
{
    return Left.WorkloadRevision == Right.WorkloadRevision &&
        Left.Backend == Right.Backend && Left.DeviceClass == Right.DeviceClass &&
        Left.OutputDeviceProfileId == Right.OutputDeviceProfileId &&
        Left.TransformVersion == Right.TransformVersion &&
        Left.ExposureStops == Right.ExposureStops &&
        Left.SettingsDigest == Right.SettingsDigest;
}

} // namespace

bool FOutputTransformSdrBaselineRegistryV3::LoadRegistry(
    const Stoner::Core::FString& Path,
    Stoner::Core::FString& OutFailure)
{
    using namespace Stoner::Core;
    Records.clear();
    OutFailure = {};
    std::ifstream Input(Path.ToStdString(), std::ios::binary);
    if (!Input)
    {
        OutFailure = "sdr-v3-registry-json";
        return false;
    }
    std::string Bytes{std::istreambuf_iterator<char>(Input), {}};
    if (Bytes.size() > 1024u * 1024u)
    {
        OutFailure = "sdr-v3-registry-bounds";
        return false;
    }
    yyjson_read_err Error{};
    yyjson_doc* Document = yyjson_read_opts(
        Bytes.data(), Bytes.size(), YYJSON_READ_NOFLAG, nullptr, &Error);
    if (!Document)
    {
        OutFailure = "sdr-v3-registry-json";
        return false;
    }
    yyjson_val* Root = yyjson_doc_get_root(Document);
    FString Schema;
    FString RegistryId;
    uint32 Version = 0;
    yyjson_val* Values = yyjson_obj_get(Root, "records");
    const bool bHeaderValid = HasExactJsonKeys(
            Root, {"schema", "schemaVersion", "registryId", "records"}) &&
        ReadJsonString(Root, "schema", Schema) &&
        Schema == FString("stoner.sdr-image-baseline-registry") &&
        ReadJsonUInt(Root, "schemaVersion", Version) && Version == 3 &&
        ReadJsonString(Root, "registryId", RegistryId) &&
        RegistryId == FString("output-transform-sdr-baselines-v3") &&
        yyjson_is_arr(Values) && yyjson_arr_size(Values) <= 64;
    if (!bHeaderValid)
    {
        yyjson_doc_free(Document);
        OutFailure = "sdr-v3-registry-schema";
        return false;
    }
    std::set<std::string> BaselineIds;
    size_t Index = 0;
    size_t Maximum = 0;
    yyjson_val* Value = nullptr;
    yyjson_arr_foreach(Values, Index, Maximum, Value)
    {
        FOutputTransformSdrBaselineV3 Record;
        if (!ParseBaselineV3Record(Value, Record) ||
            !BaselineIds.insert(Record.BaselineId.ToStdString()).second ||
            std::any_of(Records.begin(), Records.end(), [&](const auto& Existing) {
                return HasSameBaselineV3AuthorityKey(Existing, Record);
            }))
        {
            yyjson_doc_free(Document);
            Records.clear();
            OutFailure = "sdr-v3-record-schema-or-identity";
            return false;
        }
        Records.push_back(std::move(Record));
    }
    yyjson_doc_free(Document);
    return true;
}

bool FOutputTransformSdrBaselineRegistryV3::SelectAccepted(
    const Stoner::Core::FString& WorkloadRevision,
    const Stoner::Core::FString& Backend,
    const Stoner::Core::FString& DeviceClass,
    const Stoner::Core::FString& OutputDeviceProfileId,
    const Stoner::Core::FString& TransformVersion,
    double ExposureStops,
    const Stoner::Core::FString& SettingsDigest,
    FOutputTransformSdrBaselineV3& OutBaseline,
    Stoner::Core::FString& OutFailure) const
{
    using namespace Stoner::Core;
    OutBaseline = {};
    OutFailure = {};
    if (!IsFreshV3Workload(WorkloadRevision) ||
        !IsBaselineV3Token(DeviceClass) ||
        !std::isfinite(ExposureStops) || !IsBaselineV3Digest(SettingsDigest))
    {
        OutFailure = "sdr-v3-selection-contract";
        return false;
    }
    const FOutputTransformSdrBaselineV3* Match = nullptr;
    bool bFoundNonAccepted = false;
    for (const auto& Candidate : Records)
    {
        if (Candidate.WorkloadRevision != WorkloadRevision ||
            Candidate.Backend != Backend || Candidate.DeviceClass != DeviceClass ||
            Candidate.OutputDeviceProfileId != OutputDeviceProfileId ||
            Candidate.TransformVersion != TransformVersion ||
            Candidate.ExposureStops != ExposureStops ||
            Candidate.SettingsDigest != SettingsDigest)
            continue;
        if (Candidate.State != FString("accepted"))
        {
            bFoundNonAccepted = true;
            continue;
        }
        Match = &Candidate;
    }
    if (!Match)
    {
        OutFailure = bFoundNonAccepted ? FString("sdr-v3-state-not-accepted")
                                      : FString("sdr-v3-baseline-missing");
        return false;
    }
    OutBaseline = *Match;
    return true;
}

bool FOutputTransformSdrBaselineRegistryV3::IsAllowedStateTransition(
    const Stoner::Core::FString& From,
    const Stoner::Core::FString& To) noexcept
{
    return (From == Stoner::Core::FString("candidate") &&
            To == Stoner::Core::FString("calibrated")) ||
        (From == Stoner::Core::FString("calibrated") &&
         To == Stoner::Core::FString("reviewed")) ||
        (From == Stoner::Core::FString("reviewed") &&
         To == Stoner::Core::FString("accepted")) ||
        (From == Stoner::Core::FString("accepted") &&
         To == Stoner::Core::FString("superseded"));
}

bool FProductionCanonicalImage::IsValid() const noexcept
{
    if (Width == 0 || Height == 0 ||
        LinearRgb.size() != static_cast<Stoner::Core::usize>(Width) * Height * 3u)
        return false;
    return std::all_of(LinearRgb.begin(), LinearRgb.end(), IsUnit);
}

bool FProductionPixelRegion::IsValid(
    Stoner::Core::uint32 Width,
    Stoner::Core::uint32 Height) const noexcept
{
    return MinimumX < MaximumXExclusive &&
        MinimumY < MaximumYExclusive &&
        MaximumXExclusive <= Width && MaximumYExclusive <= Height;
}

bool NormalizeProductionReadback(
    const FProductionReadbackView& Source,
    FProductionCanonicalImage& OutImage,
    Stoner::Core::FString& OutFailure)
{
    using namespace Stoner::Core;
    OutImage = {};
    OutFailure = {};
    const uint32 PixelBytes = BytesPerPixel(Source.Format);
    const uint64 TightPitch = static_cast<uint64>(Source.Width) * PixelBytes;
    const uint64 RequiredBytes = static_cast<uint64>(Source.RowPitchBytes) *
        Source.Height;
    if (Source.Width == 0 || Source.Height == 0 || PixelBytes == 0 ||
        Source.RowPitchBytes < TightPitch || RequiredBytes > Source.Bytes.size())
    {
        OutFailure = "invalid-readback-layout";
        return false;
    }

    FProductionCanonicalImage Candidate;
    Candidate.Width = Source.Width;
    Candidate.Height = Source.Height;
    try
    {
        Candidate.LinearRgb.resize(
            static_cast<usize>(Source.Width) * Source.Height * 3u);
    }
    catch (const std::bad_alloc&)
    {
        OutFailure = "image-allocation";
        return false;
    }

    for (uint32 Y = 0; Y < Source.Height; ++Y)
    {
        const uint32 SourceY = Source.Origin == EProductionImageOrigin::TopLeft
            ? Y : Source.Height - 1u - Y;
        const uint8* Row = Source.Bytes.data() +
            static_cast<usize>(SourceY) * Source.RowPitchBytes;
        for (uint32 X = 0; X < Source.Width; ++X)
        {
            float R = 0.0f;
            float G = 0.0f;
            float B = 0.0f;
            if (!ReadPixel(Row + static_cast<usize>(X) * PixelBytes,
                    Source.Format, R, G, B) ||
                !std::isfinite(R) || !std::isfinite(G) || !std::isfinite(B) ||
                R < 0.0f || G < 0.0f || B < 0.0f ||
                R > 1.0f || G > 1.0f || B > 1.0f)
            {
                OutFailure = "non-finite-or-out-of-range-pixel";
                return false;
            }
            if (Source.Transfer == EProductionColorTransfer::SRGB)
            {
                R = SrgbToLinear(R);
                G = SrgbToLinear(G);
                B = SrgbToLinear(B);
            }
            const usize Destination =
                (static_cast<usize>(Y) * Source.Width + X) * 3u;
            Candidate.LinearRgb[Destination] = R;
            Candidate.LinearRgb[Destination + 1u] = G;
            Candidate.LinearRgb[Destination + 2u] = B;
        }
    }
    OutImage = std::move(Candidate);
    return true;
}

bool NormalizeProductionSignedNormalReadback(
    const FProductionReadbackView& Source,
    FProductionCanonicalImage& OutImage,
    Stoner::Core::FString& OutFailure)
{
    using namespace Stoner::Core;
    OutImage = {};
    OutFailure = {};
    const uint32 PixelBytes = BytesPerPixel(Source.Format);
    const uint64 TightPitch = static_cast<uint64>(Source.Width) * PixelBytes;
    const uint64 RequiredBytes = static_cast<uint64>(Source.RowPitchBytes) *
        Source.Height;
    if ((Source.Format != EProductionReadbackPixelFormat::RGBA16Float &&
         Source.Format != EProductionReadbackPixelFormat::RGBA32Float) ||
        Source.Transfer != EProductionColorTransfer::Linear ||
        Source.Width == 0 || Source.Height == 0 ||
        Source.RowPitchBytes < TightPitch || RequiredBytes > Source.Bytes.size())
    {
        OutFailure = "invalid-normal-readback-layout";
        return false;
    }
    FProductionCanonicalImage Candidate;
    Candidate.Width = Source.Width;
    Candidate.Height = Source.Height;
    try
    {
        Candidate.LinearRgb.resize(
            static_cast<usize>(Source.Width) * Source.Height * 3u);
    }
    catch (const std::bad_alloc&)
    {
        OutFailure = "image-allocation";
        return false;
    }
    for (uint32 Y = 0; Y < Source.Height; ++Y)
    {
        const uint32 SourceY = Source.Origin == EProductionImageOrigin::TopLeft
            ? Y : Source.Height - 1u - Y;
        const uint8* Row = Source.Bytes.data() +
            static_cast<usize>(SourceY) * Source.RowPitchBytes;
        for (uint32 X = 0; X < Source.Width; ++X)
        {
            float NormalX = 0.0f;
            float NormalY = 0.0f;
            float NormalZ = 0.0f;
            if (!ReadPixel(Row + static_cast<usize>(X) * PixelBytes,
                    Source.Format, NormalX, NormalY, NormalZ) ||
                !std::isfinite(NormalX) || !std::isfinite(NormalY) ||
                !std::isfinite(NormalZ) || NormalX < -1.0f ||
                NormalX > 1.0f || NormalY < -1.0f || NormalY > 1.0f ||
                NormalZ < -1.0f || NormalZ > 1.0f)
            {
                OutFailure = "invalid-signed-normal";
                return false;
            }
            const usize Destination =
                (static_cast<usize>(Y) * Source.Width + X) * 3u;
            Candidate.LinearRgb[Destination] = NormalX * 0.5f + 0.5f;
            Candidate.LinearRgb[Destination + 1u] = NormalY * 0.5f + 0.5f;
            Candidate.LinearRgb[Destination + 2u] = NormalZ * 0.5f + 0.5f;
        }
    }
    OutImage = std::move(Candidate);
    return true;
}

bool SampleProductionReadbackPixel(
    const FProductionReadbackView& Source,
    Stoner::Core::uint32 X,
    Stoner::Core::uint32 Y,
    Stoner::Core::FVector3& OutValue,
    Stoner::Core::FString& OutFailure)
{
    using namespace Stoner::Core;
    OutValue = {};
    OutFailure = {};
    const uint32 PixelBytes = BytesPerPixel(Source.Format);
    const uint64 TightPitch = static_cast<uint64>(Source.Width) * PixelBytes;
    const uint64 RequiredBytes = static_cast<uint64>(Source.RowPitchBytes) *
        Source.Height;
    if (Source.Width == 0 || Source.Height == 0 || X >= Source.Width ||
        Y >= Source.Height || PixelBytes == 0 ||
        Source.RowPitchBytes < TightPitch || RequiredBytes > Source.Bytes.size())
    {
        OutFailure = "sample-readback-layout";
        return false;
    }
    const uint32 SourceY = Source.Origin == EProductionImageOrigin::TopLeft
        ? Y : Source.Height - 1u - Y;
    const uint8* Pixel = Source.Bytes.data() +
        static_cast<usize>(SourceY) * Source.RowPitchBytes +
        static_cast<usize>(X) * PixelBytes;
    if (!ReadPixel(Pixel, Source.Format, OutValue.X, OutValue.Y, OutValue.Z) ||
        !std::isfinite(OutValue.X) || !std::isfinite(OutValue.Y) ||
        !std::isfinite(OutValue.Z))
    {
        OutFailure = "sample-non-finite";
        return false;
    }
    if (Source.Transfer == EProductionColorTransfer::SRGB)
    {
        if (!IsUnit(OutValue.X) || !IsUnit(OutValue.Y) || !IsUnit(OutValue.Z))
        {
            OutFailure = "sample-srgb-range";
            return false;
        }
        OutValue.X = SrgbToLinear(OutValue.X);
        OutValue.Y = SrgbToLinear(OutValue.Y);
        OutValue.Z = SrgbToLinear(OutValue.Z);
    }
    return true;
}

bool SampleProductionReadbackRegion(
    const FProductionReadbackView& Source,
    const FProductionPixelRegion& Region,
    float Quantile,
    FProductionReadbackRegionSample& OutSample,
    Stoner::Core::FString& OutFailure)
{
    using namespace Stoner::Core;
    OutSample = {};
    OutFailure = {};
    const uint32 PixelBytes = BytesPerPixel(Source.Format);
    const uint64 TightPitch = static_cast<uint64>(Source.Width) * PixelBytes;
    const uint64 RequiredBytes = static_cast<uint64>(Source.RowPitchBytes) *
        Source.Height;
    if (!Region.IsValid(Source.Width, Source.Height) || PixelBytes == 0 ||
        Source.RowPitchBytes < TightPitch || RequiredBytes > Source.Bytes.size() ||
        !std::isfinite(Quantile) || Quantile < 0.0f || Quantile > 1.0f)
    {
        OutFailure = "sample-region-contract";
        return false;
    }

    const usize SampleCount =
        static_cast<usize>(Region.MaximumXExclusive - Region.MinimumX) *
        (Region.MaximumYExclusive - Region.MinimumY);
    TArray<float> R;
    TArray<float> G;
    TArray<float> B;
    try
    {
        R.reserve(SampleCount);
        G.reserve(SampleCount);
        B.reserve(SampleCount);
    }
    catch (const std::bad_alloc&)
    {
        OutFailure = "sample-region-allocation";
        return false;
    }
    for (uint32 Y = Region.MinimumY; Y < Region.MaximumYExclusive; ++Y)
    {
        const uint32 SourceY = Source.Origin == EProductionImageOrigin::TopLeft
            ? Y : Source.Height - 1u - Y;
        const uint8* Row = Source.Bytes.data() +
            static_cast<usize>(SourceY) * Source.RowPitchBytes;
        for (uint32 X = Region.MinimumX; X < Region.MaximumXExclusive; ++X)
        {
            float Red = 0.0f;
            float Green = 0.0f;
            float Blue = 0.0f;
            if (!ReadPixel(Row + static_cast<usize>(X) * PixelBytes,
                    Source.Format, Red, Green, Blue) ||
                !std::isfinite(Red) || !std::isfinite(Green) ||
                !std::isfinite(Blue))
                continue;
            if (Source.Transfer == EProductionColorTransfer::SRGB)
            {
                if (!IsUnit(Red) || !IsUnit(Green) || !IsUnit(Blue))
                    continue;
                Red = SrgbToLinear(Red);
                Green = SrgbToLinear(Green);
                Blue = SrgbToLinear(Blue);
            }
            R.push_back(Red);
            G.push_back(Green);
            B.push_back(Blue);
        }
    }
    if (R.empty())
    {
        OutFailure = "sample-region-no-valid-samples";
        return false;
    }
    std::sort(R.begin(), R.end());
    std::sort(G.begin(), G.end());
    std::sort(B.begin(), B.end());
    const usize Index = static_cast<usize>(
        Quantile * static_cast<float>(R.size() - 1u));
    OutSample.Value = {R[Index], G[Index], B[Index]};
    OutSample.ValidSampleFraction = static_cast<float>(R.size()) /
        static_cast<float>(SampleCount);
    return true;
}

bool MeasureProductionReadbackDirectionalCoverage(
    const FProductionReadbackView& Source,
    const FProductionPixelRegion& Region,
    const Stoner::Core::FVector3& ExpectedDirection,
    float MinimumDot,
    float& OutCoverage,
    Stoner::Core::FString& OutFailure)
{
    using namespace Stoner::Core;
    OutCoverage = 0.0f;
    OutFailure = {};
    const float ExpectedLength = std::sqrt(
        ExpectedDirection.X * ExpectedDirection.X +
        ExpectedDirection.Y * ExpectedDirection.Y +
        ExpectedDirection.Z * ExpectedDirection.Z);
    const uint32 PixelBytes = BytesPerPixel(Source.Format);
    const uint64 TightPitch = static_cast<uint64>(Source.Width) * PixelBytes;
    const uint64 RequiredBytes = static_cast<uint64>(Source.RowPitchBytes) *
        Source.Height;
    if (!Region.IsValid(Source.Width, Source.Height) || PixelBytes == 0 ||
        Source.RowPitchBytes < TightPitch || RequiredBytes > Source.Bytes.size() ||
        !std::isfinite(ExpectedLength) || ExpectedLength <= 0.0f ||
        !std::isfinite(MinimumDot) || MinimumDot < -1.0f || MinimumDot > 1.0f)
    {
        OutFailure = "direction-region-contract";
        return false;
    }
    const FVector3 Expected = ExpectedDirection * (1.0f / ExpectedLength);
    usize Matched = 0;
    usize Valid = 0;
    const usize SampleCount =
        static_cast<usize>(Region.MaximumXExclusive - Region.MinimumX) *
        (Region.MaximumYExclusive - Region.MinimumY);
    for (uint32 Y = Region.MinimumY; Y < Region.MaximumYExclusive; ++Y)
    {
        const uint32 SourceY = Source.Origin == EProductionImageOrigin::TopLeft
            ? Y : Source.Height - 1u - Y;
        const uint8* Row = Source.Bytes.data() +
            static_cast<usize>(SourceY) * Source.RowPitchBytes;
        for (uint32 X = Region.MinimumX; X < Region.MaximumXExclusive; ++X)
        {
            FVector3 Direction;
            if (!ReadPixel(Row + static_cast<usize>(X) * PixelBytes,
                    Source.Format, Direction.X, Direction.Y, Direction.Z) ||
                !std::isfinite(Direction.X) || !std::isfinite(Direction.Y) ||
                !std::isfinite(Direction.Z))
                continue;
            const float Length = std::sqrt(Direction.X * Direction.X +
                Direction.Y * Direction.Y + Direction.Z * Direction.Z);
            if (!std::isfinite(Length) || Length < 0.8f || Length > 1.2f)
                continue;
            ++Valid;
            Direction = Direction * (1.0f / Length);
            const float Dot = Direction.X * Expected.X +
                Direction.Y * Expected.Y + Direction.Z * Expected.Z;
            if (Dot >= MinimumDot) ++Matched;
        }
    }
    if (Valid == 0)
    {
        OutFailure = "direction-region-no-valid-samples";
        return false;
    }
    OutCoverage = static_cast<float>(Matched) /
        static_cast<float>(SampleCount);
    return true;
}

FProductionSemanticProbeResult RunProductionSemanticProbes(
    const FProductionSemanticProbeRequest& Request)
{
    FProductionSemanticProbeResult Result;
    const auto Fail = [&Result](const Stoner::Core::FString& Reason) {
        Result.FirstFailure = Reason;
        return Result;
    };
    const auto Pass = [&Result](const Stoner::Core::FString& ProbeId) {
        Result.PassedProbeIds.push_back(ProbeId);
        Result.PassedProbeCount = static_cast<Stoner::Core::uint32>(
            Result.PassedProbeIds.size());
    };
    if (!Request.Color || !Request.Color->IsValid())
        return Fail("color-image");
    Pass("color-image");
    if (Request.ExpectedFrameToken == 0 ||
        Request.ExpectedFrameToken != Request.ObservedFrameToken)
        return Fail("current-frame");
    Pass("current-frame");

    const auto& Pixels = Request.Color->LinearRgb;
    Stoner::Core::usize Covered = 0;
    float Minimum = 1.0f;
    float Maximum = 0.0f;
    for (Stoner::Core::usize Index = 0; Index < Pixels.size(); Index += 3u)
    {
        const float Luminance = 0.2126f * Pixels[Index] +
            0.7152f * Pixels[Index + 1u] + 0.0722f * Pixels[Index + 2u];
        Minimum = std::min(Minimum, Luminance);
        Maximum = std::max(Maximum, Luminance);
        if (Luminance > 1.0f / 255.0f) ++Covered;
    }
    if (Maximum - Minimum <= 1.0f / 255.0f)
        return Fail("nonblank");
    Pass("nonblank");
    const float Coverage = static_cast<float>(Covered) /
        static_cast<float>(Request.Color->Width * Request.Color->Height);
    if (!std::isfinite(Coverage) ||
        Coverage < Request.MinimumCoverageFraction ||
        Coverage > Request.MaximumCoverageFraction)
        return Fail("coverage");
    Pass("coverage");

    for (const auto& Probe : Request.Regions)
    {
        if (Probe.Name.IsEmpty() ||
            !Probe.Region.IsValid(Request.Color->Width, Request.Color->Height) ||
            !std::isfinite(Probe.Tolerance) || Probe.Tolerance < 0.0f ||
            !std::isfinite(Probe.MinimumValidSampleFraction) ||
            Probe.MinimumValidSampleFraction <= 0.0f ||
            Probe.MinimumValidSampleFraction > 1.0f ||
            !std::isfinite(Probe.Quantile) || Probe.Quantile < 0.0f ||
            Probe.Quantile > 1.0f)
            return Fail("region-contract");
        Stoner::Core::TArray<float> Red;
        Stoner::Core::TArray<float> Green;
        Stoner::Core::TArray<float> Blue;
        Stoner::Core::usize Matching = 0;
        const Stoner::Core::usize SampleCount = static_cast<Stoner::Core::usize>(
            Probe.Region.MaximumXExclusive - Probe.Region.MinimumX) *
            (Probe.Region.MaximumYExclusive - Probe.Region.MinimumY);
        Red.reserve(SampleCount);
        Green.reserve(SampleCount);
        Blue.reserve(SampleCount);
        for (Stoner::Core::uint32 Y = Probe.Region.MinimumY;
             Y < Probe.Region.MaximumYExclusive; ++Y)
        {
            for (Stoner::Core::uint32 X = Probe.Region.MinimumX;
                 X < Probe.Region.MaximumXExclusive; ++X)
            {
                const Stoner::Core::usize Pixel =
                    (static_cast<Stoner::Core::usize>(Y) *
                        Request.Color->Width + X) * 3u;
                const float R = Pixels[Pixel];
                const float G = Pixels[Pixel + 1u];
                const float B = Pixels[Pixel + 2u];
                Red.push_back(R);
                Green.push_back(G);
                Blue.push_back(B);
                if (std::abs(R - Probe.Expected.X) <= Probe.Tolerance &&
                    std::abs(G - Probe.Expected.Y) <= Probe.Tolerance &&
                    std::abs(B - Probe.Expected.Z) <= Probe.Tolerance)
                    ++Matching;
            }
        }
        std::sort(Red.begin(), Red.end());
        std::sort(Green.begin(), Green.end());
        std::sort(Blue.begin(), Blue.end());
        const float Quantile = Probe.Statistic == EProductionRegionStatistic::Median
            ? 0.5f : Probe.Quantile;
        const Stoner::Core::usize StatisticIndex =
            static_cast<Stoner::Core::usize>(
                Quantile * static_cast<float>(SampleCount - 1u));
        const float ValidFraction = static_cast<float>(Matching) /
            static_cast<float>(SampleCount);
        if (ValidFraction < Probe.MinimumValidSampleFraction ||
            std::abs(Red[StatisticIndex] - Probe.Expected.X) > Probe.Tolerance ||
            std::abs(Green[StatisticIndex] - Probe.Expected.Y) > Probe.Tolerance ||
            std::abs(Blue[StatisticIndex] - Probe.Expected.Z) > Probe.Tolerance)
            return Fail(Stoner::Core::FString(
                std::string("region-") + Probe.Name.ToStdString()));
        Pass(Stoner::Core::FString(
            std::string("region-") + Probe.Name.ToStdString()));
    }
    static const Stoner::Core::TArray<Stoner::Core::FString> MandatoryRegions = {
        "orientation", "primitive-material", "base-color", "normal-response",
        "metallic-roughness", "emissive"};
    Stoner::Core::TArray<Stoner::Core::FString> RequiredRegions = MandatoryRegions;
    RequiredRegions.insert(RequiredRegions.end(),
        Request.RequiredRegionNames.begin(), Request.RequiredRegionNames.end());
    for (const auto& Required : RequiredRegions)
    {
        const auto Found = std::find_if(
            Request.Regions.begin(), Request.Regions.end(),
            [&Required](const FProductionRegionProbe& Probe) {
                return Probe.Name == Required;
            });
        if (Required.IsEmpty() || Found == Request.Regions.end())
            return Fail(Stoner::Core::FString(
                std::string("missing-region-") + Required.ToStdString()));
    }
    if (!Request.Normal)
        return Fail("normal-attachment");
    if (Request.Normal)
    {
        if (!Request.Normal->IsValid() ||
            Request.Normal->Width != Request.Color->Width ||
            Request.Normal->Height != Request.Color->Height)
            return Fail("normal-attachment");
        bool bObservedUnitNormal = false;
        for (Stoner::Core::usize Index = 0;
             Index < Request.Normal->LinearRgb.size(); Index += 3u)
        {
            const float X = Request.Normal->LinearRgb[Index] * 2.0f - 1.0f;
            const float Y = Request.Normal->LinearRgb[Index + 1u] * 2.0f - 1.0f;
            const float Z = Request.Normal->LinearRgb[Index + 2u] * 2.0f - 1.0f;
            const float Length = std::sqrt(X * X + Y * Y + Z * Z);
            bObservedUnitNormal = bObservedUnitNormal ||
                std::abs(Length - 1.0f) <= 0.1f;
        }
        if (!bObservedUnitNormal) return Fail("normal-semantic");
        Pass("normal-semantic");
    }
    if (!Request.Depth)
        return Fail("depth-attachment");
    if (Request.Depth)
    {
        if (!Request.Depth->IsValid() ||
            Request.Depth->Width != Request.Color->Width ||
            Request.Depth->Height != Request.Color->Height)
            return Fail("depth-attachment");
        const auto [MinimumDepth, MaximumDepth] = std::minmax_element(
            Request.Depth->LinearRgb.begin(), Request.Depth->LinearRgb.end());
        if (MaximumDepth == Request.Depth->LinearRgb.end() ||
            *MaximumDepth - *MinimumDepth <= 1.0f / 65535.0f)
            return Fail("depth-semantic");
        Pass("depth-semantic");
    }
    Result.bPassed = true;
    return Result;
}

FProductionFlipResult CompareProductionImagesWithFlip(
    const FProductionCanonicalImage& Reference,
    const FProductionCanonicalImage& Candidate,
    const FProductionFlipPolicy& Policy)
{
    FProductionFlipResult Result;
    if (!Reference.IsValid() || !Candidate.IsValid() ||
        Reference.Width != Candidate.Width ||
        Reference.Height != Candidate.Height)
    {
        Result.FailureReason = "image-contract";
        return Result;
    }
    if (!IsUnit(Policy.MeanMax) || !IsUnit(Policy.P95Max) ||
        !IsUnit(Policy.MaximumMax) || !IsUnit(Policy.BadPixelThreshold) ||
        Policy.BadPixelThreshold <= 0.0f ||
        !IsUnit(Policy.BadPixelFractionMax))
    {
        Result.FailureReason = "flip-policy";
        return Result;
    }

    FLIP::Parameters Parameters;
    float Mean = 0.0f;
    float* RawErrors = nullptr;
    FLIP::evaluate(
        Reference.LinearRgb.data(), Candidate.LinearRgb.data(),
        static_cast<int>(Reference.Width), static_cast<int>(Reference.Height),
        false, Parameters, false, true, Mean, &RawErrors);
    std::unique_ptr<float[]> Errors(RawErrors);
    if (!Errors)
    {
        Result.FailureReason = "flip-evaluation";
        return Result;
    }
    const Stoner::Core::usize Count =
        static_cast<Stoner::Core::usize>(Reference.Width) * Reference.Height;
    Stoner::Core::TArray<float> Ordered(Errors.get(), Errors.get() + Count);
    if (!std::all_of(Ordered.begin(), Ordered.end(), IsUnit))
    {
        Result.FailureReason = "flip-non-finite";
        return Result;
    }
    std::sort(Ordered.begin(), Ordered.end());
    const Stoner::Core::usize P95Index = Count == 0 ? 0 :
        static_cast<Stoner::Core::usize>(std::ceil(Count * 0.95)) - 1u;
    const auto Bad = std::count_if(Ordered.begin(), Ordered.end(),
        [&Policy](float Value) { return Value > Policy.BadPixelThreshold; });
    Result.bMeasured = true;
    Result.Mean = Mean;
    Result.P95 = Ordered[P95Index];
    Result.Maximum = Ordered.back();
    Result.BadPixelFraction = static_cast<float>(Bad) /
        static_cast<float>(Count);
    Result.bPassed = Result.Mean <= Policy.MeanMax &&
        Result.P95 <= Policy.P95Max &&
        Result.Maximum <= Policy.MaximumMax &&
        Result.BadPixelFraction <= Policy.BadPixelFractionMax;
    if (!Result.bPassed) Result.FailureReason = "flip-threshold";
    return Result;
}

bool ValidateProductionNativeImageEvidence(
    const FProductionNativeImageEvidence& Evidence,
    Stoner::Core::FString& OutFailure)
{
    OutFailure = {};
    const auto Fail = [&OutFailure](const char* Reason) {
        OutFailure = Reason;
        return false;
    };
    if (Evidence.RequestedBackend.IsEmpty() ||
        Evidence.ExecutedBackend != Evidence.RequestedBackend)
        return Fail("backend-substitution");
    if (Evidence.RuntimeMode != Stoner::Core::FString("native") ||
        !Evidence.bNativeExecution)
        return Fail("native-execution");
    if (!Evidence.bSubmissionCompleted || !Evidence.bGpuReadback)
        return Fail("gpu-completion");
    if (Evidence.WorkloadRevision.IsEmpty() ||
        Evidence.WorkloadRevision != Evidence.BaselineWorkloadRevision)
        return Fail("workload-revision");
    if (Evidence.bPresented && !Evidence.bWindowOnlyCapture)
        return Fail("window-only-capture");
    return true;
}
