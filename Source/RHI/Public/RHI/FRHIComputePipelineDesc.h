#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::RHI
{

class IRHIPipelineLayout;
class IRHIShaderModule;

struct FRHIComputePipelineDesc
{
    Stoner::Core::TArray<Stoner::Core::TSharedPtr<IRHIShaderModule>> ShaderModules;
    Stoner::Core::TSharedPtr<IRHIPipelineLayout> PipelineLayout;
};

} // namespace Stoner::RHI
