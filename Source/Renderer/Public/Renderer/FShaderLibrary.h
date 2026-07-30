#pragma once

#include "Core/CoreMinimal.h"
#include "Renderer/FMaterialParameterSet.h"
#include "Renderer/FShaderPermutation.h"

#include <span>

namespace Stoner::Renderer
{

struct FShaderVariant
{
    Stoner::Core::FString VariantId;
    FShaderPermutation Permutation;
    Stoner::Core::FString StageSummary;
};

struct FShaderRequiredParameter
{
    Stoner::Core::FString Name;
    EMaterialParameterValueType Type = EMaterialParameterValueType::Scalar;
};

struct FShaderRecord
{
    Stoner::Core::FString ShaderId;
    Stoner::Core::FString DiagnosticsName;
    Stoner::Core::TArray<Stoner::Core::FString> AllowedPermutationFlags;
    Stoner::Core::TArray<FShaderVariant> Variants;
    Stoner::Core::TArray<FShaderRequiredParameter> RequiredParameters;
    bool bInvalidated = false;
};

class FShaderLibrary
{
public:
    [[nodiscard]] EMaterialResult RegisterShaderRecord(FShaderRecord Record, FMaterialDiagnosticLog* Diagnostics = nullptr);
    [[nodiscard]] EMaterialResult RegisterShaderRecords(
        std::span<const FShaderRecord> Records,
        FMaterialDiagnosticLog* Diagnostics = nullptr);
    [[nodiscard]] const FShaderRecord* FindRecord(const Stoner::Core::FString& ShaderId) const noexcept;
    [[nodiscard]] EMaterialResult ResolveVariant(const Stoner::Core::FString& ShaderId, const FShaderPermutation& Permutation,
        const FShaderVariant*& OutVariant, FMaterialDiagnosticLog* Diagnostics = nullptr) const;
    [[nodiscard]] EMaterialResult ValidateRequiredParameters(const FShaderRecord& Record, const FMaterialParameterSet& Parameters,
        FMaterialDiagnosticLog* Diagnostics = nullptr) const;
    void InvalidateRecord(const Stoner::Core::FString& ShaderId);
    void Clear();

    [[nodiscard]] const Stoner::Core::TArray<FShaderRecord>& GetRecords() const noexcept;
    [[nodiscard]] Stoner::Core::FString Dump() const;

private:
    [[nodiscard]] EMaterialResult RegisterShaderRecordValidated(
        FShaderRecord Record,
        FMaterialDiagnosticLog* Diagnostics);

    Stoner::Core::TArray<FShaderRecord> Records;
};

} // namespace Stoner::Renderer
