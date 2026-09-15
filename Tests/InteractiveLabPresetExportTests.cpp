#include "FLabPresetStore.h"
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

int RunInteractiveLabPresetExportTests()
{
    using namespace Stoner::Core;
    using namespace Stoner::Application;
    using namespace Stoner::Renderer;
    namespace fs=std::filesystem;
    int Failed=0;
    const auto Check=[&](bool OK,const char* Name) {
        std::cout<<(OK ? "[PASS] " : "[FAIL] ")<<Name<<'\n'; if (!OK) ++Failed; return OK;
    };
    const auto Scratch=fs::temp_directory_path()/("LabPresetExport-"+
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(Scratch/"exports"); fs::create_directories(Scratch/"protected");
    FString Root,Protected;
    Check(FPlatformFileSystem::CanonicalizeExistingPath(FString((Scratch/"exports").string()),Root).IsSuccess() &&
        FPlatformFileSystem::CanonicalizeExistingPath(FString((Scratch/"protected").string()),Protected).IsSuccess(),
        "preset store fixture resolves owned export and protected roots");
    FLabPresetStoreConfig Config{Root,{Protected}};
    const TArray<uint8> Artifact={0,1,2,3,255};
    const auto File=ExportLabFile(Config,"preview.raw",Artifact);
    TArray<uint8> RoundTrip;
    Check(File.bPublished && File.Status.IsSuccess() &&
        FPlatformFileSystem::ReadRegularFileBounded(File.TargetPath,Artifact.size(),RoundTrip).IsSuccess() && RoundTrip==Artifact,
        "lab artifact publication preserves arbitrary bytes through protected no-replace export");
    Check(!ExportLabFile(Config,"preview.raw",TArray<uint8>{9}).bPublished &&
        !ExportLabFile(Config,"../escape.raw",Artifact).bPublished &&
        !ExportLabFile({Protected,{Protected}},"denied.raw",Artifact).bPublished,
        "lab artifact publication rejects collisions, traversal and protected roots");
    Check(!ExportLabFile(Config,"empty.raw",{}).bPublished,
        "lab artifact publication rejects empty payloads");
    {
        const TArray<uint8> Large(1024*1024,0x5a);
        const auto LargeFile=ExportLabFile(Config,"large.raw",Large);
        Check(LargeFile.bPublished && LargeFile.Status.IsSuccess(),
            "capture artifacts may exceed the preset codec limit within their separate byte bound");
        const TArray<uint8> TooLarge(64ull*1024*1024+1,0);
        Check(!ExportLabFile(Config,"oversize.raw",TooLarge).bPublished &&
            !FPlatformFileSystem::Exists(FString(Root.ToStdString()+"/oversize.raw")),
            "oversized lab artifacts reject before temporary publication");
    }
    const FLabDebugStage Stage{"ManualExposure",ERenderGraphColorDomain::SceneLinearRec709D65,{}};
    const std::span<const FLabDebugStage> Stages{&Stage,1};
    const FLabPresetWorkload Workload{"fixture-lantern-v1","StaticModel:灯笼😀.glb#idx.scene.0",FString(std::string(64,'1'))};
    FLabPreset P;
    if (!Check(FLabPresetStore::Read("Tests/Fixtures/InteractiveLab/preset-v1.json",Workload,Stages,P).IsSuccess(),
        "preset store reads the bounded authenticated fixture through Core")) return Failed;
    auto Published=FLabPresetStore::Export(Config,"saved.json",P,Stages);
    FLabPreset ReadBack;
    Check(Published.Status.IsSuccess() && Published.bPublished && !Published.bTemporaryRetained &&
        FLabPresetStore::Read(Published.TargetPath,Workload,Stages,ReadBack).IsSuccess() && ReadBack.Workload==P.Workload,
        "preset export publishes a complete independently readable record");
    auto Changed=P; Changed.Output.ExposureStops=2;
    const auto Collision=FLabPresetStore::Export(Config,"saved.json",Changed,Stages);
    Check(Collision.Status.Result==EPlatformFileResult::AlreadyExists && !Collision.bPublished && !Collision.bTemporaryRetained &&
        FLabPresetStore::Read(Published.TargetPath,Workload,Stages,ReadBack).IsSuccess() && ReadBack.Output.ExposureStops==-3,
        "default repeated export preserves the previous preset and cleans its own temp");
    const auto Overwrite=FLabPresetStore::Export(Config,"saved.json",Changed,Stages,true);
    Check(Overwrite.Status.IsSuccess() && Overwrite.bPublished &&
        FLabPresetStore::Read(Published.TargetPath,Workload,Stages,ReadBack).IsSuccess() && ReadBack.Output.ExposureStops==2,
        "explicit overwrite atomically replaces the selected preset");
    auto ReadOnly=Config; ReadOnly.ProtectedPaths.push_back(Published.TargetPath);
    Check(!FLabPresetStore::Export(ReadOnly,"saved.json",P,Stages,true).bPublished &&
        FLabPresetStore::Read(Published.TargetPath,Workload,Stages,ReadBack).IsSuccess() && ReadBack.Output.ExposureStops==2,
        "an imported read-only file remains protected even from explicit overwrite");
    for (const char* Name : {"../escape.json","a/b.json","a\\b.json","CON.json",".hidden","bad:stream.json"})
        Check(FLabPresetStore::Export(Config,Name,P,Stages).Status.Result==EPlatformFileResult::InvalidArgument,
            "preset export rejects traversal, device aliases and non-component names");
    auto Denied=Config; Denied.ExportRoot=Protected;
    Check(!FLabPresetStore::Export(Denied,"accepted.json",P,Stages).bPublished && !fs::exists(Scratch/"protected/accepted.json"),
        "protected roots reject export before creating any destination");
    FString InterruptedTemp;
    const auto Interrupted=FLabPresetStore::Export(Config,"interrupted.json",P,Stages,false,[&](const FString& Temp,const FString&) {
        InterruptedTemp=Temp;
        FLabPreset Candidate;
        Check(FLabPresetStore::Read(Temp,Workload,Stages,Candidate).IsSuccess(),"pre-publication temporary bytes are complete and durable");
        return false;
    });
    Check(!Interrupted.Status.IsSuccess() && !Interrupted.bPublished && !Interrupted.bTemporaryRetained &&
        !FPlatformFileSystem::Exists(InterruptedTemp) && !fs::exists(Scratch/"exports/interrupted.json"),
        "interrupted publication removes only its owned temp and never exposes a final file");
    const TArray<uint8> OtherBytes{4,5,6};
    const auto Tampered=FLabPresetStore::Export(Config,"tampered.json",P,Stages,false,[&](const FString& Temp,const FString&) {
        return FPlatformFileSystem::WriteFileDurable(Temp,OtherBytes).IsSuccess();
    });
    Check(!Tampered.Status.IsSuccess() && !Tampered.bPublished && !Tampered.bTemporaryRetained &&
        !fs::exists(Scratch/"exports/tampered.json"),"changed temporary bytes cannot become a published preset");
    const auto Race=FLabPresetStore::Export(Config,"raced.json",P,Stages,false,[&](const FString&,const FString& Target) {
        return FPlatformFileSystem::WriteFileDurable(Target,OtherBytes).IsSuccess();
    });
    TArray<uint8> Kept;
    Check(Race.Status.Result==EPlatformFileResult::AlreadyExists && !Race.bPublished && !Race.bTemporaryRetained &&
        FPlatformFileSystem::ReadFile(Race.TargetPath,Kept) && Kept==OtherBytes,
        "a destination created after temp preparation wins without check-then-write replacement");
    std::error_code LinkError;
    fs::create_directory_symlink(Scratch/"protected",Scratch/"alias",LinkError);
    if (!LinkError)
    {
        auto Alias=Config; Alias.ExportRoot=FString((Scratch/"alias").string());
        Check(!FLabPresetStore::Export(Alias,"escaped.json",P,Stages).bPublished,
            "a symlink export root cannot redirect publication");
        Check(FPlatformFileSystem::WriteFileDurable(FString((Scratch/"protected/keep.json").string()),OtherBytes).IsSuccess(),
            "symlink protection fixture creates its sentinel");
        fs::create_symlink(Scratch/"protected/keep.json",Scratch/"exports/link.json",LinkError);
        Check(!LinkError && !FLabPresetStore::Export(Config,"link.json",P,Stages,true).bPublished &&
            FPlatformFileSystem::ReadFile(FString((Scratch/"protected/keep.json").string()),Kept) && Kept==OtherBytes,
            "explicit overwrite never follows a destination symlink into protected content");
        const auto LateLink=FLabPresetStore::Export(Config,"late-link.json",P,Stages,true,[&](const FString&,const FString&) {
            fs::create_symlink(Scratch/"protected/keep.json",Scratch/"exports/late-link.json",LinkError);
            return !LinkError;
        });
        Check(!LinkError && !LateLink.bPublished && !LateLink.bTemporaryRetained &&
            FPlatformFileSystem::ReadFile(FString((Scratch/"protected/keep.json").string()),Kept) && Kept==OtherBytes,
            "protection is revalidated when a destination symlink appears immediately before publication");
    }
    else std::cout<<"[UNSUPPORTED] host cannot create preset symlink fixtures\n";
    std::array<FLabPresetExportResult,4> Results;
    std::array<std::thread,4> Threads;
    for (size_t I=0;I<Threads.size();++I) Threads[I]=std::thread([&,I] {
        auto Value=P; Value.Output.ExposureStops=static_cast<float>(I);
        Results[I]=FLabPresetStore::Export(Config,"concurrent.json",Value,Stages);
    });
    for (auto& Thread : Threads) Thread.join();
    size_t Winners=0,Losers=0; bool Clean=true;
    for (const auto& R : Results) { Winners+=R.bPublished; Losers+=R.Status.Result==EPlatformFileResult::AlreadyExists; Clean &= !R.bTemporaryRetained; }
    Check(Winners==1 && Losers==3 && Clean &&
        FLabPresetStore::Read(FString(Root.ToStdString()+"/concurrent.json"),Workload,Stages,ReadBack).IsSuccess(),
        "concurrent preset exports retain exactly one complete authenticated winner");
    bool Temps=false;
    for (const auto& E : fs::directory_iterator(Scratch/"exports"))
        Temps |= E.path().filename().string().starts_with(".lab-preset-");
    Check(!Temps,"successful and rejected exports leave no owned temporary files");
    fs::remove_all(Scratch);
    return Failed;
}
