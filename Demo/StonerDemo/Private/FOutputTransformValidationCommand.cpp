#include "FOutputTransformValidationCommand.h"

#include "RHI/ERHIPresentationColorSpace.h"

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string_view>

namespace Stoner::Demo
{
namespace
{

bool IsToken(std::string_view Value) noexcept
{
    if (Value.empty() || Value.size() > 128) return false;
    const auto Allowed = [](char Character) noexcept
    {
        return (Character >= 'a' && Character <= 'z') ||
            (Character >= 'A' && Character <= 'Z') ||
            (Character >= '0' && Character <= '9') ||
            Character == '.' || Character == '_' || Character == '-';
    };
    if (!((Value.front() >= 'a' && Value.front() <= 'z') ||
          (Value.front() >= 'A' && Value.front() <= 'Z') ||
          (Value.front() >= '0' && Value.front() <= '9')))
        return false;
    for (const char Character : Value)
        if (!Allowed(Character)) return false;
    return true;
}

bool IsSha256(std::string_view Value) noexcept
{
    if (Value.size() != 64) return false;
    for (const char Character : Value)
        if (!((Character >= '0' && Character <= '9') ||
              (Character >= 'a' && Character <= 'f')))
            return false;
    return true;
}

const char* FormatName(RHI::ERHIFormat Format) noexcept
{
    switch (Format)
    {
    case RHI::ERHIFormat::B8G8R8A8_UNorm: return "bgra8-unorm";
    case RHI::ERHIFormat::R8G8B8A8_UNorm: return "rgba8-unorm";
    case RHI::ERHIFormat::R8G8B8A8_sRGB: return "rgba8-srgb";
    case RHI::ERHIFormat::R10G10B10A2_UNorm: return "bgr10a2-unorm";
    case RHI::ERHIFormat::R16G16B16A16_Float: return "rgba16-float";
    default: return "unknown";
    }
}

const char* DisplayAdaptationName(
    RHI::ERHIPresentationDisplayAdaptation Value) noexcept
{
    switch (Value)
    {
    case RHI::ERHIPresentationDisplayAdaptation::None: return "none";
    case RHI::ERHIPresentationDisplayAdaptation::SystemColorManagement:
        return "system-color-management";
    }
    return "none";
}

void SetReason(Core::FString* OutReason, const char* Reason)
{
    if (OutReason) *OutReason = Reason;
}

std::string JsonString(std::string_view Value)
{
    std::string Result;
    Result.reserve(Value.size() + 2);
    Result.push_back('"');
    constexpr char Hex[] = "0123456789abcdef";
    for (const unsigned char Character : Value)
    {
        switch (Character)
        {
        case '"': Result += "\\\""; break;
        case '\\': Result += "\\\\"; break;
        case '\b': Result += "\\b"; break;
        case '\f': Result += "\\f"; break;
        case '\n': Result += "\\n"; break;
        case '\r': Result += "\\r"; break;
        case '\t': Result += "\\t"; break;
        default:
            if (Character < 0x20)
            {
                Result += "\\u00";
                Result.push_back(Hex[Character >> 4]);
                Result.push_back(Hex[Character & 0x0f]);
            }
            else
                Result.push_back(static_cast<char>(Character));
            break;
        }
    }
    Result.push_back('"');
    return Result;
}

} // namespace

bool FOutputTransformValidationCommand::SerializeNormalizedNativeProbe(
    const FOutputTransformValidationProbeInput& Input,
    Core::FString& OutJson,
    Core::FString* OutReason)
{
    OutJson.Clear();
    if (OutReason) OutReason->Clear();
    const auto& Execution = Input.Execution;
    const auto& Frame = Execution.PresentationFrame;
    const auto& State = Execution.ResolvedPresentationState;
    const bool bKnownProfile =
        Input.ProfileKind == Core::FString("native-sdr") ||
        Input.ProfileKind == Core::FString("native-hdr-nonvisual") ||
        Input.ProfileKind == Core::FString("lifecycle") ||
        Input.ProfileKind == Core::FString("failure-injection");
    if (!IsToken(Input.HostPlatform.View()) ||
        !IsToken(Input.Backend.View()) || !bKnownProfile ||
        !IsToken(Input.WorkloadRevision.View()) ||
        !IsToken(Input.DeviceClass.View()) ||
        !IsSha256(Input.CapabilityDigest.View()) ||
        !IsToken(Input.OutputDeviceProfileId.View()) ||
        !IsToken(Input.TransformVersion.View()) ||
        !IsSha256(Input.InsertionDigest.View()) ||
        !std::isfinite(Input.ExposureStops) ||
        Input.Width == 0 || Input.Height == 0 ||
        Input.FirstFrameToken == 0 || Input.LastFrameToken == 0 ||
        Input.SettledFrameToken == 0 ||
        Input.FirstFrameToken > Input.LastFrameToken ||
        Input.SettledFrameToken < Input.FirstFrameToken ||
        Input.SettledFrameToken > Input.LastFrameToken ||
        Input.Width > 16384 || Input.Height > 16384)
    {
        SetReason(OutReason, "native probe identity is invalid");
        return false;
    }
    const bool bSuccess = Execution.Succeeded();
    if (bSuccess &&
        (!Execution.bNativeFrameAcquired || !Execution.bNativeSubmitted ||
         !Execution.bNativeCompletionObserved ||
         !Execution.bNativeReadbackCompleted ||
         !Execution.bNativePresented ||
         Execution.OutstandingTerminalOwnerCount != 0 ||
         !Frame.IsValid() || !State.IsValid() || !Frame.Matches(State) ||
         Frame.FrameToken != Execution.FrameToken ||
         Frame.Width != Input.Width || Frame.Height != Input.Height ||
         !IsSha256(Input.ReadbackDigest.View())))
    {
        SetReason(OutReason,
            "successful native probe lacks same-frame completion or retained a terminal owner");
        return false;
    }
    if (!bSuccess &&
        (Input.FirstFailureCode.IsEmpty() ||
         Input.FirstFailureStage.IsEmpty() ||
         Input.FirstFailureMessage.IsEmpty() ||
         Input.FirstFailureMessage.Len() > 1024))
    {
        SetReason(OutReason, "failed native probe lacks a bounded first failure");
        return false;
    }

    const bool bHdr = Input.ProfileKind ==
        Core::FString("native-hdr-nonvisual");
    if (bHdr && Input.Backend != Core::FString("metal"))
    {
        SetReason(OutReason, "Feature 029 exposes no non-Metal HDR probe mode");
        return false;
    }
    if (bHdr && State.bHasHDRMetadata)
    {
        SetReason(OutReason,
            "Metal HDR probe must retain EDRMetadata=nil and reject system tone mapping metadata");
        return false;
    }

    std::ostringstream Json;
    Json << std::setprecision(9)
         << "{\"backend\":\"" << Input.Backend.CStr()
         << "\",\"capabilityDigest\":\"" << Input.CapabilityDigest.CStr()
         << "\",\"commandCompleted\":"
         << (Execution.bNativeCompletionObserved ? "true" : "false")
         << ",\"deviceClass\":\"" << Input.DeviceClass.CStr()
         << "\",\"displayAdaptation\":\""
         << DisplayAdaptationName(State.DisplayAdaptation)
         << "\",\"exposureStops\":" << Input.ExposureStops
         << ",\"firstFailure\":";
    if (bSuccess)
        Json << "null";
    else
        Json << "{\"code\":\"" << Input.FirstFailureCode.CStr()
             << "\",\"message\":"
             << JsonString(Input.FirstFailureMessage.View())
             << ",\"stage\":\"" << Input.FirstFailureStage.CStr()
             << "\"}";
    Json << ",\"format\":\"" << FormatName(State.Format)
         << "\",\"firstFrameToken\":\"" << Input.FirstFrameToken
         << "\",\"frameToken\":\"" << Execution.FrameToken
         << "\",\"hdrMetadataDigest\":";
    if (State.bHasHDRMetadata)
        Json << "\"" << State.MetadataDigest.CStr() << "\"";
    else
        Json << "null";
    Json << ",\"height\":" << Input.Height
         << ",\"hostPlatform\":\"" << Input.HostPlatform.CStr()
         << "\",\"insertionDigest\":\"" << Input.InsertionDigest.CStr()
         << "\",\"lastFrameToken\":\"" << Input.LastFrameToken
         << "\",\"modeGeneration\":" << State.ModeGeneration
         << ",\"outputDeviceProfileId\":\""
         << Input.OutputDeviceProfileId.CStr()
         << "\",\"outstandingTerminalOwnerCount\":"
         << Execution.OutstandingTerminalOwnerCount
         << ",\"presented\":"
         << (Execution.bNativePresented ? "true" : "false")
         << ",\"presentationFrameToken\":";
    if (Execution.bNativePresented)
        Json << "\"" << Frame.FrameToken << "\"";
    else
        Json << "null";
    Json << ",\"profileKind\":\"" << Input.ProfileKind.CStr()
         << "\",\"readbackCompleted\":"
         << (Execution.bNativeReadbackCompleted ? "true" : "false")
         << ",\"readbackDigest\":";
    if (Execution.bNativeReadbackCompleted)
        Json << "\"" << Input.ReadbackDigest.CStr() << "\"";
    else
        Json << "null";
    Json << ",\"readbackFrameToken\":";
    if (Execution.bNativeReadbackCompleted)
        Json << "\"" << Frame.FrameToken << "\"";
    else
        Json << "null";
    Json << ",\"schema\":\"stoner.output-native-probe\""
         << ",\"schemaVersion\":2"
         << ",\"settledFrameToken\":\"" << Input.SettledFrameToken << "\""
         << ",\"status\":\"" << (bSuccess ? "passed" : "failed")
         << "\",\"transformVersion\":\""
         << Input.TransformVersion.CStr()
         << "\",\"width\":" << Input.Width
         << ",\"workloadRevision\":\""
         << Input.WorkloadRevision.CStr() << "\"}\n";
    OutJson = Json.str();
    return true;
}

} // namespace Stoner::Demo
