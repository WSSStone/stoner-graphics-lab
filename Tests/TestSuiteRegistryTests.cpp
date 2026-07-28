#include "TestSuiteRegistryTests.h"

#include "TestSuiteRegistry.h"

#include <iostream>
#include <cstdlib>
#include <sstream>
#include <string>
#include <vector>

#if !defined(_WIN32)
#include <sys/wait.h>
#endif

namespace
{

void Record(FTestSuiteRegistryTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

void TestRegistrationAndListing(FTestSuiteRegistryTestResult& Result)
{
    FTestSuiteRegistry Registry;
    Registry.Register("zeta", [] { return 0; });
    Registry.Register("alpha", [] { return 0; });
    Record(Result, !Registry.Register("alpha", [] { return 0; }), "suite duplicate rejected");

    std::ostringstream Output;
    std::ostringstream Error;
    Record(
        Result,
        Registry.Execute({"--list-suites"}, Output, Error) == 0 &&
            Output.str() == "alpha\nzeta\n" && Error.str().empty(),
        "suite listing is canonical and side-effect free");
}

void TestSelectionAndExitCodes(FTestSuiteRegistryTestResult& Result)
{
    FTestSuiteRegistry Registry;
    int AlphaRuns = 0;
    int AssetRuns = 0;
    Registry.Register("alpha", [&AlphaRuns] { ++AlphaRuns; return 0; });
    Registry.Register("asset", [&AssetRuns] { ++AssetRuns; return 1; });

    std::ostringstream Output;
    std::ostringstream Error;
    Record(
        Result,
        Registry.Execute({"--suite", "asset", "--suite", "asset"}, Output, Error) == 1 &&
            AssetRuns == 1 && AlphaRuns == 0,
        "selected fake asset failure propagates and duplicates collapse");

    AlphaRuns = 0;
    AssetRuns = 0;
    Record(
        Result,
        Registry.Execute({}, Output, Error) == 1 && AlphaRuns == 1 && AssetRuns == 1,
        "no arguments runs all suites");

    Record(
        Result,
        Registry.Execute({"--suite"}, Output, Error) == 2 &&
            Registry.Execute({"--suite", "missing"}, Output, Error) == 2 &&
            Registry.Execute({"--unknown"}, Output, Error) == 2,
        "malformed and unknown selections return usage status");
}

int RunChild(const char* ExecutablePath, const char* Arguments)
{
#if defined(_WIN32)
    const std::string NullDevice = "NUL";
#else
    const std::string NullDevice = "/dev/null";
#endif
    const std::string Command = "\"" + std::string(ExecutablePath) + "\" " +
        Arguments + " > " + NullDevice + " 2>&1";
    const int Status = std::system(Command.c_str());
#if defined(_WIN32)
    return Status;
#else
    return Status >= 0 && WIFEXITED(Status) ? WEXITSTATUS(Status) : -1;
#endif
}

void TestExecutableModes(
    FTestSuiteRegistryTestResult& Result,
    const char* ExecutablePath)
{
    Record(
        Result,
        RunChild(ExecutablePath, "--list-suites") == 0 &&
            RunChild(ExecutablePath, "--suite asset") == 0 &&
            RunChild(ExecutablePath, "--unknown") == 2,
        "executable list, asset-only, and invalid usage exit codes");
}

} // namespace

FTestSuiteRegistryTestResult RunTestSuiteRegistryTests(const char* ExecutablePath)
{
    FTestSuiteRegistryTestResult Result;
    TestRegistrationAndListing(Result);
    TestSelectionAndExitCodes(Result);
    TestExecutableModes(Result, ExecutablePath);
    return Result;
}
