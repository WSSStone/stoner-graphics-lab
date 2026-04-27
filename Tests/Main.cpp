// Minimal test executable - validates the full include chain
#include "Application/ApplicationMinimal.h"
#include "CoreFoundationTests.h"
#include "CoreMathTests.h"

int main()
{
    const FCoreFoundationTestResult CoreResult = RunCoreFoundationTests();
    const FCoreMathTestResult MathResult = RunCoreMathTests();
    return CoreResult.Failed == 0 && MathResult.Failed == 0 ? 0 : 1;
}
