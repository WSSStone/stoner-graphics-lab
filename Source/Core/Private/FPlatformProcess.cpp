#include "Core/FPlatformProcess.h"

#include "Core/SGPlatform.h"

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

bool HasExplicitPathMarker(const FString& Path)
{
    const auto View = Path.View();
    return View.find('/') != decltype(View)::npos ||
        View.find('\\') != decltype(View)::npos ||
        View.find(':') != decltype(View)::npos;
}

} // namespace

FDynamicModuleHandle FPlatformProcess::LoadDynamicModule(const FString& ExplicitPath)
{
    if (ExplicitPath.IsEmpty() || !HasExplicitPathMarker(ExplicitPath))
    {
        return {};
    }

#if SG_PLATFORM_WINDOWS
    HMODULE Module = LoadLibraryA(ExplicitPath.CStr());
    return {reinterpret_cast<void*>(Module)};
#else
    void* Module = dlopen(ExplicitPath.CStr(), RTLD_NOW | RTLD_LOCAL);
    return {Module};
#endif
}

void* FPlatformProcess::GetSymbol(FDynamicModuleHandle Module, const char* SymbolName) noexcept
{
    if (!Module.IsValid() || SymbolName == nullptr || SymbolName[0] == '\0')
    {
        return nullptr;
    }

#if SG_PLATFORM_WINDOWS
    FARPROC Symbol = GetProcAddress(reinterpret_cast<HMODULE>(Module.Handle), SymbolName);
    return reinterpret_cast<void*>(Symbol);
#else
    return dlsym(Module.Handle, SymbolName);
#endif
}

void FPlatformProcess::FreeDynamicModule(FDynamicModuleHandle& Module) noexcept
{
    if (!Module.IsValid())
    {
        return;
    }

#if SG_PLATFORM_WINDOWS
    FreeLibrary(reinterpret_cast<HMODULE>(Module.Handle));
#else
    dlclose(Module.Handle);
#endif
    Module.Handle = nullptr;
}

} // namespace Stoner::Core
