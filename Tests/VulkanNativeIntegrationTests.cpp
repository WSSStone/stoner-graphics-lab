#include "VulkanNativeIntegrationTests.h"
#include "ShaderTestFixtures.h"

#include "VulkanRHI/FVulkanComputePipeline.h"
#include "VulkanRHI/FVulkanDevice.h"
#include "VulkanRHI/FVulkanGraphicsPipeline.h"
#include "VulkanRHI/FVulkanNativeContext.h"
#include "VulkanRHI/FVulkanShaderModule.h"

#include <array>
#include <fstream>
#include <iostream>
#include <memory>
#include <utility>

namespace
{

void Record(FVulkanNativeIntegrationTestResult& Result, bool bPassed, const char* Name)
{
    if (bPassed) { ++Result.Passed; std::cout << "[PASS] " << Name << '\n'; }
    else { ++Result.Failed; std::cout << "[FAIL] " << Name << '\n'; }
}

Stoner::Core::TArray<Stoner::Core::uint32> ReadShaderWords(
    const char* Path)
{
    std::ifstream Stream(Path, std::ios::binary | std::ios::ate);
    if (!Stream)
    {
        return {};
    }
    const std::streamsize Size = Stream.tellg();
    if (Size <= 0 || (Size % 4) != 0)
    {
        return {};
    }
    Stoner::Core::TArray<Stoner::Core::uint32> Words(
        static_cast<std::size_t>(Size) /
        sizeof(Stoner::Core::uint32));
    Stream.seekg(0);
    Stream.read(
        reinterpret_cast<char*>(Words.data()),
        Size);
    return Stream.good() ? Words
                         : Stoner::Core::TArray<Stoner::Core::uint32>{};
}

Stoner::RHI::FRHIShaderModuleDesc NativeShaderDesc(
    Stoner::RHI::ERHIShaderStage Stage,
    const char* EntryPoint,
    const char* Identity,
    Stoner::Core::TArray<Stoner::Core::uint32> Words)
{
    Stoner::RHI::FRHIShaderModuleDesc Desc;
    Desc.Stage = Stage;
    Desc.EntryPoint = EntryPoint;
    Desc.PayloadIdentity = Identity;
    Desc.Bytecode.Words = std::move(Words);
    return Desc;
}

} // namespace

FVulkanNativeIntegrationTestResult RunVulkanNativeIntegrationTests()
{
    using namespace Stoner::Backend::Vulkan;
    using namespace Stoner::RHI;
    FVulkanNativeIntegrationTestResult Result;
    constexpr std::array VisibleFailurePoints = {
        EVulkanVisibleFrameFailurePoint::AcquireSuboptimal,
        EVulkanVisibleFrameFailurePoint::Record,
        EVulkanVisibleFrameFailurePoint::SubmitAfterFenceReset};
    bool bVisibleFailureLifecycleStable = true;
    for (EVulkanVisibleFrameFailurePoint FailurePoint : VisibleFailurePoints)
    {
        const FVulkanVisibleFrameFailureReport Report =
            FVulkanNativeContext::RunVisibleFrameFailureLifecycleValidation(
                FailurePoint);
        bVisibleFailureLifecycleStable =
            bVisibleFailureLifecycleStable &&
            Report.InjectedFailure == FailurePoint &&
            Report.bAcquiredStateReleased &&
            Report.bFenceReadyForReuse &&
            Report.NextAcquireResult == ERHIResult::Success &&
            Report.bPassed;
    }
    Record(Result, bVisibleFailureLifecycleStable,
        "Vulkan visible frame failure lifecycle releases acquired state and reusable fences");

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

    FRHIPipelineLayoutDesc PipelineLayoutDesc;
    PipelineLayoutDesc.Bindings = {{
        0,
        0,
        ERHIDescriptorType::UniformBuffer,
        1,
        ERHIShaderStageFlags::Vertex |
            ERHIShaderStageFlags::Fragment |
            ERHIShaderStageFlags::Compute}};
    const auto PipelineLayout =
        ShaderDevice.CreatePipelineLayout(PipelineLayoutDesc);
    const auto NativeVertex = ShaderDevice.CreateShaderModule(
        NativeShaderDesc(
            ERHIShaderStage::Vertex,
            "main",
            "native-pipeline-vs",
            ReadShaderWords(
                "Demo/StonerDemo/Shaders/Triangle.vert.spv")));
    const auto NativeFragment = ShaderDevice.CreateShaderModule(
        NativeShaderDesc(
            ERHIShaderStage::Fragment,
            "main",
            "native-pipeline-fs",
            ReadShaderWords(
                "Demo/StonerDemo/Shaders/Triangle.frag.spv")));
    const auto NativeCompute = ShaderDevice.CreateShaderModule(
        NativeShaderDesc(
            ERHIShaderStage::Compute,
            "NativeCS",
            "native-pipeline-cs",
            Stoner::Tests::MakeMinimalShaderBytecode(
                ERHIShaderStage::Compute, "NativeCS")));
    FRHIGraphicsPipelineDesc GraphicsDesc;
    GraphicsDesc.PipelineLayout = PipelineLayout.Object;
    GraphicsDesc.ShaderModules = {
        NativeVertex.Object, NativeFragment.Object};
    GraphicsDesc.VertexInput.Stride = sizeof(float) * 5;
    GraphicsDesc.VertexInput.Attributes = {
        {0, ERHIFormat::R32G32_Float, 0},
        {1, ERHIFormat::R32G32B32_Float, sizeof(float) * 2}};
    GraphicsDesc.RenderTargets.ColorFormats = {
        ERHIFormat::R8G8B8A8_UNorm};
    const auto NativeGraphicsPipeline =
        ShaderDevice.CreateGraphicsPipeline(GraphicsDesc);
    FRHIComputePipelineDesc ComputeDesc;
    ComputeDesc.PipelineLayout = PipelineLayout.Object;
    ComputeDesc.ShaderModules = {NativeCompute.Object};
    const auto NativeComputePipeline =
        ShaderDevice.CreateComputePipeline(ComputeDesc);
    auto ConcreteGraphicsPipeline =
        std::dynamic_pointer_cast<FVulkanGraphicsPipeline>(
            NativeGraphicsPipeline.Object);
    auto ConcreteComputePipeline =
        std::dynamic_pointer_cast<FVulkanComputePipeline>(
            NativeComputePipeline.Object);
    Record(Result,
        PipelineLayout.Succeeded() &&
            NativeVertex.Succeeded() &&
            NativeFragment.Succeeded() &&
            NativeCompute.Succeeded() &&
            NativeGraphicsPipeline.Succeeded() &&
            NativeComputePipeline.Succeeded() &&
            ConcreteGraphicsPipeline &&
            ConcreteComputePipeline &&
            ConcreteGraphicsPipeline->GetRuntimeMode() ==
                ERHIRuntimeObjectMode::RealRuntime &&
            ConcreteComputePipeline->GetRuntimeMode() ==
                ERHIRuntimeObjectMode::RealRuntime &&
            ConcreteGraphicsPipeline->HasNativeObject() &&
            ConcreteComputePipeline->HasNativeObject(),
        "Vulkan RHI pipeline factories retain real native graphics and compute pipelines");

    const auto ShutdownShader = ShaderDevice.CreateShaderModule(ShaderDesc);
    Record(Result,
        ConcreteShader->Invalidate() == ERHIResult::Success &&
            !ConcreteShader->HasNativeObject() &&
            ConcreteGraphicsPipeline &&
            ConcreteGraphicsPipeline->Invalidate() ==
                ERHIResult::Success &&
            !ConcreteGraphicsPipeline->HasNativeObject() &&
            ShutdownShader.Succeeded() &&
            ShaderDevice.Shutdown() == ERHIResult::Success &&
            ShutdownShader.Object->GetLifecycleState() ==
                ERHIResourceLifecycleState::Invalidated &&
            NativeComputePipeline.Object &&
            NativeComputePipeline.Object->GetLifecycleState() ==
                ERHIResourceLifecycleState::Invalidated &&
            !ShaderDevice.HasNativeShaderRuntime(),
        "Vulkan RHI device destroys native shader and pipeline ownership on explicit invalidation and shutdown");
    return Result;
}
