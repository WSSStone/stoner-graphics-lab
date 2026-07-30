#pragma once

#include "Asset/FAssetDiagnostics.h"
#include "Asset/FKTX2TextureArtifact.h"
#include "Asset/FTextureTranscode.h"
#include "Asset/FTextureAsset.h"
#include "IKTX2Encoder.h"

#include <span>

struct ktxTexture2;

namespace Stoner::Asset::Private
{

class FKTX2TextureHandle
{
public:
    FKTX2TextureHandle() = default;
    ~FKTX2TextureHandle();
    FKTX2TextureHandle(const FKTX2TextureHandle&) = delete;
    FKTX2TextureHandle& operator=(const FKTX2TextureHandle&) = delete;
    FKTX2TextureHandle(FKTX2TextureHandle&& Other) noexcept;
    FKTX2TextureHandle& operator=(
        FKTX2TextureHandle&& Other) noexcept;

    [[nodiscard]] ktxTexture2* Get() const noexcept;
    [[nodiscard]] ktxTexture2** Put() noexcept;
    void Reset() noexcept;

private:
    ktxTexture2* Texture_ = nullptr;
};

class FKTX2ContainerCodec
{
public:
    [[nodiscard]] static EAssetResult WriteUncompressed(
        const FTextureAsset& Texture,
        const Core::TArray<FKTX2EncoderMetadata>& Metadata,
        Core::TArray<Core::uint8>& OutBytes,
        FAssetDiagnosticList* OutDiagnostics = nullptr);

    [[nodiscard]] static EAssetResult Open(
        std::span<const Core::uint8> Bytes,
        FKTX2TextureHandle& OutTexture,
        FAssetDiagnosticList* OutDiagnostics = nullptr);
};

} // namespace Stoner::Asset::Private
