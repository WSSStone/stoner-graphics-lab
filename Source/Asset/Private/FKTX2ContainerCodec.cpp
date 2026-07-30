#include "FKTX2ContainerCodec.h"

#include "ktx.h"
#include "vkformat_enum.h"

#include <algorithm>
#include <cstdlib>
#include <string>

namespace Stoner::Asset::Private
{
namespace
{

void AddOpenDiagnostic(FAssetDiagnosticList* Diagnostics)
{
    if (Diagnostics == nullptr)
    {
        return;
    }
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = EAssetStage::Container;
    Diagnostic.Result = EAssetResult::MalformedContainer;
    Diagnostic.Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic.Code = Core::FString("asset.ktx2.libktx-open");
    Diagnostic.Participant = Core::FString("container.libktx");
    Diagnostic.Field = Core::FString("container");
    Diagnostic.Reason = Core::FString("container validation failed");
    Diagnostics->push_back(std::move(Diagnostic));
}

Core::uint32 ToVkFormat(
    EImageTexelFormat Format,
    EImageColorSpace ColorSpace)
{
    const bool SRGB = ColorSpace == EImageColorSpace::SRGB;
    switch (Format)
    {
    case EImageTexelFormat::R8_UNorm:
        return SRGB ? VK_FORMAT_R8_SRGB : VK_FORMAT_R8_UNORM;
    case EImageTexelFormat::R8G8_UNorm:
        return SRGB ? VK_FORMAT_R8G8_SRGB : VK_FORMAT_R8G8_UNORM;
    case EImageTexelFormat::R8G8B8_UNorm:
        return SRGB
            ? VK_FORMAT_R8G8B8_SRGB
            : VK_FORMAT_R8G8B8_UNORM;
    case EImageTexelFormat::R8G8B8A8_UNorm:
        return SRGB
            ? VK_FORMAT_R8G8B8A8_SRGB
            : VK_FORMAT_R8G8B8A8_UNORM;
    case EImageTexelFormat::R32G32B32_Float:
        return SRGB ? VK_FORMAT_UNDEFINED
                    : VK_FORMAT_R32G32B32_SFLOAT;
    case EImageTexelFormat::R16G16B16A16_Float:
        return SRGB ? VK_FORMAT_UNDEFINED
                    : VK_FORMAT_R16G16B16A16_SFLOAT;
    case EImageTexelFormat::R32G32B32A32_Float:
        return SRGB ? VK_FORMAT_UNDEFINED
                    : VK_FORMAT_R32G32B32A32_SFLOAT;
    case EImageTexelFormat::Unknown: break;
    }
    return VK_FORMAT_UNDEFINED;
}

void AddWriteDiagnostic(
    FAssetDiagnosticList* Diagnostics,
    const char* Field,
    KTX_error_code NativeResult = KTX_SUCCESS)
{
    if (Diagnostics == nullptr)
    {
        return;
    }
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = EAssetStage::Container;
    Diagnostic.Result = EAssetResult::CookFailure;
    Diagnostic.Severity = EAssetDiagnosticSeverity::Error;
    Diagnostic.Code = Core::FString("asset.ktx2.libktx-write");
    Diagnostic.Participant = Core::FString("container.libktx");
    Diagnostic.Field = Core::FString(Field);
    Diagnostic.Reason = Core::FString(
        "canonical uncompressed container write failed");
    if (NativeResult != KTX_SUCCESS)
    {
        Diagnostic.Actual = Core::FString(
            std::to_string(static_cast<int>(NativeResult)));
    }
    Diagnostics->push_back(std::move(Diagnostic));
}

} // namespace

FKTX2TextureHandle::~FKTX2TextureHandle()
{
    Reset();
}

FKTX2TextureHandle::FKTX2TextureHandle(
    FKTX2TextureHandle&& Other) noexcept
    : Texture_(Other.Texture_)
{
    Other.Texture_ = nullptr;
}

FKTX2TextureHandle& FKTX2TextureHandle::operator=(
    FKTX2TextureHandle&& Other) noexcept
{
    if (this != &Other)
    {
        Reset();
        Texture_ = Other.Texture_;
        Other.Texture_ = nullptr;
    }
    return *this;
}

ktxTexture2* FKTX2TextureHandle::Get() const noexcept
{
    return Texture_;
}

ktxTexture2** FKTX2TextureHandle::Put() noexcept
{
    Reset();
    return &Texture_;
}

void FKTX2TextureHandle::Reset() noexcept
{
    if (Texture_ != nullptr)
    {
        ktxTexture2_Destroy(Texture_);
        Texture_ = nullptr;
    }
}

EAssetResult FKTX2ContainerCodec::WriteUncompressed(
    const FTextureAsset& Texture,
    const Core::TArray<FKTX2EncoderMetadata>& Metadata,
    Core::TArray<Core::uint8>& OutBytes,
    FAssetDiagnosticList* OutDiagnostics)
{
    OutBytes.clear();
    if (Texture.GetMips().empty())
    {
        AddWriteDiagnostic(OutDiagnostics, "mips");
        return EAssetResult::InvalidInput;
    }
    const EImageTexelFormat Format =
        Texture.GetMips().front().GetFormat();
    const Core::uint32 VkFormat =
        ToVkFormat(Format, Texture.GetColorSpace());
    if (VkFormat == VK_FORMAT_UNDEFINED)
    {
        AddWriteDiagnostic(OutDiagnostics, "format");
        return EAssetResult::UnsupportedCompression;
    }
    for (const FImageMip& Mip : Texture.GetMips())
    {
        if (Mip.GetFormat() != Format)
        {
            AddWriteDiagnostic(OutDiagnostics, "mipFormat");
            return EAssetResult::InvalidInput;
        }
    }

    ktxTextureCreateInfo CreateInfo{};
    CreateInfo.vkFormat = VkFormat;
    CreateInfo.baseWidth =
        Texture.GetMips().front().GetExtent().Width;
    CreateInfo.baseHeight =
        Texture.GetMips().front().GetExtent().Height;
    CreateInfo.baseDepth = 1;
    CreateInfo.numDimensions = 2;
    CreateInfo.numLevels =
        static_cast<ktx_uint32_t>(Texture.GetMips().size());
    CreateInfo.numLayers = 1;
    CreateInfo.numFaces = 1;
    CreateInfo.isArray = KTX_FALSE;
    CreateInfo.generateMipmaps = KTX_FALSE;

    FKTX2TextureHandle Handle;
    if (ktxTexture2_Create(
            &CreateInfo,
            KTX_TEXTURE_CREATE_ALLOC_STORAGE,
            Handle.Put()) != KTX_SUCCESS)
    {
        AddWriteDiagnostic(OutDiagnostics, "create");
        return EAssetResult::CookFailure;
    }
    for (Core::usize Index = 0;
         Index < Texture.GetMips().size();
         ++Index)
    {
        const auto Bytes = Texture.GetMips()[Index].GetBytes();
        const KTX_error_code SetResult =
            ktxTexture_SetImageFromMemory(
                ktxTexture(Handle.Get()),
                static_cast<ktx_uint32_t>(Index),
                0,
                0,
                Bytes.data(),
                Bytes.size());
        if (SetResult != KTX_SUCCESS)
        {
            AddWriteDiagnostic(OutDiagnostics, "level", SetResult);
            return EAssetResult::CookFailure;
        }
    }

    Core::TArray<FKTX2EncoderMetadata> Entries = Metadata;
    FKTX2EncoderMetadata Writer;
    Writer.Key = Core::FString("KTXwriter");
    constexpr char WriterValue[] = "StonerGraphicsLab/022-v1";
    Writer.Value.assign(
        WriterValue, WriterValue + sizeof(WriterValue));
    Entries.push_back(std::move(Writer));
    std::sort(
        Entries.begin(),
        Entries.end(),
        [](const auto& Left, const auto& Right)
        {
            return Left.Key < Right.Key;
        });
    for (const FKTX2EncoderMetadata& Entry : Entries)
    {
        if (ktxHashList_AddKVPair(
                &Handle.Get()->kvDataHead,
                Entry.Key.CStr(),
                static_cast<unsigned int>(Entry.Value.size()),
                Entry.Value.data()) != KTX_SUCCESS)
        {
            AddWriteDiagnostic(OutDiagnostics, "metadata");
            return EAssetResult::CookFailure;
        }
    }

    ktx_uint8_t* Written = nullptr;
    ktx_size_t WrittenSize = 0;
    if (ktxTexture2_WriteToMemory(
            Handle.Get(), &Written, &WrittenSize) != KTX_SUCCESS ||
        Written == nullptr || WrittenSize == 0)
    {
        std::free(Written);
        AddWriteDiagnostic(OutDiagnostics, "serialize");
        return EAssetResult::CookFailure;
    }
    OutBytes.assign(Written, Written + WrittenSize);
    std::free(Written);
    return EAssetResult::Success;
}

EAssetResult FKTX2ContainerCodec::Open(
    std::span<const Core::uint8> Bytes,
    FKTX2TextureHandle& OutTexture,
    FAssetDiagnosticList* OutDiagnostics)
{
    OutTexture.Reset();
    const KTX_error_code Result = ktxTexture2_CreateFromMemory(
        Bytes.data(),
        Bytes.size(),
        KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
        OutTexture.Put());
    if (Result != KTX_SUCCESS)
    {
        OutTexture.Reset();
        AddOpenDiagnostic(OutDiagnostics);
        return EAssetResult::MalformedContainer;
    }
    return EAssetResult::Success;
}

} // namespace Stoner::Asset::Private
