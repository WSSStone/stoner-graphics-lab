#include "Renderer/FDeferredLightVolume.h"

#include <algorithm>
#include <cmath>

namespace Stoner::Renderer
{

namespace
{

[[nodiscard]] float Clamp01(float Value) noexcept
{
    return Stoner::Core::FMath::Clamp(Value, 0.0f, 1.0f);
}

} // namespace

FDeferredLightVolumeClassification ClassifyDeferredLightVolume(
    const FDeferredLightRecord& Light, const FDeferredViewData& View) noexcept
{
    FDeferredLightVolumeClassification Result;
    if (Light.Type == EDeferredLightType::Directional)
    {
        Result.Acceptance = EDeferredLightAcceptance::AcceptedFullView;
        Result.Scissor = {0, 0, View.Extent.Width, View.Extent.Height};
        Result.bIntersectsView = true;
        return Result;
    }

    const float CameraDistance = (Light.Position - View.CameraPosition).Length();
    const Stoner::Core::FVector3 ViewPosition = View.View.TransformPoint(Light.Position);
    const float BoundingRadius = Light.Range;
    const bool bCameraInside = CameraDistance <= BoundingRadius;
    const bool bNearPlane = Stoner::Core::FMath::Abs(
        Stoner::Core::FMath::Abs(ViewPosition.Z) - View.DepthPolicy.NearPlane) <= BoundingRadius;

    const Stoner::Core::FVector4 Clip = View.ViewProjection.TransformVector4(
        {Light.Position.X, Light.Position.Y, Light.Position.Z, 1.0f});
    if (!Stoner::Core::FMath::IsFinite(Clip.X) || !Stoner::Core::FMath::IsFinite(Clip.Y) ||
        !Stoner::Core::FMath::IsFinite(Clip.Z) || !Stoner::Core::FMath::IsFinite(Clip.W) ||
        Stoner::Core::FMath::IsNearlyZero(Clip.W))
    {
        return Result;
    }
    const float InverseW = 1.0f / Stoner::Core::FMath::Abs(Clip.W);
    const float CenterX = Clip.X * InverseW;
    const float CenterY = Clip.Y * InverseW;
    const float CenterZ = Clip.Z * InverseW;
    const float RadiusNdc = BoundingRadius * InverseW;
    const bool bOutside = CenterX + RadiusNdc < -1.0f || CenterX - RadiusNdc > 1.0f ||
        CenterY + RadiusNdc < -1.0f || CenterY - RadiusNdc > 1.0f ||
        CenterZ + RadiusNdc < 0.0f || CenterZ - RadiusNdc > 1.0f;
    if (bOutside && !bCameraInside)
    {
        Result.Acceptance = EDeferredLightAcceptance::CulledOutsideView;
        return Result;
    }

    const float MinU = Clamp01((CenterX - RadiusNdc) * 0.5f + 0.5f);
    const float MaxU = Clamp01((CenterX + RadiusNdc) * 0.5f + 0.5f);
    const float MinV = Clamp01((CenterY - RadiusNdc) * 0.5f + 0.5f);
    const float MaxV = Clamp01((CenterY + RadiusNdc) * 0.5f + 0.5f);
    const Stoner::Core::uint32 MinX = static_cast<Stoner::Core::uint32>(
        std::floor(MinU * static_cast<float>(View.Extent.Width)));
    const Stoner::Core::uint32 MinY = static_cast<Stoner::Core::uint32>(
        std::floor(MinV * static_cast<float>(View.Extent.Height)));
    const Stoner::Core::uint32 MaxX = static_cast<Stoner::Core::uint32>(
        std::ceil(MaxU * static_cast<float>(View.Extent.Width)));
    const Stoner::Core::uint32 MaxY = static_cast<Stoner::Core::uint32>(
        std::ceil(MaxV * static_cast<float>(View.Extent.Height)));
    Result.Scissor = {MinX, MinY, std::max(1u, MaxX - MinX), std::max(1u, MaxY - MinY)};
    Result.bIntersectsView = true;
    Result.Acceptance = bCameraInside
        ? EDeferredLightAcceptance::AcceptedVolumeCameraInside
        : (bNearPlane ? EDeferredLightAcceptance::AcceptedVolumeNearPlaneIntersection
            : EDeferredLightAcceptance::AcceptedVolumeOutsideCamera);
    return Result;
}

void ApplyDeferredLightVolumeCulling(FDeferredLightSet& LightSet,
    const FDeferredViewData& View, bool bCullOutsideView)
{
    Stoner::Core::TArray<FDeferredLightRecord> Accepted;
    for (FDeferredLightRecord Record : LightSet.Accepted)
    {
        const FDeferredLightVolumeClassification Classification =
            ClassifyDeferredLightVolume(Record, View);
        Record.Acceptance = Classification.Acceptance;
        if (Classification.Acceptance == EDeferredLightAcceptance::CulledOutsideView &&
            bCullOutsideView)
        {
            Record.Reason = "outside-view";
            LightSet.Culled.push_back(Record);
        }
        else
        {
            if (Record.Acceptance == EDeferredLightAcceptance::CulledOutsideView)
            {
                Record.Acceptance = EDeferredLightAcceptance::AcceptedVolumeOutsideCamera;
            }
            Accepted.push_back(Record);
        }
    }
    LightSet.Accepted = std::move(Accepted);
    SortDeferredLightRecords(LightSet.Accepted);
    SortDeferredLightRecords(LightSet.Culled);
}

} // namespace Stoner::Renderer
