#pragma once

struct FTriangleDemoIntegrationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FTriangleDemoIntegrationTestResult RunTriangleDemoIntegrationTests();
