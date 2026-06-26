#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/FRHIDescriptorBinding.h"

namespace Stoner::RHI
{

struct FRHIPipelineLayoutDesc
{
    Stoner::Core::TArray<FRHIDescriptorBinding> Bindings;
};

} // namespace Stoner::RHI
