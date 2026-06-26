#pragma once

#include "Core/CoreMinimal.h"
#include "RHI/ERHIFormat.h"
#include "RHI/ERHIPipelineState.h"

namespace Stoner::RHI
{

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
