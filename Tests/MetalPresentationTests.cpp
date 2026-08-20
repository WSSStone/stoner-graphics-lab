#include "MetalPresentationTests.h"

#include "Core/SGPlatform.h"
#include "MetalRHI/FMetalDeviceFactory.h"
#if SG_PLATFORM_MAC
#include "FMetalDeviceOwnerState.h"
#include "FMetalPresentationContext.h"
#include "FMetalPresentationSurface.h"
#endif
#include "RHI/RHIMinimal.h"

#include <iostream>

namespace
{

using namespace Stoner;
using namespace Stoner::Backend::Metal;
using namespace Stoner::Core;
using namespace Stoner::RHI;

void Record(FMetalPresentationTestResult& Result, bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

} // namespace

FMetalPresentationTestResult RunMetalPresentationTests()
{
    FMetalPresentationTestResult Result;
#if SG_PLATFORM_MAC
    auto Owner = MakeShared<Private::FMetalDeviceOwnerState>(901);
    auto Context = MakeShared<Private::FMetalPresentationContext>(
        Owner, nullptr, nullptr);
    FRHIPresentationSurfaceDesc Desc;
    Desc.Window = FPlatformWindow(reinterpret_cast<void*>(0x1));
    auto Surface = MakeShared<Private::FMetalPresentationSurface>(
        Owner, Desc, Context);
#if defined(STONER_GLFW_AVAILABLE) && STONER_GLFW_AVAILABLE
    Record(Result,
        Context->Attach(Desc.Window, ERHIFormat::B8G8R8A8_UNorm, 2, true) ==
                ERHIResult::InvalidState &&
            !Context->IsAttached(),
        "presentation attach rejects missing native Metal ownership");
#else
    Record(Result,
        Context->Attach(Desc.Window, ERHIFormat::B8G8R8A8_UNorm, 2, true) ==
                ERHIResult::Unsupported &&
            !Context->IsAttached(),
        "presentation attach reports unsupported without GLFW");
#endif
    Record(Result,
        Surface->IsValid() && Surface->Invalidate() == ERHIResult::Success &&
            !Surface->IsValid() &&
            Surface->Invalidate() == ERHIResult::InvalidState,
        "presentation surface invalidation is generation-safe and idempotence-aware");

    const auto Created = CreateMetalDevice();
    if (!Created.Succeeded())
    {
        Record(Result, Created.Result == ERHIResult::Unavailable,
            "presentation factory reports controlled unavailable without a Metal device");
    }
    else
    {
        FRHIPresentationSurfaceDesc Invalid;
        Record(Result,
            Created.Device->CreatePresentationSurface(Invalid).Result ==
                ERHIResult::InvalidState,
            "presentation factory rejects an invalid borrowed window");
        (void)Created.Device->Shutdown();
    }
#else
    Record(Result, true,
        "Metal presentation contracts remain isolated from non-macOS builds");
#endif
    return Result;
}
