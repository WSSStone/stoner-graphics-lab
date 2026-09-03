#include "RendererPostProcessInsertionTests.h"

#include "Renderer/FPostProcessInsertion.h"

#include <iostream>
#include <string>

namespace
{

using namespace Stoner::Renderer;
using namespace Stoner::RHI;

void Record(FRendererPostProcessInsertionTestResult& Result, bool bPassed,
    const char* Name)
{
    (bPassed ? ++Result.Passed : ++Result.Failed);
    std::cout << (bPassed ? "[PASS] " : "[FAIL] ") << Name << '\n';
}

FPostProcessOperationDesc MakeOperation(const char* Id,
    EPostProcessInsertionPoint Point, Stoner::Core::int32 Order)
{
    FPostProcessOperationDesc Operation;
    Operation.OperationId = Id;
    Operation.StrategyVersion = std::string(Id) + ".Strategy.v1";
    Operation.InsertionPoint = Point;
    Operation.OrderKey = Order;
    Operation.InputDomain = Point == EPostProcessInsertionPoint::PreTonemap
        ? ERenderGraphColorDomain::SceneLinearRec709D65
        : ERenderGraphColorDomain::DisplayLinearRec709D65;
    Operation.OutputDomain = Operation.InputDomain;
    return Operation;
}

FPostProcessCompositeResolution ResolvePre(
    const FPostProcessComposite& Composite)
{
    return Composite.Resolve(EPostProcessInsertionPoint::PreTonemap,
        1920, 1080, ERHISampleCount::One,
        ERenderGraphColorDomain::SceneLinearRec709D65);
}

void TestStableIdentityOrderAndDependencies(
    FRendererPostProcessInsertionTestResult& Result)
{
    FPostProcessOperationDesc Later = MakeOperation(
        "Pre.ColorAdjust", EPostProcessInsertionPoint::PreTonemap, 20);
    Later.DependsOn = {"Pre.Denoise"};
    Later.Reads = {"InputColor", "DenoisedMoments"};
    FPostProcessOperationDesc Earlier = MakeOperation(
        "Pre.Denoise", EPostProcessInsertionPoint::PreTonemap, 10);
    Earlier.Writes = {"OutputColor", "DenoisedMoments"};

    FPostProcessComposite First;
    FPostProcessComposite Second;
    (void)First.Add(Later);
    (void)First.Add(Earlier);
    (void)Second.Add(Earlier);
    (void)Second.Add(Later);
    const FPostProcessCompositeResolution FirstResolution = ResolvePre(First);
    const FPostProcessCompositeResolution SecondResolution = ResolvePre(Second);
    Record(Result, FirstResolution.Succeeded() &&
            SecondResolution.Succeeded() &&
            FirstResolution.Operations.size() == 2 &&
            FirstResolution.Operations[0].Declaration.OperationId ==
                "Pre.Denoise" &&
            FirstResolution.Operations[1].Declaration.OperationId ==
                "Pre.ColorAdjust" &&
            SecondResolution.Operations[0].Declaration.OperationId ==
                FirstResolution.Operations[0].Declaration.OperationId &&
            SecondResolution.Operations[1].Declaration.OperationId ==
                FirstResolution.Operations[1].Declaration.OperationId,
        "Permuted declarations resolve to stable dependency-compatible order");
    Record(Result, FirstResolution.Operations[0].ResolvedIndex == 1 &&
            FirstResolution.Operations[1].ResolvedIndex == 2 &&
            FirstResolution.Operations[0].Width == 1920 &&
            FirstResolution.Operations[0].Height == 1080 &&
            FirstResolution.Operations[0].SampleCount == ERHISampleCount::One &&
            FirstResolution.Operations[0].ColorDomain ==
                ERenderGraphColorDomain::SceneLinearRec709D65,
        "Resolved operations inherit exact extent sample count and color domain");
}

void TestDuplicateAndDependencyFailures(
    FRendererPostProcessInsertionTestResult& Result)
{
    FPostProcessComposite DuplicateId;
    (void)DuplicateId.Add(MakeOperation(
        "Pre.Duplicate", EPostProcessInsertionPoint::PreTonemap, 1));
    (void)DuplicateId.Add(MakeOperation(
        "Pre.Duplicate", EPostProcessInsertionPoint::PreTonemap, 2));
    Record(Result, ResolvePre(DuplicateId).Result ==
            EPostProcessInsertionResult::DuplicateOperation,
        "Duplicate operation identities fail closed");

    FPostProcessComposite DuplicateOrder;
    (void)DuplicateOrder.Add(MakeOperation(
        "Pre.First", EPostProcessInsertionPoint::PreTonemap, 3));
    (void)DuplicateOrder.Add(MakeOperation(
        "Pre.Second", EPostProcessInsertionPoint::PreTonemap, 3));
    Record(Result, ResolvePre(DuplicateOrder).Result ==
            EPostProcessInsertionResult::DuplicateOrder,
        "Duplicate insertion order keys are rejected rather than tie-broken");

    FPostProcessOperationDesc Missing = MakeOperation(
        "Pre.Missing", EPostProcessInsertionPoint::PreTonemap, 4);
    Missing.DependsOn = {"Pre.Unknown"};
    FPostProcessComposite MissingComposite;
    (void)MissingComposite.Add(Missing);
    Record(Result, ResolvePre(MissingComposite).Result ==
            EPostProcessInsertionResult::MissingDependency,
        "Missing insertion dependencies fail before graph declaration");

    FPostProcessOperationDesc CycleA = MakeOperation(
        "Pre.CycleA", EPostProcessInsertionPoint::PreTonemap, 5);
    FPostProcessOperationDesc CycleB = MakeOperation(
        "Pre.CycleB", EPostProcessInsertionPoint::PreTonemap, 6);
    CycleA.DependsOn = {"Pre.CycleB"};
    CycleB.DependsOn = {"Pre.CycleA"};
    FPostProcessComposite Cycle;
    (void)Cycle.Add(CycleA);
    (void)Cycle.Add(CycleB);
    Record(Result, ResolvePre(Cycle).Result ==
            EPostProcessInsertionResult::DependencyCycle,
        "Insertion dependency cycles fail deterministically");
}

void TestBoundsHazardsAndOwnership(
    FRendererPostProcessInsertionTestResult& Result)
{
    FPostProcessComposite Maximum;
    bool bAcceptedMaximum = true;
    for (Stoner::Core::uint32 Index = 0;
         Index < FPostProcessComposite::MaximumOperations; ++Index)
    {
        const std::string Id = "Pre.Bounded" + std::to_string(Index);
        bAcceptedMaximum = bAcceptedMaximum && Maximum.Add(MakeOperation(
            Id.c_str(), EPostProcessInsertionPoint::PreTonemap,
            static_cast<Stoner::Core::int32>(Index)));
    }
    Record(Result, bAcceptedMaximum && !Maximum.Add(MakeOperation(
            "Pre.Overflow", EPostProcessInsertionPoint::PreTonemap, 17)) &&
            ResolvePre(Maximum).Succeeded(),
        "Composite accepts exactly sixteen operations and rejects the seventeenth");

    FPostProcessOperationDesc TooManyResources = MakeOperation(
        "Pre.Resources", EPostProcessInsertionPoint::PreTonemap, 1);
    TooManyResources.Reads = {"InputColor", "R1", "R2", "R3", "R4",
        "R5", "R6", "R7", "R8"};
    FPostProcessComposite ResourceBound;
    (void)ResourceBound.Add(TooManyResources);
    Record(Result, ResolvePre(ResourceBound).Result ==
            EPostProcessInsertionResult::InvalidOperation,
        "Per-operation resource declarations are bounded to eight reads and writes");

    FPostProcessOperationDesc ReadFuture = MakeOperation(
        "Pre.ReadFuture", EPostProcessInsertionPoint::PreTonemap, 1);
    ReadFuture.Reads = {"InputColor", "FutureResource"};
    FPostProcessComposite ReadBeforeWrite;
    (void)ReadBeforeWrite.Add(ReadFuture);
    Record(Result, ResolvePre(ReadBeforeWrite).Result ==
            EPostProcessInsertionResult::ReadBeforeWrite,
        "Undeclared future resource reads fail before native work");

    FPostProcessOperationDesc WriterA = MakeOperation(
        "Pre.WriterA", EPostProcessInsertionPoint::PreTonemap, 1);
    FPostProcessOperationDesc WriterB = MakeOperation(
        "Pre.WriterB", EPostProcessInsertionPoint::PreTonemap, 2);
    WriterA.Writes = {"OutputColor", "SharedResource"};
    WriterB.Writes = {"OutputColor", "SharedResource"};
    FPostProcessComposite DuplicateWriter;
    (void)DuplicateWriter.Add(WriterA);
    (void)DuplicateWriter.Add(WriterB);
    Record(Result, ResolvePre(DuplicateWriter).Result ==
            EPostProcessInsertionResult::DuplicateWriter,
        "Duplicate auxiliary-resource writers fail deterministically");

    FPostProcessOperationDesc Forbidden = MakeOperation(
        "Pre.Forbidden", EPostProcessInsertionPoint::PreTonemap, 1);
    Forbidden.bClaimsOutputTransfer = true;
    FPostProcessComposite ForbiddenComposite;
    (void)ForbiddenComposite.Add(Forbidden);
    Record(Result, ResolvePre(ForbiddenComposite).Result ==
            EPostProcessInsertionResult::ForbiddenOwnership,
        "Insertion operations cannot own tone output transfer or formal output");

    FPostProcessOperationDesc Temporal = MakeOperation(
        "Pre.Temporal", EPostProcessInsertionPoint::PreTonemap, 1);
    Temporal.bUsesTemporalState = true;
    FPostProcessComposite TemporalComposite;
    (void)TemporalComposite.Add(Temporal);
    Record(Result, ResolvePre(TemporalComposite).Result ==
            EPostProcessInsertionResult::InvariantViolation,
        "Feature 029 insertion contracts reject temporal state ownership");

    FPostProcessOperationDesc WrongDomain = MakeOperation(
        "Pre.WrongDomain", EPostProcessInsertionPoint::PreTonemap, 1);
    WrongDomain.OutputDomain =
        ERenderGraphColorDomain::DisplayLinearRec709D65;
    WrongDomain.bPreservesExtent = false;
    FPostProcessComposite WrongDomainComposite;
    (void)WrongDomainComposite.Add(WrongDomain);
    Record(Result, ResolvePre(WrongDomainComposite).Result ==
            EPostProcessInsertionResult::InvariantViolation,
        "Insertion cannot change inherited extent sample count or color domain");
}

} // namespace

FRendererPostProcessInsertionTestResult RunRendererPostProcessInsertionTests()
{
    FRendererPostProcessInsertionTestResult Result;
    TestStableIdentityOrderAndDependencies(Result);
    TestDuplicateAndDependencyFailures(Result);
    TestBoundsHazardsAndOwnership(Result);
    return Result;
}
