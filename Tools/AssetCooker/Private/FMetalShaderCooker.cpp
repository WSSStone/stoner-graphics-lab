#include "FMetalShaderCooker.h"

#include "Asset/FAssetCookContractCodec.h"

#include <algorithm>
#include <filesystem>
#include <span>
#include <string>

namespace Stoner::AssetCooker::Private
{
namespace
{

Asset::FAssetDigest TextDigest(std::string_view Text)
{
    return Asset::FAssetDigest::FromBytes(std::span<const Core::uint8>(
        reinterpret_cast<const Core::uint8*>(Text.data()), Text.size()));
}

const Asset::FAssetShaderPayloadChoice* MetalChoice(
    const Asset::FAssetTargetProfile& Profile)
{
    const auto Found = std::find_if(
        Profile.ShaderPayloadChoices.begin(),
        Profile.ShaderPayloadChoices.end(),
        [](const auto& Choice)
        {
            return Choice.Backend == Asset::EAssetGraphicsBackend::Metal &&
                Choice.Format == Asset::EAssetShaderPayloadFormat::MetalLibrary;
        });
    return Found == Profile.ShaderPayloadChoices.end() ? nullptr : &*Found;
}

bool StringSetting(
    const Asset::FAssetProducerSettingsRecord& Record,
    const char* Name,
    const char* Expected)
{
    const auto* Setting = Record.Find(Core::FString(Name));
    const auto* Value = Setting
        ? std::get_if<Core::FString>(&Setting->Value) : nullptr;
    return Value && *Value == Core::FString(Expected);
}

Asset::EAssetResult FinalizeResult(EMetalLibraryFinalizeStatus Status)
{
    switch (Status)
    {
    case EMetalLibraryFinalizeStatus::Success:
        return Asset::EAssetResult::Success;
    case EMetalLibraryFinalizeStatus::HostUnsupported:
    case EMetalLibraryFinalizeStatus::ToolchainUnavailable:
        return Asset::EAssetResult::TargetUnavailable;
    case EMetalLibraryFinalizeStatus::TimedOut:
        return Asset::EAssetResult::DeadlineExceeded;
    case EMetalLibraryFinalizeStatus::InvalidRequest:
    case EMetalLibraryFinalizeStatus::EvidenceMismatch:
        return Asset::EAssetResult::InvalidInput;
    case EMetalLibraryFinalizeStatus::CompilerFailed:
    case EMetalLibraryFinalizeStatus::EmptyOutput:
    case EMetalLibraryFinalizeStatus::IoFailure:
        return Asset::EAssetResult::CookFailure;
    }
    return Asset::EAssetResult::CookFailure;
}

} // namespace

Asset::FAssetParticipantId FMetalShaderCooker::ParticipantId()
{
    Asset::FAssetParticipantId Value;
    (void)Asset::FAssetParticipantId::Create(
        Core::FString("cooker.metal-shader"), Value);
    return Value;
}

Asset::FAssetProducerVersion FMetalShaderCooker::ProducerVersion()
{
    Asset::FAssetProducerVersion Value;
    (void)Asset::FAssetProducerVersion::Create(
        Core::FString("027-v2"), Value);
    return Value;
}

Asset::FAssetExtensionCapability FMetalShaderCooker::GetCapability() const
{
    Asset::FAssetExtensionCapability Result;
    Result.Kind = Asset::EAssetExtensionKind::Cooker;
    Result.Participant = ParticipantId();
    Result.ProducerVersion = ProducerVersion();
    Result.Priority = 200;
    Result.FormatHints = {Core::FString("spirv")};
    return Result;
}

Asset::EAssetResult FMetalShaderCooker::GetRelevantProfileEvidence(
    const Asset::FAssetTargetProfileEvidence& Profile,
    Asset::FAssetProfileProjectionEvidence& OutEvidence) const
{
    OutEvidence = {};
    const auto* Record =
        Profile.Profile.BuildPolicy.FindProducer(ParticipantId());
    if (!Record || Record->SchemaVersion != 1 ||
        Record->Settings.size() != 5 ||
        !StringSetting(*Record, "bindingPolicy", "metal-direct-binding-v1") ||
        !StringSetting(*Record, "deploymentTarget", "12.0") ||
        !StringSetting(*Record, "finalization", "required") ||
        !StringSetting(*Record, "mslVersion", "2.4") ||
        !StringSetting(
            *Record, "spirvCrossCommit",
            "a0fba56c34a6700f1724bf9b751da5b488a3775c"))
        return Asset::EAssetResult::InvalidInput;
    const Core::TArray<Core::FString> Fields = {
        Core::FString("cpuArchitecture"),
        Core::FString("graphicsBackend"),
        Core::FString("metalShaderTarget"),
        Core::FString("platform"),
        Core::FString("shaderPayloadChoices")};
    return Asset::FAssetCookContractCodec::BuildProfileProjection(
        Profile, ParticipantId(), 1, Fields, OutEvidence);
}

Asset::EAssetResult BuildMetalShaderDerivedEvidence(
    const Asset::FShaderPayloadAsset& Payload,
    const FMetalShaderCookParameters& Parameters,
    const Asset::FAssetTargetProfileEvidence& Profile,
    FSpirvCrossMslResult& OutDerivation,
    Core::TArray<Asset::FAssetDerivedNamedEvidence>& OutEvidence) noexcept
{
    OutDerivation = {};
    OutEvidence.clear();
    if (Payload.GetBackend() != Asset::EShaderBackendFamily::Vulkan ||
        Payload.GetFormat() != Asset::EShaderPayloadFormat::SPIRV ||
        !Parameters.ShaderAssetId.IsValid() ||
        !Parameters.ShaderAssetVersion.IsAvailable() ||
        !Parameters.GlslDigest || !Parameters.GlslDigest->IsAvailable() ||
        Parameters.WorkingDirectory.IsEmpty() ||
        !Parameters.ToolchainEvidence.IsValid() ||
        (Parameters.Architecture != Core::FString("arm64") &&
         Parameters.Architecture != Core::FString("x86_64")) ||
        Profile.Validate() != Asset::EAssetResult::Success ||
        Profile.Profile.Platform != Asset::EAssetTargetPlatform::MacOS ||
        Profile.Profile.GraphicsBackend != Asset::EAssetGraphicsBackend::Metal ||
        !Profile.Profile.MetalShaderTarget || !MetalChoice(Profile.Profile))
        return Asset::EAssetResult::InvalidInput;

    FSpirvCrossMslRequest Derive;
    Derive.SpirvBytes = Payload.GetBytes();
    Derive.Stage = Payload.GetStage();
    Derive.EntryPoint = Payload.GetEntryPoint();
    Derive.InterfaceBindings = Parameters.InterfaceBindings;
    const Asset::EAssetResult Result =
        DeriveMetalShaderSource(Derive, OutDerivation);
    if (Result != Asset::EAssetResult::Success) return Result;

    const auto& Metal = *Profile.Profile.MetalShaderTarget;
    OutEvidence = {
        {Core::FString("authoritative-glsl"), *Parameters.GlslDigest},
        {Core::FString("apple-toolchain"),
         TextDigest(
             Parameters.ToolchainEvidence.MetalCompiler.ToStdString() + ";" +
             Parameters.ToolchainEvidence.XcodeBuild.ToStdString() + ";" +
             Parameters.ToolchainEvidence.Sdk.ToStdString())},
        {Core::FString("binding-policy"),
         OutDerivation.BindingEvidence.CanonicalDigest},
        {Core::FString("native-library-contract"),
         TextDigest("metal-library;schema=2;finalization=required")},
        {Core::FString("target-profile"),
         Profile.EffectiveProfileDigest},
        {Core::FString("transformation"),
         OutDerivation.OptionsDigest}};
    const std::string Target =
        Metal.DeploymentTarget.ToStdString() + ";" +
        Metal.MslVersion.ToStdString() + ";" +
        Parameters.Architecture.ToStdString();
    OutEvidence.push_back({Core::FString("target-tuple"), TextDigest(Target)});
    std::sort(
        OutEvidence.begin(), OutEvidence.end(),
        [](const auto& Left, const auto& Right)
        { return Left.Name < Right.Name; });
    return Asset::EAssetResult::Success;
}

Asset::FAssetCookResult FMetalShaderCooker::Cook(
    const Asset::FAssetCookRequest& Request)
{
    Asset::FAssetCookResult Result;
    Result.TargetProfileEvidence = Request.TargetProfileEvidence;
    Result.TargetProfile = Request.TargetProfile;
    const auto Payload =
        std::dynamic_pointer_cast<const Asset::FShaderPayloadAsset>(
            Request.Payload);
    const auto Parameters =
        std::dynamic_pointer_cast<const FMetalShaderCookParameters>(
            Request.Parameters);
    if (!Payload || !Parameters || !Request.TargetProfileEvidence ||
        Request.Metadata.Validate() != Asset::EAssetResult::Success ||
        Request.Metadata.Id != Payload->GetId() ||
        GetRelevantProfileEvidence(
            *Request.TargetProfileEvidence,
            Result.ProfileProjection) != Asset::EAssetResult::Success)
    {
        Result.Result = Asset::EAssetResult::InvalidInput;
        return Result;
    }
    const auto* Choice = MetalChoice(Request.TargetProfileEvidence->Profile);
    FSpirvCrossMslResult Derived;
    Core::TArray<Asset::FAssetDerivedNamedEvidence> DerivedEvidence;
    Result.Result = BuildMetalShaderDerivedEvidence(
        *Payload, *Parameters, *Request.TargetProfileEvidence,
        Derived, DerivedEvidence);
    if (Result.Result != Asset::EAssetResult::Success || !Choice)
        return Result;

    FMetalShaderEvidence Evidence;
    Evidence.ShaderAssetId = Parameters->ShaderAssetId;
    Evidence.ShaderAssetVersion = Parameters->ShaderAssetVersion;
    Evidence.GlslDigest = Parameters->GlslDigest;
    Evidence.SpirvDigest = Derived.SpirvDigest;
    Evidence.Stage = Payload->GetStage();
    Evidence.EntryPoint = Payload->GetEntryPoint();
    Evidence.InterfaceDigest = Derived.InterfaceDigest;
    Evidence.SpirvCrossOptionsDigest = Derived.OptionsDigest;
    Evidence.BindingEvidence = Derived.BindingEvidence;
    Evidence.TargetProfile = Choice->Profile;
    Evidence.NormalizedMslDigest = Derived.NormalizedMslDigest;
    if (FinalizeMetalShaderEvidence(Evidence) !=
        Asset::EAssetResult::Success)
    {
        Result.Result = Asset::EAssetResult::InvalidInput;
        return Result;
    }

    FMetalLibraryCompileRequest Finalize;
    Finalize.WorkingDirectory = Parameters->WorkingDirectory;
    Finalize.Architecture = Parameters->Architecture;
    Finalize.TargetProfile = Choice->Profile;
    Finalize.NormalizedMsl = Derived.NormalizedMsl;
    Finalize.DerivationEvidence = Evidence;
    std::error_code WorkingDirectoryError;
    std::filesystem::create_directories(
        Finalize.WorkingDirectory.ToStdString(), WorkingDirectoryError);
    if (WorkingDirectoryError)
    {
        Result.Result = Asset::EAssetResult::AccessDenied;
        return Result;
    }
    FMetalLibraryCompileResult Library =
        FinalizeMetalLibrary(Finalize, Parameters->ToolExecutor);
    Result.Result = FinalizeResult(Library.Status);
    if (Result.Result != Asset::EAssetResult::Success) return Result;
    if (Library.Toolchain != Parameters->ToolchainEvidence)
    {
        Result.Result = Asset::EAssetResult::SourceChanged;
        return Result;
    }

    Asset::FAssetVersion Version = Payload->GetVersion();
    Version.ContentDigest =
        Asset::FAssetDigest::FromBytes(Library.LibraryBytes);
    Version.CookDigest = Version.ContentDigest;
    Version.Producer = ParticipantId();
    Version.ProducerVersion = ProducerVersion();
    Version.TargetProfile = Choice->Profile;
    const auto& Native = *Library.NativeEvidence.NativeLibrary;
    Asset::FShaderNativeLibraryEvidence NativeLibraryEvidence;
    NativeLibraryEvidence.DerivationEvidenceDigest = Evidence.EvidenceDigest;
    NativeLibraryEvidence.TargetProfile = Choice->Profile;
    NativeLibraryEvidence.Architecture = Native.Architecture;
    NativeLibraryEvidence.Compiler = Native.Compiler;
    NativeLibraryEvidence.XcodeBuild = Native.XcodeBuild;
    NativeLibraryEvidence.Sdk = Native.Sdk;
    NativeLibraryEvidence.DeploymentTarget = Evidence.DeploymentTarget;
    NativeLibraryEvidence.LanguageVersion = Evidence.MslVersion;
    NativeLibraryEvidence.ArgumentDigest = Native.ArgumentDigest;
    NativeLibraryEvidence.LibraryDigest = Native.LibraryDigest;
    NativeLibraryEvidence.SizeBytes = Native.SizeBytes;
    NativeLibraryEvidence.Finalizer = ParticipantId();
    NativeLibraryEvidence.FinalizerVersion = ProducerVersion();
    if (Asset::FinalizeShaderNativeLibraryEvidence(NativeLibraryEvidence) !=
        Asset::EAssetResult::Success)
    {
        Result.Result = Asset::EAssetResult::InvalidInput;
        return Result;
    }
    Asset::FShaderPayloadAsset MetalPayload;
    Result.Result = Asset::FShaderPayloadAsset::CreateWithNativeEvidence(
        Payload->GetId(), std::move(Version),
        Asset::EShaderBackendFamily::Metal, Choice->Profile,
        Asset::EShaderPayloadFormat::MetalLibrary, Payload->GetStage(),
        Payload->GetEntryPoint(), Payload->GetPermutation(),
        std::move(Library.LibraryBytes), Derived.BindingEvidence,
        std::move(NativeLibraryEvidence), MetalPayload);
    if (Result.Result != Asset::EAssetResult::Success) return Result;
    Result.Payload =
        Core::MakeShared<const Asset::FShaderPayloadAsset>(
            std::move(MetalPayload));
    Asset::FAssetCookedPayloadEnvelope Envelope;
    Result.Result = Asset::FAssetCookContractCodec::WriteTypedPayload(
        *Result.Payload, {}, Result.Artifact, &Envelope);
    if (Result.Result != Asset::EAssetResult::Success)
    {
        Result.Payload.reset();
        Result.Artifact.clear();
        return Result;
    }
    Result.CookDigest = Envelope.EnvelopeDigest;
    return Result;
}

} // namespace Stoner::AssetCooker::Private
