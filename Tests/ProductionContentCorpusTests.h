#pragma once

struct FProductionContentCorpusTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FProductionContentCorpusTestResult
RunProductionContentCorpusTests();
