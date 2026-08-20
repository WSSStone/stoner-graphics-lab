#pragma once

#include "Asset/FAssetId.h"
#include "Asset/FAssetPayload.h"
#include "Asset/FAssetVersion.h"
#include "Asset/FMaterialShaderTypes.h"
#include "Asset/FShaderNativeBindingEvidence.h"
#include "Asset/FShaderNativeLibraryEvidence.h"
#include "Core/TArray.h"

#include <optional>

namespace Stoner::Asset
{

class FShaderPayloadAsset final : public FAssetPayload
{
public:
    [[nodiscard]] static EAssetResult Create(
        FAssetId Id,
        FAssetVersion Version,
        EShaderBackendFamily Backend,
        Core::FString Profile,
        EShaderPayloadFormat Format,
        EShaderStage Stage,
        Core::FString EntryPoint,
        FShaderPermutationKey Permutation,
        Core::TArray<Core::uint8> Bytes,
        FShaderPayloadAsset& OutAsset);

    [[nodiscard]] static EAssetResult CreateWithNativeEvidence(
        FAssetId Id,
        FAssetVersion Version,
        EShaderBackendFamily Backend,
        Core::FString Profile,
        EShaderPayloadFormat Format,
        EShaderStage Stage,
        Core::FString EntryPoint,
        FShaderPermutationKey Permutation,
        Core::TArray<Core::uint8> Bytes,
        FShaderNativeBindingEvidence NativeBindingEvidence,
        FShaderNativeLibraryEvidence NativeLibraryEvidence,
        FShaderPayloadAsset& OutAsset);

    [[nodiscard]] Core::FString GetAssetType() const override;
    [[nodiscard]] const FAssetId& GetId() const noexcept;
    [[nodiscard]] const FAssetVersion& GetVersion() const noexcept;
    [[nodiscard]] EShaderBackendFamily GetBackend() const noexcept;
    [[nodiscard]] const Core::FString& GetProfile() const noexcept;
    [[nodiscard]] EShaderPayloadFormat GetFormat() const noexcept;
    [[nodiscard]] EShaderStage GetStage() const noexcept;
    [[nodiscard]] const Core::FString& GetEntryPoint() const noexcept;
    [[nodiscard]] const FShaderPermutationKey& GetPermutation() const noexcept;
    [[nodiscard]] const Core::TArray<Core::uint8>& GetBytes() const noexcept;
    [[nodiscard]] const FShaderNativeBindingEvidence*
        GetNativeBindingEvidence() const noexcept;
    [[nodiscard]] const FShaderNativeLibraryEvidence*
        GetNativeLibraryEvidence() const noexcept;

private:
    FAssetId Id_;
    FAssetVersion Version_;
    EShaderBackendFamily Backend_ = EShaderBackendFamily::Vulkan;
    Core::FString Profile_;
    EShaderPayloadFormat Format_ = EShaderPayloadFormat::SPIRV;
    EShaderStage Stage_ = EShaderStage::Vertex;
    Core::FString EntryPoint_;
    FShaderPermutationKey Permutation_;
    Core::TArray<Core::uint8> Bytes_;
    std::optional<FShaderNativeBindingEvidence> NativeBindingEvidence_;
    std::optional<FShaderNativeLibraryEvidence> NativeLibraryEvidence_;
};

} // namespace Stoner::Asset
