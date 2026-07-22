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

} // namespace Stoner::RHI
