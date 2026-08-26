#include "FBoundCookedGeneration.h"

#include "Asset/FAssetCookContractCodec.h"
#include "Asset/FAssetCookedEnvelopeAuthentication.h"
#include "Core/FPlatformFileSystem.h"

#include <filesystem>
#include <algorithm>
#include <utility>

namespace Stoner::Asset::Private
{
namespace
{
Core::FString Join(const Core::FString& Root, const char* Relative)
{
    return Core::FString((std::filesystem::path(Root.ToStdString()) / Relative)
        .lexically_normal().generic_string());
}

void Diagnostic(
    FAssetDiagnosticList& Out,
    EAssetResult Result,
    const char* Code,
    const char* Message)
{
    FAssetDiagnostic Value;
    Value.Result = Result;
    Value.Stage = EAssetStage::Validate;
    Value.Code = Core::FString(Code);
    Value.Reason = Core::FString(Message);
    Out.push_back(std::move(Value));
}

bool SupportsRequiredExtension(
    const FAssetExtensionRegistry& Registry,
    const Core::FString& Required)
{
    for (const EAssetExtensionKind Kind : {
             EAssetExtensionKind::Resolver,
             EAssetExtensionKind::Importer,
             EAssetExtensionKind::Loader,
             EAssetExtensionKind::Cooker})
    {
        const auto Capabilities = Registry.Snapshot(Kind);
        if (std::any_of(
                Capabilities.begin(), Capabilities.end(),
                [&Required](const FAssetExtensionCapability& Capability)
                {
                    return Capability.bRuntimeCompatible &&
                        Capability.Participant.ToString() == Required;
                }))
            return true;
    }
    return false;
}
} // namespace

EAssetResult FBoundCookedGeneration::Bind(
    const FAssetManagerConfig& Config,
    FBoundCookedGeneration& OutGeneration,
    FAssetDiagnosticList& OutDiagnostics)
{
    OutGeneration.Reset();
    OutDiagnostics.clear();
    if (Config.Validate() != EAssetResult::Success ||
        Config.Mode != EAssetManagerMode::StrictCooked)
        return EAssetResult::InvalidInput;

    if (Config.CookedGenerationValidation)
    {
        const auto& Validated = *Config.CookedGenerationValidation;
        const auto Authentication =
            Config.CookedEnvelopeAuthentication
                ? Config.CookedEnvelopeAuthentication->Inspect()
                : FAssetCookedEnvelopeAuthenticationInspection{};
        const Core::FString ExpectedGenerationDirectory = Join(
            Config.PublicationRoot,
            ("Generations/" +
                Validated.Pointer.GenerationId.ToLowerHex().ToStdString()).
                    c_str());
        if (!Validated.Succeeded() ||
            Validated.Category != EPublishedCorruptionCategory::None ||
            Validated.Pointer.Validate() != EAssetResult::Success ||
            Validated.Manifest.Validate() != EAssetResult::Success ||
            Validated.Pointer.GenerationId !=
                Validated.Manifest.GenerationId ||
            Validated.Pointer.ManifestDigest != Validated.ManifestDigest ||
            Validated.GenerationDirectory != ExpectedGenerationDirectory ||
            Validated.IndexedPayloads != Validated.Manifest.Records.size() ||
            Validated.UnexpectedFiles != 0 ||
            Validated.Manifest.TargetProfile.EffectiveProfileDigest !=
                Config.TargetEvidence->EffectiveProfileDigest ||
            !Config.CookedEnvelopeAuthentication->MatchesBinding(
                Config.PublicationRoot,
                Validated.Pointer.GenerationId) ||
            !Authentication.bReaderLeaseHeld ||
            Authentication.Capacity < Validated.Manifest.Records.size())
        {
            Diagnostic(OutDiagnostics, EAssetResult::Conflict,
                "runtime.generation.validation-authority-invalid",
                "Validated generation authority does not match the held reader lease");
            return EAssetResult::Conflict;
        }
        for (const Core::FString& Required :
             Validated.Manifest.RequiredExtensions)
        {
            if (!SupportsRequiredExtension(*Config.ExtensionRegistry, Required))
            {
                Diagnostic(OutDiagnostics,
                    EAssetResult::UnknownRequiredExtension,
                    "runtime.generation.extension-missing",
                    "A required runtime extension is unavailable");
                return EAssetResult::UnknownRequiredExtension;
            }
        }
        OutGeneration.ValidatedGeneration_ =
            Config.CookedGenerationValidation;
        OutGeneration.Authentication_ =
            Config.CookedEnvelopeAuthentication;
        return EAssetResult::Success;
    }

    Core::TArray<Core::uint8> PointerBytes;
    Core::FPlatformFileInfo PointerInfo;
    const Core::FString PointerPath = Join(Config.PublicationRoot, "Current.json");
    if (!Core::FPlatformFileSystem::QueryRegularFile(
            PointerPath, 1024ULL * 1024ULL, PointerInfo).IsSuccess() ||
        !Core::FPlatformFileSystem::ReadFile(PointerPath, PointerBytes) ||
        PointerBytes.size() != PointerInfo.ByteSize)
    {
        Diagnostic(OutDiagnostics, EAssetResult::NotFound,
            "runtime.generation.pointer-missing", "Current pointer is unavailable");
        return EAssetResult::NotFound;
    }
    FCurrentGenerationPointer Pointer;
    if (FAssetCookContractCodec::ParseCurrentPointer(
            PointerBytes, Pointer) != EAssetResult::Success)
    {
        Diagnostic(OutDiagnostics, EAssetResult::CorruptPayload,
            "runtime.generation.pointer-invalid", "Current pointer is invalid");
        return EAssetResult::CorruptPayload;
    }

    FGenerationReaderLease Lease;
    const EAssetResult LeaseResult = FGenerationReaderLease::Acquire(
        Config.PublicationRoot, Config.LeaseCoordinationRoot,
        Pointer.GenerationId, Config.LeaseTimeoutMilliseconds, Lease);
    if (LeaseResult != EAssetResult::Success)
    {
        Diagnostic(OutDiagnostics, LeaseResult,
            "runtime.generation.lease-failed", "Generation reader ownership failed");
        return LeaseResult;
    }

    const Core::FString GenerationDirectory = Join(
        Config.PublicationRoot,
        ("Generations/" + Pointer.GenerationId.ToLowerHex().ToStdString()).c_str());
    FPublishedGenerationValidationRequest Request;
    Request.SubjectRoot = GenerationDirectory;
    Request.Subject = EPublishedValidationSubject::GenerationDirectory;
    Request.Policy = EPublishedGenerationValidationPolicy::IndexAndLayout;
    Request.ExpectedGenerationId = Pointer.GenerationId;
    const auto Validated = FPublishedGenerationValidator::Validate(Request);
    if (!Validated.Succeeded() ||
        Validated.ManifestDigest != Pointer.ManifestDigest ||
        Validated.Manifest.TargetProfile.EffectiveProfileDigest !=
            Config.TargetEvidence->EffectiveProfileDigest)
    {
        Diagnostic(OutDiagnostics, EAssetResult::Conflict,
            "runtime.generation.validation-failed",
            "Bound generation does not match pointer or target evidence");
        return EAssetResult::Conflict;
    }
    for (const Core::FString& Required :
         Validated.Manifest.RequiredExtensions)
    {
        if (!SupportsRequiredExtension(*Config.ExtensionRegistry, Required))
        {
            Diagnostic(OutDiagnostics,
                EAssetResult::UnknownRequiredExtension,
                "runtime.generation.extension-missing",
                "A required runtime extension is unavailable");
            return EAssetResult::UnknownRequiredExtension;
        }
    }

    OutGeneration.GenerationDirectory_ = GenerationDirectory;
    OutGeneration.Pointer_ = std::move(Pointer);
    OutGeneration.Manifest_ = Validated.Manifest;
    OutGeneration.ReaderLease_ = std::move(Lease);
    return EAssetResult::Success;
}

bool FBoundCookedGeneration::IsBound() const noexcept
{
    return ReaderLease_.IsHeld() ||
        (Authentication_ &&
            Authentication_->Inspect().bReaderLeaseHeld);
}

void FBoundCookedGeneration::Reset() noexcept
{
    ValidatedGeneration_.reset();
    Authentication_.reset();
    ReaderLease_.Release();
    GenerationDirectory_ = {};
    Pointer_ = {};
    Manifest_ = {};
}

} // namespace Stoner::Asset::Private
