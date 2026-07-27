#include "LoggingAssertionTests.h"

#include "Core/CoreMinimal.h"

#include <atomic>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <io.h>
    #include <windows.h>
#else
    #include <sys/wait.h>
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
        std::string Content;
        if (fseek(CaptureFile, 0, SEEK_END) == 0)
        {
            const long Size = ftell(CaptureFile);
            if (Size > 0 && fseek(CaptureFile, 0, SEEK_SET) == 0)
            {
                Content.resize(static_cast<size_t>(Size));
                const size_t BytesRead =
                    fread(Content.data(), 1, Content.size(), CaptureFile);
                Content.resize(BytesRead);
            }
        }
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

struct FLoggingChildResult
{
    bool Started = false;
    bool Completed = false;
    bool TerminatedBeforeFallback = false;
    int ExitCode = 0;
    std::string Stderr;
};

std::string ReadCaptureFile(FILE* CaptureFile)
{
    std::string Content;
    if (CaptureFile == nullptr || fseek(CaptureFile, 0, SEEK_END) != 0)
    {
        return Content;
    }

    const long Size = ftell(CaptureFile);
    if (Size <= 0 || fseek(CaptureFile, 0, SEEK_SET) != 0)
    {
        return Content;
    }

    Content.resize(static_cast<size_t>(Size));
    const size_t BytesRead = fread(Content.data(), 1, Content.size(), CaptureFile);
    Content.resize(BytesRead);
    return Content;
}

FLoggingChildResult RunLoggingChild(
    const char* TestExecutablePath,
    const char* ChildArgument)
{
    FLoggingChildResult Result;
    if (TestExecutablePath == nullptr || TestExecutablePath[0] == '\0')
    {
        return Result;
    }

    FILE* CaptureFile = std::tmpfile();
    if (CaptureFile == nullptr)
    {
        return Result;
    }

#if defined(_WIN32)
    const intptr_t CaptureOsHandle = _get_osfhandle(_fileno(CaptureFile));
    HANDLE ChildStderr = INVALID_HANDLE_VALUE;
    if (CaptureOsHandle != -1)
    {
        DuplicateHandle(
            GetCurrentProcess(),
            reinterpret_cast<HANDLE>(CaptureOsHandle),
            GetCurrentProcess(),
            &ChildStderr,
            0,
            TRUE,
            DUPLICATE_SAME_ACCESS);
    }

    SECURITY_ATTRIBUTES Security{};
    Security.nLength = sizeof(Security);
    Security.bInheritHandle = TRUE;
    HANDLE ChildStdin = CreateFileA(
        "NUL", GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
        &Security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    HANDLE ChildStdout = CreateFileA(
        "NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
        &Security, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    if (ChildStderr != INVALID_HANDLE_VALUE &&
        ChildStdin != INVALID_HANDLE_VALUE &&
        ChildStdout != INVALID_HANDLE_VALUE)
    {
        STARTUPINFOA Startup{};
        Startup.cb = sizeof(Startup);
        Startup.dwFlags = STARTF_USESTDHANDLES;
        Startup.hStdInput = ChildStdin;
        Startup.hStdOutput = ChildStdout;
        Startup.hStdError = ChildStderr;

        PROCESS_INFORMATION Process{};
        std::string CommandLine =
            "\"" + std::string(TestExecutablePath) + "\" " +
            ChildArgument;
        std::vector<char> MutableCommandLine(CommandLine.begin(), CommandLine.end());
        MutableCommandLine.push_back('\0');

        const BOOL Created = CreateProcessA(
            TestExecutablePath,
            MutableCommandLine.data(),
            nullptr,
            nullptr,
            TRUE,
            0,
            nullptr,
            nullptr,
            &Startup,
            &Process);
        Result.Started = Created != FALSE;

        if (Created != FALSE)
        {
            const DWORD WaitResult = WaitForSingleObject(Process.hProcess, 30000);
            DWORD ChildExitCode = STILL_ACTIVE;
            if (WaitResult == WAIT_OBJECT_0 &&
                GetExitCodeProcess(Process.hProcess, &ChildExitCode) != FALSE)
            {
                Result.Completed = true;
                Result.ExitCode = static_cast<int>(ChildExitCode);
                Result.TerminatedBeforeFallback =
                    ChildExitCode != 0u && ChildExitCode != 42u;
            }
            else if (WaitResult == WAIT_TIMEOUT)
            {
                TerminateProcess(Process.hProcess, 124u);
                WaitForSingleObject(Process.hProcess, 5000);
            }

            CloseHandle(Process.hThread);
            CloseHandle(Process.hProcess);
        }
    }

    if (ChildStderr != INVALID_HANDLE_VALUE)
    {
        CloseHandle(ChildStderr);
    }
    if (ChildStdin != INVALID_HANDLE_VALUE)
    {
        CloseHandle(ChildStdin);
    }
    if (ChildStdout != INVALID_HANDLE_VALUE)
    {
        CloseHandle(ChildStdout);
    }
#else
    const pid_t ChildProcess = fork();
    if (ChildProcess == 0)
    {
        if (dup2(fileno(CaptureFile), STDERR_FILENO) < 0)
        {
            _exit(126);
        }

        execl(
            TestExecutablePath,
            TestExecutablePath,
            ChildArgument,
            static_cast<char*>(nullptr));
        _exit(127);
    }

    if (ChildProcess > 0)
    {
        Result.Started = true;
        int WaitStatus = 0;
        pid_t WaitResult = -1;
        do
        {
            WaitResult = waitpid(ChildProcess, &WaitStatus, 0);
        }
        while (WaitResult < 0 && errno == EINTR);

        if (WaitResult == ChildProcess)
        {
            Result.Completed = true;
            if (WIFSIGNALED(WaitStatus))
            {
                Result.ExitCode = 128 + WTERMSIG(WaitStatus);
                Result.TerminatedBeforeFallback = true;
            }
            else if (WIFEXITED(WaitStatus))
            {
                Result.ExitCode = WEXITSTATUS(WaitStatus);
                Result.TerminatedBeforeFallback =
                    Result.ExitCode != 0 &&
                    Result.ExitCode != 42 &&
                    Result.ExitCode != 126 &&
                    Result.ExitCode != 127;
            }
        }
    }
#endif

    Result.Stderr = ReadCaptureFile(CaptureFile);
    fclose(CaptureFile);
    return Result;
}

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

static std::atomic<int> GConcurrentAssertionCalls{0};

static void ConcurrentAssertionHandlerA(
    const char*, int, const char*, const char*)
{
    GConcurrentAssertionCalls.fetch_add(1, std::memory_order_relaxed);
}

static void ConcurrentAssertionHandlerB(
    const char*, int, const char*, const char*)
{
    GConcurrentAssertionCalls.fetch_add(1, std::memory_order_relaxed);
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

void TestFatalLogBehavior(
    FLoggingAssertionTestResult& Result,
    const char* TestExecutablePath)
{
    const FLoggingChildResult Child = RunLoggingChild(
        TestExecutablePath,
        GLoggingFatalChildArgument);
    Record(Result, Child.Started && Child.Completed,
           "Fatal log child process starts and completes");
    Record(Result,
           Child.Stderr.find("LogCore: Fatal: isolated fatal logging probe") !=
               std::string::npos,
           "Fatal log routes the labeled message to stderr");
    Record(Result,
           Child.TerminatedBeforeFallback,
           "Fatal log terminates before returning from SG_LOG");
}

// ============================================================================
// T024: SG_CHECK assertion test
// ============================================================================

void TestSGCheck(FLoggingAssertionTestResult& Result)
{
    int ExpressionEvaluations = 0;
    GAssertionCapture.Reset();
    FLog::SetAssertionHandler(TestAssertionHandler);

    FOutputCapture Capture;
    Capture.Start();

    SG_CHECK((++ExpressionEvaluations, false));

    Capture.Stop();

#if !defined(NDEBUG) || defined(_DEBUG)
    Record(Result,
           GAssertionCapture.WasCalled,
           "SG_CHECK(false-expression) triggers assertion handler in Debug");
    Record(Result,
           GAssertionCapture.Expression.find("ExpressionEvaluations") !=
               std::string::npos,
           "SG_CHECK reports the failed expression text");
    Record(Result,
           !GAssertionCapture.File.empty(),
           "SG_CHECK reports file path");
    Record(Result,
           GAssertionCapture.Line > 0,
           "SG_CHECK reports line number");
    Record(Result,
           ExpressionEvaluations == 1,
           "SG_CHECK evaluates its expression once in Debug");
#else
    Record(Result,
           ExpressionEvaluations == 0 && !GAssertionCapture.WasCalled,
           "SG_CHECK strips expression evaluation and dispatch in Release");
#endif

    FLog::SetAssertionHandler(nullptr);
}

// ============================================================================
// T025: SG_CHECKF assertion test
// ============================================================================

void TestSGCheckF(FLoggingAssertionTestResult& Result)
{
    int ExpressionEvaluations = 0;
    int FormatEvaluations = 0;
    GAssertionCapture.Reset();
    FLog::SetAssertionHandler(TestAssertionHandler);

    FOutputCapture Capture;
    Capture.Start();

    SG_CHECKF(
        (++ExpressionEvaluations, false),
        "Index %d out of range",
        ++FormatEvaluations);

    Capture.Stop();

#if !defined(NDEBUG) || defined(_DEBUG)
    Record(Result,
           GAssertionCapture.WasCalled,
           "SG_CHECKF(false-expression, ...) triggers handler in Debug");
    Record(Result,
           GAssertionCapture.Expression.find("ExpressionEvaluations") !=
               std::string::npos,
           "SG_CHECKF reports expression text");
    Record(Result,
           GAssertionCapture.Message.find("1") != std::string::npos,
           "SG_CHECKF reports formatted message with evaluated args");
    Record(Result,
           ExpressionEvaluations == 1 && FormatEvaluations == 1,
           "SG_CHECKF evaluates expression and format args once in Debug");
#else
    Record(Result,
           ExpressionEvaluations == 0 &&
               FormatEvaluations == 0 &&
               !GAssertionCapture.WasCalled,
           "SG_CHECKF strips expression, format args, and dispatch in Release");
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

#if !defined(NDEBUG) || defined(_DEBUG)
    Record(Result,
           GAssertionCapture.WasCalled,
           "SG_VERIFY triggers assertion handler on false in Debug");
#else
    Record(Result,
           !GAssertionCapture.WasCalled,
           "SG_VERIFY false result does not dispatch in Release");
#endif

    FLog::SetAssertionHandler(nullptr);
}

void TestDefaultAssertionHandler(
    FLoggingAssertionTestResult& Result,
    const char* TestExecutablePath)
{
    const FLoggingChildResult Child = RunLoggingChild(
        TestExecutablePath,
        GLoggingAssertionChildArgument);
    Record(Result, Child.Started && Child.Completed,
           "Default assertion child process starts and completes");

#if !defined(NDEBUG) || defined(_DEBUG)
    Record(Result,
           Child.Stderr.find("Assertion failed: false") != std::string::npos,
           "Default assertion handler logs failure before Debug break");
    Record(Result,
           Child.TerminatedBeforeFallback,
           "Default assertion handler triggers Debug break before fallback");
#else
    Record(Result,
           Child.Stderr.find("Assertion failed:") == std::string::npos,
           "Release assertion child emits no assertion report");
    Record(Result,
           Child.ExitCode == 42 && !Child.TerminatedBeforeFallback,
           "Release assertion child reaches fallback after SG_CHECK stripping");
#endif
}

void TestAssertionHandlerThreadSafety(FLoggingAssertionTestResult& Result)
{
    constexpr int Iterations = 256;
    std::atomic<bool> Start{false};
    GConcurrentAssertionCalls.store(0, std::memory_order_relaxed);
    FLog::SetAssertionHandler(ConcurrentAssertionHandlerA);

    FOutputCapture Capture;
    Capture.Start();

    std::thread Setter([&Start]()
    {
        while (!Start.load(std::memory_order_acquire))
        {
        }
        for (int Iteration = 0; Iteration < Iterations; ++Iteration)
        {
            FLog::SetAssertionHandler((Iteration & 1) == 0
                ? ConcurrentAssertionHandlerA
                : ConcurrentAssertionHandlerB);
        }
    });

    std::thread Dispatcher([&Start]()
    {
        Start.store(true, std::memory_order_release);
        for (int Iteration = 0; Iteration < Iterations; ++Iteration)
        {
            FLog::HandleAssertionFailure(
                __FILE__,
                __LINE__,
                "concurrent assertion handler probe");
        }
    });

    Setter.join();
    Dispatcher.join();
    Capture.Stop();
    FLog::SetAssertionHandler(nullptr);

    Record(Result,
           GConcurrentAssertionCalls.load(std::memory_order_relaxed) ==
               Iterations,
           "Assertion handler replacement is safe during concurrent dispatch");
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
    const ELogSeverity OrigGlobal = FLog::GetGlobalMinSeverity();
    const ELogSeverity OrigCategory = LogCore.GetMinSeverity();

    LogCore.SetMinSeverity(ELogSeverity::Verbose);
    FLog::SetGlobalMinSeverity(ELogSeverity::Info);

    {
        FOutputCapture Capture;
        Capture.Start();
        int SideEffectCounter = 0;
        SG_LOG(
            LogCore,
            Verbose,
            "should be suppressed by global %d",
            ++SideEffectCounter);
        std::string Output = Capture.Stop();

        Record(Result,
               Output.find("should be suppressed by global") == std::string::npos,
               "Global severity filter suppresses Verbose when min is Info");
        Record(Result,
               SideEffectCounter == 0,
               "Global severity early-out does not evaluate format arguments");
    }

    FLog::SetGlobalMinSeverity(OrigGlobal);
    LogCore.SetMinSeverity(OrigCategory);
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

void TestThresholdThreadSafety(FLoggingAssertionTestResult& Result)
{
    const ELogSeverity OrigCategory = LogCore.GetMinSeverity();
    const ELogSeverity OrigGlobal = FLog::GetGlobalMinSeverity();
    constexpr int Iterations = 10000;
    std::atomic<bool> Start{false};
    std::atomic<int> ValidReads{0};

    std::thread Writer([&Start]()
    {
        while (!Start.load(std::memory_order_acquire))
        {
        }

        for (int Iteration = 0; Iteration < Iterations; ++Iteration)
        {
            const ELogSeverity Severity = (Iteration & 1) == 0
                ? ELogSeverity::Verbose
                : ELogSeverity::Error;
            LogCore.SetMinSeverity(Severity);
            FLog::SetGlobalMinSeverity(Severity);
        }
    });

    std::thread Reader([&Start, &ValidReads]()
    {
        Start.store(true, std::memory_order_release);
        for (int Iteration = 0; Iteration < Iterations; ++Iteration)
        {
            const auto Category = static_cast<int>(LogCore.GetMinSeverity());
            const auto Global = static_cast<int>(FLog::GetGlobalMinSeverity());
            if (Category >= static_cast<int>(ELogSeverity::Verbose) &&
                Category <= static_cast<int>(ELogSeverity::Fatal) &&
                Global >= static_cast<int>(ELogSeverity::Verbose) &&
                Global <= static_cast<int>(ELogSeverity::Fatal))
            {
                ValidReads.fetch_add(1, std::memory_order_relaxed);
            }
        }
    });

    Writer.join();
    Reader.join();
    LogCore.SetMinSeverity(OrigCategory);
    FLog::SetGlobalMinSeverity(OrigGlobal);

    Record(Result,
           ValidReads.load(std::memory_order_relaxed) == Iterations,
           "Runtime logging thresholds remain valid under concurrent access");
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

FLoggingAssertionTestResult RunLoggingAssertionTests(
    const char* TestExecutablePath)
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
    TestFatalLogBehavior(Result, TestExecutablePath); // T013

    // Phase 4: User Story 2
    TestSGCheck(Result);                       // T024
    TestSGCheckF(Result);                      // T025
    TestSGVerify(Result);                      // T026
    TestDefaultAssertionHandler(Result, TestExecutablePath); // CR001-B02-F014

    // Phase 5: User Story 3
    TestPerCategoryFiltering(Result);          // T031
    TestGlobalSeverityFiltering(Result);       // T032
    TestEarlyOutZeroOverhead(Result);          // T033

    // Phase 6: User Story 4
    TestCustomCategoryDeclaration(Result);     // T036
    TestCategorySelfRegistration(Result);      // T037

    // Phase 7: Polish
    TestThreadSafety(Result);                  // T041
    TestThresholdThreadSafety(Result);         // CR001-B02-F011
    TestAssertionHandlerThreadSafety(Result);  // CR001-B02-F013
    TestZeroConfigStartup(Result);             // T042

    std::cout << "[INFO] Logging & Assertion tests passed=" << Result.Passed
              << " failed=" << Result.Failed << '\n';
    return Result;
}
