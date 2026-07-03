#include "Renderer/FForwardLightData.h"

#include <algorithm>

namespace Stoner::Renderer
{

namespace
{

float Luminance(const Stoner::Core::FColor& Color) noexcept
{
    return 0.2126f * Color.R + 0.7152f * Color.G + 0.0722f * Color.B;
}

void AddRejected(FForwardLightSet& Set, const FForwardPointLight& Light, const char* Code)
{
    Set.RejectedLights.push_back({Light.LightId, Light.Name, Code});
}

} // namespace

bool FForwardLightSet::HasAcceptedLights() const noexcept
{
    return bHasDirectionalLight || !AcceptedPointLights.empty();
}

int FForwardLightSet::GetAcceptedLightCount() const noexcept
{
    return (bHasDirectionalLight ? 1 : 0) + static_cast<int>(AcceptedPointLights.size());
}

bool IsValidForwardDirectionalLight(const FForwardDirectionalLight& Light, FForwardDiagnosticLog* Diagnostics)
{
    bool bValid = true;
    if (!IsStableForwardId(Light.LightId) || Light.Name.IsEmpty())
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Error, EForwardDiagnosticCategory::Light,
                EForwardResult::InvalidLight, "FWD-LIGHT-ID", Light.Name, "directional light requires stable identity");
        }
    }
    if (!IsForwardFinite(Light.Direction) || Light.Direction.LengthSquared() <= Stoner::Core::FMath::DefaultTolerance)
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Error, EForwardDiagnosticCategory::Light,
                EForwardResult::InvalidLight, "FWD-DIR-DIRECTION", Light.Name, "directional light direction must be finite and non-zero");
        }
    }
    if (!IsForwardFinite(Light.Color) || !IsForwardFinite(Light.Intensity) || Light.Intensity < 0.0f)
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Error, EForwardDiagnosticCategory::Light,
                EForwardResult::InvalidLight, "FWD-DIR-INTENSITY", Light.Name, "directional light color and intensity must be finite and non-negative");
        }
    }
    return bValid;
}

bool IsValidForwardPointLight(const FForwardPointLight& Light, FForwardDiagnosticLog* Diagnostics)
{
    bool bValid = true;
    if (!IsStableForwardId(Light.LightId) || Light.Name.IsEmpty())
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Error, EForwardDiagnosticCategory::Light,
                EForwardResult::InvalidLight, "FWD-POINT-ID", Light.Name, "point light requires stable identity");
        }
    }
    if (!IsForwardFinite(Light.Position))
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Error, EForwardDiagnosticCategory::Light,
                EForwardResult::InvalidLight, "FWD-POINT-POSITION", Light.Name, "point light position must be finite");
        }
    }
    if (!IsForwardFinite(Light.Color) || !IsForwardFinite(Light.Intensity) || Light.Intensity < 0.0f)
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Error, EForwardDiagnosticCategory::Light,
                EForwardResult::InvalidLight, "FWD-POINT-INTENSITY", Light.Name, "point light color and intensity must be finite and non-negative");
        }
    }
    if (!IsForwardFinite(Light.Range) || Light.Range <= 0.0f)
    {
        bValid = false;
        if (Diagnostics != nullptr)
        {
            Diagnostics->Add(EForwardDiagnosticSeverity::Error, EForwardDiagnosticCategory::Light,
                EForwardResult::InvalidLight, "FWD-POINT-RANGE", Light.Name, "point light range must be positive");
        }
    }
    return bValid;
}

float ComputeForwardPointLightInfluence(const FForwardPointLight& Light, const FForwardViewData& View) noexcept
{
    const float Distance = (Light.Position - View.CameraPosition).Length();
    const float RangeFactor = Stoner::Core::FMath::Clamp(1.0f - Distance / Stoner::Core::FMath::Max(Light.Range, 0.0001f), 0.0f, 1.0f);
    const float Effectiveness = Light.Intensity * Stoner::Core::FMath::Max(Luminance(Light.Color), 0.0f);
    return Effectiveness * RangeFactor / Stoner::Core::FMath::Max(Distance * Distance, 1.0f);
}

FForwardLightSet PrepareForwardLightSet(const Stoner::Core::TArray<FForwardDirectionalLight>& DirectionalLights,
    const Stoner::Core::TArray<FForwardPointLight>& PointLights,
    const FForwardViewData& View,
    int PointLightLimit,
    FForwardDiagnosticLog* Diagnostics)
{
    FForwardLightSet Set;

    for (const FForwardDirectionalLight& Light : DirectionalLights)
    {
        if (!Light.bPrimary)
        {
            continue;
        }

        if (!IsValidForwardDirectionalLight(Light, Diagnostics))
        {
            continue;
        }

        if (!Set.bHasDirectionalLight)
        {
            Set.bHasDirectionalLight = true;
            Set.DirectionalLight = Light;
        }
        else
        {
            if (Diagnostics != nullptr)
            {
                Diagnostics->Add(EForwardDiagnosticSeverity::Warning, EForwardDiagnosticCategory::Light,
                    EForwardResult::InvalidLight, "FWD-DIR-MULTIPLE-PRIMARY", Light.Name,
                    "additional primary directional light ignored; exactly one primary is accepted");
            }
        }
    }

    Stoner::Core::TArray<FForwardPointLight> ValidPointLights;
    for (FForwardPointLight Light : PointLights)
    {
        if (!IsValidForwardPointLight(Light, Diagnostics))
        {
            AddRejected(Set, Light, "FWD-POINT-INVALID");
            continue;
        }
        Light.InfluenceScore = ComputeForwardPointLightInfluence(Light, View);
        ValidPointLights.push_back(Light);
    }

    std::sort(ValidPointLights.begin(), ValidPointLights.end(), [](const FForwardPointLight& Left, const FForwardPointLight& Right) {
        if (!Stoner::Core::FMath::IsNearlyEqual(Left.InfluenceScore, Right.InfluenceScore))
        {
            return Left.InfluenceScore > Right.InfluenceScore;
        }
        if (Left.LightId != Right.LightId)
        {
            return Left.LightId < Right.LightId;
        }
        return Left.Name < Right.Name;
    });

    const int Limit = Stoner::Core::FMath::Max(PointLightLimit, 0);
    for (std::size_t Index = 0; Index < ValidPointLights.size(); ++Index)
    {
        if (static_cast<int>(Index) < Limit)
        {
            Set.AcceptedPointLights.push_back(ValidPointLights[Index]);
        }
        else
        {
            AddRejected(Set, ValidPointLights[Index], "FWD-POINT-LIMIT");
        }
    }

    if (Diagnostics != nullptr && static_cast<int>(ValidPointLights.size()) > Limit)
    {
        Diagnostics->Add(EForwardDiagnosticSeverity::Info, EForwardDiagnosticCategory::Light,
            EForwardResult::Success, "FWD-POINT-LIMIT", "PointLights",
            "point light candidates exceeded configured limit and were influence-sorted");
    }

    return Set;
}

} // namespace Stoner::Renderer
