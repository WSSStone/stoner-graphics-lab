#pragma once

namespace Stoner::Application
{

enum class ESceneResult
{
    Success,
    InvalidEntity,
    StaleEntity,
    DuplicateComponent,
    MissingComponent,
    InvalidComponentData,
    HierarchyCycle,
    InvalidHierarchyOperation,
    CapacityExceeded,
    Unsupported
};

[[nodiscard]] const char* ToString(ESceneResult Result) noexcept;

} // namespace Stoner::Application
