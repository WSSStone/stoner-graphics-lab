#include "FGLTFDiagnostics.h"

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
    const Core::FString& Reason)
{
    if (Diagnostics == nullptr || Diagnostics->size() >= MaximumDiagnostics)
        return;
    FAssetDiagnostic Diagnostic;
    Diagnostic.Stage = Stage;
    Diagnostic.Result = Result;
    Diagnostic.Severity = Severity;
    Diagnostic.Code = Code;
    Diagnostic.Participant = Participant;
    Diagnostic.Subject = Subject;
    Diagnostic.Field = Field;
    Diagnostic.Reason = Reason;
    Diagnostics->push_back(std::move(Diagnostic));
}

} // namespace Stoner::Asset::Private
