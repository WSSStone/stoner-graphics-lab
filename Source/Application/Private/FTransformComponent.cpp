#include "Application/FTransformComponent.h"

namespace Stoner::Application
{

namespace
{

[[nodiscard]] bool IsFiniteVector(const Stoner::Core::FVector3& Vector) noexcept
{
    return Stoner::Core::FMath::IsFinite(Vector.X) &&
        Stoner::Core::FMath::IsFinite(Vector.Y) &&
        Stoner::Core::FMath::IsFinite(Vector.Z);
}

[[nodiscard]] bool IsFiniteQuat(const Stoner::Core::FQuat& Quat) noexcept
{
    return Stoner::Core::FMath::IsFinite(Quat.X) &&
        Stoner::Core::FMath::IsFinite(Quat.Y) &&
        Stoner::Core::FMath::IsFinite(Quat.Z) &&
        Stoner::Core::FMath::IsFinite(Quat.W);
}

[[nodiscard]] bool IsFiniteTransform(const Stoner::Core::FTransform& Transform) noexcept
{
    return IsFiniteVector(Transform.Translation) &&
        IsFiniteQuat(Transform.Rotation) &&
        IsFiniteVector(Transform.Scale);
}

} // namespace

FTransformComponent FTransformComponent::Identity() noexcept
{
    return FTransformComponent(Stoner::Core::FTransform::Identity());
}

bool FTransformComponent::IsValid() const noexcept
{
    return IsFiniteTransform(LocalTransform);
}

} // namespace Stoner::Application
