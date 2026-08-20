#pragma once

#include "MetalRHI/FMetalBackendConfig.h"
#include "MetalRHI/FMetalBackendDiagnostics.h"
#include "RHI/ERHIResult.h"

namespace Stoner::Backend::Metal::Private
{

struct FMetalAdapterSelection
{
    RHI::ERHIResult Result = RHI::ERHIResult::Unavailable;
    void* RetainedNativeDevice = nullptr;
    FMetalAdapterSummary Selected;
    Core::TArray<FMetalAdapterSummary> Candidates;
    Core::FString StableReason;

    FMetalAdapterSelection() = default;
    ~FMetalAdapterSelection();
    FMetalAdapterSelection(const FMetalAdapterSelection&) = delete;
    FMetalAdapterSelection& operator=(const FMetalAdapterSelection&) = delete;
    FMetalAdapterSelection(FMetalAdapterSelection&& Other) noexcept;
    FMetalAdapterSelection& operator=(FMetalAdapterSelection&& Other) noexcept;

    [[nodiscard]] void* ReleaseNativeDevice() noexcept;
};

[[nodiscard]] RHI::ERHIResult SelectMetalAdapterCandidate(
    const Core::TArray<FMetalAdapterSummary>& Candidates,
    const FMetalBackendConfig& Config,
    Core::usize& OutIndex,
    Core::FString& OutStableReason) noexcept;

[[nodiscard]] FMetalAdapterSelection SelectMetalAdapter(
    const FMetalBackendConfig& Config) noexcept;

} // namespace Stoner::Backend::Metal::Private
