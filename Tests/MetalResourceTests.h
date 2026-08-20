#pragma once

struct FMetalResourceTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FMetalResourceTestResult RunMetalResourceTests();
