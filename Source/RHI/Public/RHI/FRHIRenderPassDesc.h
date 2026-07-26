#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIPipelineState.h"

namespace Stoner::RHI
{

struct FRHIClearColor
{
    float Red = 0.0f;
    float Green = 0.0f;
    float Blue = 0.0f;
    float Alpha = 1.0f;
};

struct FRHIRenderPassClearValues
{
    Stoner::Core::TArray<FRHIClearColor> Colors;
    float Depth = 1.0f;
    Stoner::Core::uint32 Stencil = 0;
};

struct FRHIRenderPassAttachmentDesc
{
    ERHIAttachmentRole Role = ERHIAttachmentRole::Color;
    ERHIFormat Format = ERHIFormat::Unknown;
    ERHISampleCount SampleCount = ERHISampleCount::One;
    ERHIAttachmentLoadOp LoadOp = ERHIAttachmentLoadOp::Clear;
    ERHIAttachmentStoreOp StoreOp = ERHIAttachmentStoreOp::Store;
};

struct FRHIRenderPassDesc
{
    Stoner::Core::TArray<FRHIRenderPassAttachmentDesc> Attachments;
};

[[nodiscard]] constexpr bool IsValidRHIRenderPassAttachmentDesc(
    const FRHIRenderPassAttachmentDesc& Desc) noexcept
{
    if (!IsValidRHIAttachmentRole(Desc.Role) ||
        !IsValidRHIFormat(Desc.Format) ||
        !IsValidRHISampleCount(Desc.SampleCount) ||
        !IsValidRHIAttachmentLoadOp(Desc.LoadOp) ||
        !IsValidRHIAttachmentStoreOp(Desc.StoreOp))
    {
        return false;
    }
    return Desc.Role == ERHIAttachmentRole::Color
        ? !IsDepthStencilFormat(Desc.Format)
        : IsDepthStencilFormat(Desc.Format);
}

[[nodiscard]] inline bool IsValidRHIRenderPassDesc(const FRHIRenderPassDesc& Desc) noexcept
{
    if (Desc.Attachments.empty())
    {
        return false;
    }

    bool bHasDepthStencil = false;
    const ERHISampleCount SampleCount = Desc.Attachments.front().SampleCount;
    for (const FRHIRenderPassAttachmentDesc& Attachment : Desc.Attachments)
    {
        if (!IsValidRHIRenderPassAttachmentDesc(Attachment) ||
            Attachment.SampleCount != SampleCount)
        {
            return false;
        }
        if (Attachment.Role == ERHIAttachmentRole::DepthStencil)
        {
            if (bHasDepthStencil)
            {
                return false;
            }
            bHasDepthStencil = true;
        }
    }
    return true;
}

} // namespace Stoner::RHI
