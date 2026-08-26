#pragma once

#include "Asset/FAssetManagerConfig.h"
#include "Asset/FAssetDiagnostics.h"
#include "Asset/FGenerationReaderLease.h"
#include "Asset/FPublishedGenerationValidator.h"

namespace Stoner::Asset::Private
{

class FBoundCookedGeneration
{
public:
    FBoundCookedGeneration() = default;
    FBoundCookedGeneration(const FBoundCookedGeneration&) = delete;
    FBoundCookedGeneration& operator=(const FBoundCookedGeneration&) = delete;
    FBoundCookedGeneration(FBoundCookedGeneration&&) noexcept = default;
    FBoundCookedGeneration& operator=(FBoundCookedGeneration&&) noexcept = default;

    [[nodiscard]] static EAssetResult Bind(
        const FAssetManagerConfig& Config,
        FBoundCookedGeneration& OutGeneration,
        FAssetDiagnosticList& OutDiagnostics);

    [[nodiscard]] bool IsBound() const noexcept;
    [[nodiscard]] const FCurrentGenerationPointer& GetPointer() const noexcept
    {
        return ValidatedGeneration_
            ? ValidatedGeneration_->Pointer : Pointer_;
    }
    [[nodiscard]] const FAssetCookManifest& GetManifest() const noexcept
    {
        return ValidatedGeneration_
            ? ValidatedGeneration_->Manifest : Manifest_;
    }
    [[nodiscard]] const Core::FString& GetGenerationDirectory() const noexcept
    {
        return ValidatedGeneration_
            ? ValidatedGeneration_->GenerationDirectory
            : GenerationDirectory_;
    }
    [[nodiscard]] const FGenerationReaderLease& GetReaderLease() const noexcept
    {
        return ReaderLease_;
    }
    void Reset() noexcept;

private:
    Core::TSharedPtr<const FPublishedGenerationValidationResult>
        ValidatedGeneration_;
    Core::TSharedPtr<FAssetCookedEnvelopeAuthentication>
        Authentication_;
    Core::FString GenerationDirectory_;
    FCurrentGenerationPointer Pointer_;
    FAssetCookManifest Manifest_;
    FGenerationReaderLease ReaderLease_;
};

} // namespace Stoner::Asset::Private
