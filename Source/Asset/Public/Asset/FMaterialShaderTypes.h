#pragma once

#include "Asset/FAssetDigest.h"
#include "Asset/FAssetTargetProfile.h"
#include "Asset/FAssetId.h"
#include "Asset/FTextureAsset.h"
#include "Asset/TSoftAssetRef.h"
#include "Core/FColor.h"
#include "Core/FVector4.h"
#include "Core/TArray.h"

#include <variant>

namespace Stoner::Asset
{

class FShaderAsset;
class FShaderSourceAsset;
class FShaderPayloadAsset;
class FMaterialAsset;
class FMaterialInstanceAsset;

template <>
struct TAssetTypeTraits<FShaderAsset>
{
    static Core::FString GetAssetType() { return Core::FString("ShaderProgram"); }
};

template <>
struct TAssetTypeTraits<FShaderSourceAsset>
{
    static Core::FString GetAssetType() { return Core::FString("ShaderSource"); }
};

template <>
struct TAssetTypeTraits<FShaderPayloadAsset>
{
    static Core::FString GetAssetType() { return Core::FString("ShaderPayload"); }
};

template <>
struct TAssetTypeTraits<FMaterialAsset>
{
    static Core::FString GetAssetType() { return Core::FString("Material"); }
};

template <>
struct TAssetTypeTraits<FMaterialInstanceAsset>
{
    static Core::FString GetAssetType()
    {
        return Core::FString("MaterialInstance");
    }
};

enum class EShaderProgramKind : Core::uint8
{
    Graphics,
    Compute
};

enum class EShaderStage : Core::uint8
{
    Vertex,
    Fragment,
    Compute
};

enum class EShaderSourceLanguage : Core::uint8
{
    GLSL
};

enum class EShaderBackendFamily : Core::uint8
{
    Vulkan,
    Metal,
    DirectX12,
    OpenGL,
    GLES
};

enum class EShaderPayloadFormat : Core::uint8
{
    SPIRV,
    MSL,
    DXIL,
    GLSL,
    MetalLibrary
};

enum class EShaderResourceKind : Core::uint8
{
    UniformBuffer = 0,
    SampledTexture = 1,
    Sampler = 2,
    StorageBuffer = 3,
    StorageTexture = 4,
    CombinedTextureSampler = 5
};

enum class EMaterialAssetDomain : Core::uint8
{
    Surface,
    PostProcess,
    UI,
    Decal
};

enum class EMaterialAssetBlendMode : Core::uint8
{
    Opaque,
    Translucent,
    Additive,
    Masked
};

enum class EMaterialAssetParameterType : Core::uint8
{
    Scalar,
    Vector,
    Color,
    TextureReference,
    TextureBinding
};

enum class EAssetSamplerFilter : Core::uint8
{
    Nearest,
    Linear,
    Automatic
};

enum class EAssetSamplerMipFilter : Core::uint8
{
    None,
    Nearest,
    Linear,
    Automatic
};

enum class EAssetSamplerAddressMode : Core::uint8
{
    Repeat,
    MirroredRepeat,
    ClampToEdge
};

[[nodiscard]] constexpr bool IsValidAssetSamplerFilter(
    EAssetSamplerFilter Value) noexcept
{
    switch (Value)
    {
    case EAssetSamplerFilter::Nearest:
    case EAssetSamplerFilter::Linear:
    case EAssetSamplerFilter::Automatic:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr bool IsValidAssetSamplerMipFilter(
    EAssetSamplerMipFilter Value) noexcept
{
    switch (Value)
    {
    case EAssetSamplerMipFilter::None:
    case EAssetSamplerMipFilter::Nearest:
    case EAssetSamplerMipFilter::Linear:
    case EAssetSamplerMipFilter::Automatic:
        return true;
    }
    return false;
}

[[nodiscard]] constexpr bool IsValidAssetSamplerAddressMode(
    EAssetSamplerAddressMode Value) noexcept
{
    switch (Value)
    {
    case EAssetSamplerAddressMode::Repeat:
    case EAssetSamplerAddressMode::MirroredRepeat:
    case EAssetSamplerAddressMode::ClampToEdge:
        return true;
    }
    return false;
}

struct FMaterialSamplerIntent
{
    EAssetSamplerFilter MinFilter = EAssetSamplerFilter::Automatic;
    EAssetSamplerFilter MagFilter = EAssetSamplerFilter::Linear;
    EAssetSamplerMipFilter MipFilter = EAssetSamplerMipFilter::Automatic;
    EAssetSamplerAddressMode AddressU = EAssetSamplerAddressMode::Repeat;
    EAssetSamplerAddressMode AddressV = EAssetSamplerAddressMode::Repeat;

    [[nodiscard]] bool operator==(const FMaterialSamplerIntent&) const = default;
};

struct FMaterialTextureBinding
{
    TSoftAssetRef<FTextureAsset> Texture;
    Core::uint32 TexCoordSet = 0;
    FMaterialSamplerIntent Sampler;

    [[nodiscard]] static EAssetResult Create(
        const FAssetId& TextureId,
        Core::uint32 InTexCoordSet,
        FMaterialSamplerIntent InSampler,
        FMaterialTextureBinding& OutBinding);
    [[nodiscard]] bool operator==(
        const FMaterialTextureBinding& Other) const noexcept
    {
        return Texture.GetId() == Other.Texture.GetId() &&
            TexCoordSet == Other.TexCoordSet &&
            Sampler == Other.Sampler;
    }
};

struct FMaterialAssetRenderState
{
    bool bDepthTest = true;
    bool bDepthWrite = true;
    bool bTwoSided = false;
    [[nodiscard]] bool operator==(const FMaterialAssetRenderState&) const = default;
};

struct FShaderPermutationKey
{
    Core::TArray<Core::FString> Flags;
    [[nodiscard]] Core::FString ToString() const;
    [[nodiscard]] bool operator==(const FShaderPermutationKey&) const = default;
};

struct FShaderSourceReference
{
    EShaderStage Stage = EShaderStage::Vertex;
    Core::FString EntryPoint;
    EShaderSourceLanguage Language = EShaderSourceLanguage::GLSL;
    TSoftAssetRef<FShaderSourceAsset> Source;
    Core::FString Locator;
    FAssetDigest ExpectedDigest;
};

struct FShaderPayloadReference
{
    EShaderBackendFamily Backend = EShaderBackendFamily::Vulkan;
    Core::FString Profile;
    EShaderPayloadFormat Format = EShaderPayloadFormat::SPIRV;
    EShaderStage Stage = EShaderStage::Vertex;
    Core::FString EntryPoint;
    FShaderPermutationKey Permutation;
    TSoftAssetRef<FShaderPayloadAsset> Payload;
    Core::FString Locator;
    FAssetDigest ExpectedDigest;
    Core::FString Producer;
    Core::FString ProducerVersion;
};

struct FShaderVariantDefinition
{
    Core::FString VariantName;
    FShaderPermutationKey Permutation;
    Core::TArray<FShaderPayloadReference> Payloads;
};

struct FShaderRequiredParameter
{
    Core::FString Name;
    EMaterialAssetParameterType Type = EMaterialAssetParameterType::Scalar;
};

struct FShaderInterfaceBinding
{
    Core::uint32 SetIndex = 0;
    Core::uint32 BindingIndex = 0;
    EShaderResourceKind Kind = EShaderResourceKind::UniformBuffer;
    Core::uint32 ArrayCount = 1;
    Core::TArray<EShaderStage> Visibility;
    Core::FString Name;
};

struct FShaderConstantRange
{
    Core::uint32 OffsetBytes = 0;
    Core::uint32 SizeBytes = 0;
    Core::TArray<EShaderStage> Visibility;
};

struct FShaderTargetRequest
{
    EShaderBackendFamily Backend = EShaderBackendFamily::Vulkan;
    std::optional<EAssetTargetCpuArchitecture> CpuArchitecture;
    Core::TArray<Core::FString> AcceptableProfiles;
    FShaderPermutationKey Permutation;
};

struct FMaterialAssetParameterValue
{
    EMaterialAssetParameterType Type = EMaterialAssetParameterType::Scalar;
    std::variant<
        float,
        Core::FVector4,
        Core::FColor,
        FAssetId,
        FMaterialTextureBinding> Value = 0.0f;

    [[nodiscard]] static FMaterialAssetParameterValue FromScalar(float Value);
    [[nodiscard]] static FMaterialAssetParameterValue FromVector(
        Core::FVector4 Value);
    [[nodiscard]] static FMaterialAssetParameterValue FromColor(
        Core::FColor Value);
    [[nodiscard]] static FMaterialAssetParameterValue FromTexture(
        FAssetId Value);
    [[nodiscard]] static FMaterialAssetParameterValue FromTextureBinding(
        FMaterialTextureBinding Value);
    [[nodiscard]] bool operator==(const FMaterialAssetParameterValue&) const = default;
};

struct FMaterialAssetParameter
{
    Core::FString Name;
    FMaterialAssetParameterValue Value;
    [[nodiscard]] bool operator==(const FMaterialAssetParameter&) const = default;
};

struct FMaterialParentReference
{
    std::variant<TSoftAssetRef<FMaterialAsset>, TSoftAssetRef<FMaterialInstanceAsset>>
        Reference;
};

} // namespace Stoner::Asset
