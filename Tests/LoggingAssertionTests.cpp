#include "LoggingAssertionTests.h"

#include "Core/CoreMinimal.h"

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <sstream>

#if defined(_WIN32)
    #include <io.h>
#else
    #include <unistd.h>
#endif

namespace
{

using namespace Stoner::Core;

void Record(FLoggingAssertionTestResult& Result, bool Passed, const char* Name)
{
    if (Passed)
    {
        ++Result.Passed;
        std::cout << "[PASS] " << Name << '\n';
    }
    else
    {
        ++Result.Failed;
        std::cout << "[FAIL] " << Name << '\n';
    }
}

// ============================================================================
// Capture infrastructure: redirect sink output to a buffer for verification.
// We capture stderr by redirecting the file descriptor temporarily.
// For simplicity, we capture by hooking into the sink via a temporary file.
// ============================================================================

// Simple output capture using temporary file redirection.
struct FOutputCapture
{
    int SavedStdout = -1;
    int SavedStderr = -1;
    FILE* CaptureFile = nullptr;
#if !defined(_WIN32)
    char TempPath[256] = {};
#endif
    bool Active = false;

    void Start()
    {
    #if defined(_WIN32)
        CaptureFile = std::tmpfile();
        if (!CaptureFile) return;
    #else
        std::snprintf(TempPath, sizeof(TempPath), "/tmp/sg_log_test_XXXXXX");
        const int Fd = mkstemp(TempPath);
        if (Fd < 0) return;
        CaptureFile = fdopen(Fd, "w+");
        if (!CaptureFile)
        {
            close(Fd);
            return;
        }
    #endif

        // Flush before redirecting.
        fflush(stdout);
        fflush(stderr);
        std::cout.flush();
        std::cerr.flush();

        // Save original file descriptors.
        SavedStdout = Dup(StdoutFileDescriptor());
        SavedStderr = Dup(StderrFileDescriptor());
        if (SavedStdout < 0 || SavedStderr < 0)
        {
            CloseSavedDescriptors();
            fclose(CaptureFile);
            CaptureFile = nullptr;
            return;
        }

        // Redirect stdout and stderr to the capture file.
        if (!Dup2(Fileno(CaptureFile), StdoutFileDescriptor()) ||
            !Dup2(Fileno(CaptureFile), StderrFileDescriptor()))
        {
            RestoreSavedDescriptors();
            fclose(CaptureFile);
            CaptureFile = nullptr;
            return;
        }
        Active = true;
    }

    std::string Stop()
    {
        if (!Active) return "";

        fflush(stdout);
        fflush(stderr);

        // Restore original file descriptors.
        RestoreSavedDescriptors();
        std::cout.clear();
        std::cerr.clear();
        Active = false;

        // Read captured content.
        fseek(CaptureFile, 0, SEEK_END);
        long Size = ftell(CaptureFile);
        fseek(CaptureFile, 0, SEEK_SET);

        std::string Content(static_cast<size_t>(Size), '\0');
        fread(&Content[0], 1, static_cast<size_t>(Size), CaptureFile);
        fclose(CaptureFile);
        CaptureFile = nullptr;

#if !defined(_WIN32)
        std::remove(TempPath);
#endif
        return Content;
    }

    static int StdoutFileDescriptor()
    {
    #if defined(_WIN32)
        return _fileno(stdout);
    #else
        return STDOUT_FILENO;
    #endif
    }

    static int StderrFileDescriptor()
    {
    #if defined(_WIN32)
        return _fileno(stderr);
    #else
        return STDERR_FILENO;
    #endif
    }

    static int Fileno(FILE* File)
    {
    #if defined(_WIN32)
        return _fileno(File);
    #else
        return fileno(File);
    #endif
    }

    static int Dup(int FileDescriptor)
    {
    #if defined(_WIN32)
        return _dup(FileDescriptor);
    #else
        return dup(FileDescriptor);
    #endif
    }

    static bool Dup2(int SourceFileDescriptor, int TargetFileDescriptor)
    {
    #if defined(_WIN32)
        return _dup2(SourceFileDescriptor, TargetFileDescriptor) == 0;
    #else
        return dup2(SourceFileDescriptor, TargetFileDescriptor) >= 0;
    #endif
    }

    static void Close(int FileDescriptor)
    {
    #if defined(_WIN32)
        _close(FileDescriptor);
    #else
        close(FileDescriptor);
    #endif
    }

    void CloseSavedDescriptors()
    {
        if (SavedStdout >= 0)
        {
            Close(SavedStdout);
            SavedStdout = -1;
        }
        if (SavedStderr >= 0)
        {
            Close(SavedStderr);
            SavedStderr = -1;
        }
    }

    void RestoreSavedDescriptors()
    {
        if (SavedStdout >= 0)
        {
            Dup2(SavedStdout, StdoutFileDescriptor());
        }
        if (SavedStderr >= 0)
        {
            Dup2(SavedStderr, StderrFileDescriptor());
        }
        CloseSavedDescriptors();
    }
};

// ============================================================================
// Assertion handler capture for testing.
// ============================================================================

struct FAssertionCapture
{
    bool WasCalled = false;
    std::string File;
    int Line = 0;
    std::string Expression;
    std::string Message;

    void Reset()
    {
        WasCalled = false;
        File.clear();
        Line = 0;
        Expression.clear();
        Message.clear();
    }
};

static FAssertionCapture GAssertionCapture;

static void TestAssertionHandler(const char* File, int Line,
                                 const char* Expression, const char* Message)
{
    GAssertionCapture.WasCalled = true;
    GAssertionCapture.File = File ? File : "";
    GAssertionCapture.Line = Line;
    GAssertionCapture.Expression = Expression ? Expression : "";
    GAssertionCapture.Message = Message ? Message : "";
}

// ============================================================================
// T008: ELogSeverity enum tests
// ============================================================================

void TestELogSeverity(FLoggingAssertionTestResult& Result)
{
    // Verify ordering: Verbose < Info < Warning < Error < Fatal
    Record(Result,
           static_cast<int>(ELogSeverity::Verbose) < static_cast<int>(ELogSeverity::Info),
           "ELogSeverity Verbose < Info");
    Record(Result,
           static_cast<int>(ELogSeverity::Info) < static_cast<int>(ELogSeverity::Warning),
           "ELogSeverity Info < Warning");
    Record(Result,
           static_cast<int>(ELogSeverity::Warning) < static_cast<int>(ELogSeverity::Error),
           "ELogSeverity Warning < Error");
    Record(Result,
           static_cast<int>(ELogSeverity::Error) < static_cast<int>(ELogSeverity::Fatal),
           "ELogSeverity Error < Fatal");

    // Verify SeverityToString returns correct labels.
    Record(Result,
           std::strcmp(SeverityToString(ELogSeverity::Verbose), "Verbose") == 0,
           "SeverityToString Verbose");
    Record(Result,
           std::strcmp(SeverityToString(ELogSeverity::Info), "Info") == 0,
           "SeverityToString Info");
    Record(Result,
           std::strcmp(SeverityToString(ELogSeverity::Warning), "Warning") == 0,
           "SeverityToString Warning");
    Record(Result,
           std::strcmp(SeverityToString(ELogSeverity::Error), "Error") == 0,
           "SeverityToString Error");
    Record(Result,
           std::strcmp(SeverityToString(ELogSeverity::Fatal), "Fatal") == 0,
           "SeverityToString Fatal");
}

// ============================================================================
// T009: FLogCategory construction tests
// ============================================================================

void TestFLogCategory(FLoggingAssertionTestResult& Result)
{
    // Verify pre-defined category name.
    Record(Result,
           std::strcmp(LogCore.GetName(), "LogCore") == 0,
           "FLogCategory LogCore name is 'LogCore'");

    // Verify default severity.
    Record(Result,
           LogCore.GetDefaultMinSeverity() == ELogSeverity::Verbose,
           "FLogCategory LogCore default severity is Verbose");

    // Verify GetMinSeverity returns current threshold.
    ELogSeverity OrigSeverity = LogCore.GetMinSeverity();
    Record(Result,
           OrigSeverity == ELogSeverity::Verbose,
           "FLogCategory LogCore initial min severity is Verbose");

    // Verify SetMinSeverity updates the threshold.
    LogCore.SetMinSeverity(ELogSeverity::Warning);
    Record(Result,
           LogCore.GetMinSeverity() == ELogSeverity::Warning,
           "FLogCategory SetMinSeverity updates threshold");

    // Restore original severity.
    LogCore.SetMinSeverity(OrigSeverity);
}

// ============================================================================
// T010: FLogConsoleSink output format tests
// ============================================================================

void TestFLogConsoleSinkFormat(FLoggingAssertionTestResult& Result)
{
    FOutputCapture Capture;
    Capture.Start();

    SG_LOG(LogCore, Info, "Hello %s", "World");

    std::string Output = Capture.Stop();

    // Verify format: [HH:MM:SS.mmm] LogCore: Info: Hello World\n
    Record(Result,
           Output.find("LogCore") != std::string::npos,
           "FLogConsoleSink output contains category name");
    Record(Result,
           Output.find("Info") != std::string::npos,
           "FLogConsoleSink output contains severity label");
    Record(Result,
           Output.find("Hello World") != std::string::npos,
           "FLogConsoleSink output contains formatted message");
    Record(Result,
           Output.size() > 0 && Output[0] == '[',
           "FLogConsoleSink output starts with timestamp bracket");
    Record(Result,
           Output.find('\n') != std::string::npos,
           "FLogConsoleSink output ends with newline");

    // Verify timestamp format [HH:MM:SS.mmm] — check for colon pattern.
    bool HasTimestamp = (Output.size() > 13 &&
                         Output[3] == ':' && Output[6] == ':' && Output[9] == '.');
    Record(Result, HasTimestamp, "FLogConsoleSink output has [HH:MM:SS.mmm] timestamp");
}

// ============================================================================
// T011: FLog::LogMessage severity routing tests
// ============================================================================

void TestFLogMessageSeverityRouting(FLoggingAssertionTestResult& Result)
{
    // Install custom assertion handler to prevent Fatal from aborting.
    FLog::SetAssertionHandler(TestAssertionHandler);

    // Test all five severity levels produce output.
    for (int i = 0; i <= 3; ++i) // Skip Fatal (would abort)
    {
        FOutputCapture Capture;
        Capture.Start();

        switch (i)
        {
            case 0: SG_LOG(LogCore, Verbose, "test verbose"); break;
            case 1: SG_LOG(LogCore, Info, "test info"); break;
            case 2: SG_LOG(LogCore, Warning, "test warning"); break;
            case 3: SG_LOG(LogCore, Error, "test error"); break;
        }

        std::string Output = Capture.Stop();

        const char* Labels[] = {"Verbose", "Info", "Warning", "Error"};
        char TestName[128];
        std::snprintf(TestName, sizeof(TestName),
                      "FLog::LogMessage %s produces labeled output", Labels[i]);
        Record(Result, Output.find(Labels[i]) != std::string::npos, TestName);
    }

    // Restore default handler.
    FLog::SetAssertionHandler(nullptr);
}

// ============================================================================
// T012: SG_LOG macro early-out test
// ============================================================================

void TestSGLogMacroEarlyOut(FLoggingAssertionTestResult& Result)
{
    // Set LogCore to Warning — Info messages should be filtered.
    ELogSeverity OrigSeverity = LogCore.GetMinSeverity();
    LogCore.SetMinSeverity(ELogSeverity::Warning);

    int SideEffectCounter = 0;

    // This should NOT evaluate the format arguments because Info < Warning.
    SG_LOG(LogCore, Info, "counter=%d", ++SideEffectCounter);

    Record(Result,
           SideEffectCounter == 0,
           "SG_LOG macro early-out does not evaluate format args when filtered");

    // This SHOULD evaluate because Error >= Warning.
    FOutputCapture Capture;
    Capture.Start();
    SG_LOG(LogCore, Error, "counter=%d", ++SideEffectCounter);
    Capture.Stop();

    Record(Result,
           SideEffectCounter == 1,
           "SG_LOG macro evaluates format args when not filtered");

    // Restore.
    LogCore.SetMinSeverity(OrigSeverity);
}

// ============================================================================
// T013: Fatal log behavior test
// ============================================================================

void TestFatalLogBehavior(FLoggingAssertionTestResult& Result)
{
    // Install custom assertion handler to intercept Fatal without aborting.
    // Note: Fatal log calls SG_DEBUG_BREAK() then std::abort().
    // We can't easily test the actual abort, but we can test that
    // the logging system formats Fatal messages correctly.
    // For this test, we verify Fatal output format only (without actually calling Fatal).

    FOutputCapture Capture;
    Capture.Start();
    SG_LOG(LogCore, Error, "Simulated fatal condition: %s", "out of memory");
    std::string Output = Capture.Stop();

    Record(Result,
           Output.find("Error") != std::string::npos &&
           Output.find("out of memory") != std::string::npos,
           "Fatal-like log produces formatted output with message");

    // Test that assertion handler is invokable via HandleAssertionFailure.
    GAssertionCapture.Reset();
    FLog::SetAssertionHandler(TestAssertionHandler);

    FLog::HandleAssertionFailure("test.cpp", 42, "false", nullptr);

    Record(Result,
           GAssertionCapture.WasCalled,
           "FLog::HandleAssertionFailure invokes custom assertion handler");
    Record(Result,
           GAssertionCapture.Line == 42,
           "FLog::HandleAssertionFailure passes correct line number");

    FLog::SetAssertionHandler(nullptr);
}

// ============================================================================
// T024: SG_CHECK assertion test
// ============================================================================

void TestSGCheck(FLoggingAssertionTestResult& Result)
{
    GAssertionCapture.Reset();
    FLog::SetAssertionHandler(TestAssertionHandler);

    FOutputCapture Capture;
    Capture.Start();

    SG_CHECK(false);

    Capture.Stop();

#ifdef _DEBUG
    Record(Result,
           GAssertionCapture.WasCalled,
           "SG_CHECK(false) triggers assertion handler in Debug");
    Record(Result,
           GAssertionCapture.Expression.find("false") != std::string::npos,
           "SG_CHECK(false) reports expression text 'false'");
    Record(Result,
           !GAssertionCapture.File.empty(),
           "SG_CHECK(false) reports file path");
    Record(Result,
           GAssertionCapture.Line > 0,
           "SG_CHECK(false) reports line number");
#else
    Record(Result,
           !GAssertionCapture.WasCalled,
           "SG_CHECK(false) is stripped in Release build");
#endif

    FLog::SetAssertionHandler(nullptr);
}

// ============================================================================
// T025: SG_CHECKF assertion test
// ============================================================================

void TestSGCheckF(FLoggingAssertionTestResult& Result)
{
    GAssertionCapture.Reset();
    FLog::SetAssertionHandler(TestAssertionHandler);

    FOutputCapture Capture;
    Capture.Start();

    SG_CHECKF(false, "Index %d out of range", 42);

    Capture.Stop();

#ifdef _DEBUG
    Record(Result,
           GAssertionCapture.WasCalled,
           "SG_CHECKF(false, ...) triggers assertion handler in Debug");
    Record(Result,
           GAssertionCapture.Expression.find("false") != std::string::npos,
           "SG_CHECKF(false, ...) reports expression text");
    Record(Result,
           GAssertionCapture.Message.find("42") != std::string::npos,
           "SG_CHECKF(false, ...) reports formatted message with args");
#else
    Record(Result,
           !GAssertionCapture.WasCalled,
           "SG_CHECKF(false, ...) is stripped in Release build");
#endif

    FLog::SetAssertionHandler(nullptr);
}

// ============================================================================
// T026: SG_VERIFY assertion test
// ============================================================================

void TestSGVerify(FLoggingAssertionTestResult& Result)
{
    int Counter = 0;

    GAssertionCapture.Reset();
    FLog::SetAssertionHandler(TestAssertionHandler);

    // SG_VERIFY always evaluates the expression.
    auto IncrementAndReturnTrue = [&Counter]() -> bool { ++Counter; return true; };
    auto IncrementAndReturnFalse = [&Counter]() -> bool { ++Counter; return false; };

    SG_VERIFY(IncrementAndReturnTrue());

    Record(Result,
           Counter == 1,
           "SG_VERIFY always evaluates expression (true case)");

    GAssertionCapture.Reset();

    {
        FOutputCapture Capture;
        Capture.Start();
        SG_VERIFY(IncrementAndReturnFalse());
        Capture.Stop();
    }

    Record(Result,
           Counter == 2,
           "SG_VERIFY always evaluates expression (false case)");

#ifdef _DEBUG
    Record(Result,
           GAssertionCapture.WasCalled,
           "SG_VERIFY triggers assertion handler on false in Debug");
#endif

    FLog::SetAssertionHandler(nullptr);
}

// ============================================================================
// T031: Per-category filtering test
// ============================================================================

void TestPerCategoryFiltering(FLoggingAssertionTestResult& Result)
{
    ELogSeverity OrigSeverity = LogCore.GetMinSeverity();

    // Set LogCore to Warning — Info should be suppressed.
    LogCore.SetMinSeverity(ELogSeverity::Warning);

    {
        FOutputCapture Capture;
        Capture.Start();
        SG_LOG(LogCore, Info, "should be suppressed");
        std::string Output = Capture.Stop();

        Record(Result,
               Output.find("should be suppressed") == std::string::npos,
               "Per-category filter suppresses Info when min is Warning");
    }

    {
        FOutputCapture Capture;
        Capture.Start();
        SG_LOG(LogCore, Error, "should appear");
        std::string Output = Capture.Stop();

        Record(Result,
               Output.find("should appear") != std::string::npos,
               "Per-category filter allows Error when min is Warning");
    }

    LogCore.SetMinSeverity(OrigSeverity);
}

// ============================================================================
// T032: Global severity filtering test
// ============================================================================

void TestGlobalSeverityFiltering(FLoggingAssertionTestResult& Result)
{
    ELogSeverity OrigGlobal = FLog::GetGlobalMinSeverity();

    FLog::SetGlobalMinSeverity(ELogSeverity::Info);

    {
        FOutputCapture Capture;
        Capture.Start();
        SG_LOG(LogCore, Verbose, "should be suppressed by global");
        std::string Output = Capture.Stop();

        Record(Result,
               Output.find("should be suppressed by global") == std::string::npos,
               "Global severity filter suppresses Verbose when min is Info");
    }

    FLog::SetGlobalMinSeverity(OrigGlobal);
}

// ============================================================================
// T033: Early-out zero-overhead verification
// ============================================================================

void TestEarlyOutZeroOverhead(FLoggingAssertionTestResult& Result)
{
    ELogSeverity OrigSeverity = LogCore.GetMinSeverity();
    LogCore.SetMinSeverity(ELogSeverity::Error);

    int SideEffectCounter = 0;

    // Info < Error, so this should be filtered at macro level.
    SG_LOG(LogCore, Info, "counter=%d", ++SideEffectCounter);

    Record(Result,
           SideEffectCounter == 0,
           "Early-out zero-overhead: side-effect counter not incremented when filtered");

    LogCore.SetMinSeverity(OrigSeverity);
}

// ============================================================================
// T036: Custom category declaration test
// ============================================================================

} // close anonymous namespace

// Declare and define a custom test category at file scope.
SG_DECLARE_LOG_CATEGORY_EXTERN(LogTestCustom, Stoner::Core::ELogSeverity::Verbose)
SG_DEFINE_LOG_CATEGORY(LogTestCustom)

namespace
{

using namespace Stoner::Core;

void TestCustomCategoryDeclaration(FLoggingAssertionTestResult& Result)
{
    FOutputCapture Capture;
    Capture.Start();
    SG_LOG(LogTestCustom, Info, "custom test message");
    std::string Output = Capture.Stop();

    Record(Result,
           Output.find("LogTestCustom") != std::string::npos,
           "Custom category LogTestCustom appears in output");
    Record(Result,
           Output.find("custom test message") != std::string::npos,
           "Custom category message content is correct");
}

// ============================================================================
// T037: Category self-registration test
// ============================================================================

void TestCategorySelfRegistration(FLoggingAssertionTestResult& Result)
{
    const auto& AllCategories = FLogCategory::GetAllCategories();

    bool FoundLogCore = false;
    bool FoundLogTestCustom = false;

    for (const auto* Cat : AllCategories)
    {
        if (std::strcmp(Cat->GetName(), "LogCore") == 0) FoundLogCore = true;
        if (std::strcmp(Cat->GetName(), "LogTestCustom") == 0) FoundLogTestCustom = true;
    }

    Record(Result, FoundLogCore, "LogCore is in global category registry");
    Record(Result, FoundLogTestCustom, "LogTestCustom is in global category registry");
}

// ============================================================================
// T041: Thread-safety verification
// ============================================================================

void TestThreadSafety(FLoggingAssertionTestResult& Result)
{
    constexpr int NumThreads = 4;
    constexpr int MessagesPerThread = 50;

    FOutputCapture Capture;
    Capture.Start();

    std::vector<std::thread> Threads;
    for (int t = 0; t < NumThreads; ++t)
    {
        Threads.emplace_back([t]()
        {
            for (int i = 0; i < MessagesPerThread; ++i)
            {
                SG_LOG(LogCore, Info, "Thread%d-Msg%d", t, i);
            }
        });
    }

    for (auto& T : Threads)
    {
        T.join();
    }

    std::string Output = Capture.Stop();

    // Count complete lines (each should end with \n and start with [).
    int CompleteLines = 0;
    std::istringstream Stream(Output);
    std::string Line;
    bool AnyCorrupted = false;

    while (std::getline(Stream, Line))
    {
        if (Line.empty()) continue;
        ++CompleteLines;
        // Each line should start with '[' (timestamp) and contain "Thread".
        if (Line[0] != '[' || Line.find("Thread") == std::string::npos)
        {
            AnyCorrupted = true;
        }
    }

    Record(Result,
           CompleteLines == NumThreads * MessagesPerThread,
           "Thread-safety: all log lines produced");
    Record(Result,
           !AnyCorrupted,
           "Thread-safety: no interleaved/corrupted log lines");
}

// ============================================================================
// T042: Zero-configuration startup verification
// ============================================================================

void TestZeroConfigStartup(FLoggingAssertionTestResult& Result)
{
    // SG_LOG should work without any explicit initialization.
    // This test runs early — if we got here, logging already works.
    FOutputCapture Capture;
    Capture.Start();
    SG_LOG(LogCore, Info, "zero-config startup test");
    std::string Output = Capture.Stop();

    Record(Result,
           Output.find("zero-config startup test") != std::string::npos,
           "SG_LOG works without explicit initialization (zero-config startup)");
}

} // namespace

FLoggingAssertionTestResult RunLoggingAssertionTests()
{
    FLoggingAssertionTestResult Result;

    std::cout << "[INFO] Running Logging & Assertion tests\n";

    // Phase 2: Foundational
    TestELogSeverity(Result);                  // T008

    // Phase 3: User Story 1
    TestFLogCategory(Result);                  // T009
    TestFLogConsoleSinkFormat(Result);         // T010
    TestFLogMessageSeverityRouting(Result);    // T011
    TestSGLogMacroEarlyOut(Result);            // T012
    TestFatalLogBehavior(Result);              // T013

    // Phase 4: User Story 2
    TestSGCheck(Result);                       // T024
    TestSGCheckF(Result);                      // T025
    TestSGVerify(Result);                      // T026

    // Phase 5: User Story 3
    TestPerCategoryFiltering(Result);          // T031
    TestGlobalSeverityFiltering(Result);       // T032
    TestEarlyOutZeroOverhead(Result);          // T033

    // Phase 6: User Story 4
    TestCustomCategoryDeclaration(Result);     // T036
    TestCategorySelfRegistration(Result);      // T037

    // Phase 7: Polish
    TestThreadSafety(Result);                  // T041
    TestZeroConfigStartup(Result);             // T042

    std::cout << "[INFO] Logging & Assertion tests passed=" << Result.Passed
              << " failed=" << Result.Failed << '\n';
    return Result;
}
