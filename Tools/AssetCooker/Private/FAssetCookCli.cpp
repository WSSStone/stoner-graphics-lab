#include "FAssetCookCli.h"

#include "Asset/AssetMinimal.h"
#include "AssetCooker/FAssetCookRunner.h"
#include "Core/FPlatformFileSystem.h"
#include "FDerivedDataStore.h"
#include "FMetalLibraryCompiler.h"
#include "FPublishedGenerationValidator.h"

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>

namespace Stoner::AssetCooker::Private
{
namespace
{

struct FParsedOptions
{
    std::map<std::string, Core::TArray<Core::FString>> Values;
    std::set<std::string> Flags;
};

FAssetCookCliResult Failure(
    EAssetCookResultCategory Category,
    const char* Reason)
{
    FAssetCookCliResult Result;
    Result.Category = Category;
    Result.StableReason = Core::FString(Reason);
    return Result;
}

bool ParseUnsigned(
    const Core::FString& Text,
    Core::uint32 Minimum,
    Core::uint32 Maximum,
    Core::uint32& Out)
{
    Out = 0;
    const char* Begin = Text.View().data();
    const char* End = Begin + Text.Len();
    const auto Result = std::from_chars(Begin, End, Out);
    return Result.ec == std::errc{} && Result.ptr == End &&
        Out >= Minimum && Out <= Maximum;
}

bool ParseAssetId(const Core::FString& Text, Asset::FAssetId& Out)
{
    const std::string Value = Text.ToStdString();
    const std::size_t Colon = Value.find(':');
    if (Colon == std::string::npos || Colon == 0 || Colon + 1 >= Value.size())
        return false;
    const std::size_t Hash = Value.find('#', Colon + 1);
    std::optional<Core::FString> Subresource;
    if (Hash != std::string::npos)
    {
        if (Hash + 1 >= Value.size()) return false;
        Subresource = Core::FString(Value.substr(Hash + 1));
    }
    if (Asset::FAssetId::Create(
            Core::FString(Value.substr(0, Colon)),
            Core::FString(Value.substr(
                Colon + 1,
                Hash == std::string::npos
                    ? std::string::npos : Hash - Colon - 1)),
            Subresource, Out) != Asset::EAssetResult::Success)
        return false;
    return Out.ToString() == Text;
}

bool CanonicalPath(const Core::FString& Text, std::filesystem::path& Out)
{
    if (Text.IsEmpty()) return false;
    std::error_code Error;
    Out = std::filesystem::absolute(Text.ToStdString(), Error).lexically_normal();
    if (Error) return false;
    const auto Canonical = std::filesystem::weakly_canonical(Out, Error);
    if (!Error) Out = Canonical;
    return true;
}

bool Contains(
    const std::filesystem::path& Root,
    const std::filesystem::path& Candidate)
{
    auto RootPart = Root.begin();
    auto CandidatePart = Candidate.begin();
    for (; RootPart != Root.end(); ++RootPart, ++CandidatePart)
        if (CandidatePart == Candidate.end() || *RootPart != *CandidatePart)
            return false;
    return true;
}

bool ValidateReportAlias(
    const Core::FString& Report,
    std::initializer_list<Core::FString> Protected)
{
    if (Report.IsEmpty()) return true;
    std::filesystem::path ReportPath;
    if (!CanonicalPath(Report, ReportPath)) return false;
    for (const auto& Value : Protected)
    {
        if (Value.IsEmpty()) continue;
        std::filesystem::path ProtectedPath;
        if (!CanonicalPath(Value, ProtectedPath) ||
            Contains(ProtectedPath, ReportPath) ||
            Contains(ReportPath, ProtectedPath))
            return false;
    }
    return true;
}

bool IsAllowed(
    std::string_view Name,
    std::initializer_list<std::string_view> Allowed)
{
    return std::find(Allowed.begin(), Allowed.end(), Name) != Allowed.end();
}

bool ParseOptions(
    std::span<const Core::FString> Arguments,
    std::initializer_list<std::string_view> ValueOptions,
    std::initializer_list<std::string_view> Repeatable,
    std::initializer_list<std::string_view> Flags,
    FParsedOptions& Out)
{
    for (Core::usize Index = 1; Index < Arguments.size(); ++Index)
    {
        const std::string Name = Arguments[Index].ToStdString();
        if (!Name.starts_with("--")) return false;
        if (IsAllowed(Name, Flags))
        {
            if (!Out.Flags.insert(Name).second) return false;
            continue;
        }
        if (!IsAllowed(Name, ValueOptions) || Index + 1 >= Arguments.size())
            return false;
        auto& Values = Out.Values[Name];
        if (!Values.empty() && !IsAllowed(Name, Repeatable)) return false;
        const Core::FString& Value = Arguments[++Index];
        if (Value.IsEmpty() || Value.View().starts_with("--")) return false;
        Values.push_back(Value);
    }
    return true;
}

const Core::FString* One(const FParsedOptions& Options, const char* Name)
{
    const auto Iterator = Options.Values.find(Name);
    return Iterator == Options.Values.end() || Iterator->second.size() != 1
        ? nullptr : &Iterator->second.front();
}

const Core::TArray<Core::FString>& Many(
    const FParsedOptions& Options,
    const char* Name)
{
    static const Core::TArray<Core::FString> Empty;
    const auto Iterator = Options.Values.find(Name);
    return Iterator == Options.Values.end() ? Empty : Iterator->second;
}

bool ReadProfile(
    const Core::FString& Path,
    Asset::FAssetTargetProfileEvidence& Out)
{
    Core::TArray<Core::uint8> Bytes;
    return Core::FPlatformFileSystem::ReadFile(Path, Bytes) &&
        Asset::FAssetCookContractCodec::ParseTargetProfile(Bytes, Out) ==
            Asset::EAssetResult::Success;
}

Core::TArray<Core::uint8> Bytes(std::string_view Text)
{
    return {Text.begin(), Text.end()};
}

bool WriteReport(const Core::FString& Path, const Core::FString& Report)
{
    if (Path.IsEmpty()) return true;
    const auto Parent = std::filesystem::path(Path.ToStdString()).parent_path();
    if (!Parent.empty() && !Core::FPlatformFileSystem::CreateDirectory(
            Core::FString(Parent.generic_string())))
        return false;
    return Core::FPlatformFileSystem::WriteFileDurable(
        Path, Bytes(Report.View())).IsSuccess();
}

FAssetCookReportDocument BaseDocument(
    EAssetCookReportCommand Command,
    EAssetCookResultCategory Category,
    const Core::FString& Reason)
{
    FAssetCookReportDocument Document;
    Document.Command = Command;
    Document.Result = Category;
    Document.StableReason = Reason;
    if (Category != EAssetCookResultCategory::Success)
    {
        FAssetCookReportDiagnostic Diagnostic;
        Diagnostic.Category = Core::FString(
            FAssetCookReportCodec::ResultToken(Category));
        Diagnostic.Stage = Core::FString(
            FAssetCookReportCodec::CommandToken(Command));
        Diagnostic.Reason = Reason;
        Document.Diagnostics.push_back(std::move(Diagnostic));
    }
    return Document;
}

void AddManifestEvidence(
    const Asset::FAssetCookManifest& Manifest,
    FAssetCookReportDocument& Document,
    EAssetCookAction Action)
{
    Document.bHasPipeline = true;
    Document.Pipeline.EffectiveProfileDigest =
        Manifest.TargetProfile.EffectiveProfileDigest;
    Document.Pipeline.SnapshotDigest = Manifest.SnapshotDigest;
    Document.Pipeline.Counts.ReachableAssets =
        static_cast<Core::uint32>(Manifest.Records.size());
    Document.Pipeline.Counts.ReuseIneligible =
        static_cast<Core::uint32>(Manifest.Records.size());
    Document.Pipeline.Assets.reserve(Manifest.Records.size());
    for (Core::usize Index = 0; Index < Manifest.Records.size(); ++Index)
    {
        const auto& Record = Manifest.Records[Index];
        FAssetCookAssetReport Entry;
        Entry.PlanIndex = static_cast<Core::uint32>(Index);
        Entry.AssetId = Record.AssetId;
        Entry.Action = Action;
        Entry.StableReason = Core::FString("published.payload.valid");
        Entry.DerivedKey = Record.DerivedKey;
        Entry.ArtifactDigest = Record.EnvelopeDigest;
        for (const auto& Source : Record.SourceManifest)
            Entry.SourceEvidence.emplace_back(
                Source.Role.ToStdString() + ":" +
                Source.AssetId.ToString().ToStdString() + "@" +
                Source.Version.ToLowerHex().ToStdString());
        for (const auto& Dependency : Record.Dependencies)
            Entry.DependencyEvidence.emplace_back(
                Dependency.Role.ToStdString() + ":" +
                Dependency.AssetId.ToString().ToStdString() +
                (Dependency.RequiredVersion
                    ? "@" + Dependency.RequiredVersion->ToLowerHex().ToStdString()
                    : ""));
        Document.PayloadBytes += Record.PayloadBytes;
        Document.Pipeline.Assets.push_back(std::move(Entry));
    }
    Document.GenerationId = Manifest.GenerationId;
    Document.GenerationBytes = Document.PayloadBytes;
}

FAssetCookCliResult Finish(
    const FAssetCookCliInvocation& Invocation,
    FAssetCookReportDocument Document)
{
    FAssetCookCliResult Result;
    Result.Category = Document.Result;
    Result.StableReason = Document.StableReason;
    if (FAssetCookReportCodec::Write(
            Document, Invocation.bNormalizedReport,
            Result.CanonicalReport) != Asset::EAssetResult::Success)
        return Failure(EAssetCookResultCategory::InternalFailure,
            "asset-cooker.report.encode-failed");
    if (!WriteReport(Invocation.ReportPath, Result.CanonicalReport))
        return Failure(EAssetCookResultCategory::IoFailure,
            "asset-cooker.report.write-failed");
    return Result;
}

FAssetCookCliResult ExecuteCook(const FAssetCookCliInvocation& Invocation)
{
    FAssetCookReport Pipeline;
    const FAssetCookResult Cooked = FAssetCookRunner::Run(
        Invocation.CookRequest, Pipeline);
    auto Document = BaseDocument(
        Invocation.Command, Cooked.Category, Cooked.StableReason);
    if (Pipeline.EffectiveProfileDigest.IsAvailable())
    {
        Document.Pipeline = std::move(Pipeline);
        Document.bHasPipeline = true;
    }
    if (Cooked.Succeeded() && Invocation.Command == EAssetCookReportCommand::Cook)
    {
        Document.GenerationId = Cooked.Manifest.GenerationId;
        Document.Staged = 1;
        Document.Published = 1;
        for (const auto& Artifact : Cooked.Artifacts)
            Document.PayloadBytes += Artifact.Bytes.size();
        Document.GenerationBytes =
            Document.PayloadBytes + Cooked.CanonicalManifest.Len();
    }
    return Finish(Invocation, std::move(Document));
}

FAssetCookCliResult ExecuteValidate(const FAssetCookCliInvocation& Invocation)
{
    FPublishedGenerationValidationRequest Request;
    Request.bRejectUnexpectedFiles = Invocation.bStrictFiles;
    if (Invocation.GenerationId)
    {
        Request.Subject = EPublishedValidationSubject::GenerationDirectory;
        Request.ExpectedGenerationId = Invocation.GenerationId;
        Request.SubjectRoot = Core::FString(
            (std::filesystem::path(Invocation.OutputRoot.ToStdString()) /
             "Generations" /
             Invocation.GenerationId->ToLowerHex().ToStdString()).generic_string());
    }
    else Request.SubjectRoot = Invocation.OutputRoot;
    const auto Validated = FPublishedGenerationValidator::Validate(Request);
    const auto Category = Validated.Succeeded()
        ? EAssetCookResultCategory::Success
        : EAssetCookResultCategory::PublishedValidationFailure;
    auto Document = BaseDocument(
        Invocation.Command, Category, Validated.StableReason);
    if (Validated.Succeeded())
        AddManifestEvidence(
            Validated.Manifest, Document, EAssetCookAction::Validate);
    return Finish(Invocation, std::move(Document));
}

bool ReadDdcEntry(
    const Core::FString& Root,
    const Asset::FAssetDerivedKey& Key,
    FDerivedDataLookupResult& Out)
{
    const auto Paths = FDerivedDataStore::PathsFor(Root, Key);
    Core::TArray<Core::uint8> MetadataBytes;
    if (!Core::FPlatformFileSystem::ReadFile(Paths.EntryMetadata, MetadataBytes))
        return false;
    Asset::FAssetDerivedDataEntry Entry;
    if (Asset::FAssetCookContractCodec::ParseDerivedDataEntry(
            MetadataBytes, {}, Entry) != Asset::EAssetResult::Success ||
        Entry.DerivedKey != Key)
        return false;
    FDerivedDataLookupRequest Request;
    Request.Root = Root;
    Request.DerivedKey = Key;
    Request.Evidence = Entry.Evidence;
    Request.RequiredExtensions = Entry.RequiredExtensions;
    Out = FDerivedDataStore::Lookup(Request);
    return true;
}

Core::TArray<Asset::FAssetDerivedKey> EnumerateDdcKeys(
    const Core::FString& Root)
{
    Core::TArray<Asset::FAssetDerivedKey> Keys;
    const auto Entries = std::filesystem::path(Root.ToStdString()) / "Entries";
    std::error_code Error;
    if (!std::filesystem::exists(Entries, Error)) return Keys;
    for (std::filesystem::recursive_directory_iterator Iterator(
             Entries, std::filesystem::directory_options::skip_permission_denied,
             Error), End;
         !Error && Iterator != End; Iterator.increment(Error))
    {
        if (!Iterator->is_regular_file(Error) ||
            Iterator->path().filename() != "Entry.json") continue;
        Asset::FAssetDerivedKey Key;
        if (Asset::FAssetDerivedKey::ParseLowerHex(
                Core::FString(Iterator->path().parent_path().filename().string()),
                Key) == Asset::EAssetResult::Success)
            Keys.push_back(Key);
    }
    std::sort(Keys.begin(), Keys.end(), [](const auto& Left, const auto& Right)
        { return Left.ToString() < Right.ToString(); });
    Keys.erase(std::unique(Keys.begin(), Keys.end()), Keys.end());
    return Keys;
}

FAssetCookCliResult ExecuteValidateCache(
    const FAssetCookCliInvocation& Invocation)
{
    Core::TArray<Asset::FAssetDerivedKey> Keys = Invocation.DerivedKey
        ? Core::TArray<Asset::FAssetDerivedKey>{*Invocation.DerivedKey}
        : EnumerateDdcKeys(Invocation.DerivedDataRoot);
    auto Document = BaseDocument(
        Invocation.Command, EAssetCookResultCategory::Success,
        Core::FString("asset-cooker.cache.validate.success"));
    Document.bHasPipeline = true;
    Core::uint32 Errors = 0;
    for (Core::usize Index = 0; Index < Keys.size(); ++Index)
    {
        FDerivedDataLookupResult Lookup;
        const bool Read = ReadDdcEntry(
            Invocation.DerivedDataRoot, Keys[Index], Lookup);
        FAssetCookAssetReport Entry;
        Entry.PlanIndex = static_cast<Core::uint32>(Index);
        Entry.DerivedKey = Keys[Index];
        if (Read && Lookup.Status == EDerivedDataLookupStatus::Hit)
        {
            Entry.AssetId = Lookup.Entry.AssetId;
            Entry.Action = EAssetCookAction::Validate;
            Entry.StableReason = Core::FString("ddc.entry.valid");
            for (const auto& Source : Lookup.Entry.Evidence.SourceManifest)
                Entry.SourceEvidence.emplace_back(
                    Source.Locator.ToString().ToStdString() + "@" +
                    Source.Version.ToLowerHex().ToStdString());
            for (const auto& Dependency : Lookup.Entry.Evidence.Dependencies)
                Entry.DependencyEvidence.emplace_back(
                    Dependency.Id.ToString().ToStdString() + "@" +
                    (Dependency.Version.SourceDigest.IsAvailable()
                        ? Dependency.Version.SourceDigest.ToLowerHex().ToStdString()
                        : Dependency.Version.ContentDigest.IsAvailable()
                            ? Dependency.Version.ContentDigest.ToLowerHex().ToStdString()
                            : Dependency.Version.CookDigest.ToLowerHex().ToStdString()));
        }
        else
        {
            ++Errors;
            const Core::FString CacheAssetPath(
                "Cache/" + Keys[Index].ToString().ToStdString());
            (void)Asset::FAssetId::Create(
                Core::FString("DerivedData"),
                CacheAssetPath,
                std::nullopt, Entry.AssetId);
            Entry.Action = EAssetCookAction::Fail;
            Entry.StableReason = Core::FString("ddc.entry.invalid");
            FAssetCookReportDiagnostic Diagnostic;
            Diagnostic.Category = Core::FString("cache-failure");
            Diagnostic.Stage = Core::FString("validate-cache");
            Diagnostic.DerivedKey = Keys[Index];
            Diagnostic.Reason = Entry.StableReason;
            Document.Diagnostics.push_back(std::move(Diagnostic));
            if (Errors >= Invocation.MaxErrors) break;
        }
        Document.Pipeline.Assets.push_back(std::move(Entry));
    }
    Document.Pipeline.Counts.ReachableAssets =
        static_cast<Core::uint32>(Document.Pipeline.Assets.size());
    Document.Pipeline.Counts.ReuseIneligible =
        Document.Pipeline.Counts.ReachableAssets;
    Document.Pipeline.Counts.Failed = Errors;
    if (Invocation.DerivedKey && Keys.empty()) Errors = 1;
    if (Errors != 0)
    {
        Document.Result = EAssetCookResultCategory::CacheFailure;
        Document.StableReason = Core::FString("asset-cooker.cache.validate.failed");
    }
    return Finish(Invocation, std::move(Document));
}

FAssetCookCliResult ExecuteInspect(const FAssetCookCliInvocation& Invocation)
{
    if (!Invocation.TargetProfilePath.IsEmpty())
    {
        Asset::FAssetTargetProfileEvidence Profile;
        if (!ReadProfile(Invocation.TargetProfilePath, Profile))
            return Finish(Invocation, BaseDocument(
                Invocation.Command, EAssetCookResultCategory::InvalidProfile,
                Core::FString("asset-cooker.inspect.profile-invalid")));
        auto Document = BaseDocument(
            Invocation.Command, EAssetCookResultCategory::Success,
            Core::FString("asset-cooker.inspect.profile-success"));
        Document.Pipeline.EffectiveProfileDigest = Profile.EffectiveProfileDigest;
        Document.Pipeline.SnapshotDigest = Asset::FAssetDigest::FromBytes(
            std::span<const Core::uint8>{});
        Document.Pipeline.Counts.ReuseIneligible = 0;
        Document.bHasPipeline = true;
        FAssetCookReportDiagnostic Evidence;
        Evidence.Category = Core::FString("inspection");
        Evidence.Stage = Core::FString("target-profile");
        Evidence.TargetProfileDigest = Profile.EffectiveProfileDigest;
        Evidence.Field = Core::FString("canonicalEffectiveConfiguration");
        Evidence.Reason = Core::FString(
            "target-profile.canonical-effective-configuration-available");
        Document.Diagnostics.push_back(std::move(Evidence));
        return Finish(Invocation, std::move(Document));
    }
    if (!Invocation.OutputRoot.IsEmpty()) return ExecuteValidate(Invocation);
    return ExecuteValidateCache(Invocation);
}

const char* MetalDoctorReason(EMetalLibraryFinalizeStatus Status)
{
    switch (Status)
    {
    case EMetalLibraryFinalizeStatus::Success:
        return "asset-cooker.doctor.metal.success";
    case EMetalLibraryFinalizeStatus::HostUnsupported:
        return "asset-cooker.doctor.metal.host-unsupported";
    case EMetalLibraryFinalizeStatus::ToolchainUnavailable:
        return "asset-cooker.doctor.metal.toolchain-unavailable";
    case EMetalLibraryFinalizeStatus::TimedOut:
        return "asset-cooker.doctor.metal.timed-out";
    case EMetalLibraryFinalizeStatus::InvalidRequest:
        return "asset-cooker.doctor.metal.invalid-request";
    case EMetalLibraryFinalizeStatus::CompilerFailed:
        return "asset-cooker.doctor.metal.command-failed";
    case EMetalLibraryFinalizeStatus::EmptyOutput:
        return "asset-cooker.doctor.metal.empty-output";
    case EMetalLibraryFinalizeStatus::EvidenceMismatch:
        return "asset-cooker.doctor.metal.evidence-mismatch";
    case EMetalLibraryFinalizeStatus::IoFailure:
        return "asset-cooker.doctor.metal.io-failure";
    }
    return "asset-cooker.doctor.metal.internal-failure";
}

FAssetCookCliResult ExecuteDoctor(const FAssetCookCliInvocation& Invocation)
{
    Asset::FAssetTargetProfileEvidence Profile;
    if (!ReadProfile(Invocation.TargetProfilePath, Profile) ||
        Profile.Profile.Platform != Asset::EAssetTargetPlatform::MacOS ||
        Profile.Profile.GraphicsBackend != Asset::EAssetGraphicsBackend::Metal ||
        !Profile.Profile.MetalShaderTarget)
        return Finish(Invocation, BaseDocument(
            Invocation.Command, EAssetCookResultCategory::InvalidProfile,
            Core::FString("asset-cooker.doctor.metal.profile-invalid")));

    FMetalToolchainEvidence Evidence;
    const EMetalLibraryFinalizeStatus Status =
        InspectMetalToolchain(60000, 256U * 1024U, Evidence);
    const bool bSucceeded = Status == EMetalLibraryFinalizeStatus::Success;
    auto Document = BaseDocument(
        Invocation.Command,
        bSucceeded ? EAssetCookResultCategory::Success
                   : EAssetCookResultCategory::CookFailure,
        Core::FString(MetalDoctorReason(Status)));
    Document.Pipeline.EffectiveProfileDigest = Profile.EffectiveProfileDigest;
    Document.Pipeline.SnapshotDigest = Asset::FAssetDigest::FromBytes(
        std::span<const Core::uint8>{});
    Document.bHasPipeline = true;
    if (bSucceeded)
    {
        const auto AddEvidence = [&Document](
            const char* Field, const Core::FString& Value)
        {
            FAssetCookReportDiagnostic Diagnostic;
            Diagnostic.Category = Core::FString("toolchain-evidence");
            Diagnostic.Stage = Core::FString("doctor");
            Diagnostic.Field = Core::FString(Field);
            Diagnostic.Reason = Value;
            Document.Diagnostics.push_back(std::move(Diagnostic));
        };
        AddEvidence("metalCompiler", Evidence.MetalCompiler);
        AddEvidence("xcodeBuild", Evidence.XcodeBuild);
        AddEvidence("sdk", Evidence.Sdk);
    }
    return Finish(Invocation, std::move(Document));
}

} // namespace

EAssetCookResultCategory FAssetCookCli::Parse(
    std::span<const Core::FString> Arguments,
    FAssetCookCliInvocation& OutInvocation,
    Core::FString& OutReason)
{
    OutInvocation = {};
    OutReason = Core::FString("asset-cooker.cli.invalid-arguments");
    if (Arguments.empty()) return EAssetCookResultCategory::InvalidArguments;
    const std::string Command = Arguments.front().ToStdString();
    FParsedOptions Options;
    if (Command == "cook" || Command == "plan")
    {
        if (!ParseOptions(Arguments,
                {"--source-root", "--root", "--target-profile", "--output",
                 "--ddc", "--workers", "--lease-timeout-ms", "--report"},
                {"--source-root", "--root"},
                {"--cook-all", "--clean", "--normalized-report"}, Options))
            return EAssetCookResultCategory::InvalidArguments;
        const auto& Sources = Many(Options, "--source-root");
        const auto& Roots = Many(Options, "--root");
        const auto* Profile = One(Options, "--target-profile");
        const auto* Output = One(Options, "--output");
        const auto* Ddc = One(Options, "--ddc");
        const bool bCookAll = Options.Flags.contains("--cook-all");
        if (Sources.empty() || !Profile || !Output || !Ddc ||
            (bCookAll == !Roots.empty()) ||
            (Command == "plan" &&
             (Options.Flags.contains("--clean") ||
              One(Options, "--lease-timeout-ms"))))
            return EAssetCookResultCategory::InvalidArguments;
        OutInvocation.Command = Command == "cook"
            ? EAssetCookReportCommand::Cook : EAssetCookReportCommand::Plan;
        auto& Request = OutInvocation.CookRequest;
        std::map<std::string, Core::FString> UniqueSources;
        for (const auto& Source : Sources)
        {
            std::filesystem::path Canonical;
            if (!CanonicalPath(Source, Canonical))
                return EAssetCookResultCategory::InvalidArguments;
            UniqueSources.emplace(Canonical.generic_string(), Source);
        }
        for (const auto& [Canonical, Source] : UniqueSources)
        {
            (void)Canonical;
            Request.SourceRoots.push_back(Source);
        }
        Request.SelectionMode = bCookAll
            ? Asset::EAssetCookSelectionMode::CookAll
            : Asset::EAssetCookSelectionMode::ExplicitRoots;
        for (const auto& Root : Roots)
        {
            Asset::FAssetId Id;
            if (!ParseAssetId(Root, Id))
                return EAssetCookResultCategory::InvalidArguments;
            Request.ExplicitRoots.push_back(std::move(Id));
        }
        std::sort(Request.ExplicitRoots.begin(), Request.ExplicitRoots.end());
        if (std::adjacent_find(
                Request.ExplicitRoots.begin(), Request.ExplicitRoots.end()) !=
            Request.ExplicitRoots.end())
            return EAssetCookResultCategory::InvalidArguments;
        Request.TargetProfilePath = *Profile;
        Request.OutputRoot = *Output;
        Request.DerivedDataRoot = *Ddc;
        Request.ScratchRoot = Core::FString(Output->ToStdString() + ".scratch");
        Request.Mode = Command == "plan"
            ? EAssetCookRunMode::PlanOnly : EAssetCookRunMode::Cook;
        Request.CachePolicy = Options.Flags.contains("--clean")
            ? EAssetCookCachePolicy::IgnoreExisting
            : EAssetCookCachePolicy::Incremental;
        if (const auto* Workers = One(Options, "--workers");
            Workers && !ParseUnsigned(*Workers, 1, 32, Request.WorkerCount))
            return EAssetCookResultCategory::InvalidArguments;
        if (Command == "plan") Request.LeaseTimeout = std::chrono::milliseconds(0);
        else if (const auto* Timeout = One(Options, "--lease-timeout-ms"))
        {
            Core::uint32 Milliseconds = 0;
            if (!ParseUnsigned(*Timeout, 0, 600000, Milliseconds))
                return EAssetCookResultCategory::InvalidArguments;
            Request.LeaseTimeout = std::chrono::milliseconds(Milliseconds);
        }
        if (const auto* Report = One(Options, "--report"))
            Request.ReportPath = *Report;
        OutInvocation.OutputRoot = *Output;
        OutInvocation.DerivedDataRoot = *Ddc;
        OutInvocation.TargetProfilePath = *Profile;
        OutInvocation.ReportPath = Request.ReportPath;
    }
    else if (Command == "validate")
    {
        if (!ParseOptions(Arguments,
                {"--output", "--generation", "--report"}, {},
                {"--strict-files", "--normalized-report"}, Options))
            return EAssetCookResultCategory::InvalidArguments;
        const auto* Output = One(Options, "--output");
        if (!Output) return EAssetCookResultCategory::InvalidArguments;
        OutInvocation.Command = EAssetCookReportCommand::Validate;
        OutInvocation.OutputRoot = *Output;
        OutInvocation.bStrictFiles = Options.Flags.contains("--strict-files");
        if (const auto* Generation = One(Options, "--generation"))
        {
            Asset::FAssetDigest Digest;
            if (Asset::FAssetDigest::ParseLowerHex(*Generation, Digest) !=
                Asset::EAssetResult::Success)
                return EAssetCookResultCategory::InvalidArguments;
            OutInvocation.GenerationId = Digest;
        }
        if (const auto* Report = One(Options, "--report"))
            OutInvocation.ReportPath = *Report;
    }
    else if (Command == "validate-cache")
    {
        if (!ParseOptions(Arguments,
                {"--ddc", "--key", "--max-errors", "--report"}, {},
                {"--normalized-report"}, Options))
            return EAssetCookResultCategory::InvalidArguments;
        const auto* Ddc = One(Options, "--ddc");
        if (!Ddc) return EAssetCookResultCategory::InvalidArguments;
        OutInvocation.Command = EAssetCookReportCommand::ValidateCache;
        OutInvocation.DerivedDataRoot = *Ddc;
        if (const auto* Key = One(Options, "--key"))
        {
            Asset::FAssetDerivedKey Parsed;
            if (Asset::FAssetDerivedKey::ParseLowerHex(*Key, Parsed) !=
                Asset::EAssetResult::Success)
                return EAssetCookResultCategory::InvalidArguments;
            OutInvocation.DerivedKey = Parsed;
        }
        if (const auto* Maximum = One(Options, "--max-errors");
            Maximum && !ParseUnsigned(
                *Maximum, 1, 4096, OutInvocation.MaxErrors))
            return EAssetCookResultCategory::InvalidArguments;
        if (const auto* Report = One(Options, "--report"))
            OutInvocation.ReportPath = *Report;
    }
    else if (Command == "inspect")
    {
        if (!ParseOptions(Arguments,
                {"--output", "--generation", "--ddc", "--key",
                 "--target-profile", "--report"}, {},
                {"--normalized-report"}, Options))
            return EAssetCookResultCategory::InvalidArguments;
        const auto* Output = One(Options, "--output");
        const auto* Ddc = One(Options, "--ddc");
        const auto* Profile = One(Options, "--target-profile");
        const bool OutputSubject = Output && !Ddc && !Profile;
        const bool DdcSubject = Ddc && !Output && !Profile && One(Options, "--key");
        const bool ProfileSubject = Profile && !Output && !Ddc &&
            !One(Options, "--key") && !One(Options, "--generation");
        if (static_cast<int>(OutputSubject) + static_cast<int>(DdcSubject) +
                static_cast<int>(ProfileSubject) != 1 ||
            (One(Options, "--generation") && !OutputSubject))
            return EAssetCookResultCategory::InvalidArguments;
        OutInvocation.Command = EAssetCookReportCommand::Inspect;
        if (Output) OutInvocation.OutputRoot = *Output;
        if (Ddc) OutInvocation.DerivedDataRoot = *Ddc;
        if (Profile) OutInvocation.TargetProfilePath = *Profile;
        if (const auto* Key = One(Options, "--key"))
        {
            Asset::FAssetDerivedKey Parsed;
            if (Asset::FAssetDerivedKey::ParseLowerHex(*Key, Parsed) !=
                Asset::EAssetResult::Success)
                return EAssetCookResultCategory::InvalidArguments;
            OutInvocation.DerivedKey = Parsed;
        }
        if (const auto* Generation = One(Options, "--generation"))
        {
            Asset::FAssetDigest Digest;
            if (Asset::FAssetDigest::ParseLowerHex(*Generation, Digest) !=
                Asset::EAssetResult::Success)
                return EAssetCookResultCategory::InvalidArguments;
            OutInvocation.GenerationId = Digest;
        }
        if (const auto* Report = One(Options, "--report"))
            OutInvocation.ReportPath = *Report;
    }
    else if (Command == "doctor")
    {
        if (!ParseOptions(Arguments,
                {"--target-profile", "--report"}, {},
                {"--normalized-report"}, Options))
            return EAssetCookResultCategory::InvalidArguments;
        const auto* Profile = One(Options, "--target-profile");
        if (!Profile) return EAssetCookResultCategory::InvalidArguments;
        OutInvocation.Command = EAssetCookReportCommand::Doctor;
        OutInvocation.TargetProfilePath = *Profile;
        if (const auto* Report = One(Options, "--report"))
            OutInvocation.ReportPath = *Report;
    }
    else return EAssetCookResultCategory::InvalidArguments;

    OutInvocation.bNormalizedReport = Options.Flags.contains("--normalized-report");
    if (!ValidateReportAlias(
            OutInvocation.ReportPath,
            {OutInvocation.OutputRoot, OutInvocation.DerivedDataRoot}))
        return EAssetCookResultCategory::InvalidArguments;
    for (const auto& SourceRoot : OutInvocation.CookRequest.SourceRoots)
        if (!ValidateReportAlias(OutInvocation.ReportPath, {SourceRoot}))
            return EAssetCookResultCategory::InvalidArguments;
    OutReason = Core::FString("asset-cooker.cli.success");
    return EAssetCookResultCategory::Success;
}

FAssetCookCliResult FAssetCookCli::Execute(
    const FAssetCookCliInvocation& Invocation)
{
    if (Invocation.Command == EAssetCookReportCommand::Cook ||
        Invocation.Command == EAssetCookReportCommand::Plan)
    {
        auto Request = Invocation.CookRequest;
        if (!ReadProfile(Request.TargetProfilePath, Request.TargetProfile))
            return Finish(Invocation, BaseDocument(
                Invocation.Command, EAssetCookResultCategory::InvalidProfile,
                Core::FString("asset-cooker.profile.invalid")));
        if (Request.Validate() != Asset::EAssetResult::Success)
            return Finish(Invocation, BaseDocument(
                Invocation.Command, EAssetCookResultCategory::InvalidArguments,
                Core::FString("asset-cooker.request.invalid")));
        FAssetCookCliInvocation Ready = Invocation;
        Ready.CookRequest = std::move(Request);
        return ExecuteCook(Ready);
    }
    if (Invocation.Command == EAssetCookReportCommand::Validate)
        return ExecuteValidate(Invocation);
    if (Invocation.Command == EAssetCookReportCommand::ValidateCache)
        return ExecuteValidateCache(Invocation);
    if (Invocation.Command == EAssetCookReportCommand::Doctor)
        return ExecuteDoctor(Invocation);
    return ExecuteInspect(Invocation);
}

} // namespace Stoner::AssetCooker::Private
