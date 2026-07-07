#pragma once

#include "Application/FSceneRenderSummary.h"

namespace Stoner::Application
{

class FWorld;

class FRenderSystem
{
public:
    [[nodiscard]] static FSceneRenderSummary Collect(const FWorld& World);
};

} // namespace Stoner::Application
