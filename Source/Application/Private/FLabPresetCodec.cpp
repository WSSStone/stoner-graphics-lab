#include "FLabPresetCodec.h"
#include "Asset/FAssetDigest.h"
#include "Core/FUnicode.h"
#include "Renderer/FOutputTransformSettings.h"
#include "yyjson/yyjson.h"
#include <algorithm>
#include <cmath>
#include <memory>
#include <limits>
#include <vector>

namespace Stoner::Application
{
namespace
{
using namespace Stoner::Core;
using namespace Stoner::Renderer;
bool Text(const FString& S)
{
    if (S.IsEmpty() || S.View().size()>4096 || S.View().find('\0')!=std::string_view::npos) return false;
    FString Checked;
    return FUnicode::NormalizeNFC(S,Checked)==EUnicodeResult::Success;
}
bool Digest(const FString& S)
{
    Asset::FAssetDigest Value;
    return Asset::FAssetDigest::ParseLowerHex(S,Value)==Asset::EAssetResult::Success;
}
bool Valid(const FLabPreset& P,std::span<const FLabDebugStage> Stages)
{
    const auto& C=P.Camera;
    const auto& E=P.SourceContext.DrawableExtent;
    if (!Text(P.Workload.Revision) || !Text(P.Workload.ProductionRoot) || !Digest(P.Workload.SourceIdentityDigest) ||
        !Text(P.SourceContext.Backend) || !Digest(P.SourceContext.CookedGeneration) || !Text(P.SourceContext.SoftwareRevision) ||
        !E.IsPositive() || E.Width>4096 || E.Height>4096 || uint64(E.Width)*E.Height>7864320) return false;
    const float Values[]={C.Position.X,C.Position.Y,C.Position.Z,C.YawRadians,C.PitchRadians,
        C.VerticalFovRadians,C.NearPlane,C.FarPlane,C.MovementSpeed};
    if (!std::all_of(std::begin(Values),std::end(Values),[](float V) { return std::isfinite(V); }) ||
        C.PitchRadians<FMath::DegreesToRadians(-89.0f) || C.PitchRadians>FMath::DegreesToRadians(89.0f) ||
        C.VerticalFovRadians<FMath::DegreesToRadians(20.0f) || C.VerticalFovRadians>FMath::DegreesToRadians(90.0f) ||
        C.NearPlane<0.0001f || C.FarPlane<=C.NearPlane || C.FarPlane>1000000.0f ||
        C.MovementSpeed<0.01f || C.MovementSpeed>100.0f) return false;
    auto O=P.Output;
    O.CameraRevision=O.SettingsRevision=O.DisplayGeneration=O.OutputModeGeneration=1;
    O.EffectiveProfileId=O.RequestedProfileId;
    O.UIReferenceWhiteNits=O.NativePackingWhiteNits=100;
    if (!O.IsValid()) return false;
    const auto& B=O.DebugBypass;
    return B.Mode==EOutputTransformDebugBypassMode::Disabled || std::any_of(Stages.begin(),Stages.end(),[&](const auto& S) {
        return (S.ProfileId.IsEmpty() || S.ProfileId==O.RequestedProfileId) &&
            B.IsValidForResolvedStageDomain(S.Name,S.Domain);
    });
}
bool BoundedTree(yyjson_val* V,size_t Depth,size_t& Count)
{
    if (!V || Depth>8 || ++Count>256) return false;
    size_t I,N; yyjson_val* Child; yyjson_val* Key;
    if (yyjson_is_arr(V)) yyjson_arr_foreach(V,I,N,Child)
    { if (!BoundedTree(Child,Depth+1,Count)) return false; }
    if (yyjson_is_obj(V)) yyjson_obj_foreach(V,I,N,Key,Child)
    { if (++Count>256 || !BoundedTree(Child,Depth+1,Count)) return false; }
    return true;
}
bool Keys(yyjson_val* V,std::initializer_list<const char*> Names)
{
    if (!yyjson_is_obj(V) || yyjson_obj_size(V)!=Names.size()) return false;
    // Exact cardinality plus presence of every distinct required key rejects
    // unknown, missing and duplicate members without a second key registry.
    return std::all_of(Names.begin(),Names.end(),[&](const char* K) { return yyjson_obj_get(V,K)!=nullptr; });
}
bool String(yyjson_val* V,FString& Out)
{
    if (!yyjson_is_str(V) || yyjson_get_len(V)>4096) return false;
    Out=FString(std::string_view(yyjson_get_str(V),yyjson_get_len(V)));
    return Out.View().find('\0')==std::string_view::npos;
}
bool Float(yyjson_val* V,float& Out)
{
    if (!yyjson_is_num(V)) return false;
    const double Number=yyjson_get_num(V);
    if (!std::isfinite(Number) || std::abs(Number)>std::numeric_limits<float>::max()) return false;
    Out=Number==0 ? 0.0f : static_cast<float>(Number);
    return std::isfinite(Out);
}
}

bool FLabPresetCodec::Decode(std::span<const uint8> Bytes,const FLabPresetWorkload& Expected,
    std::span<const FLabDebugStage> Stages,FLabPreset& OutPreset,FString& OutReason)
{
    OutReason="Invalid or over-budget preset JSON";
    if (Bytes.empty() || Bytes.size()>MaximumBytes) return false;
    std::vector<unsigned char> Pool(256*1024);
    yyjson_alc Allocator{};
    if (!yyjson_alc_pool_init(&Allocator,Pool.data(),Pool.size())) return false;
    std::unique_ptr<yyjson_doc,decltype(&yyjson_doc_free)> Doc(yyjson_read_opts(
        reinterpret_cast<char*>(const_cast<uint8*>(Bytes.data())),Bytes.size(),YYJSON_READ_NOFLAG,&Allocator,nullptr),yyjson_doc_free);
    auto* Root=Doc ? yyjson_doc_get_root(Doc.get()) : nullptr;
    size_t Count=0;
    if (!BoundedTree(Root,1,Count) || !Keys(Root,{"schema","schemaVersion","authority","workload","sourceContext","camera","output","ui","digest"})) return false;
    FString Schema,Authority,ReceivedDigest;
    auto* Version=yyjson_obj_get(Root,"schemaVersion");
    if (!String(yyjson_obj_get(Root,"schema"),Schema) || Schema!="stoner.interactive-lab-preset" ||
        !yyjson_is_uint(Version) || yyjson_get_uint(Version)!=1 ||
        !String(yyjson_obj_get(Root,"authority"),Authority) || Authority!="interactive-preview" ||
        !String(yyjson_obj_get(Root,"digest"),ReceivedDigest) || !Digest(ReceivedDigest)) return false;
    auto* W=yyjson_obj_get(Root,"workload"); auto* S=yyjson_obj_get(Root,"sourceContext");
    auto* C=yyjson_obj_get(Root,"camera"); auto* O=yyjson_obj_get(Root,"output");
    auto* UI=yyjson_obj_get(Root,"ui"); auto* B=yyjson_obj_get(O,"debugBypass");
    if (!Keys(W,{"revision","productionRoot","sourceIdentityDigest"}) ||
        !Keys(S,{"drawableWidth","drawableHeight","backend","cookedGeneration","softwareRevision"}) ||
        !Keys(C,{"position","yawRadians","pitchRadians","verticalFovRadians","nearPlane","farPlane","movementSpeed"}) ||
        !Keys(O,{"requestedProfileId","sdrToneMapVersion","hdrViewingVersion","exposureStops","debugBypass"}) ||
        !Keys(B,{"mode","stageName","sourceDomain","visualizationMinimum","visualizationMaximum"}) || !Keys(UI,{"whiteMultiplier"})) return false;
    FLabPreset P;
    bool OK=String(yyjson_obj_get(W,"revision"),P.Workload.Revision) && String(yyjson_obj_get(W,"productionRoot"),P.Workload.ProductionRoot) &&
        String(yyjson_obj_get(W,"sourceIdentityDigest"),P.Workload.SourceIdentityDigest) &&
        String(yyjson_obj_get(S,"backend"),P.SourceContext.Backend) && String(yyjson_obj_get(S,"cookedGeneration"),P.SourceContext.CookedGeneration) &&
        String(yyjson_obj_get(S,"softwareRevision"),P.SourceContext.SoftwareRevision);
    auto* Width=yyjson_obj_get(S,"drawableWidth"); auto* Height=yyjson_obj_get(S,"drawableHeight");
    OK=OK && yyjson_is_uint(Width) && yyjson_is_uint(Height) && yyjson_get_uint(Width)<=4096 && yyjson_get_uint(Height)<=4096;
    if (!OK) return false;
    P.SourceContext.DrawableExtent={static_cast<uint32>(yyjson_get_uint(Width)),static_cast<uint32>(yyjson_get_uint(Height))};
    auto* Position=yyjson_obj_get(C,"position");
    OK=yyjson_is_arr(Position) && yyjson_arr_size(Position)==3 && Float(yyjson_arr_get(Position,0),P.Camera.Position.X) &&
        Float(yyjson_arr_get(Position,1),P.Camera.Position.Y) && Float(yyjson_arr_get(Position,2),P.Camera.Position.Z) &&
        Float(yyjson_obj_get(C,"yawRadians"),P.Camera.YawRadians) && Float(yyjson_obj_get(C,"pitchRadians"),P.Camera.PitchRadians) &&
        Float(yyjson_obj_get(C,"verticalFovRadians"),P.Camera.VerticalFovRadians) && Float(yyjson_obj_get(C,"nearPlane"),P.Camera.NearPlane) &&
        Float(yyjson_obj_get(C,"farPlane"),P.Camera.FarPlane) && Float(yyjson_obj_get(C,"movementSpeed"),P.Camera.MovementSpeed) &&
        String(yyjson_obj_get(O,"requestedProfileId"),P.Output.RequestedProfileId) && String(yyjson_obj_get(O,"sdrToneMapVersion"),P.Output.SdrToneMapVersion) &&
        String(yyjson_obj_get(O,"hdrViewingVersion"),P.Output.HdrViewingVersion) && Float(yyjson_obj_get(O,"exposureStops"),P.Output.ExposureStops) &&
        Float(yyjson_obj_get(UI,"whiteMultiplier"),P.Output.UIWhiteMultiplier);
    FString Mode,Domain;
    OK=OK && String(yyjson_obj_get(B,"mode"),Mode) && String(yyjson_obj_get(B,"sourceDomain"),Domain) &&
        String(yyjson_obj_get(B,"stageName"),P.Output.DebugBypass.StageName) &&
        Float(yyjson_obj_get(B,"visualizationMinimum"),P.Output.DebugBypass.VisualizationMinimum) &&
        Float(yyjson_obj_get(B,"visualizationMaximum"),P.Output.DebugBypass.VisualizationMaximum);
    bool ModeKnown=false,DomainKnown=false;
    for (auto M : {EOutputTransformDebugBypassMode::Disabled,EOutputTransformDebugBypassMode::BoundedVisualization,EOutputTransformDebugBypassMode::HDRPreservingReadback})
        if (Mode==ToString(M)) { ModeKnown=true; P.Output.DebugBypass.Mode=M; }
    for (auto D : {ERenderGraphColorDomain::Unspecified,ERenderGraphColorDomain::SceneLinearRec709D65,ERenderGraphColorDomain::DisplayLinearRec709D65,
        ERenderGraphColorDomain::DisplayLinearRec2020D65,ERenderGraphColorDomain::EncodedSrgb,ERenderGraphColorDomain::EncodedBt709,
        ERenderGraphColorDomain::EncodedGamma22,ERenderGraphColorDomain::EncodedPqRec2020D65,ERenderGraphColorDomain::ExtendedSrgbLinear})
        if (Domain==ToString(D)) { DomainKnown=true; P.Output.DebugBypass.SourceDomain=D; }
    if (!OK || !ModeKnown || !DomainKnown) return false;
    if (!(P.Workload==Expected)) { OutReason="Preset workload identity mismatch"; return false; }
    TArray<uint8> Normalized;
    if (!Encode(P,Stages,Normalized,OutReason)) return false;
    const std::string Suffix="\"digest\":\""+ReceivedDigest.ToStdString()+"\"}";
    if (!std::string_view(reinterpret_cast<const char*>(Normalized.data()),Normalized.size()).ends_with(Suffix))
    { OutReason="Preset digest mismatch"; return false; }
    OutPreset=std::move(P); OutReason.Clear(); return true;
}

bool FLabPresetCodec::Encode(const FLabPreset& P,std::span<const FLabDebugStage> Stages,
    TArray<uint8>& OutBytes,FString& OutReason)
{
    OutReason.Clear();
    if (!Valid(P,Stages)) { OutReason="Invalid complete preset values or debug stage"; return false; }
    // Both DOM and writer allocations share one explicit, bounded arena.
    std::vector<unsigned char> Pool(256*1024);
    yyjson_alc Allocator{};
    if (!yyjson_alc_pool_init(&Allocator,Pool.data(),Pool.size()))
    { OutReason="Preset writer allocator unavailable"; return false; }
    std::unique_ptr<yyjson_mut_doc,decltype(&yyjson_mut_doc_free)> Doc(yyjson_mut_doc_new(&Allocator),yyjson_mut_doc_free);
    if (!Doc) { OutReason="Preset writer allocation failed"; return false; }
    auto* D=Doc.get();
    auto* Root=yyjson_mut_obj(D);
    yyjson_mut_doc_set_root(D,Root);
    bool OK=Root!=nullptr;
    const auto Str=[&](yyjson_mut_val* Object,const char* Key,const FString& Value) {
        OK=yyjson_mut_obj_add_strncpy(D,Object,Key,Value.CStr(),Value.View().size()) && OK;
    };
    const auto Real=[&](yyjson_mut_val* Object,const char* Key,float Value) {
        OK=yyjson_mut_obj_add_real(D,Object,Key,Value==0 ? 0.0 : static_cast<double>(Value)) && OK;
    };
    Str(Root,"schema","stoner.interactive-lab-preset");
    OK=yyjson_mut_obj_add_uint(D,Root,"schemaVersion",1) && OK;
    Str(Root,"authority","interactive-preview");
    auto* W=yyjson_mut_obj_add_obj(D,Root,"workload");
    Str(W,"revision",P.Workload.Revision); Str(W,"productionRoot",P.Workload.ProductionRoot);
    Str(W,"sourceIdentityDigest",P.Workload.SourceIdentityDigest);
    auto* S=yyjson_mut_obj_add_obj(D,Root,"sourceContext");
    OK=yyjson_mut_obj_add_uint(D,S,"drawableWidth",P.SourceContext.DrawableExtent.Width) && OK;
    OK=yyjson_mut_obj_add_uint(D,S,"drawableHeight",P.SourceContext.DrawableExtent.Height) && OK;
    Str(S,"backend",P.SourceContext.Backend); Str(S,"cookedGeneration",P.SourceContext.CookedGeneration);
    Str(S,"softwareRevision",P.SourceContext.SoftwareRevision);
    auto* C=yyjson_mut_obj_add_obj(D,Root,"camera");
    auto* Position=yyjson_mut_arr(D);
    for (float V : {P.Camera.Position.X,P.Camera.Position.Y,P.Camera.Position.Z})
        OK=yyjson_mut_arr_add_real(D,Position,V==0 ? 0.0 : static_cast<double>(V)) && OK;
    OK=yyjson_mut_obj_add_val(D,C,"position",Position) && OK;
    Real(C,"yawRadians",P.Camera.YawRadians); Real(C,"pitchRadians",P.Camera.PitchRadians);
    Real(C,"verticalFovRadians",P.Camera.VerticalFovRadians); Real(C,"nearPlane",P.Camera.NearPlane);
    Real(C,"farPlane",P.Camera.FarPlane); Real(C,"movementSpeed",P.Camera.MovementSpeed);
    auto* O=yyjson_mut_obj_add_obj(D,Root,"output");
    Str(O,"requestedProfileId",P.Output.RequestedProfileId); Str(O,"sdrToneMapVersion",P.Output.SdrToneMapVersion);
    Str(O,"hdrViewingVersion",P.Output.HdrViewingVersion); Real(O,"exposureStops",P.Output.ExposureStops);
    auto* B=yyjson_mut_obj_add_obj(D,O,"debugBypass");
    Str(B,"mode",ToString(P.Output.DebugBypass.Mode)); Str(B,"stageName",P.Output.DebugBypass.StageName);
    Str(B,"sourceDomain",ToString(P.Output.DebugBypass.SourceDomain));
    Real(B,"visualizationMinimum",P.Output.DebugBypass.VisualizationMinimum);
    Real(B,"visualizationMaximum",P.Output.DebugBypass.VisualizationMaximum);
    auto* UI=yyjson_mut_obj_add_obj(D,Root,"ui"); Real(UI,"whiteMultiplier",P.Output.UIWhiteMultiplier);
    if (!OK) { OutReason="Preset writer arena exhausted"; return false; }
    size_t Size=0;
    char* Normalized=yyjson_mut_write_opts(D,YYJSON_WRITE_NOFLAG,&Allocator,&Size,nullptr);
    if (!Normalized || Size>MaximumBytes)
    { OutReason="Preset normalized bytes exceed budget"; return false; }
    const auto Hash=Asset::FAssetDigest::FromBytes(std::span<const uint8>(reinterpret_cast<const uint8*>(Normalized),Size));
    Allocator.free(Allocator.ctx,Normalized);
    Str(Root,"digest",Hash.ToLowerHex());
    char* Encoded=OK ? yyjson_mut_write_opts(D,YYJSON_WRITE_NOFLAG,&Allocator,&Size,nullptr) : nullptr;
    if (!Encoded || Size>MaximumBytes)
    { OutReason="Preset encoded bytes exceed budget"; return false; }
    TArray<uint8> Candidate(reinterpret_cast<const uint8*>(Encoded),reinterpret_cast<const uint8*>(Encoded)+Size);
    Allocator.free(Allocator.ctx,Encoded);
    OutBytes=std::move(Candidate);
    return true;
}
}
