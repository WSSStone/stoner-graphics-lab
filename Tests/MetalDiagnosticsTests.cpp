#include "MetalDiagnosticsTests.h"

#include "FMetalDiagnostics.h"

#include <iostream>
#include <string>

namespace
{

using namespace Stoner;
using namespace Stoner::Backend::Metal;
using namespace Stoner::Backend::Metal::Private;

void Record(FMetalDiagnosticsTestResult& Result, bool Passed, const char* Name)
{
    (Passed ? ++Result.Passed : ++Result.Failed);
    std::cout << (Passed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FMetalBackendDiagnostics BuildTrace()
{
    FMetalDiagnostics Diagnostics;
    for (Core::uint64 Index = 0; Index < 3; ++Index)
    {
        FMetalDiagnosticRecord Entry;
        Entry.Operation = Core::FString("Submit");
        Entry.Context = Core::FString(
            "native=0x1234abcd\nqueue=graphics");
        Entry.Result = RHI::ERHIResult::Failed;
        Entry.StableReason = Core::FString("metal-submit-failed");
        Entry.ObjectIdentity = 40 + Index;
        Entry.FrameIdentity = 70 + Index;
        Entry.CapabilityReason = Core::FString("shared-events");
        Entry.RecoveryState = Core::FString("terminal");
        Diagnostics.Add(std::move(Entry));
    }
    return Diagnostics.Snapshot();
}

std::string Serialize(const FMetalBackendDiagnostics& Diagnostics)
{
    std::string Result;
    for (const auto& Entry : Diagnostics.Records)
    {
        Result += Entry.Backend.ToStdString() + "|";
        Result += Entry.Operation.ToStdString() + "|";
        Result += Entry.Context.ToStdString() + "|";
        Result += std::to_string(Entry.ObjectIdentity) + "|";
        Result += std::to_string(Entry.FrameIdentity) + "|";
        Result += Entry.StableReason.ToStdString() + "|";
        Result += Entry.CapabilityReason.ToStdString() + "|";
        Result += Entry.RecoveryState.ToStdString() + "\n";
    }
    return Result;
}

void TestStableNormalizedTrace(FMetalDiagnosticsTestResult& Result)
{
    const auto First = BuildTrace();
    const std::string Canonical = Serialize(First);
    bool bRepeated = true;
    for (int Iteration = 1; Iteration < 20; ++Iteration)
        bRepeated = bRepeated && Serialize(BuildTrace()) == Canonical;
    Record(Result,
        bRepeated && First.Records.size() == 3 &&
            First.Records.front().Context.View().find("0x1234abcd") ==
                std::string_view::npos &&
            First.Records.front().Context.View().find("<native-address>") !=
                std::string_view::npos &&
            First.Records.front().ObjectIdentity == 40 &&
            First.Records.front().FrameIdentity == 70,
        "normalized diagnostics redact addresses and repeat byte-stably");
}

void TestBoundedCollection(FMetalDiagnosticsTestResult& Result)
{
    FMetalDiagnostics Diagnostics;
    for (Core::usize Index = 0;
         Index < FMetalDiagnostics::MaxRecordCount + 7; ++Index)
    {
        FMetalDiagnosticRecord Entry;
        Entry.Operation = Core::FString(std::string(
            FMetalDiagnostics::MaxTextLength + 20, 'x'));
        Entry.ObjectIdentity = Index;
        Diagnostics.Add(std::move(Entry));
    }
    const auto Snapshot = Diagnostics.Snapshot();
    Record(Result,
        Snapshot.bTruncated &&
            Snapshot.Records.size() == FMetalDiagnostics::MaxRecordCount &&
            Snapshot.Records.front().Operation.Len() ==
                FMetalDiagnostics::MaxTextLength &&
            Snapshot.Records.front().ObjectIdentity == 0 &&
            Snapshot.Records.back().ObjectIdentity ==
                FMetalDiagnostics::MaxRecordCount - 1,
        "diagnostic collection is bounded, ordered, and reports truncation");
}

} // namespace

FMetalDiagnosticsTestResult RunMetalDiagnosticsTests()
{
    FMetalDiagnosticsTestResult Result;
    TestStableNormalizedTrace(Result);
    TestBoundedCollection(Result);
    return Result;
}
