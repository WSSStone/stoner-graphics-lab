#pragma once

#include "RHI/FRHIBufferDesc.h"

namespace Stoner::RHI
{

// Source bytes are consumed before UploadBuffer returns; callers retain ownership.
struct FRHIBufferUploadDesc
{
    Stoner::Core::uint64 DestinationOffset = 0;
    const void* Data = nullptr;
    Stoner::Core::uint64 DataSizeBytes = 0;
};

[[nodiscard]] constexpr bool IsValidRHIBufferUploadDesc(
    const FRHIBufferDesc& BufferDesc,
    const FRHIBufferUploadDesc& Upload) noexcept
{
    return IsValidRHIBufferDesc(BufferDesc) &&
        Upload.Data != nullptr &&
        Upload.DataSizeBytes > 0 &&
        Upload.DestinationOffset <= BufferDesc.SizeInBytes &&
        Upload.DataSizeBytes <= BufferDesc.SizeInBytes - Upload.DestinationOffset;
}

} // namespace Stoner::RHI
