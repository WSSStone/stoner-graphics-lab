// Minimal test executable - validates the full include chain
#include "Application/ApplicationMinimal.h"
#include "CoreFoundationTests.h"
#include "CoreMathTests.h"
#include "LoggingAssertionTests.h"

int main()
{
    const FCoreFoundationTestResult CoreResult = RunCoreFoundationTests();
    const FCoreMathTestResult MathResult = RunCoreMathTests();
    const FLoggingAssertionTestResult LogResult = RunLoggingAssertionTests();
    return CoreResult.Failed == 0 && MathResult.Failed == 0 && LogResult.Failed == 0 ? 0 : 1;
}
