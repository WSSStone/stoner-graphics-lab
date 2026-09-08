#include "FLabPresetStore.h"
#include "Core/FPlatformTime.h"
#include <algorithm>
#include <atomic>
#include <limits>

namespace Stoner::Application
{
namespace
{
using namespace Stoner::Core;
FPlatformFileStatus Fail(EPlatformFileResult Result,const char* Context) { return {Result,0,Context}; }
bool PathText(const FString& P) { return !P.IsEmpty() && P.View().size()<=4096 && P.View().find('\0')==std::string_view::npos; }
bool Component(const FString& F)
{
    const auto S=F.View();
    if (S.empty() || S.size()>128 || S.front()=='.' || S.back()=='.') return false;
    if (!std::all_of(S.begin(),S.end(),[](char C) {
        return (C>='a' && C<='z') || (C>='A' && C<='Z') || (C>='0' && C<='9') || C=='.' || C=='_' || C=='-';
    })) return false;
    std::string Stem(S.substr(0,S.find('.')));
    for (auto& C : Stem) if (C>='a' && C<='z') C=static_cast<char>(C-'a'+'A');
    if (Stem=="CON" || Stem=="PRN" || Stem=="AUX" || Stem=="NUL") return false;
    return !(Stem.size()==4 && (Stem.starts_with("COM") || Stem.starts_with("LPT")) && Stem[3]>='1' && Stem[3]<='9');
}
FPlatformFileStatus Validate(const FLabPresetStoreConfig& C,const FString& Name,FString& Target)
{
    if (!PathText(C.ExportRoot) || !Component(Name) || C.ProtectedPaths.empty() || C.ProtectedPaths.size()>64)
        return Fail(EPlatformFileResult::InvalidArgument,"preset-export:configuration-or-name");
    FString Canonical;
    auto Status=FPlatformFileSystem::CanonicalizeExistingPath(C.ExportRoot,Canonical);
    if (!Status.IsSuccess()) return Status;
    if (Canonical!=C.ExportRoot) return Fail(EPlatformFileResult::OutsideRoot,"preset-export:noncanonical-root");
    Target=FString(C.ExportRoot.ToStdString()+"/"+Name.ToStdString());
    bool Inside=false;
    Status=FPlatformFileSystem::CheckContainedPath(C.ExportRoot,Target,Inside);
    if (!Status.IsSuccess()) return Status;
    if (!Inside) return Fail(EPlatformFileResult::OutsideRoot,"preset-export:destination-escape");
    for (const auto& Protected : C.ProtectedPaths)
    {
        if (!PathText(Protected)) return Fail(EPlatformFileResult::InvalidArgument,"preset-export:invalid-protection");
        for (const auto& Candidate : {C.ExportRoot,Target})
        {
            bool Denied=false;
            Status=FPlatformFileSystem::CheckContainedPath(Protected,Candidate,Denied);
            if (!Status.IsSuccess()) return Status;
            if (Denied) return Fail(EPlatformFileResult::OutsideRoot,"preset-export:protected-destination");
        }
    }
    FPlatformFileInfo Info;
    Status=FPlatformFileSystem::QueryRegularFile(Target,std::numeric_limits<uint64>::max(),Info);
    return Status.Result==EPlatformFileResult::NotFound ? FPlatformFileStatus{} : Status;
}
}
FPlatformFileStatus FLabPresetStore::Read(const FString& Path,const FLabPresetWorkload& Expected,
    std::span<const FLabDebugStage> Stages,FLabPreset& OutPreset)
{
    if (!PathText(Path)) return Fail(EPlatformFileResult::InvalidArgument,"preset-read:path");
    TArray<uint8> Bytes;
    const auto Read=FPlatformFileSystem::ReadRegularFileBounded(Path,FLabPresetCodec::MaximumBytes,Bytes);
    if (!Read.IsSuccess()) return Read;
    FString Reason;
    if (!FLabPresetCodec::Decode(Bytes,Expected,Stages,OutPreset,Reason))
        return {EPlatformFileResult::InvalidArgument,0,Reason};
    return {};
}
FLabPresetExportResult FLabPresetStore::Export(const FLabPresetStoreConfig& Config,const FString& Filename,
    const FLabPreset& Preset,std::span<const FLabDebugStage> Stages,bool Overwrite,
    const std::function<bool(const FString&,const FString&)>& BeforePublish)
{
    FLabPresetExportResult Result;
    Result.Status=Validate(Config,Filename,Result.TargetPath);
    if (!Result.Status.IsSuccess()) return Result;
    TArray<uint8> Bytes;
    FString Reason;
    if (!FLabPresetCodec::Encode(Preset,Stages,Bytes,Reason))
    { Result.Status={EPlatformFileResult::InvalidArgument,0,Reason}; return Result; }
    static std::atomic<uint64> Counter=0;
    FString Temporary;
    bool Created=false;
    for (unsigned Attempt=0;Attempt<4;++Attempt)
    {
        Temporary=FString(Config.ExportRoot.ToStdString()+"/.lab-preset-"+std::to_string(FPlatformTime::Now().time_since_epoch().count())+
            "-"+std::to_string(Counter.fetch_add(1))+".tmp");
        Result.Status=FPlatformFileSystem::WriteFileExclusiveDurable(Temporary,Bytes,Created);
        if (Result.Status.Result!=EPlatformFileResult::AlreadyExists) break;
    }
    const auto Cleanup=[&] {
        if (!Created || Result.bPublished) return;
        FString Current;
        if (!FPlatformFileSystem::CanonicalizeExistingPath(Config.ExportRoot,Current).IsSuccess() || Current!=Config.ExportRoot)
        { Result.bTemporaryRetained=true; return; }
        const auto Removed=FPlatformFileSystem::RemoveFileContained(Config.ExportRoot,Temporary);
        Result.bTemporaryRetained=!Removed.IsSuccess() && Removed.Result!=EPlatformFileResult::NotFound;
    };
    if (!Result.Status.IsSuccess()) { Cleanup(); return Result; }
    if (BeforePublish && !BeforePublish(Temporary,Result.TargetPath))
    {
        Result.Status=Fail(EPlatformFileResult::IoError,"preset-export:interrupted-before-publication");
        Cleanup(); return Result;
    }
    Result.Status=Validate(Config,Filename,Result.TargetPath);
    TArray<uint8> Verified;
    if (Result.Status.IsSuccess()) Result.Status=FPlatformFileSystem::ReadRegularFileBounded(Temporary,FLabPresetCodec::MaximumBytes,Verified);
    if (Result.Status.IsSuccess() && Verified!=Bytes) Result.Status=Fail(EPlatformFileResult::InvalidArgument,"preset-export:temporary-changed");
    if (!Result.Status.IsSuccess()) { Cleanup(); return Result; }
    Result.Status=Overwrite
        ? FPlatformFileSystem::ReplaceFileAtomic(Temporary,Result.TargetPath,Result.bPublished)
        : FPlatformFileSystem::PublishFileNoReplace(Temporary,Result.TargetPath,Result.bPublished);
    // Never delete or retry a committed destination when durability reporting
    // fails after rename. The UI receives both native status and publication.
    Cleanup();
    return Result;
}
}
