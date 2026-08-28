#include "ProductionContentEquivalenceTests.h"

#include "Asset/AssetMinimal.h"
#include "Core/FPlatformFileSystem.h"
#include "ProductionAssetClosureTestSupport.h"
#include "ProductionAssetEquivalence.h"

#include <cstdlib>
#include <charconv>
#include <filesystem>
#include <iostream>
#include <string>

namespace
{
using namespace Stoner;
using namespace Stoner::Asset;

void Record(
    FProductionContentEquivalenceTestResult& Result,
    bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

const char* Environment(const char* Name)
{
    const char* Value = std::getenv(Name);
    return Value && *Value != '\0' ? Value : nullptr;
}

Core::uint64 RequestTimeoutMilliseconds()
{
    constexpr Core::uint64 DefaultSeconds = 30;
    constexpr Core::uint64 MaximumSeconds = 600;
    const char* Value = Environment(
        "STONER_PRODUCTION_REQUEST_TIMEOUT_SECONDS");
    if (!Value) return DefaultSeconds * 1000;
    Core::uint64 Seconds = 0;
    const std::string_view Text(Value);
    const auto Parsed = std::from_chars(
        Text.data(), Text.data() + Text.size(), Seconds);
    if (Parsed.ec != std::errc{} ||
        Parsed.ptr != Text.data() + Text.size() ||
        Seconds < DefaultSeconds || Seconds > MaximumSeconds)
        return DefaultSeconds * 1000;
    return Seconds * 1000;
}

class FDirectoryRevocation
{
public:
    explicit FDirectoryRevocation(std::filesystem::path Original)
        : Original_(std::move(Original)),
          Hidden_(Original_.string() + ".revoked")
    {
    }

    [[nodiscard]] bool Revoke()
    {
        std::error_code Error;
        std::filesystem::remove_all(Hidden_, Error);
        Error.clear();
        std::filesystem::rename(Original_, Hidden_, Error);
        bRevoked_ = !Error;
        return bRevoked_;
    }

    ~FDirectoryRevocation()
    {
        if (!bRevoked_) return;
        std::error_code Error;
        std::filesystem::rename(Hidden_, Original_, Error);
    }

private:
    std::filesystem::path Original_;
    std::filesystem::path Hidden_;
    bool bRevoked_ = false;
};

} // namespace

FProductionContentEquivalenceTestResult
RunProductionContentEquivalenceTests()
{
    FProductionContentEquivalenceTestResult Result;
    const char* Publication = Environment(
        "STONER_PRODUCTION_PUBLICATION_ROOT");
    const char* Coordination = Environment(
        "STONER_PRODUCTION_LEASE_ROOT");
    const char* TargetProfile = Environment(
        "STONER_PRODUCTION_TARGET_PROFILE");
    const char* PackageRoot = Environment(
        "STONER_PRODUCTION_PACKAGE_ROOT");
    const char* ShaderRoot = Environment(
        "STONER_PRODUCTION_SHADER_ROOT");
    if (!Publication || !Coordination || !TargetProfile ||
        !PackageRoot || !ShaderRoot)
    {
        Record(Result, false,
            "production equivalence requires explicit publication, lease, target, package, and shader roots");
        return Result;
    }
    const Core::uint64 RequestTimeout = RequestTimeoutMilliseconds();

    FPublishedGenerationValidationRequest ValidationRequest;
    ValidationRequest.SubjectRoot = Core::FString(Publication);
    const FPublishedGenerationValidationResult Validated =
        FPublishedGenerationValidator::Validate(ValidationRequest);
    Record(Result,
        Validated.Succeeded() && Validated.ValidatedPayloads ==
            Validated.Manifest.Records.size(),
        "strict generation validates every indexed payload before manager construction");
    if (!Validated.Succeeded()) return Result;

    Core::TArray<Core::uint8> ProfileBytes;
    FAssetTargetProfileEvidence Profile;
    const bool ProfileValid = Core::FPlatformFileSystem::ReadFile(
            Core::FString(TargetProfile), ProfileBytes) &&
        FAssetCookContractCodec::ParseTargetProfile(
            ProfileBytes, Profile) == EAssetResult::Success &&
        Profile == Validated.Manifest.TargetProfile;
    Record(Result, ProfileValid,
        "strict generation target evidence exactly matches the requested profile");
    if (!ProfileValid) return Result;

    FProductionAssetExtensionSet Extensions;
    const bool ExtensionsReady = CreateProductionAssetExtensionSet(
        PackageRoot, ShaderRoot, Extensions);
    Record(Result, ExtensionsReady,
        "runtime extension registry composes source importers, cooked loaders, and KTX2 loader");
    if (!ExtensionsReady) return Result;

    auto SharedProfile =
        Core::MakeShared<const FAssetTargetProfileEvidence>(Profile);
    FAssetManagerConfig DevelopmentConfig;
    DevelopmentConfig.Mode = EAssetManagerMode::DevelopmentSource;
    DevelopmentConfig.ExtensionRegistry = Extensions.Registry;
    DevelopmentConfig.SourceRoot = Core::FString("production");
    DevelopmentConfig.TargetEvidence = SharedProfile;
    DevelopmentConfig.WorkerCount = 4;
    DevelopmentConfig.ExtensionDeadlineMilliseconds = RequestTimeout;
    DevelopmentConfig.Limits.MaxPayloadBytes =
        1024ULL * 1024ULL * 1024ULL;
    DevelopmentConfig.Limits.MaxAggregatePayloadBytes =
        8ULL * 1024ULL * 1024ULL * 1024ULL;
    Core::TSharedPtr<FAssetManager> DevelopmentManager;
    FAssetDiagnosticList Diagnostics;
    const bool DevelopmentCreated = FAssetManager::Create(
        DevelopmentConfig, DevelopmentManager, Diagnostics) ==
        EAssetResult::Success;
    FProductionAssetClosure DevelopmentClosure;
    Core::FString DevelopmentFailure;
    const bool DevelopmentLoaded = DevelopmentCreated &&
        LoadProductionAssetClosure(
            *DevelopmentManager,
            Validated.Manifest,
            false,
            DevelopmentClosure,
            DevelopmentFailure,
            RequestTimeout);
    const FAssetManagerInspection DevelopmentInspection = DevelopmentCreated
        ? DevelopmentManager->Inspect() : FAssetManagerInspection{};
    Record(Result,
        DevelopmentLoaded &&
            DevelopmentClosure.Entries.size() ==
                Validated.Manifest.Records.size() &&
            DevelopmentInspection.ResolverExecutions > 0 &&
            DevelopmentInspection.ImporterExecutions > 0 &&
            DevelopmentInspection.AuthoringDecoderExecutions > 0,
        "development mode loads and retains the complete typed production closure");
    if (!DevelopmentLoaded)
    {
        std::cout << "  development closure failure="
                  << DevelopmentFailure.CStr() << '\n';
        for (const auto& Operation : DevelopmentInspection.Operations)
        {
            if (Operation.Result == EAssetResult::Success ||
                Operation.FailurePath.empty())
                continue;
            std::cout << "  development failure path=";
            for (Core::usize Index = 0;
                 Index < Operation.FailurePath.size(); ++Index)
            {
                if (Index != 0) std::cout << " -> ";
                std::cout << Operation.FailurePath[Index]
                    .ToString().CStr();
            }
            std::cout << " result="
                      << static_cast<unsigned int>(Operation.Result)
                      << '\n';
        }
        if (DevelopmentManager) (void)DevelopmentManager->Shutdown();
        return Result;
    }

    FDirectoryRevocation PackageRevocation(PackageRoot);
    FDirectoryRevocation ShaderRevocation(ShaderRoot);
    const bool SourcesRevoked = PackageRevocation.Revoke() &&
        ShaderRevocation.Revoke();
    Record(Result, SourcesRevoked,
        "authoritative package and shader source roots are unavailable before strict manager construction");
    if (!SourcesRevoked)
    {
        (void)DevelopmentManager->Shutdown();
        return Result;
    }

    std::filesystem::create_directories(Coordination);
    FAssetManagerConfig CookedConfig;
    CookedConfig.Mode = EAssetManagerMode::StrictCooked;
    CookedConfig.ExtensionRegistry = Extensions.Registry;
    CookedConfig.PublicationRoot = Core::FString(Publication);
    CookedConfig.LeaseCoordinationRoot = Core::FString(Coordination);
    CookedConfig.TargetEvidence = SharedProfile;
    CookedConfig.WorkerCount = 4;
    CookedConfig.ExtensionDeadlineMilliseconds = RequestTimeout;
    CookedConfig.Limits = DevelopmentConfig.Limits;
    Core::TSharedPtr<FAssetManager> CookedManager;
    Diagnostics.clear();
    const EAssetResult CookedCreateResult = FAssetManager::Create(
        CookedConfig, CookedManager, Diagnostics);
    const bool CookedCreated = CookedCreateResult == EAssetResult::Success;
    FProductionAssetClosure CookedClosure;
    Core::FString CookedFailure;
    const bool CookedLoaded = CookedCreated &&
        LoadProductionAssetClosure(
            *CookedManager,
            Validated.Manifest,
            true,
            CookedClosure,
            CookedFailure,
            RequestTimeout);
    const FAssetManagerInspection CookedInspection = CookedCreated
        ? CookedManager->Inspect() : FAssetManagerInspection{};
    Record(Result,
        CookedLoaded &&
            CookedClosure.Entries.size() ==
                Validated.Manifest.Records.size() &&
            CookedInspection.BoundGeneration ==
                Validated.Manifest.GenerationId &&
            CookedInspection.ResolverExecutions == 0 &&
            CookedInspection.ImporterExecutions == 0 &&
            CookedInspection.AuthoringDecoderExecutions == 0 &&
            CookedInspection.SourceFallbackExecutions == 0 &&
            CookedInspection.StrictLoaderExecutions >=
                Validated.Manifest.Records.size(),
        "strict mode loads the complete closure with zero source participants or fallback");
    if (!CookedLoaded)
    {
        std::cout << "  strict closure failure="
                  << CookedFailure.CStr() << '\n';
        if (!CookedCreated)
            std::cout << "  strict manager create result="
                      << static_cast<unsigned int>(CookedCreateResult)
                      << " diagnostics="
                      << FAssetDiagnostics::FormatNormalized(Diagnostics).CStr()
                      << '\n';
    }

    FProductionAssetEquivalenceReport Equivalence;
    const bool Equivalent = CookedLoaded &&
        CompareProductionAssetClosures(
            DevelopmentClosure, CookedClosure, Profile, Equivalence);
    Record(Result,
        Equivalent && Equivalence.ComparedAssets ==
            Validated.Manifest.Records.size() &&
            Equivalence.ComparedGeometryValues > 0 &&
            Equivalence.ComparedTextureSamples > 0,
        "development and strict cooked closures are semantically equivalent across every payload family");
    if (!Equivalent)
        std::cout << "  equivalence failure="
                  << Equivalence.FirstFailure.CStr() << '\n';

    CookedClosure = {};
    DevelopmentClosure = {};
    if (CookedManager) (void)CookedManager->Shutdown();
    if (DevelopmentManager) (void)DevelopmentManager->Shutdown();
    return Result;
}
