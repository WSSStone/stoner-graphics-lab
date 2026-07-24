#pragma once

#include "Core/CoreMinimal.h"

namespace Stoner::RHI
{

struct FRHITextureBufferCopyRegion
{
    Stoner::Core::uint32 SourceMipLevel = 0;
    Stoner::Core::uint32 SourceArrayLayer = 0;
    Stoner::Core::uint32 SourceX = 0;
    Stoner::Core::uint32 SourceY = 0;
    Stoner::Core::uint32 SourceZ = 0;
    Stoner::Core::uint32 Width = 1;
    Stoner::Core::uint32 Height = 1;
    Stoner::Core::uint32 Depth = 1;
    Stoner::Core::uint64 DestinationOffsetBytes = 0;
    Stoner::Core::uint32 DestinationRowLengthTexels = 0;
    Stoner::Core::uint32 DestinationImageHeightTexels = 0;
};

} // namespace Stoner::RHI
