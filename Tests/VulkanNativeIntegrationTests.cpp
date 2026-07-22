#include "VulkanNativeIntegrationTests.h"

#include "VulkanRHI/FVulkanNativeContext.h"

#include <iostream>

namespace
{

void Record(FVulkanNativeIntegrationTestResult& Result, bool bPassed, const char* Name)
{
    if (bPassed) { ++Result.Passed; std::cout << "[PASS] " << Name << '\n'; }
    else { ++Result.Failed; std::cout << "[FAIL] " << Name << '\n'; }
}

} // namespace

FVulkanNativeIntegrationTestResult RunVulkanNativeIntegrationTests()
{
    using namespace Stoner::Backend::Vulkan;
    using namespace Stoner::RHI;
    FVulkanNativeIntegrationTestResult Result;
    FVulkanNativeContext Context;
    const ERHIResult InitializeResult = Context.Initialize(ERHIRuntimeMode::NativeHeadless);
    if (InitializeResult == ERHIResult::Unsupported || InitializeResult == ERHIResult::Unavailable)
    {
        Record(Result, true, "Vulkan native integration reports unavailable runtime explicitly");
        return Result;
    }
    Record(Result, InitializeResult == ERHIResult::Success && Context.GetSnapshot().ProvesNativeExecution(),
        "Vulkan native integration creates real instance and device");
    Record(Result, Context.ExecuteOffscreenTriangle("Demo/StonerDemo/Shaders/Triangle.vert.spv",
        "Demo/StonerDemo/Shaders/Triangle.frag.spv") == ERHIResult::Success,
        "Vulkan native integration uploads vertices and submits offscreen triangle");
    Record(Result, Context.GetSnapshot().GetTotalLiveObjectCount() == 2,
        "Vulkan native integration releases frame-local resources after completion");
    Record(Result, Context.Shutdown() == ERHIResult::Success && Context.GetSnapshot().GetTotalLiveObjectCount() == 0,
        "Vulkan native integration shutdown reaches zero live objects");
    return Result;
}
