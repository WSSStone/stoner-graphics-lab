#pragma once

struct FApplicationSceneEcsTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FApplicationSceneEcsTestResult RunApplicationSceneEcsTests();
