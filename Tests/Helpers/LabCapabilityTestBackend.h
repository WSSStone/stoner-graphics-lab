#pragma once

#include "FDemoBackendFactory.h"

// Typed capability and replacement-result faults over a real native runtime.
// Actual resource creation, submission and retirement remain native. These
// fixtures do not claim a physical disconnect or a spontaneous driver failure.
namespace Stoner::Demo::Tests
{
struct FLabCapabilityMask
{
    enum class EMode { All, SdrOnly, None };
    EMode Mode = EMode::All;
    enum class EReplacementFailure { None, BeforeReplacement, AfterReplacement };
    EReplacementFailure NextFailure = EReplacementFailure::None;
    Core::uint32 InjectedFailures = 0;
    Core::uint64 FormerGeneration = 0, ReplacementGeneration = 0;
    void Apply(FDemoLabPresentationStatus& Status) const
    {
        auto& Caps = Status.Capabilities;
        if (Mode == EMode::None) Caps.SupportedPairs.clear();
        else if (Mode == EMode::SdrOnly)
            std::erase_if(Caps.SupportedPairs,[](const auto& Pair) {
                return Pair.ColorSpace != RHI::ERHIPresentationColorSpace::SrgbNonlinear &&
                    Pair.ColorSpace != RHI::ERHIPresentationColorSpace::SdrPassThrough &&
                    Pair.ColorSpace != RHI::ERHIPresentationColorSpace::Bt709Nonlinear;
            });
        if (Mode != EMode::All) Caps.bSupportsExtendedRange = false;
    }
};
class FCapabilityRuntime final : public IDemoBackendRuntime
{
public:
    FCapabilityRuntime(Core::TUniquePtr<IDemoBackendRuntime> Runtime, FLabCapabilityMask& Mask)
        : Inner(std::move(Runtime)), Mask(Mask) {}
    EDemoGraphicsBackend GetBackend() const noexcept override { return Inner->GetBackend(); }
    RHI::ERHIResult Initialize(EDemoRunMode M,const Core::FPlatformWindow& W,Core::uint32 F,bool V) override { return Inner->Initialize(M,W,F,V); }
    RHI::ERHIResult InitializeLab(const Core::FPlatformWindow& W,Core::uint32 F,bool V,bool H) override { return Inner->InitializeLab(W,F,V,H); }
    RHI::ERHIResult PrepareTriangle(const RHI::FRHIShaderModuleDesc& V,const RHI::FRHIShaderModuleDesc& F,Core::uint32 W,Core::uint32 H) override { return Inner->PrepareTriangle(V,F,W,H); }
    RHI::ERHIResult AcquireFrame(FDemoBackendFrame& F) override { return Inner->AcquireFrame(F); }
    RHI::ERHIResult SubmitFrame(const FDemoBackendFrame& F) override { return Inner->SubmitFrame(F); }
    RHI::ERHIResult RecreatePresentation(Core::uint32 W,Core::uint32 H) override { return Inner->RecreatePresentation(W,H); }
    RHI::ERHIResult PrepareProductionPresentation(Core::uint32 W,Core::uint32 H) override { return Inner->PrepareProductionPresentation(W,H); }
    RHI::ERHIResult PresentProductionImage(std::span<const Core::uint8> B,Core::uint32 W,Core::uint32 H,Core::uint32 P,FDemoProductionPresentationResult& R) override { return Inner->PresentProductionImage(B,W,H,P,R); }
    RHI::ERHIResult ExecuteOffscreenTriangle(const Renderer::FForwardFramePlan& P,const RHI::FRHIShaderModuleDesc& V,const RHI::FRHIShaderModuleDesc& F) override { return Inner->ExecuteOffscreenTriangle(P,V,F); }
    RHI::FRHIRuntimeSnapshot GetSnapshot() const noexcept override { return Inner->GetSnapshot(); }
    Core::TSharedPtr<RHI::IRHIDevice> GetDevice() const noexcept override { return Inner->GetDevice(); }
    RHI::ERHIResult Shutdown() override { return Inner->Shutdown(); }
    RHI::ERHIResult QueryLabPresentation(FDemoLabPresentationStatus& S) const override
    { const auto R=Inner->QueryLabPresentation(S); Mask.Apply(S); return R; }
    RHI::ERHIResult PrepareLabPresentation(const RHI::FRHISwapchainDesc& D,FDemoLabPresentationStatus& S,Core::FString* E) override
    { const auto R=Inner->PrepareLabPresentation(D,S,E); Mask.Apply(S); return R; }
    RHI::ERHIResult ReconfigureLabPresentation(const RHI::FRHISwapchainDesc& D,FDemoLabPresentationStatus& S,Core::FString* E) override
    {
        const auto Q=QueryLabPresentation(S);
        if (Q != RHI::ERHIResult::Success) return Q;
        if (bDeferredReplacementFailure)
        {
            bDeferredReplacementFailure=false; ++Mask.InjectedFailures;
            if (E) *E="injected delayed failure after native replacement";
            return RHI::ERHIResult::Failed;
        }
        if (!S.Capabilities.SupportsPair(D.PreferredFormat,D.PreferredColorSpace)) return RHI::ERHIResult::Unsupported;
        using Failure = FLabCapabilityMask::EReplacementFailure;
        if (Mask.NextFailure == Failure::BeforeReplacement)
        {
            Mask.NextFailure=Failure::None; ++Mask.InjectedFailures;
            if (E) *E="injected rejection before native replacement";
            return RHI::ERHIResult::Failed;
        }
        const auto Former=S.ResolvedState.SwapchainImageGeneration;
        const auto R=Inner->ReconfigureLabPresentation(D,S,E); Mask.Apply(S);
        if (R == RHI::ERHIResult::Success && Mask.NextFailure == Failure::AfterReplacement)
        {
            Mask.NextFailure=Failure::None; bDeferredReplacementFailure=true;
            Mask.FormerGeneration=Former;
            Mask.ReplacementGeneration=S.ResolvedState.SwapchainImageGeneration;
            if (E) *E="injected failure after native replacement";
            return RHI::ERHIResult::NotReady;
        }
        return R;
    }
    RHI::ERHIResult AcquireLabTarget(Core::uint64 T,Core::uint32 I,RHI::FRHIBorrowedAcquiredTarget& O,Core::FString* E) override { return Inner->AcquireLabTarget(T,I,O,E); }
    bool OwnsLabAcquireAttempt(Core::uint64 T,Core::uint32 I) const noexcept override { return Inner->OwnsLabAcquireAttempt(T,I); }
    RHI::ERHIResult PresentLabTarget(const RHI::FRHIBorrowedAcquiredTarget& T,const RHI::FRHIRenderLease& R,RHI::FRHIPresentationLease& L,Core::FString* E) override { return Inner->PresentLabTarget(T,R,L,E); }
    RHI::ERHIResult CancelLabTarget(Core::uint64 T,Core::uint32 I,const Core::TSharedPtr<RHI::IRHIFence>& F,bool& A,Core::FString* E) override { return Inner->CancelLabTarget(T,I,F,A,E); }
    RHI::ERHIResult PollLabPresentation(const RHI::FRHIPresentationLease& L,bool& C,Core::FString* E) override { return Inner->PollLabPresentation(L,C,E); }
private:
    Core::TUniquePtr<IDemoBackendRuntime> Inner;
    FLabCapabilityMask& Mask;
    bool bDeferredReplacementFailure = false;
};
class FCapabilityFactory final : public IDemoBackendFactory
{
public:
    explicit FCapabilityFactory(FLabCapabilityMask& Mask) : Mask(Mask) {}
    FDemoBackendCreateResult Create(EDemoGraphicsBackend Backend) const override
    {
        auto Result = FDemoBackendFactory().Create(Backend);
        if (Result.Succeeded()) Result.Runtime = Core::MakeUnique<FCapabilityRuntime>(std::move(Result.Runtime),Mask);
        return Result;
    }
private:
    FLabCapabilityMask& Mask;
};
}
