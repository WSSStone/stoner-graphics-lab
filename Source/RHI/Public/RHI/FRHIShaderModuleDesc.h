#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIShaderStage.h"

namespace Stoner::RHI
{

struct FRHIShaderModuleDesc
{
    ERHIShaderStage Stage = ERHIShaderStage::Unknown;
    Stoner::Core::FString EntryPoint;
    Stoner::Core::FString PayloadIdentity;
    Stoner::Core::FString DebugName;
};

[[nodiscard]] inline bool IsSupportedRHIShaderStage(ERHIShaderStage Stage) noexcept
{
    return Stage == ERHIShaderStage::Vertex || Stage == ERHIShaderStage::Fragment || Stage == ERHIShaderStage::Compute;
}

[[nodiscard]] inline bool IsValidRHIShaderModuleDesc(const FRHIShaderModuleDesc& Desc) noexcept
{
    return IsSupportedRHIShaderStage(Desc.Stage) && !Desc.EntryPoint.IsEmpty() && !Desc.PayloadIdentity.IsEmpty();
}

} // namespace Stoner::RHI
