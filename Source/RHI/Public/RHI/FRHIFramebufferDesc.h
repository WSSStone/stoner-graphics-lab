#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::RHI
{

class IRHIRenderPass;
class IRHITexture;

struct FRHIFramebufferAttachment
{
    Stoner::Core::TSharedPtr<IRHITexture> Texture;
    Stoner::Core::uint32 ArrayLayer = 0;
    Stoner::Core::uint32 MipLevel = 0;
};

struct FRHIFramebufferDesc
{
    Stoner::Core::TSharedPtr<IRHIRenderPass> RenderPass;
    Stoner::Core::TArray<FRHIFramebufferAttachment> Attachments;
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
};

} // namespace Stoner::RHI
