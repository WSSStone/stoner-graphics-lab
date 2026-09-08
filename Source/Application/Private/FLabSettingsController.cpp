#include "FLabSettingsController.h"
#include "Renderer/FOutputTransformSettings.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Stoner::Application
{
namespace
{
using namespace Stoner::Core;
using namespace Stoner::Renderer;
bool ValidCapabilities(const FLabSettingsCapabilities& C)
{
    if (!C.DisplayGeneration || C.Outputs.size() > 16 || C.DebugStages.size() > 32) return false;
    FOutputTransformSettingsValidator Validator;
    for (std::size_t I = 0; I < C.Outputs.size(); ++I)
    {
        const auto& O = C.Outputs[I];
        const auto* Profile = Validator.FindProfile(O.ProfileId);
        if (!Profile || !std::isfinite(O.ReferenceWhiteNits) || O.ReferenceWhiteNits <= 0 ||
            !std::isfinite(O.NativePackingWhiteNits) || O.NativePackingWhiteNits <= 0) return false;
        if (Profile->MetadataPolicy == EOutputMetadataPolicy::EDRState)
        {
            if (O.ReferenceWhiteNits != O.NativePackingWhiteNits) return false;
        }
        else if (O.ReferenceWhiteNits != Profile->ReferenceWhiteNits ||
            O.NativePackingWhiteNits != Profile->ReferenceWhiteNits) return false;
        for (std::size_t J = 0; J < I; ++J) if (C.Outputs[J].ProfileId == O.ProfileId) return false;
    }
    for (std::size_t I = 0; I < C.DebugStages.size(); ++I)
    {
        FLabDebugBypass B; B.Mode = EOutputTransformDebugBypassMode::BoundedVisualization;
        B.StageName = C.DebugStages[I].Name; B.SourceDomain = C.DebugStages[I].Domain;
        if (!B.IsValid() || (!C.DebugStages[I].ProfileId.IsEmpty() &&
            !Validator.FindProfile(C.DebugStages[I].ProfileId))) return false;
        for (std::size_t J = 0; J < I; ++J) if (C.DebugStages[J].Name == B.StageName &&
            C.DebugStages[J].ProfileId == C.DebugStages[I].ProfileId) return false;
    }
    return true;
}
}

bool FLabSettingsController::Resolve(const FLabSettingsSnapshot& Input, FLabSettingsSnapshot& Out, bool Fallback)
{
    if (!Input.IsValid()) { Failure = "Invalid complete settings request"; return false; }
    auto Found = std::find_if(Capabilities.Outputs.begin(),Capabilities.Outputs.end(),[&](const auto& O) {
        return O.ProfileId == Input.RequestedProfileId;
    });
    if (Found == Capabilities.Outputs.end() && Fallback)
    {
        // Prefer the existing default, otherwise another advertised SDR profile.
        Found = std::find_if(Capabilities.Outputs.begin(),Capabilities.Outputs.end(),[](const auto& O) {
            return O.ProfileId == GDefaultSDROutputDeviceProfile;
        });
        if (Found == Capabilities.Outputs.end())
            Found = std::find_if(Capabilities.Outputs.begin(),Capabilities.Outputs.end(),[](const auto& O) {
                const auto* P = FOutputTransformSettingsValidator().FindProfile(O.ProfileId);
                return P && P->DynamicRange == EOutputDynamicRange::SDR;
            });
    }
    if (Found == Capabilities.Outputs.end()) { Failure = "Requested output unavailable; no permitted output transition"; return false; }
    const auto& Debug = Input.DebugBypass;
    const bool DebugUnavailable = Debug.Mode != EOutputTransformDebugBypassMode::Disabled &&
        std::none_of(Capabilities.DebugStages.begin(),Capabilities.DebugStages.end(),[&](const auto& Stage) {
            return (Stage.ProfileId.IsEmpty() || Stage.ProfileId == Found->ProfileId) &&
                Debug.IsValidForResolvedStageDomain(Stage.Name,Stage.Domain);
        });
    const auto& Prior = Requested.DebugBypass;
    const bool RetainedFallback = Fallback && Found->ProfileId != Input.RequestedProfileId &&
        Debug.Mode == Prior.Mode && Debug.StageName == Prior.StageName && Debug.SourceDomain == Prior.SourceDomain &&
        Debug.VisualizationMinimum == Prior.VisualizationMinimum && Debug.VisualizationMaximum == Prior.VisualizationMaximum;
    if (DebugUnavailable && !RetainedFallback) { Failure = "Debug stage or color domain unavailable"; return false; }
    Out = Input;
    if (DebugUnavailable) Out.DebugBypass = {};
    Out.EffectiveProfileId = Found->ProfileId;
    Out.DisplayGeneration = Capabilities.DisplayGeneration;
    Out.UIReferenceWhiteNits = Found->ReferenceWhiteNits;
    Out.NativePackingWhiteNits = Found->NativePackingWhiteNits;
    return Out.IsValid();
}

bool FLabSettingsController::Initialize(const FLabSettingsSnapshot& Initial, const FLabSettingsCapabilities& Caps)
{
    if (bInitialized || !ValidCapabilities(Caps) || Initial.DisplayGeneration != Caps.DisplayGeneration) return false;
    Capabilities = Caps;
    FLabSettingsSnapshot Resolved;
    if (!Resolve(Initial,Resolved,false)) return false;
    Requested = Initial; Effective = Resolved; Revision = Initial.SettingsRevision;
    bInitialized = true; bPaused = false; Failure.Clear();
    return true;
}

bool FLabSettingsController::Queue(const FLabSettingsSnapshot& Input, bool Fallback)
{
    FLabSettingsSnapshot Candidate;
    if (Revision == std::numeric_limits<uint64>::max()) { Failure = "Settings revision exhausted"; return false; }
    if (!Resolve(Input,Candidate,Fallback)) return false;
    Candidate.SettingsRevision = ++Revision;
    Pending = Candidate;
    Failure = Candidate.RequestedProfileId != Candidate.EffectiveProfileId
        ? "Requested output unavailable; explicit SDR fallback pending" : "";
    return true;
}

bool FLabSettingsController::Request(const FLabSettingsSnapshot& Input)
{
    if (!bInitialized || Input.DisplayGeneration != Capabilities.DisplayGeneration ||
        Input.CameraRevision < Effective.CameraRevision || Input.CameraRevision < Requested.CameraRevision)
    { Failure = "Stale settings display or camera identity"; return false; }
    // Editing parameters does not abandon previously accepted output intent
    // merely because a display change currently requires SDR fallback. A new
    // unavailable profile still rejects without replacing that intent.
    if (!Queue(Input,Input.RequestedProfileId == Requested.RequestedProfileId)) return false;
    Requested = Input; Requested.SettingsRevision = Revision;
    return true;
}

const FLabSettingsTransaction* FLabSettingsController::BeginEligible(bool Eligible)
{
    if (!bInitialized || !Eligible || Active || !Pending) return nullptr;
    if (NextToken == std::numeric_limits<uint64>::max() ||
        Effective.OutputModeGeneration == std::numeric_limits<uint64>::max())
    { Failure = "Settings transaction identity exhausted"; bPaused = true; return nullptr; }
    FLabSettingsTransaction T;
    T.Token = ++NextToken; T.Settings = *Pending;
    T.bRequiresOutputTransition = bPaused || T.Settings.DisplayGeneration != Effective.DisplayGeneration ||
        T.Settings.EffectiveProfileId != Effective.EffectiveProfileId ||
        T.Settings.NativePackingWhiteNits != Effective.NativePackingWhiteNits ||
        T.Settings.UIReferenceWhiteNits != Effective.UIReferenceWhiteNits;
    T.Settings.OutputModeGeneration = Effective.OutputModeGeneration + (T.bRequiresOutputTransition ? 1 : 0);
    Active = T; Pending.reset();
    return &*Active;
}

bool FLabSettingsController::Complete(uint64 Token, bool Success, bool FormerUsable)
{
    if (!Active || Active->Token != Token) return false;
    const auto Completed = *Active; Active.reset();
    if (Completed.Settings.DisplayGeneration != Capabilities.DisplayGeneration)
    {
        bPaused = bPaused || !FormerUsable;
        Failure = "Discarded stale display-generation completion";
        return false;
    }
    if (!Success)
    {
        bPaused = bPaused || !FormerUsable;
        Failure = FormerUsable ? "Settings transition failed; former output retained"
            : "Settings transition failed; former output unusable, rendering paused";
        return true;
    }
    Effective = Completed.Settings; bPaused = false;
    Failure = Effective.RequestedProfileId != Effective.EffectiveProfileId
        ? "Requested output unavailable; explicit SDR fallback effective" : "";
    return true;
}

bool FLabSettingsController::RefreshCapabilities(const FLabSettingsCapabilities& Caps, bool FormerUsable)
{
    if (!bInitialized || !ValidCapabilities(Caps) || Caps.DisplayGeneration <= Capabilities.DisplayGeneration) return false;
    Capabilities = Caps;
    const bool Supported = std::any_of(Caps.Outputs.begin(),Caps.Outputs.end(),[&](const auto& O) {
        return O.ProfileId == Effective.EffectiveProfileId && O.ReferenceWhiteNits == Effective.UIReferenceWhiteNits &&
            O.NativePackingWhiteNits == Effective.NativePackingWhiteNits;
    });
    bPaused = bPaused || !FormerUsable || !Supported;
    // Keep an active native operation bounded and outstanding until completion;
    // its old display tag prevents it from publishing into this generation.
    Pending.reset();
    Requested.DisplayGeneration = Caps.DisplayGeneration;
    if (!Queue(Requested,true)) bPaused = true;
    return true;
}
}
