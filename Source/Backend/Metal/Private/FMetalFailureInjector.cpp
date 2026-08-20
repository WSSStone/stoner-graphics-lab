#include "FMetalFailureInjector.h"

#include <utility>

namespace Stoner::Backend::Metal::Private
{
namespace
{

struct FMetalFailureState
{
    Core::TArray<EMetalFailurePoint> Points;
    Core::usize NextIndex = 0;
};

thread_local FMetalFailureState GFailureState;

} // namespace

const char* ToStableName(EMetalFailurePoint Point) noexcept
{
    switch (Point)
    {
    case EMetalFailurePoint::DeviceInitialization:
        return "metal-failure-device-initialization";
    case EMetalFailurePoint::ResourceAllocation:
        return "metal-failure-resource-allocation";
    case EMetalFailurePoint::PipelineCreation:
        return "metal-failure-pipeline-creation";
    case EMetalFailurePoint::CommandRecording:
        return "metal-failure-command-recording";
    case EMetalFailurePoint::CommandSubmission:
        return "metal-failure-command-submission";
    case EMetalFailurePoint::Synchronization:
        return "metal-failure-synchronization";
    case EMetalFailurePoint::DrawableAcquisition:
        return "metal-failure-drawable-acquisition";
    case EMetalFailurePoint::Presentation:
        return "metal-failure-presentation";
    case EMetalFailurePoint::Shutdown:
        return "metal-failure-shutdown";
    }
    return "metal-failure-unknown";
}

void FMetalFailureInjector::ConfigureOneShot(EMetalFailurePoint Point)
{
    ConfigureSequence(Core::TArray<EMetalFailurePoint>{Point});
}

void FMetalFailureInjector::ConfigureSequence(
    const Core::TArray<EMetalFailurePoint>& Points)
{
    GFailureState.Points = Points;
    GFailureState.NextIndex = 0;
}

void FMetalFailureInjector::Reset() noexcept
{
    GFailureState.Points.clear();
    GFailureState.NextIndex = 0;
}

bool FMetalFailureInjector::ShouldFail(EMetalFailurePoint Point) noexcept
{
    if (GFailureState.NextIndex >= GFailureState.Points.size() ||
        GFailureState.Points[GFailureState.NextIndex] != Point)
        return false;
    ++GFailureState.NextIndex;
    return true;
}

Core::uint64 FMetalFailureInjector::GetConsumedCount() noexcept
{
    return static_cast<Core::uint64>(GFailureState.NextIndex);
}

Core::uint64 FMetalFailureInjector::GetRemainingCount() noexcept
{
    return static_cast<Core::uint64>(
        GFailureState.Points.size() - GFailureState.NextIndex);
}

FScopedMetalFailureInjection::FScopedMetalFailureInjection(
    EMetalFailurePoint Point)
{
    FMetalFailureInjector::ConfigureOneShot(Point);
}

FScopedMetalFailureInjection::FScopedMetalFailureInjection(
    const Core::TArray<EMetalFailurePoint>& Points)
{
    FMetalFailureInjector::ConfigureSequence(Points);
}

FScopedMetalFailureInjection::~FScopedMetalFailureInjection()
{
    FMetalFailureInjector::Reset();
}

} // namespace Stoner::Backend::Metal::Private
