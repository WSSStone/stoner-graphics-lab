#pragma once
#include "RHI/FRHIGraphicsPipelineDesc.h"

namespace Stoner::Backend::Metal::Private
{
[[nodiscard]] Core::FString BuildMetalGraphicsPipelineKey(
    const RHI::FRHIGraphicsPipelineDesc& Desc);
}
