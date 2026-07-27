#include "VulkanNativeIntegrationTests.h"
#include "ShaderTestFixtures.h"

#include "VulkanRHI/FVulkanDevice.h"
#include "VulkanRHI/FVulkanNativeContext.h"
#include "VulkanRHI/FVulkanShaderModule.h"

#include <iostream>
#include <memory>

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

    FVulkanDevice ShaderDevice;
    FVulkanInstanceDesc DeviceDesc;
    DeviceDesc.RuntimeMode = EVulkanInstanceRuntimeMode::DeterministicFallback;
    DeviceDesc.bRequestValidation = false;
    Record(Result,
        ShaderDevice.Initialize(DeviceDesc) == ERHIResult::Success &&
            ShaderDevice.EnableNativeShaderRuntime() == ERHIResult::Success &&
            ShaderDevice.HasNativeShaderRuntime(),
        "Vulkan RHI device enables an owner-safe native shader runtime");

    FRHIShaderModuleDesc ShaderDesc;
    ShaderDesc.Stage = ERHIShaderStage::Vertex;
    ShaderDesc.EntryPoint = "NativeVS";
    ShaderDesc.PayloadIdentity = "native-rhi-shader";
    ShaderDesc.Bytecode.Words =
        Stoner::Tests::MakeMinimalShaderBytecode(
            ERHIShaderStage::Vertex, "NativeVS");
    const auto NativeShader = ShaderDevice.CreateShaderModule(ShaderDesc);
    auto ConcreteShader =
        std::dynamic_pointer_cast<FVulkanShaderModule>(NativeShader.Object);
    Record(Result,
        NativeShader.Succeeded() && ConcreteShader &&
            ConcreteShader->GetRuntimeMode() ==
                ERHIRuntimeObjectMode::RealRuntime &&
            ConcreteShader->GetValidationMode() ==
                ERHIShaderBytecodeValidationMode::Runtime &&
            ConcreteShader->HasNativeObject(),
        "Vulkan RHI shader factory retains a real native shader module");

    FRHIShaderModuleDesc WrongStage = ShaderDesc;
    WrongStage.Stage = ERHIShaderStage::Fragment;
    Record(Result,
        ShaderDevice.CreateShaderModule(WrongStage).Result ==
            ERHIResult::InvalidState,
        "Vulkan native shader factory rejects execution-model mismatch before runtime creation");

    const auto ShutdownShader = ShaderDevice.CreateShaderModule(ShaderDesc);
    Record(Result,
        ConcreteShader->Invalidate() == ERHIResult::Success &&
            !ConcreteShader->HasNativeObject() &&
            ShutdownShader.Succeeded() &&
            ShaderDevice.Shutdown() == ERHIResult::Success &&
            ShutdownShader.Object->GetLifecycleState() ==
                ERHIResourceLifecycleState::Invalidated &&
            !ShaderDevice.HasNativeShaderRuntime(),
        "Vulkan RHI device destroys native shader ownership on explicit invalidation and shutdown");
    return Result;
}
