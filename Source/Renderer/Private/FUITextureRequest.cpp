#include "Renderer/FUITextureRequest.h"

namespace Stoner::Renderer
{

namespace
{

[[nodiscard]] bool IsValidColorDomain(
    EUITextureColorDomain Domain) noexcept
{
    switch (Domain)
    {
    case EUITextureColorDomain::SRGBRec709:
    case EUITextureColorDomain::LinearRec709:
    case EUITextureColorDomain::AlphaCoverage:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsValidOperation(EUITextureOperation Operation) noexcept
{
    switch (Operation)
    {
    case EUITextureOperation::Create:
    case EUITextureOperation::Update:
    case EUITextureOperation::Destroy:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsRGBA8Format(Stoner::RHI::ERHIFormat Format) noexcept
{
    return Format == Stoner::RHI::ERHIFormat::R8G8B8A8_UNorm ||
        Format == Stoner::RHI::ERHIFormat::R8G8B8A8_sRGB;
}

[[nodiscard]] bool IsBoundedSlot(Stoner::Core::uint32 Slot) noexcept
{
    return Slot >= FUITextureId::MinimumSlot &&
        Slot <= FUITextureId::MaximumSlot;
}

[[nodiscard]] bool IsValidUtf8(
    const Stoner::Core::FString& Value) noexcept
{
    const std::string_view Bytes = Value.View();
    if (Bytes.size() > 1024)
    {
        return false;
    }

    for (std::size_t Index = 0; Index < Bytes.size();)
    {
        const auto Byte = static_cast<unsigned char>(Bytes[Index]);
        if (Byte <= 0x7Fu)
        {
            ++Index;
            continue;
        }

        std::size_t Length = 0;
        Stoner::Core::uint32 CodePoint = 0;
        Stoner::Core::uint32 Minimum = 0;
        if (Byte >= 0xC2u && Byte <= 0xDFu)
        {
            Length = 2;
            CodePoint = Byte & 0x1Fu;
            Minimum = 0x80u;
        }
        else if (Byte >= 0xE0u && Byte <= 0xEFu)
        {
            Length = 3;
            CodePoint = Byte & 0x0Fu;
            Minimum = 0x800u;
        }
        else if (Byte >= 0xF0u && Byte <= 0xF4u)
        {
            Length = 4;
            CodePoint = Byte & 0x07u;
            Minimum = 0x10000u;
        }
        else
        {
            return false;
        }

        if (Index + Length > Bytes.size())
        {
            return false;
        }
        for (std::size_t Offset = 1; Offset < Length; ++Offset)
        {
            const auto Continuation =
                static_cast<unsigned char>(Bytes[Index + Offset]);
            if ((Continuation & 0xC0u) != 0x80u)
            {
                return false;
            }
            CodePoint = (CodePoint << 6u) | (Continuation & 0x3Fu);
        }

        if (CodePoint < Minimum || CodePoint > 0x10FFFFu ||
            (CodePoint >= 0xD800u && CodePoint <= 0xDFFFu))
        {
            return false;
        }
        Index += Length;
    }
    return true;
}

[[nodiscard]] bool IsValidResult(Stoner::RHI::ERHIResult Result) noexcept
{
    switch (Result)
    {
    case Stoner::RHI::ERHIResult::Success:
    case Stoner::RHI::ERHIResult::InvalidState:
    case Stoner::RHI::ERHIResult::Unsupported:
    case Stoner::RHI::ERHIResult::Timeout:
    case Stoner::RHI::ERHIResult::NotReady:
    case Stoner::RHI::ERHIResult::ResizeRequired:
    case Stoner::RHI::ERHIResult::Unavailable:
    case Stoner::RHI::ERHIResult::Failed:
        return true;
    }
    return false;
}

[[nodiscard]] bool IsValidState(EUITextureState State) noexcept
{
    switch (State)
    {
    case EUITextureState::Requested:
    case EUITextureState::Prepared:
    case EUITextureState::UploadQueued:
    case EUITextureState::Ready:
    case EUITextureState::Retiring:
    case EUITextureState::Destroyed:
    case EUITextureState::Rejected:
        return true;
    }
    return false;
}

} // namespace

bool FUITextureRequest::IsValid() const noexcept
{
    if (RequestId == 0 || !IsValidOperation(Operation) ||
        !IsValidColorDomain(ColorDomain))
    {
        return false;
    }

    if (Operation == EUITextureOperation::Create)
    {
        if (!IsBoundedSlot(LogicalSlot) || TextureId.Slot != 0 ||
            TextureId.Generation != 0 ||
            ExpectedGeneration != 0)
        {
            return false;
        }
    }
    else
    {
        if (!TextureId.IsValid() || ExpectedGeneration == 0 ||
            ExpectedGeneration != TextureId.Generation ||
            (LogicalSlot != 0 && LogicalSlot != TextureId.Slot))
        {
            return false;
        }
    }

    if (Operation == EUITextureOperation::Destroy)
    {
        return Width == 0 && Height == 0 &&
            Format == Stoner::RHI::ERHIFormat::Unknown &&
            PixelBytes.empty();
    }

    if (Width == 0 || Height == 0 || Width > MaximumDimension ||
        Height > MaximumDimension || !IsRGBA8Format(Format))
    {
        return false;
    }

    const Stoner::Core::uint64 ExpectedBytes =
        static_cast<Stoner::Core::uint64>(Width) * Height * 4ull;
    if (ExpectedBytes > MaximumPayloadBytes)
    {
        return false;
    }

    return PixelBytes.size() == ExpectedBytes;
}

bool FUITextureResult::IsValid() const noexcept
{
    if (RequestId == 0 || !IsValidResult(Result) || !IsValidState(State) ||
        !IsValidUtf8(Diagnostic))
    {
        return false;
    }
    if (Result == Stoner::RHI::ERHIResult::Success)
    {
        return State != EUITextureState::Rejected &&
            State != EUITextureState::Requested && TextureId.IsValid();
    }
    return State == EUITextureState::Rejected ||
        (Result == Stoner::RHI::ERHIResult::NotReady &&
            (State == EUITextureState::Requested ||
             (State == EUITextureState::Retiring && TextureId.IsValid())));
}

const char* ToString(EUITextureColorDomain Domain) noexcept
{
    switch (Domain)
    {
    case EUITextureColorDomain::SRGBRec709: return "SRGBRec709";
    case EUITextureColorDomain::LinearRec709: return "LinearRec709";
    case EUITextureColorDomain::AlphaCoverage: return "AlphaCoverage";
    }
    return "Unknown";
}

const char* ToString(EUITextureOperation Operation) noexcept
{
    switch (Operation)
    {
    case EUITextureOperation::Create: return "Create";
    case EUITextureOperation::Update: return "Update";
    case EUITextureOperation::Destroy: return "Destroy";
    }
    return "Unknown";
}

const char* ToString(EUITextureSourceKind Source) noexcept
{
    switch (Source)
    {
    case EUITextureSourceKind::CPUUpload: return "CPUUpload";
    case EUITextureSourceKind::RendererProduced: return "RendererProduced";
    }
    return "Unknown";
}

const char* ToString(EUITextureState State) noexcept
{
    switch (State)
    {
    case EUITextureState::Requested: return "Requested";
    case EUITextureState::Prepared: return "Prepared";
    case EUITextureState::UploadQueued: return "UploadQueued";
    case EUITextureState::Ready: return "Ready";
    case EUITextureState::Retiring: return "Retiring";
    case EUITextureState::Destroyed: return "Destroyed";
    case EUITextureState::Rejected: return "Rejected";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
