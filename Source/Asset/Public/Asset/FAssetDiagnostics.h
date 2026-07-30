#pragma once

#include "Asset/EAssetResult.h"
#include "Core/FString.h"
#include "Core/TArray.h"

#include <optional>

namespace Stoner::Asset
{

struct FAssetDiagnostic
{
    EAssetStage Stage = EAssetStage::Identity;
    EAssetResult Result = EAssetResult::Success;
    EAssetDiagnosticSeverity Severity = EAssetDiagnosticSeverity::Info;
    Core::FString Code;
    Core::FString Subject;
    Core::FString Participant;
    Core::FString Field;
    Core::FString Actual;
    Core::FString Limit;
    Core::FString Reason;
    std::optional<Core::uint32> Level;

    [[nodiscard]] bool operator==(const FAssetDiagnostic&) const = default;
};

using FAssetDiagnosticList = Core::TArray<FAssetDiagnostic>;

class FAssetDiagnostics
{
public:
    [[nodiscard]] static Core::FString Format(const FAssetDiagnostic& Diagnostic);
    [[nodiscard]] static Core::FString FormatNormalized(FAssetDiagnosticList Diagnostics);
    [[nodiscard]] static FAssetDiagnostic FirstActionable(FAssetDiagnosticList Diagnostics);
};

} // namespace Stoner::Asset
