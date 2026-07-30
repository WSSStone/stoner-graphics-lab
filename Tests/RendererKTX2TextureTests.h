#pragma once

struct FRendererKTX2TextureTestResult
{
    int Passed = 0;
    int Failed = 0;
};

[[nodiscard]] FRendererKTX2TextureTestResult
RunRendererKTX2TextureTests();
