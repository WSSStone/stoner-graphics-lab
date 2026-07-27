#include "Core/FPlatformProcess.h"

#include "Core/SGPlatform.h"
#include "FPlatformProcessInternal.h"

#include <filesystem>
#include <utility>

#if SG_PLATFORM_WINDOWS
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace Stoner::Core
{

namespace
{

std::filesystem::path ToNativePath(const FString& Path)
{
    const std::string Utf8Path = Path.ToStdString();
    std::u8string NativeUtf8Path;
    NativeUtf8Path.reserve(Utf8Path.size());
    for (const unsigned char Byte : Utf8Path)
    {
        NativeUtf8Path.push_back(static_cast<char8_t>(Byte));
    }
    return std::filesystem::path(NativeUtf8Path);
}

} // namespace

bool Detail::IsExplicitDynamicModulePath(const FString& Path)
{
    if (Path.IsEmpty())
    {
        return false;
    }

    const std::filesystem::path NativePath = ToNativePath(Path);
#if SG_PLATFORM_WINDOWS
    return NativePath.has_parent_path() || NativePath.has_root_name();
#else
    return NativePath.has_parent_path();
#endif
}

FDynamicModuleHandle::~FDynamicModuleHandle() noexcept
{
    Reset();
}

FDynamicModuleHandle::FDynamicModuleHandle(FDynamicModuleHandle&& Other) noexcept
    : Handle_(std::exchange(Other.Handle_, nullptr))
{
}

FDynamicModuleHandle& FDynamicModuleHandle::operator=(FDynamicModuleHandle&& Other) noexcept
{
    if (this != &Other)
    {
        Reset();
        Handle_ = std::exchange(Other.Handle_, nullptr);
    }
    return *this;
}

void FDynamicModuleHandle::Reset() noexcept
{
    if (!IsValid())
    {
        return;
    }

#if SG_PLATFORM_WINDOWS
    FreeLibrary(reinterpret_cast<HMODULE>(Handle_));
#else
    dlclose(Handle_);
#endif
    Handle_ = nullptr;
}

FDynamicModuleHandle FPlatformProcess::LoadDynamicModule(const FString& ExplicitPath)
{
    if (!Detail::IsExplicitDynamicModulePath(ExplicitPath))
    {
        return {};
    }

    const std::filesystem::path NativePath = ToNativePath(ExplicitPath);
#if SG_PLATFORM_WINDOWS
    HMODULE Module = LoadLibraryW(NativePath.c_str());
    return FDynamicModuleHandle(reinterpret_cast<void*>(Module));
#else
    void* Module = dlopen(NativePath.c_str(), RTLD_NOW | RTLD_LOCAL);
    return FDynamicModuleHandle(Module);
#endif
}

void* FPlatformProcess::GetSymbol(
    const FDynamicModuleHandle& Module,
    const char* SymbolName) noexcept
{
    if (!Module.IsValid() || SymbolName == nullptr || SymbolName[0] == '\0')
    {
        return nullptr;
    }

#if SG_PLATFORM_WINDOWS
    FARPROC Symbol = GetProcAddress(reinterpret_cast<HMODULE>(Module.Handle_), SymbolName);
    return reinterpret_cast<void*>(Symbol);
#else
    return dlsym(Module.Handle_, SymbolName);
#endif
}

void FPlatformProcess::FreeDynamicModule(FDynamicModuleHandle& Module) noexcept
{
    Module.Reset();
}

} // namespace Stoner::Core
