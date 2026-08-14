#pragma once

#include "Asset/FAssetDiagnostics.h"

namespace Stoner::Asset::Private
{

void AppendGLTFDiagnostic(
    FAssetDiagnosticList* Diagnostics,
    Core::uint32 MaximumDiagnostics,
    EAssetStage Stage,
    EAssetResult Result,
    EAssetDiagnosticSeverity Severity,
    const Core::FString& Code,
    const Core::FString& Participant,
    const Core::FString& Subject,
    const Core::FString& Field,
    const Core::FString& Reason);

} // namespace Stoner::Asset::Private
