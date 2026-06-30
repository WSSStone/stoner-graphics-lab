#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/FRHIShaderModuleDesc.h"

namespace Stoner::RHI
{

class IRHIPipelineLayout;
class IRHIShaderModule;

struct FRHIComputePipelineDesc
{
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<IRHIShaderModule>> ShaderModules;
    Stoner::Core::TSharedPtr<IRHIPipelineLayout> PipelineLayout;
    ERHIRuntimeObjectMode RuntimeMode = ERHIRuntimeObjectMode::Unknown;
    ERHIPipelineReuseState ReuseState = ERHIPipelineReuseState::NotReusable;
    Stoner::Core::FString CompatibilitySummary;
};

} // namespace Stoner::RHI
