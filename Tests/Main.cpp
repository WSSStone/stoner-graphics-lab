// Minimal test executable - validates the full include chain
#include "Application/ApplicationMinimal.h"
#include "CoreFoundationTests.h"

int main()
{
    const FCoreFoundationTestResult CoreResult = RunCoreFoundationTests();
    return CoreResult.Failed == 0 ? 0 : 1;
}
