#include "Core/FPlatformFileSystem.h"
#include <array>
#include <atomic>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <thread>

int RunPlatformFileNoReplaceTests()
{
    using namespace Stoner::Core;
    namespace fs=std::filesystem;
    int Failed=0;
    const auto Check=[&](bool OK,const char* Name) {
        std::cout<<(OK ? "[PASS] " : "[FAIL] ")<<Name<<'\n';
        if (!OK) ++Failed;
        return OK;
    };
    const auto Root=fs::temp_directory_path()/
        ("StonerNoReplace-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directory(Root);
    const auto Path=[&](const char* Name) { return FString((Root/Name).string()); };
    const auto Read=[&](const FString& Name,const TArray<uint8>& Expected) {
        TArray<uint8> Bytes;
        return FPlatformFileSystem::ReadRegularFileBounded(Name,65536,Bytes).IsSuccess() && Bytes==Expected;
    };
    const auto Write=[&](const FString& Name,const TArray<uint8>& Bytes) {
        return FPlatformFileSystem::WriteFileDurable(Name,Bytes).IsSuccess();
    };
    Check(FPlatformFileSystem::PublishFileNoReplace({},Path("final")).Result==EPlatformFileResult::InvalidArgument,
        "no-replace rejects an empty source");
    Check(FPlatformFileSystem::PublishFileNoReplace(Path("missing"),Path("final")).Result==EPlatformFileResult::NotFound &&
        !fs::exists(Root/"final"),"missing source cannot create a destination");
    Check(Write(Path("temp"),{1,2,3}) &&
        FPlatformFileSystem::PublishFileNoReplace(Path("temp"),Path("final")).IsSuccess() &&
        Read(Path("final"),{1,2,3}) && !fs::exists(Root/"temp"),
        "no-replace publishes complete durable bytes and consumes the temporary file");
    Check(Write(Path("loser"),{4,5,6}) &&
        FPlatformFileSystem::PublishFileNoReplace(Path("loser"),Path("final")).Result==EPlatformFileResult::AlreadyExists &&
        Read(Path("final"),{1,2,3}) && Read(Path("loser"),{4,5,6}),
        "collision preserves both the existing destination and losing temporary bytes");
    bool Created=false;
    Check(FPlatformFileSystem::WriteFileExclusiveDurable(Path("exclusive"),{7,8},Created).IsSuccess() && Created,
        "exclusive durable write grants ownership only for a newly created entry");
    Check(FPlatformFileSystem::WriteFileExclusiveDurable(Path("exclusive"),{9},Created).Result==EPlatformFileResult::AlreadyExists &&
        !Created && Read(Path("exclusive"),{7,8}),"exclusive durable write preserves an existing temporary file");
    bool Published=true;
    Check(FPlatformFileSystem::PublishFileNoReplace(Path("exclusive"),Path("final"),Published).Result==EPlatformFileResult::AlreadyExists &&
        !Published,"no-replace collision explicitly reports no publication");
    Check(FPlatformFileSystem::PublishFileNoReplace(Path("exclusive"),Path("published"),Published).IsSuccess() && Published,
        "successful no-replace explicitly reports committed publication");
    Check(FPlatformFileSystem::RemoveFileContained(Path("published"),Path("published")).Result==EPlatformFileResult::OutsideRoot &&
        Read(Path("published"),{7,8}),"file cleanup refuses to remove its allowed root");
    Check(FPlatformFileSystem::RemoveFileContained(FString(Root.string()),Path("published")).IsSuccess() && !fs::exists(Root/"published"),
        "contained file cleanup removes the owned entry without recursive traversal");
    fs::create_directory(Root/"directory");
    Check(FPlatformFileSystem::PublishFileNoReplace(Path("directory"),Path("not-file")).Result==EPlatformFileResult::NotRegularFile &&
        fs::is_directory(Root/"directory") && !fs::exists(Root/"not-file"),
        "publication rejects a directory source without moving it");
    std::error_code LinkError;
    fs::create_symlink(Root/"final",Root/"link",LinkError);
    if (!LinkError)
    {
        Check(FPlatformFileSystem::WriteFileExclusiveDurable(Path("link"),{9},Created).Result==EPlatformFileResult::AlreadyExists &&
            !Created && Read(Path("final"),{1,2,3}),"exclusive write cannot follow an existing symlink");
        Check(FPlatformFileSystem::RemoveFileContained(FString(Root.string()),Path("link")).Result==EPlatformFileResult::NotRegularFile &&
            fs::is_symlink(Root/"link") && Read(Path("final"),{1,2,3}),"temporary cleanup cannot remove a symlink referent");
        Check(FPlatformFileSystem::PublishFileNoReplace(Path("link"),Path("link-copy")).Result==EPlatformFileResult::NotRegularFile &&
            fs::is_symlink(Root/"link") && !fs::exists(Root/"link-copy"),
            "publication refuses to follow or publish a symlink source");
        Check(FPlatformFileSystem::PublishFileNoReplace(Path("loser"),Path("link")).Result==EPlatformFileResult::AlreadyExists &&
            fs::is_symlink(Root/"link") && Read(Path("final"),{1,2,3}) && Read(Path("loser"),{4,5,6}),
            "no-replace preserves an existing symlink and its target");
    }
    else std::cout<<"[UNSUPPORTED] host cannot create symlink fixture\n";
    constexpr size_t Count=8;
    std::array<FPlatformFileStatus,Count> Results;
    std::array<FString,Count> Sources;
    std::array<std::thread,Count> Writers;
    bool Prepared=true;
    for (size_t I=0;I<Count;++I)
    {
        Sources[I]=FString((Root/("race-"+std::to_string(I))).string());
        Prepared &= Write(Sources[I],TArray<uint8>(4096,static_cast<uint8>(I+1)));
    }
    std::atomic<size_t> Ready=0;
    std::atomic<bool> Start=false;
    for (size_t I=0;I<Count;++I) Writers[I]=std::thread([&,I] {
        ++Ready;
        while (!Start.load()) std::this_thread::yield();
        Results[I]=FPlatformFileSystem::PublishFileNoReplace(Sources[I],Path("race-winner"));
    });
    while (Ready.load()!=Count) std::this_thread::yield();
    Start=true;
    for (auto& Thread : Writers) Thread.join();
    size_t Successes=0,Collisions=0,Winner=Count;
    bool SourcesIntact=true;
    for (size_t I=0;I<Count;++I)
    {
        if (Results[I].IsSuccess()) { ++Successes; Winner=I; SourcesIntact &= !FPlatformFileSystem::Exists(Sources[I]); }
        else
        {
            Collisions+=Results[I].Result==EPlatformFileResult::AlreadyExists;
            SourcesIntact &= Read(Sources[I],TArray<uint8>(4096,static_cast<uint8>(I+1)));
        }
    }
    Check(Prepared && Successes==1 && Collisions==Count-1,"concurrent no-replace writers have exactly one atomic winner");
    Check(Winner<Count && Read(Path("race-winner"),TArray<uint8>(4096,static_cast<uint8>(Winner+1))) && SourcesIntact,
        "concurrent publication never mixes bytes or consumes losing sources");
    Check(FPlatformFileSystem::ReplaceFileAtomic(Path("loser"),Path("final")).IsSuccess() && Read(Path("final"),{4,5,6}),
        "explicit replacement remains a separate operation");
    std::error_code Cleanup;
    fs::remove_all(Root,Cleanup);
    Check(!Cleanup,"no-replace fixture cleans its owned temporary directory");
    return Failed;
}
