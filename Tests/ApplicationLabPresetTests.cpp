#include "FLabPresetCodec.h"
#include "Core/FPlatformFileSystem.h"
#include "Renderer/FOutputTransformSettings.h"
#include "yyjson/yyjson.h"
#include <bit>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>

int RunApplicationLabPresetTests()
{
    using namespace Stoner::Core;
    using namespace Stoner::Application;
    using namespace Stoner::Renderer;
    int Failed=0;
    const auto Check=[&](bool OK,const char* Name) {
        std::cout<<(OK ? "[PASS] " : "[FAIL] ")<<Name<<'\n';
        if (!OK) ++Failed;
        return OK;
    };
    FLabPreset P;
    P.Workload={"fixture-lantern-v1","StaticModel:灯笼😀.glb#idx.scene.0",FString(std::string(64,'1'))};
    P.SourceContext={{320,200},"Metal",FString(std::string(64,'2')),"fixture"};
    P.Camera.Position={0.1f,-3.5f,-0.0f}; P.Camera.YawRadians=.25f; P.Camera.PitchRadians=-.125f;
    P.Camera.VerticalFovRadians=1; P.Camera.NearPlane=.1f; P.Camera.FarPlane=100; P.Camera.MovementSpeed=1.5f;
    P.Output.RequestedProfileId=GDefaultSDROutputDeviceProfile;
    P.Output.SdrToneMapVersion=GDefaultSDRToneMapVersion; P.Output.HdrViewingVersion=GInitialHDRViewingVersion;
    P.Output.ExposureStops=-3; P.Output.UIWhiteMultiplier=.5f;
    P.Output.DebugBypass={EOutputTransformDebugBypassMode::BoundedVisualization,"ManualExposure",
        ERenderGraphColorDomain::SceneLinearRec709D65,-2,6};
    const FLabDebugStage Stage{"ManualExposure",ERenderGraphColorDomain::SceneLinearRec709D65,{}};
    const std::span<const FLabDebugStage> Stages{&Stage,1};
    TArray<uint8> Golden,Encoded;
    FString Reason;
    if (!Check(FPlatformFileSystem::ReadRegularFileBounded("Tests/Fixtures/InteractiveLab/preset-v1.json",65536,Golden).IsSuccess() &&
        FLabPresetCodec::Encode(P,Stages,Encoded,Reason) && Encoded==Golden,
        "preset writer matches pinned field order, exact float promotion, Unicode and digest bytes"))
    { std::cout<<Reason.CStr()<<'\n'; return Failed; }
    std::unique_ptr<yyjson_doc,decltype(&yyjson_doc_free)> Doc(
        yyjson_read(reinterpret_cast<const char*>(Encoded.data()),Encoded.size(),0),yyjson_doc_free);
    auto* Root=Doc ? yyjson_doc_get_root(Doc.get()) : nullptr;
    auto* Camera=Root ? yyjson_obj_get(Root,"camera") : nullptr;
    const char* Keys[]={"yawRadians","pitchRadians","verticalFovRadians","nearPlane","farPlane","movementSpeed"};
    const float Expected[]={P.Camera.YawRadians,P.Camera.PitchRadians,P.Camera.VerticalFovRadians,
        P.Camera.NearPlane,P.Camera.FarPlane,P.Camera.MovementSpeed};
    bool Exact=Camera!=nullptr;
    for (size_t I=0;I<6 && Exact;++I)
        Exact=std::bit_cast<uint32>(static_cast<float>(yyjson_get_num(yyjson_obj_get(Camera,Keys[I]))))==std::bit_cast<uint32>(Expected[I]);
    Check(Exact && !std::signbit(yyjson_get_num(yyjson_arr_get(yyjson_obj_get(Camera,"position"),2))),
        "preset camera floats recover their original bits with positive zero normalization");
    auto NonPersistent=P;
    NonPersistent.Camera.CameraRevision=100; NonPersistent.Camera.View=FMatrix4x4::Zero();
    NonPersistent.Output.bUIVisible=false; NonPersistent.Output.SettingsRevision=99;
    NonPersistent.Output.EffectiveProfileId="transient-fallback";
    Check(FLabPresetCodec::Encode(NonPersistent,Stages,Encoded,Reason) && Encoded==Golden,
        "preset never serializes derived matrices, runtime revisions, effective fallback or UI visibility");
    for (auto Mode : {EOutputTransformDebugBypassMode::Disabled,EOutputTransformDebugBypassMode::HDRPreservingReadback})
    {
        auto Changed=P; Changed.Output.DebugBypass.Mode=Mode;
        if (Mode==EOutputTransformDebugBypassMode::Disabled)
        { Changed.Output.DebugBypass.StageName.Clear(); Changed.Output.DebugBypass.SourceDomain=ERenderGraphColorDomain::Unspecified; }
        FLabPreset Restored; TArray<uint8> RoundTrip;
        Check(FLabPresetCodec::Encode(Changed,Stages,Encoded,Reason) && Encoded!=Golden &&
            FLabPresetCodec::Decode(Encoded,P.Workload,Stages,Restored,Reason) &&
            FLabPresetCodec::Encode(Restored,Stages,RoundTrip,Reason) && RoundTrip==Encoded,
            "preset persists disabled and numeric-source modes with the full nondefault range");
    }
    const auto Reject=[&](FLabPreset Bad,const char* Name) {
        TArray<uint8> Before{17,23};
        Check(!FLabPresetCodec::Encode(Bad,Stages,Before,Reason) && Before==TArray<uint8>({17,23}) && !Reason.IsEmpty(),Name);
    };
    auto Bad=P; Bad.Camera.VerticalFovRadians=100;
    Reject(Bad,"preset rejects invalid FOV before replacing output bytes");
    Bad=P; Bad.Camera.FarPlane=Bad.Camera.NearPlane;
    Reject(Bad,"preset rejects equal near and far planes");
    Bad=P; Bad.Camera.Position.X=std::numeric_limits<float>::infinity();
    Reject(Bad,"preset rejects non-finite camera values");
    Bad=P; Bad.Output.ExposureStops=std::numeric_limits<float>::quiet_NaN();
    Reject(Bad,"preset rejects non-finite exposure");
    Bad=P; Bad.Output.SdrToneMapVersion="unknown.v2";
    Reject(Bad,"preset rejects unknown output strategy versions");
    Bad=P; Bad.Output.DebugBypass.StageName="UnknownStage";
    Reject(Bad,"preset requires freshly resolved debug stage identity");
    Bad=P; Bad.Output.DebugBypass.SourceDomain=ERenderGraphColorDomain::DisplayLinearRec709D65;
    Reject(Bad,"preset rejects a debug domain mismatch");
    Bad=P; Bad.Output.DebugBypass.VisualizationMaximum=-2;
    Reject(Bad,"preset rejects a non-positive visualization range");
    Bad=P; Bad.Workload.SourceIdentityDigest=FString(std::string(64,'A'));
    Reject(Bad,"preset rejects noncanonical source digest identity");
    Bad=P; Bad.SourceContext.DrawableExtent={4096,4096};
    Reject(Bad,"preset rejects exported extents above the pixel budget");
    Bad=P; Bad.Workload.ProductionRoot=FString(std::string(4097,'x'));
    Reject(Bad,"preset enforces the identity byte limit");
    Bad=P; Bad.Workload.Revision=FString(std::string("a\0b",3));
    Reject(Bad,"preset rejects embedded NUL in identity strings");
    Bad=P; Bad.Workload.Revision=FString(std::string("\xc0\xaf",2));
    Reject(Bad,"preset rejects malformed UTF-8 before writer normalization");
    Bad=P;
    Bad.Workload.Revision=Bad.Workload.ProductionRoot=Bad.SourceContext.Backend=Bad.SourceContext.SoftwareRevision=
        FString(std::string(4096,'\1'));
    Reject(Bad,"escaped identity expansion cannot exceed the complete encoded byte budget");
    for (const auto& Profile : FOutputTransformSettingsValidator().GetProfiles())
    {
        auto Changed=P; Changed.Output.RequestedProfileId=Profile.ProfileId;
        Changed.Camera.YawRadians=std::numeric_limits<float>::denorm_min();
        Changed.Output.DebugBypass.VisualizationMinimum=-0.0f;
        Changed.Output.UIWhiteMultiplier=2;
        FLabPreset Restored; TArray<uint8> RoundTrip;
        Check(FLabPresetCodec::Encode(Changed,Stages,Encoded,Reason) &&
            FLabPresetCodec::Decode(Encoded,P.Workload,Stages,Restored,Reason) &&
            std::bit_cast<uint32>(Restored.Camera.YawRadians)==std::bit_cast<uint32>(Changed.Camera.YawRadians) &&
            !std::signbit(Restored.Output.DebugBypass.VisualizationMinimum) &&
            FLabPresetCodec::Encode(Restored,Stages,RoundTrip,Reason) && RoundTrip==Encoded,
            "all frozen profiles round-trip subnormal float32 and normalized debug zero");
    }
    FLabPreset Decoded;
    Check(FLabPresetCodec::Decode(Golden,P.Workload,Stages,Decoded,Reason) &&
        FLabPresetCodec::Encode(Decoded,Stages,Encoded,Reason) && Encoded==Golden,
        "bounded preset decoder round-trips every persistent field and digest");
    const std::string Original(reinterpret_cast<const char*>(Golden.data()),Golden.size());
    const auto DecodeText=[&](const std::string& Text,FLabPreset& Out) {
        return FLabPresetCodec::Decode(std::span<const uint8>(reinterpret_cast<const uint8*>(Text.data()),Text.size()),
            P.Workload,Stages,Out,Reason);
    };
    auto Reordered=Original;
    const std::string Authority="\"authority\":\"interactive-preview\",";
    Reordered.erase(Reordered.find(Authority),Authority.size());
    Reordered.insert(Reordered.size()-1,",\"authority\":\"interactive-preview\"");
    Reordered=" \n\t"+Reordered+"\n";
    Check(DecodeText(Reordered,Decoded) && FLabPresetCodec::Encode(Decoded,Stages,Encoded,Reason) && Encoded==Golden,
        "preset digest accepts ordinary whitespace and different key order");
    auto Escaped=Original;
    Escaped.replace(Escaped.find("灯笼😀"),std::string("灯笼😀").size(),"\\u706f\\u7b3c\\ud83d\\ude00");
    Check(DecodeText(Escaped,Decoded) && FLabPresetCodec::Encode(Decoded,Stages,Encoded,Reason) && Encoded==Golden,
        "equivalent escaped Unicode preserves the canonical preset digest");
    auto Foreign=P.Workload; Foreign.Revision="wrong-workload";
    Decoded=P;
    Check(!FLabPresetCodec::Decode(Golden,Foreign,Stages,Decoded,Reason) && Decoded.Workload==P.Workload,
        "wrong workload rejects the entire preset without replacing the output record");
    auto OtherTarget=P; OtherTarget.SourceContext.Backend="Vulkan";
    OtherTarget.SourceContext.CookedGeneration=FString(std::string(64,'3'));
    OtherTarget.SourceContext.DrawableExtent={640,360};
    Check(FLabPresetCodec::Encode(OtherTarget,Stages,Encoded,Reason) &&
        FLabPresetCodec::Decode(Encoded,P.Workload,Stages,Decoded,Reason) &&
        Decoded.Camera.Position.X==P.Camera.Position.X && Decoded.SourceContext.DrawableExtent==FWindowExtent({640,360}),
        "equivalent workload identity permits different backend, generation and exported aspect");
    const auto RejectText=[&](std::string Text,const char* Name) {
        FLabPreset Before=P; Before.Camera.CameraRevision=99;
        const bool Rejected=!DecodeText(Text,Before);
        Check(Rejected && !Reason.IsEmpty() && Before.Camera.CameraRevision==99 &&
            FLabPresetCodec::Encode(Before,Stages,Encoded,Reason) && Encoded==Golden,Name);
    };
    const auto Changed=[&](const std::string& From,const std::string& To) {
        auto S=Original; S.replace(S.find(From),From.size(),To); return S;
    };
    RejectText(Changed("\"schemaVersion\":1","\"schemaVersion\":2"),"unknown preset version is rejected atomically");
    RejectText(Changed("\"schemaVersion\":1","\"schemaVersion\":1,\"schemaVersion\":1"),"duplicate preset keys are rejected atomically");
    RejectText(Changed("\"whiteMultiplier\":0.5","\"whiteMultiplier\":0.5,\"visible\":true"),"unknown nested preset fields are rejected");
    RejectText(Changed("\"exposureStops\":-3.0","\"exposureStops\":1e999"),"overflowing JSON numbers cannot enter a preset");
    RejectText(Changed("\"exposureStops\":-3.0","\"exposureStops\":1e39"),"finite double values overflowing float32 are rejected");
    RejectText(Changed("\"exposureStops\":-3.0","\"exposureStops\":-2.0"),"changed valid values fail the original preset digest");
    RejectText(Changed("ManualExposure","UnknownStage"),"import rejects unavailable debug stages");
    RejectText(Changed("SceneLinearRec709D65","DisplayLinearRec709D65"),"import rejects mismatched debug domains");
    RejectText(Changed("fixture-lantern-v1","fixture\\u0000lantern"),"escaped NUL cannot truncate workload identity");
    RejectText(Changed("灯笼😀",std::string("\xc0\xaf",2)),"malformed Unicode is rejected by the bounded parser");
    RejectText(Original+"{}","trailing JSON data is rejected");
    RejectText(std::string(65537,' '),"preset size is rejected before parser allocation");
    RejectText(std::string(9,'[')+"0"+std::string(9,']'),"over-depth JSON is rejected before schema decoding");
    std::string Many="[";
    for (int I=0;I<257;++I) Many+=(I ? ",0" : "0");
    RejectText(Many+"]","over-budget JSON values are rejected before schema decoding");
    return Failed;
}
