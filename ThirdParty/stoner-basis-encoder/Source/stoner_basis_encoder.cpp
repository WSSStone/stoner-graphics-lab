#include "encoder/basisu_comp.h"
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace
{

constexpr std::uint32_t RequestMagic = 0x324B4753U;
constexpr std::uint32_t AbiVersion = 1;
constexpr std::uint32_t MaxMipCount = 15;
constexpr std::uint32_t MaxKeyValueCount = 64;
constexpr std::uint32_t MaxDimension = 16384;

enum class EStatus : std::uint32_t
{
    Success = 0,
    InvalidRequest = 1,
    UnsupportedPolicy = 2,
    InvalidMipChain = 3,
    InvalidMetadata = 4,
    EncoderFailure = 5
};

basisu::uint8_vec GResult;

std::uint32_t ReadU32(const std::uint8_t* Bytes) noexcept
{
    return static_cast<std::uint32_t>(Bytes[0]) |
        (static_cast<std::uint32_t>(Bytes[1]) << 8U) |
        (static_cast<std::uint32_t>(Bytes[2]) << 16U) |
        (static_cast<std::uint32_t>(Bytes[3]) << 24U);
}

bool IsRangeValid(
    std::uint32_t Offset,
    std::uint32_t Length,
    std::uint32_t Total) noexcept
{
    return Offset <= Total && Length <= Total - Offset;
}

bool IsCStringKeyValid(
    const std::uint8_t* Bytes,
    std::uint32_t Length) noexcept
{
    if (Length < 2 || Bytes[Length - 1] != 0)
    {
        return false;
    }
    for (std::uint32_t Index = 0; Index + 1 < Length; ++Index)
    {
        if (Bytes[Index] == 0)
        {
            return false;
        }
    }
    return std::strcmp(
        reinterpret_cast<const char*>(Bytes), "KTXwriter") != 0;
}

} // namespace

extern "C"
{

std::uint32_t stoner_encoder_version() noexcept
{
    return AbiVersion;
}

std::uint32_t stoner_alloc(std::uint32_t Size) noexcept
{
    return static_cast<std::uint32_t>(
        reinterpret_cast<std::uintptr_t>(std::malloc(Size)));
}

void stoner_free(std::uint32_t Address) noexcept
{
    std::free(reinterpret_cast<void*>(static_cast<std::uintptr_t>(Address)));
}

std::uint32_t stoner_result_ptr() noexcept
{
    return GResult.empty()
        ? 0
        : static_cast<std::uint32_t>(
            reinterpret_cast<std::uintptr_t>(GResult.data()));
}

std::uint32_t stoner_result_size() noexcept
{
    return static_cast<std::uint32_t>(GResult.size());
}

void stoner_release_result() noexcept
{
    basisu::clear_vector(GResult);
}

std::uint32_t stoner_cook(
    std::uint32_t RequestAddress,
    std::uint32_t RequestSize) noexcept
{
    stoner_release_result();
    if (RequestAddress == 0 || RequestSize < 32)
    {
        return static_cast<std::uint32_t>(EStatus::InvalidRequest);
    }

    const auto* Request = reinterpret_cast<const std::uint8_t*>(
        static_cast<std::uintptr_t>(RequestAddress));
    const std::uint32_t Magic = ReadU32(Request);
    const std::uint32_t Version = ReadU32(Request + 4);
    const std::uint32_t Policy = ReadU32(Request + 8);
    const std::uint32_t Quality = ReadU32(Request + 12);
    const std::uint32_t Flags = ReadU32(Request + 16);
    const std::uint32_t MipCount = ReadU32(Request + 20);
    const std::uint32_t KeyValueCount = ReadU32(Request + 24);
    if (Magic != RequestMagic || Version != AbiVersion ||
        (Policy != 1 && Policy != 2) || Quality > 1 ||
        MipCount == 0 || MipCount > MaxMipCount ||
        KeyValueCount > MaxKeyValueCount)
    {
        return static_cast<std::uint32_t>(
            Policy == 1 || Policy == 2
                ? EStatus::InvalidRequest
                : EStatus::UnsupportedPolicy);
    }

    const std::uint32_t MipTableOffset = 32;
    const std::uint32_t MipTableBytes = MipCount * 16;
    const std::uint32_t KeyValueTableOffset =
        MipTableOffset + MipTableBytes;
    const std::uint32_t KeyValueTableBytes = KeyValueCount * 16;
    if (!IsRangeValid(MipTableOffset, MipTableBytes, RequestSize) ||
        !IsRangeValid(
            KeyValueTableOffset, KeyValueTableBytes, RequestSize))
    {
        return static_cast<std::uint32_t>(EStatus::InvalidRequest);
    }

    basisu::vector<basisu::image> Images;
    Images.resize(MipCount);
    std::uint32_t ExpectedWidth = 0;
    std::uint32_t ExpectedHeight = 0;
    for (std::uint32_t Mip = 0; Mip < MipCount; ++Mip)
    {
        const std::uint8_t* Desc = Request + MipTableOffset + Mip * 16;
        const std::uint32_t Width = ReadU32(Desc);
        const std::uint32_t Height = ReadU32(Desc + 4);
        const std::uint32_t Offset = ReadU32(Desc + 8);
        const std::uint32_t Length = ReadU32(Desc + 12);
        if (Width == 0 || Height == 0 ||
            Width > MaxDimension || Height > MaxDimension ||
            Width > UINT32_MAX / Height ||
            Width * Height > UINT32_MAX / 4 ||
            Length != Width * Height * 4 ||
            !IsRangeValid(Offset, Length, RequestSize))
        {
            return static_cast<std::uint32_t>(EStatus::InvalidMipChain);
        }
        if (Mip == 0)
        {
            ExpectedWidth = Width;
            ExpectedHeight = Height;
        }
        else
        {
            ExpectedWidth = ExpectedWidth > 1 ? ExpectedWidth >> 1U : 1;
            ExpectedHeight = ExpectedHeight > 1 ? ExpectedHeight >> 1U : 1;
            if (Width != ExpectedWidth || Height != ExpectedHeight)
            {
                return static_cast<std::uint32_t>(
                    EStatus::InvalidMipChain);
            }
        }
        Images[Mip].init(Request + Offset, Width, Height, 4);
    }

    basisu::basis_compressor_params Params;
    basisu::job_pool JobPool(1);
    Params.m_pJob_pool = &JobPool;
    Params.m_source_images.resize(1);
    Params.m_source_images[0] = Images[0];
    if (MipCount > 1)
    {
        Params.m_source_mipmap_images.resize(1);
        Params.m_source_mipmap_images[0].resize(MipCount - 1);
        for (std::uint32_t Mip = 1; Mip < MipCount; ++Mip)
        {
            Params.m_source_mipmap_images[0][Mip - 1] = Images[Mip];
        }
    }

    Params.m_status_output = false;
    Params.m_debug = false;
    Params.m_multithreading = false;
    Params.m_use_opencl = false;
    Params.m_read_source_images = false;
    Params.m_write_output_basis_files = false;
    Params.m_compute_stats = false;
    Params.m_mip_gen = false;
    Params.m_perceptual = (Flags & 1U) != 0;
    Params.m_mip_srgb = Params.m_perceptual;
    Params.m_renormalize = (Flags & 2U) != 0;
    Params.m_force_alpha = (Flags & 4U) != 0;
    Params.m_check_for_alpha = true;
    Params.m_create_ktx2_file = true;
    Params.m_ktx2_srgb_transfer_func = Params.m_perceptual;
    Params.m_ktx2_uastc_supercompression = basist::KTX2_SS_NONE;
    Params.m_uastc = Policy == 2;
    Params.m_compression_level = 2;
    Params.m_quality_level = Quality == 0 ? 192 : 255;
    Params.m_pack_uastc_flags =
        Quality == 0
        ? basisu::cPackUASTCLevelDefault
        : basisu::cPackUASTCLevelSlower;
    Params.m_rdo_uastc = false;
    Params.m_rdo_uastc_multithreading = false;

    for (std::uint32_t Index = 0; Index < KeyValueCount; ++Index)
    {
        const std::uint8_t* Desc =
            Request + KeyValueTableOffset + Index * 16;
        const std::uint32_t KeyOffset = ReadU32(Desc);
        const std::uint32_t KeyLength = ReadU32(Desc + 4);
        const std::uint32_t ValueOffset = ReadU32(Desc + 8);
        const std::uint32_t ValueLength = ReadU32(Desc + 12);
        if (!IsRangeValid(KeyOffset, KeyLength, RequestSize) ||
            !IsRangeValid(ValueOffset, ValueLength, RequestSize) ||
            !IsCStringKeyValid(Request + KeyOffset, KeyLength) ||
            ValueLength == 0)
        {
            return static_cast<std::uint32_t>(EStatus::InvalidMetadata);
        }
        basist::ktx2_transcoder::key_value Entry;
        Entry.m_key.resize(KeyLength);
        Entry.m_value.resize(ValueLength);
        std::memcpy(
            Entry.m_key.data(), Request + KeyOffset, KeyLength);
        std::memcpy(
            Entry.m_value.data(), Request + ValueOffset, ValueLength);
        Params.m_ktx2_key_values.push_back(Entry);
    }

    basisu::basisu_encoder_init(false, false);
    basisu::basis_compressor Compressor;
    if (!Compressor.init(Params) ||
        Compressor.process() != basisu::basis_compressor::cECSuccess)
    {
        return static_cast<std::uint32_t>(EStatus::EncoderFailure);
    }
    GResult = Compressor.get_output_ktx2_file();
    return GResult.empty()
        ? static_cast<std::uint32_t>(EStatus::EncoderFailure)
        : static_cast<std::uint32_t>(EStatus::Success);
}

} // extern "C"
