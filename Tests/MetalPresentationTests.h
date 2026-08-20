#pragma once

struct FMetalPresentationTestResult
{
    int Passed = 0;
    int Failed = 0;
};

FMetalPresentationTestResult RunMetalPresentationTests();
