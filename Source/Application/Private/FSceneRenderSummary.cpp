#include "Application/FSceneRenderSummary.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace Stoner::Application
{

namespace
{

template <typename T>
[[nodiscard]] bool SortBySceneKey(const T& Left, const T& Right)
{
    if (Left.bHasSortKey != Right.bHasSortKey)
    {
        return Left.bHasSortKey;
    }
    if (Left.bHasSortKey && Left.SortKey != Right.SortKey)
    {
        return Left.SortKey < Right.SortKey;
    }
    return CompareEntityIdentity(Left.Entity, Right.Entity);
}

void AppendTransform(std::ostringstream& Stream, const Stoner::Core::FTransform& Transform)
{
    Stream << std::fixed << std::setprecision(3)
        << "pos=" << Transform.Translation.X << ',' << Transform.Translation.Y << ',' << Transform.Translation.Z
        << " scale=" << Transform.Scale.X << ',' << Transform.Scale.Y << ',' << Transform.Scale.Z;
}

} // namespace

void FSceneRenderSummary::Clear()
{
    Renderables.clear();
    Lights.clear();
    Cameras.clear();
    RejectedItems.clear();
    Diagnostics.Clear();
}

void FSceneRenderSummary::AddRenderable(const FSceneRenderableItem& Item)
{
    Renderables.push_back(Item);
}

void FSceneRenderSummary::AddLight(const FSceneLightItem& Item)
{
    Lights.push_back(Item);
}

void FSceneRenderSummary::AddCamera(const FSceneCameraItem& Item)
{
    Cameras.push_back(Item);
}

void FSceneRenderSummary::AddRejected(const FSceneRejectedItem& Item)
{
    RejectedItems.push_back(Item);
    Diagnostics.Add(ESceneDiagnosticSeverity::Warning,
        ESceneDiagnosticCategory::RenderCollection,
        Item.Result,
        Item.StableCode,
        FormatEntityIdentity(Item.Entity),
        Item.Message);
}

void FSceneRenderSummary::AddDiagnostic(const FSceneDiagnosticRecord& Record)
{
    Diagnostics.Add(Record.Severity,
        Record.Category,
        Record.Result,
        Record.StableCode,
        Record.SubjectName,
        Record.Message);
}

void FSceneRenderSummary::SortStable()
{
    std::stable_sort(Renderables.begin(), Renderables.end(), SortBySceneKey<FSceneRenderableItem>);
    std::stable_sort(Lights.begin(), Lights.end(), SortBySceneKey<FSceneLightItem>);
    std::stable_sort(Cameras.begin(), Cameras.end(), SortBySceneKey<FSceneCameraItem>);
    std::stable_sort(RejectedItems.begin(), RejectedItems.end(), [](const FSceneRejectedItem& Left, const FSceneRejectedItem& Right) {
        if (Left.ComponentType != Right.ComponentType)
        {
            return static_cast<int>(Left.ComponentType) < static_cast<int>(Right.ComponentType);
        }
        return CompareEntityIdentity(Left.Entity, Right.Entity);
    });
    Diagnostics.SortStable();
}

Stoner::Core::FString FSceneRenderSummary::BuildDebugDump() const
{
    std::ostringstream Stream;
    Stream << "SceneRenderSummary\n";
    Stream << "Renderables count=" << Renderables.size() << '\n';
    for (const FSceneRenderableItem& Item : Renderables)
    {
        Stream << "  " << FormatEntityIdentity(Item.Entity).CStr()
            << " mesh=" << Item.MeshId.CStr()
            << " material=" << Item.MaterialId.CStr()
            << " sort=" << (Item.bHasSortKey ? Item.SortKey : 0) << ' ';
        AppendTransform(Stream, Item.WorldTransform);
        Stream << '\n';
    }
    Stream << "Lights count=" << Lights.size() << '\n';
    for (const FSceneLightItem& Item : Lights)
    {
        Stream << "  " << FormatEntityIdentity(Item.Entity).CStr()
            << " type=" << ToString(Item.LightType)
            << " intensity=" << std::fixed << std::setprecision(3) << Item.Intensity
            << " range=" << Item.Range
            << " pos=" << Item.WorldPosition.X << ',' << Item.WorldPosition.Y << ',' << Item.WorldPosition.Z
            << " dir=" << Item.WorldDirection.X << ',' << Item.WorldDirection.Y << ',' << Item.WorldDirection.Z << '\n';
    }
    Stream << "Cameras count=" << Cameras.size() << '\n';
    for (const FSceneCameraItem& Item : Cameras)
    {
        Stream << "  " << FormatEntityIdentity(Item.Entity).CStr()
            << " projection=" << ToString(Item.Camera.ProjectionType)
            << " active=" << (Item.Camera.bActiveCamera ? "true" : "false") << ' ';
        AppendTransform(Stream, Item.WorldTransform);
        Stream << '\n';
    }
    Stream << "Rejected count=" << RejectedItems.size() << '\n';
    for (const FSceneRejectedItem& Item : RejectedItems)
    {
        Stream << "  " << FormatEntityIdentity(Item.Entity).CStr()
            << " component=" << ToString(Item.ComponentType)
            << " result=" << ToString(Item.Result)
            << " code=" << Item.StableCode.CStr()
            << " message=" << Item.Message.CStr() << '\n';
    }
    Stream << "Diagnostics\n" << Diagnostics.Format().CStr();
    return Stoner::Core::FString(Stream.str());
}

} // namespace Stoner::Application
