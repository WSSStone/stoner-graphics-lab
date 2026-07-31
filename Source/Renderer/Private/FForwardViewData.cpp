#include "Renderer/FForwardViewData.h"

#include <sstream>

namespace Stoner::Renderer
{

bool IsForwardFinite(float Value) noexcept
{
    return Stoner::Core::FMath::IsFinite(Value);
}

bool IsForwardFinite(const Stoner::Core::FVector3& Value) noexcept
{
    return IsForwardFinite(Value.X) && IsForwardFinite(Value.Y) && IsForwardFinite(Value.Z);
}

bool IsForwardFinite(const Stoner::Core::FVector4& Value) noexcept
{
    return IsForwardFinite(Value.X) && IsForwardFinite(Value.Y) && IsForwardFinite(Value.Z) && IsForwardFinite(Value.W);
}

bool IsForwardFinite(const Stoner::Core::FColor& Value) noexcept
{
    return IsForwardFinite(Value.R) && IsForwardFinite(Value.G) && IsForwardFinite(Value.B) && IsForwardFinite(Value.A);
}

bool IsForwardFinite(const Stoner::Core::FMatrix4x4& Value) noexcept
{
    for (int Row = 0; Row < 4; ++Row)
    {
        for (int Column = 0; Column < 4; ++Column)
        {
            if (!IsForwardFinite(Value.M[Row][Column]))
            {
                return false;
            }
        }
    }
    return true;
}

bool IsPositiveForwardExtent(FForwardExtent2D Extent) noexcept
{
    return Extent.Width > 0 && Extent.Height > 0;
}

bool IsStableForwardId(Stoner::Core::uint32 Id) noexcept
{
    return Id != 0;
}

Stoner::Core::FString FormatForwardVector(Stoner::Core::FVector3 Value)
{
    std::ostringstream Stream;
    Stream << '(' << Value.X << ',' << Value.Y << ',' << Value.Z << ')';
    return Stoner::Core::FString(Stream.str());
}

Stoner::Core::FVector3 TransformWorldPositionToView(
    const Stoner::Core::FMatrix4x4& ViewMatrix,
    Stoner::Core::FVector3 WorldPosition) noexcept
{
    return ViewMatrix.TransformPoint(WorldPosition);
}

float ComputeViewSpaceForwardDepth(
    const Stoner::Core::FMatrix4x4& ViewMatrix,
    Stoner::Core::FVector3 WorldPosition) noexcept
{
    return TransformWorldPositionToView(ViewMatrix, WorldPosition).X;
}

bool FForwardViewData::IsValid(FForwardDiagnosticLog* Diagnostics) const
{
    bool bValid = true;
    if (ViewName.IsEmpty())
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Error, EForwardDiagnosticCategory::View,
                EForwardResult::InvalidView, "FWD-VIEW-NAME", "View", "view name is required");
        }
    }
    if (!IsForwardFinite(ViewMatrix) || !IsForwardFinite(ViewProjectionMatrix))
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Error, EForwardDiagnosticCategory::View,
                EForwardResult::InvalidView, "FWD-VIEW-MATRIX", ViewName, "view matrices must contain finite values");
        }
    }
    if (!IsForwardFinite(CameraPosition))
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Error, EForwardDiagnosticCategory::View,
                EForwardResult::InvalidView, "FWD-VIEW-CAMERA", ViewName, "camera position must contain finite values");
        }
    }
    if (!IsPositiveForwardExtent(Viewport.Extent))
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Error, EForwardDiagnosticCategory::View,
                EForwardResult::InvalidView, "FWD-VIEW-EXTENT", ViewName, "viewport extent must be positive");
        }
    }
    return bValid;
}

float FForwardViewData::ComputeCameraSpaceDepth(Stoner::Core::FVector3 WorldPosition) const noexcept
{
    return ComputeViewSpaceForwardDepth(ViewMatrix, WorldPosition);
}

} // namespace Stoner::Renderer
