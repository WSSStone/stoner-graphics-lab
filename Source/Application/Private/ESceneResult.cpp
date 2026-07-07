#include "Application/ESceneResult.h"

#include "Application/ESceneComponentType.h"
#include "Application/ESceneLightType.h"
#include "Application/ESceneProjectionType.h"

namespace Stoner::Application
{

const char* ToString(ESceneResult Result) noexcept
{
    switch (Result)
    {
    case ESceneResult::Success: return "Success";
    case ESceneResult::InvalidEntity: return "InvalidEntity";
    case ESceneResult::StaleEntity: return "StaleEntity";
    case ESceneResult::DuplicateComponent: return "DuplicateComponent";
    case ESceneResult::MissingComponent: return "MissingComponent";
    case ESceneResult::InvalidComponentData: return "InvalidComponentData";
    case ESceneResult::HierarchyCycle: return "HierarchyCycle";
    case ESceneResult::InvalidHierarchyOperation: return "InvalidHierarchyOperation";
    case ESceneResult::CapacityExceeded: return "CapacityExceeded";
    case ESceneResult::Unsupported: return "Unsupported";
    }
    return "Unknown";
}

const char* ToString(ESceneComponentType Type) noexcept
{
    switch (Type)
    {
    case ESceneComponentType::Transform: return "Transform";
    case ESceneComponentType::Mesh: return "Mesh";
    case ESceneComponentType::Light: return "Light";
    case ESceneComponentType::Camera: return "Camera";
    }
    return "Unknown";
}

const char* ToString(ESceneLightType Type) noexcept
{
    switch (Type)
    {
    case ESceneLightType::Directional: return "Directional";
    case ESceneLightType::Point: return "Point";
    }
    return "Unknown";
}

const char* ToString(ESceneProjectionType Type) noexcept
{
    switch (Type)
    {
    case ESceneProjectionType::Perspective: return "Perspective";
    case ESceneProjectionType::Orthographic: return "Orthographic";
    }
    return "Unknown";
}

} // namespace Stoner::Application
