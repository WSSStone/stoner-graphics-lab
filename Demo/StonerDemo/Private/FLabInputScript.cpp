#include "FLabInputScript.h"
#include "Application/FInteractiveLabSession.h"
#include "Application/FWindow.h"
#include "Asset/FAssetDigest.h"
#include "Core/FPlatformFileSystem.h"
#include "yyjson/yyjson.h"
#include <cmath>
#include <memory>
#include <set>
#include <string>
#include <sstream>
#include <iomanip>
#include "Renderer/FOutputTransformSettings.h"
namespace Stoner::Demo
{
using namespace Core;
namespace
{
bool Keys(yyjson_val* V,std::initializer_list<const char*> Allowed)
{
    if (!yyjson_is_obj(V)) return false;
    std::set<std::string> Seen;
    size_t I,N; yyjson_val *K,*Value;
    yyjson_obj_foreach(V,I,N,K,Value)
    {
        (void)Value;
        std::string Name(yyjson_get_str(K),yyjson_get_len(K));
        bool Known=false; for (auto A:Allowed) Known|=Name==A;
        if (!Known || !Seen.insert(Name).second) return false;
    }
    return true;
}
bool Text(yyjson_val* V,FString& Out)
{
    if (!yyjson_is_str(V) || !yyjson_get_len(V) || yyjson_get_len(V)>128) return false;
    const std::string S(yyjson_get_str(V),yyjson_get_len(V));
    if (S.find('\0')!=std::string::npos) return false;
    Out=S; return true;
}
}
bool FLabInputScript::Load(const FString& Path,FString& Reason)
{
    TArray<uint8> Bytes;
    if (!FPlatformFileSystem::ReadRegularFileBounded(Path,65536,Bytes).IsSuccess())
    { Reason="lab script must be a regular file of at most 64 KiB"; return false; }
    return Decode(Bytes,Reason);
}
bool FLabInputScript::Decode(const TArray<uint8>& Bytes,FString& Reason)
{
    auto Fail=[&] { Reason="invalid bounded lab script"; return false; };
    if (Bytes.empty() || Bytes.size()>65536) return Fail();
    std::unique_ptr<yyjson_doc,decltype(&yyjson_doc_free)> Doc(
        yyjson_read(reinterpret_cast<const char*>(Bytes.data()),Bytes.size(),0),yyjson_doc_free);
    auto Root=Doc ? yyjson_doc_get_root(Doc.get()) : nullptr;
    if (!Keys(Root,{"schemaVersion","steps"})) return Fail();
    auto Version=yyjson_obj_get(Root,"schemaVersion"), Array=yyjson_obj_get(Root,"steps");
    if (!yyjson_is_uint(Version) || yyjson_get_uint(Version)!=1 || !yyjson_is_arr(Array) ||
        !yyjson_arr_size(Array) || yyjson_arr_size(Array)>256) return Fail();
    TArray<FLabScriptStep> Parsed;
    size_t I,N; yyjson_val* V;
    yyjson_arr_foreach(Array,I,N,V)
    {
        if (!Keys(V,{"afterPresented","action","value","value2"})) return Fail();
        FLabScriptStep S; auto At=yyjson_obj_get(V,"afterPresented");
        if (!yyjson_is_uint(At) || yyjson_get_uint(At)>100000 ||
            !Text(yyjson_obj_get(V,"action"),S.Action)) return Fail();
        S.AfterPresented=static_cast<uint32>(yyjson_get_uint(At));
        if (!Parsed.empty() && S.AfterPresented<Parsed.back().AfterPresented) return Fail();
        auto Value=yyjson_obj_get(V,"value"),Second=yyjson_obj_get(V,"value2");
        const bool String=S.Action=="profileByCapability" || S.Action=="profile" || S.Action=="rejectProfile" || S.Action=="tone";
        if (String) { if (!Text(Value,S.Text)) return Fail(); }
        else { if (!yyjson_is_num(Value)) return Fail(); S.Value=yyjson_get_num(Value); if (!std::isfinite(S.Value)) return Fail(); }
        if (S.Action=="resize" || S.Action=="restore")
        {
            if (!yyjson_is_uint(Value) || !yyjson_is_uint(Second)) return Fail();
            S.Value2=yyjson_get_num(Second);
            if (S.Value<1 || S.Value>4096 || S.Value2<1 || S.Value2>4096 || S.Value*S.Value2>7864320) return Fail();
        }
        else if (Second) return Fail();
        if (S.Action=="exposure") { if (S.Value < -16 || S.Value > 16) return Fail(); }
        else if (S.Action=="debug") { if (S.Value!=0 && S.Value!=1 && S.Value!=2) return Fail(); }
        else if (S.Action=="scale") { if (S.Value<0.5 || S.Value>4) return Fail(); }
        else if (S.Action=="white") { if (S.Value<0.5 || S.Value>4) return Fail(); }
        else if (S.Action=="ui" || S.Action=="focus") { if (S.Value!=0 && S.Value!=1) return Fail(); }
        else if (S.Action=="minimize" || S.Action=="close") { if (S.Value!=0) return Fail(); }
        else if (S.Action!="resize" && S.Action!="restore" && !String) return Fail();
        if (!Parsed.empty() && Parsed.back().Action=="close") return Fail();
        Parsed.push_back(std::move(S));
    }
    ProfileResults.clear(); PendingProfile.reset(); Steps=std::move(Parsed); Cursor=0; Expected.reset(); ProgressTime={}; LastPresented=0; ResumeAfterPresented=0; LastCursor=0;
    Digest=Asset::FAssetDigest::FromBytes(Bytes).ToLowerHex(); Reason={}; return true;
}
bool FLabInputScript::Service(Application::FWindow& W,Application::FInteractiveLabSession& S,uint32 Presented,FString& Reason)
{
    const auto Now=std::chrono::steady_clock::now();
    if (ProgressTime==std::chrono::steady_clock::time_point{} || Presented!=LastPresented || Cursor!=LastCursor)
    { ProgressTime=Now; LastPresented=Presented; LastCursor=Cursor; }
    if (!IsComplete() && Now-ProgressTime>=std::chrono::seconds(5))
    { Reason="lab script made no progress for five seconds"; return false; }
    if (!PendingProfile && !ProfileResults.empty())
    {
        FLabProfileObservation O;
        if (!ObserveProfile || !ObserveProfile(O) ||
            O.Capabilities.CapabilityGeneration!=ProfileResults.front().Start.Capabilities.CapabilityGeneration ||
            O.Capabilities.CapabilityDigest!=ProfileResults.front().Start.Capabilities.CapabilityDigest)
        { Reason="profile capability query failed or changed after request"; return false; }
    }
    const auto* Effective=S.GetEffectiveSettings();
    if (PendingProfile)
    {
        if (!ServiceProfile(S,Reason)) return false;
        if (PendingProfile) return true;
    }
    if (Expected)
    {
        if (!S.GetSettingsFailure().IsEmpty()) { Reason="script settings transition failed"; return false; }
        if (!Effective || Effective->SettingsRevision<=Expected->SettingsRevision || S.GetPendingSettings()) return true;
        if (Effective->ExposureStops!=Expected->ExposureStops || Effective->UIWhiteMultiplier!=Expected->UIWhiteMultiplier ||
            Effective->RequestedProfileId!=Expected->RequestedProfileId || Effective->SdrToneMapVersion!=Expected->SdrToneMapVersion ||
            Effective->DebugBypass.Mode!=Expected->DebugBypass.Mode || Effective->DebugBypass.StageName!=Expected->DebugBypass.StageName)
        { Reason="script settings did not become effective"; return false; }
        Expected.reset();
    }
    if (Cursor==Steps.size() || Presented<ResumeAfterPresented || Presented<Steps[Cursor].AfterPresented) return true;
    const auto& Step=Steps[Cursor]; const auto& A=Step.Action;
    const bool Recovery=A=="restore" || A=="focus" || A=="close";
    if (!Recovery && (S.GetPendingSettings() || S.HasPendingTransition())) return true;
    bool OK=true;
    if (A=="profileByCapability") OK=StartProfile(S,Step,Reason);
    else if (A=="resize") OK=W.SetClientSize(static_cast<uint32>(Step.Value),static_cast<uint32>(Step.Value2))==Application::EApplicationResult::Success;
    else if (A=="restore") W.QueueEvent(Application::FWindowEvent::Restored(static_cast<uint32>(Step.Value),static_cast<uint32>(Step.Value2)));
    else if (A=="scale") OK=W.SetValidationContentScale(static_cast<float>(Step.Value))==Application::EApplicationResult::Success;
    else if (A=="minimize") W.QueueEvent(Application::FWindowEvent::Minimized());
    else if (A=="focus") W.QueueEvent(Step.Value ? Application::FWindowEvent::FocusGained() : Application::FWindowEvent::FocusLost());
    else if (A=="ui") OK=S.SetUIEnabled(Step.Value!=0)==Application::EApplicationResult::Success;
    else if (A=="close") OK=S.RequestExit()==Application::EApplicationResult::Success;
    else
    {
        if (!Effective || !S.GetRequestedSettings()) return true;
        auto Edit=*S.GetRequestedSettings(); Edit.SettingsRevision=Effective->SettingsRevision;
        if (A=="exposure") Edit.ExposureStops=static_cast<float>(Step.Value);
        else if (A=="white") Edit.UIWhiteMultiplier=static_cast<float>(Step.Value);
        else if (A=="debug")
        {
            Edit.DebugBypass={};
            if (Step.Value!=0)
            {
                Edit.DebugBypass.Mode=Step.Value==1 ? Renderer::EOutputTransformDebugBypassMode::BoundedVisualization : Renderer::EOutputTransformDebugBypassMode::HDRPreservingReadback;
                Edit.DebugBypass.StageName="ManualExposure";
                Edit.DebugBypass.SourceDomain=Renderer::ERenderGraphColorDomain::SceneLinearRec709D65;
            }
        }
        else if (A=="tone") Edit.SdrToneMapVersion=Step.Text;
        else Edit.RequestedProfileId=Step.Text;
        OK=S.RequestSettings(Edit);
        if (A=="rejectProfile") OK=!OK;
        else if (OK) Expected=Edit;
    }
    if (!OK) { if (Reason.IsEmpty()) Reason="script action rejected: "+A.ToStdString(); return false; }
    if (A!="minimize" && A!="close") ResumeAfterPresented=Presented+3;
    ++Cursor; return true;
}
namespace
{
FString SettingsState(const Application::FInteractiveLabSession& S)
{
    std::ostringstream Text;
    Text.imbue(std::locale::classic()); Text << std::setprecision(9);
    for (const auto* V : {S.GetEffectiveSettings(), S.GetRequestedSettings(), S.GetPendingSettings()})
    {
        if (!V) { Text << "null;"; continue; }
        Text << V->CameraRevision << ',' << V->SettingsRevision << ',' << V->DisplayGeneration << ',' << V->OutputModeGeneration
            << ',' << V->RequestedProfileId.CStr() << ',' << V->EffectiveProfileId.CStr() << ',' << V->SdrToneMapVersion.CStr()
            << ',' << V->HdrViewingVersion.CStr() << ',' << V->ExposureStops << ',' << V->bUIVisible << ',' << V->UIWhiteMultiplier
            << ',' << V->UIReferenceWhiteNits << ',' << V->NativePackingWhiteNits << ',' << static_cast<int>(V->DebugBypass.Mode)
            << ',' << V->DebugBypass.StageName.CStr() << ',' << static_cast<int>(V->DebugBypass.SourceDomain)
            << ',' << V->DebugBypass.VisualizationMinimum << ',' << V->DebugBypass.VisualizationMaximum << ';';
    }
    const auto Bytes=Text.str();
    return Asset::FAssetDigest::FromBytes({reinterpret_cast<const uint8*>(Bytes.data()),Bytes.size()}).ToLowerHex();
}
bool SameCapabilities(const RHI::FRHIPresentationCapabilities& A,const RHI::FRHIPresentationCapabilities& B)
{
    return A.SurfaceId==B.SurfaceId && A.CapabilityGeneration==B.CapabilityGeneration &&
        A.CapabilityDigest==B.CapabilityDigest && A.SupportedPairs==B.SupportedPairs && A.NativeReferenceWhiteNits==B.NativeReferenceWhiteNits &&
        A.bSupportsExtendedRange==B.bSupportsExtendedRange;
}
const char* FormatName(RHI::ERHIFormat F)
{
    using RHI::ERHIFormat;
    switch(F) {
    case ERHIFormat::R8G8B8A8_UNorm: return "rgba8-unorm";
    case ERHIFormat::B8G8R8A8_UNorm: return "bgra8-unorm";
    case ERHIFormat::R8G8B8A8_sRGB: return "rgba8-srgb";
    case ERHIFormat::R10G10B10A2_UNorm: return "rgb10a2-unorm";
    case ERHIFormat::R16G16B16A16_Float: return "rgba16-float";
    default: return "unknown";
    }
}
}
bool FLabInputScript::StartProfile(Application::FInteractiveLabSession& S,const FLabScriptStep& Step,FString& Reason)
{
    FLabProfileResult R; R.StepIndex=static_cast<uint32>(Cursor); R.RequestedProfile=Step.Text;
    const Renderer::FOutputTransformSettingsValidator Validator;
    const auto* P=Validator.FindProfile(Step.Text);
    if (!P || !ObserveProfile || !ObserveProfile(R.Start) || !R.Start.Capabilities.IsValid())
    { Reason="profile capability query failed: "+Step.Text.ToStdString(); return false; }
    if (!ProfileResults.empty() && !SameCapabilities(ProfileResults.front().Start.Capabilities,R.Start.Capabilities))
    { Reason="profile capability generation changed: "+Step.Text.ToStdString(); return false; }
    if (!S.GetEffectiveSettings() || !S.GetRequestedSettings() || S.GetPendingSettings())
    { Reason="profile request requires idle settings"; return false; }
    R.Before=*S.GetEffectiveSettings(); R.BeforeState=SettingsState(S);
    const auto& C=R.Start.Capabilities;
    R.bSupported=C.SupportsPair(P->Format,P->ColorSpace) ||
        (P->StorageClass==Renderer::EOutputProfileStorageClass::UNorm8 && C.SupportsPair(RHI::ERHIFormat::B8G8R8A8_UNorm,P->ColorSpace));
    auto Edit=*S.GetRequestedSettings(); Edit.RequestedProfileId=Step.Text;
    if (R.bSupported && R.Before.EffectiveProfileId==Step.Text)
        R.Outcome="retained";
    else
    {
        const bool Accepted=S.RequestSettings(Edit);
        if (Accepted!=R.bSupported)
        { Reason="profile admission disagrees with capability: "+Step.Text.ToStdString()+" requires "+FormatName(P->Format)+" / "+RHI::ToString(P->ColorSpace)+"; "+S.GetSettingsFailure().ToStdString(); return false; }
        if (!Accepted)
        {
            if (S.GetSettingsFailure()!="Requested output unavailable; no permitted output transition" || SettingsState(S)!=R.BeforeState)
            { Reason="profile rejection was not unsupported/no-mutation: "+Step.Text.ToStdString(); return false; }
            R.Outcome="rejected-unsupported"; R.Reason="unsupported-format-color-space";
        }
        else R.Outcome="switched";
    }
    ProfileResults.push_back(std::move(R)); PendingProfile=ProfileResults.size()-1;
    ProfileStarted=ProfileClock ? ProfileClock() : std::chrono::steady_clock::now();
    return true;
}
bool FLabInputScript::ServiceProfile(Application::FInteractiveLabSession& S,FString& Reason)
{
    auto& R=ProfileResults[*PendingProfile]; FLabProfileObservation O;
    if ((ProfileClock ? ProfileClock() : std::chrono::steady_clock::now())-ProfileStarted>=std::chrono::seconds(5))
    { Reason="profile request timed out: "+R.RequestedProfile.ToStdString(); return false; }
    if (!ObserveProfile || !ObserveProfile(O) || !SameCapabilities(R.Start.Capabilities,O.Capabilities))
    { Reason="profile capability query failed or generation changed: "+R.RequestedProfile.ToStdString(); return false; }
    const auto* E=S.GetEffectiveSettings();
    if (!E) { Reason="profile effective settings disappeared"; return false; }
    if (R.Outcome=="switched")
    {
        if (!S.GetSettingsFailure().IsEmpty()) { Reason="profile execution failed: "+R.RequestedProfile.ToStdString()+"; "+S.GetSettingsFailure().ToStdString(); return false; }
        if (S.GetPendingSettings() || E->SettingsRevision<=R.Before.SettingsRevision) return true;
        if (O.NativeOutputGeneration<=R.Start.NativeOutputGeneration || E->EffectiveProfileId!=R.RequestedProfile || E->RequestedProfileId!=R.RequestedProfile || E->OutputModeGeneration<=R.Before.OutputModeGeneration)
        { Reason="profile transaction resolved incorrectly: "+R.RequestedProfile.ToStdString(); return false; }
    }
    else if (SettingsState(S)!=R.BeforeState || O.NativeOutputGeneration!=R.Start.NativeOutputGeneration)
    { Reason="profile rejection/retention mutated settings: "+R.RequestedProfile.ToStdString(); return false; }
    if (O.FrameToken<=R.Start.FrameToken || O.ProfileId!=E->EffectiveProfileId || O.SettingsRevision!=E->SettingsRevision || O.OutputGeneration!=E->OutputModeGeneration) return true;
    R.After=*E; R.AfterState=SettingsState(S); R.Finish=std::move(O); R.bComplete=true; PendingProfile.reset();
    return true;
}

}
