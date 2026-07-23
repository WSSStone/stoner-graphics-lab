#include "Renderer/FDeferredLightData.h"

#include <algorithm>

namespace Stoner::Renderer
{

namespace
{

[[nodiscard]] bool IsFinite(const Stoner::Core::FVector3& Value) noexcept
{
    return Stoner::Core::FMath::IsFinite(Value.X) &&
        Stoner::Core::FMath::IsFinite(Value.Y) &&
        Stoner::Core::FMath::IsFinite(Value.Z);
}

[[nodiscard]] bool IsFiniteNonNegative(const Stoner::Core::FColor& Value) noexcept
{
    return Stoner::Core::FMath::IsFinite(Value.R) && Value.R >= 0.0f &&
        Stoner::Core::FMath::IsFinite(Value.G) && Value.G >= 0.0f &&
        Stoner::Core::FMath::IsFinite(Value.B) && Value.B >= 0.0f;
}

void AddInvalid(FDeferredDiagnosticLog* Diagnostics, const Stoner::Core::FString& Subject,
    const char* Code, const char* Reason)
{
    if (Diagnostics)
    {
        Diagnostics->Add(EDeferredDiagnosticSeverity::Error, EDeferredPassStage::DirectionalLighting,
            EDeferredResult::InvalidLight, Code, Subject, Reason);
    }
}

[[nodiscard]] bool CommonValid(FDeferredEntityIdentity Identity, const Stoner::Core::FString& Name,
    const Stoner::Core::FColor& Color, float Intensity, FDeferredDiagnosticLog* Diagnostics)
{
    if (!Identity.IsValid() || Name.IsEmpty())
    {
        AddInvalid(Diagnostics, Name, "DEF-LIGHT-ID", "light requires stable slot and generation identity");
        return false;
    }
    if (!IsFiniteNonNegative(Color) || !Stoner::Core::FMath::IsFinite(Intensity) || Intensity < 0.0f)
    {
        AddInvalid(Diagnostics, Name, "DEF-LIGHT-ENERGY", "light color and intensity must be finite and non-negative");
        return false;
    }
    return true;
}

FDeferredLightRecord MakeRecord(const FDeferredDirectionalLight& Light)
{
    FDeferredLightRecord Record;
    Record.Identity = Light.Identity;
    Record.Name = Light.Name;
    Record.Type = EDeferredLightType::Directional;
    Record.Direction = Light.Direction.GetSafeNormal();
    Record.Color = Light.Color;
    Record.Intensity = Light.Intensity;
    return Record;
}

FDeferredLightRecord MakeRecord(const FDeferredPointLight& Light)
{
    FDeferredLightRecord Record;
    Record.Identity = Light.Identity;
    Record.Name = Light.Name;
    Record.Type = EDeferredLightType::Point;
    Record.Position = Light.Position;
    Record.Color = Light.Color;
    Record.Intensity = Light.Intensity;
    Record.Range = Light.Range;
    return Record;
}

FDeferredLightRecord MakeRecord(const FDeferredSpotLight& Light)
{
    FDeferredLightRecord Record = MakeRecord(static_cast<const FDeferredPointLight&>(Light));
    Record.Type = EDeferredLightType::Spot;
    Record.Direction = Light.Direction.GetSafeNormal();
    Record.InnerConeAngleRadians = Light.InnerConeAngleRadians;
    Record.OuterConeAngleRadians = Light.OuterConeAngleRadians;
    return Record;
}

template <typename TLight>
void AcceptOrReject(const TLight& Light, bool bValid, FDeferredLightSet& Set)
{
    FDeferredLightRecord Record = MakeRecord(Light);
    if (!bValid)
    {
        Record.Acceptance = EDeferredLightAcceptance::RejectedInvalid;
        Record.Reason = "invalid-input";
        Set.Rejected.push_back(Record);
    }
    else if (Light.Intensity == 0.0f ||
        (Light.Color.R == 0.0f && Light.Color.G == 0.0f && Light.Color.B == 0.0f))
    {
        Record.Acceptance = EDeferredLightAcceptance::CulledOutsideView;
        Record.Reason = "zero-contribution";
        Set.Culled.push_back(Record);
    }
    else
    {
        Record.Acceptance = Record.Type == EDeferredLightType::Directional
            ? EDeferredLightAcceptance::AcceptedFullView
            : EDeferredLightAcceptance::AcceptedVolumeOutsideCamera;
        Set.Accepted.push_back(Record);
    }
}

} // namespace

Stoner::Core::uint32 FDeferredLightSet::GetAcceptedCount(EDeferredLightType Type) const noexcept
{
    Stoner::Core::uint32 Count = 0;
    for (const FDeferredLightRecord& Record : Accepted)
    {
        Count += Record.Type == Type ? 1u : 0u;
    }
    return Count;
}

bool IsValidDeferredDirectionalLight(const FDeferredDirectionalLight& Light,
    FDeferredDiagnosticLog* Diagnostics)
{
    const bool bCommon = CommonValid(Light.Identity, Light.Name, Light.Color, Light.Intensity, Diagnostics);
    if (!IsFinite(Light.Direction) ||
        Light.Direction.LengthSquared() <= Stoner::Core::FMath::DefaultTolerance)
    {
        AddInvalid(Diagnostics, Light.Name, "DEF-DIR-DIRECTION",
            "directional light direction must be finite and non-degenerate");
        return false;
    }
    return bCommon;
}

bool IsValidDeferredPointLight(const FDeferredPointLight& Light,
    FDeferredDiagnosticLog* Diagnostics)
{
    const bool bCommon = CommonValid(Light.Identity, Light.Name, Light.Color, Light.Intensity, Diagnostics);
    if (!IsFinite(Light.Position) || !Stoner::Core::FMath::IsFinite(Light.Range) || Light.Range <= 0.0f)
    {
        AddInvalid(Diagnostics, Light.Name, "DEF-POINT-BOUNDS",
            "point light position must be finite and range must be positive");
        return false;
    }
    return bCommon;
}

bool IsValidDeferredSpotLight(const FDeferredSpotLight& Light,
    FDeferredDiagnosticLog* Diagnostics)
{
    const bool bPoint = IsValidDeferredPointLight(Light, Diagnostics);
    const bool bDirection = IsFinite(Light.Direction) &&
        Light.Direction.LengthSquared() > Stoner::Core::FMath::DefaultTolerance;
    const bool bCones = Stoner::Core::FMath::IsFinite(Light.InnerConeAngleRadians) &&
        Stoner::Core::FMath::IsFinite(Light.OuterConeAngleRadians) &&
        Light.InnerConeAngleRadians >= 0.0f &&
        Light.InnerConeAngleRadians <= Light.OuterConeAngleRadians &&
        Light.OuterConeAngleRadians < Stoner::Core::FMath::Pi * 0.5f;
    if (!bDirection || !bCones)
    {
        AddInvalid(Diagnostics, Light.Name, "DEF-SPOT-CONE",
            "spot direction must be finite and cone radians require 0 <= inner <= outer < pi/2");
    }
    return bPoint && bDirection && bCones;
}

FDeferredLightSet PrepareDeferredLightSet(
    const Stoner::Core::TArray<FDeferredDirectionalLight>& DirectionalLights,
    const Stoner::Core::TArray<FDeferredPointLight>& PointLights,
    const Stoner::Core::TArray<FDeferredSpotLight>& SpotLights,
    FDeferredDiagnosticLog* Diagnostics)
{
    FDeferredLightSet Set;
    for (const FDeferredDirectionalLight& Light : DirectionalLights)
    {
        AcceptOrReject(Light, IsValidDeferredDirectionalLight(Light, Diagnostics), Set);
    }
    for (const FDeferredPointLight& Light : PointLights)
    {
        AcceptOrReject(Light, IsValidDeferredPointLight(Light, Diagnostics), Set);
    }
    for (const FDeferredSpotLight& Light : SpotLights)
    {
        AcceptOrReject(Light, IsValidDeferredSpotLight(Light, Diagnostics), Set);
    }
    SortDeferredLightRecords(Set.Accepted);
    SortDeferredLightRecords(Set.Culled);
    SortDeferredLightRecords(Set.Rejected);
    return Set;
}

void SortDeferredLightRecords(Stoner::Core::TArray<FDeferredLightRecord>& Records)
{
    std::sort(Records.begin(), Records.end(), [](const FDeferredLightRecord& Left,
        const FDeferredLightRecord& Right) {
        if (Left.Type != Right.Type)
        {
            return Left.Type < Right.Type;
        }
        if (!(Left.Identity == Right.Identity))
        {
            return Left.Identity < Right.Identity;
        }
        return Left.Name < Right.Name;
    });
}

const char* ToString(EDeferredLightType Type) noexcept
{
    switch (Type)
    {
    case EDeferredLightType::Directional: return "Directional";
    case EDeferredLightType::Point: return "Point";
    case EDeferredLightType::Spot: return "Spot";
    }
    return "Unknown";
}

const char* ToString(EDeferredLightAcceptance Acceptance) noexcept
{
    switch (Acceptance)
    {
    case EDeferredLightAcceptance::AcceptedFullView: return "AcceptedFullView";
    case EDeferredLightAcceptance::AcceptedVolumeOutsideCamera: return "AcceptedVolumeOutsideCamera";
    case EDeferredLightAcceptance::AcceptedVolumeCameraInside: return "AcceptedVolumeCameraInside";
    case EDeferredLightAcceptance::AcceptedVolumeNearPlaneIntersection: return "AcceptedVolumeNearPlaneIntersection";
    case EDeferredLightAcceptance::CulledOutsideView: return "CulledOutsideView";
    case EDeferredLightAcceptance::RejectedInvalid: return "RejectedInvalid";
    }
    return "Unknown";
}

} // namespace Stoner::Renderer
