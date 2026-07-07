#include "Application/FMeshComponent.h"

namespace Stoner::Application
{

bool FMeshComponent::IsValid() const noexcept
{
    return !MeshId.IsEmpty();
}

} // namespace Stoner::Application
