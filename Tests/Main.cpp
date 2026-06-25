// Minimal test executable - validates the full include chain
#include "Application/ApplicationMinimal.h"
#include "CoreFoundationTests.h"
#include "CoreMathTests.h"
#include "LoggingAssertionTests.h"
#include "CorePlatformTests.h"
#include "RHICoreTests.h"

int main()
{
    const FCoreFoundationTestResult CoreResult = RunCoreFoundationTests();
    const FCoreMathTestResult MathResult = RunCoreMathTests();
    const FLoggingAssertionTestResult LogResult = RunLoggingAssertionTests();
    const FCorePlatformTestResult PlatformResult = RunCorePlatformTests();
    const FRHICoreTestResult RHIResult = RunRHICoreTests();
    return CoreResult.Failed == 0 && MathResult.Failed == 0 &&
        LogResult.Failed == 0 && PlatformResult.Failed == 0 &&
        RHIResult.Failed == 0 ? 0 : 1;
}
