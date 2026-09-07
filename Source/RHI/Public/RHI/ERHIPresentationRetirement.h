#pragma once

namespace Stoner::RHI
{

// The mode describes how a backend proves that an acquired presentation
// image can be reused.  Unknown means that capability resolution has not
// produced a usable mode yet; it is intentionally distinct from a backend
// that selected AcquireHistory or NativeCallback.
enum class ERHIPresentationRetirementMode
{
    Unknown,
    PresentationFence,
    AcquireHistory,
    NativeCallback
};

// A compact, backend-neutral explanation for optional presentation policy
// selection.  Native error text and codes stay in the backend diagnostics.
enum class ERHIPresentationRetirementReason
{
    Unknown,
    Preferred,
    ForcedOff,
    QueryUnavailable,
    QueryFailed,
    ExtensionAbsent,
    FeatureAbsent,
    InstanceDependencyAbsent,
    EntryPointsUnavailable,
    OptionalEnablementFailed,
    OptionalEnablementRetryExhausted,
    FallbackCreationFailed
};

// Terminal ownership assurance is separate from render completion and from
// physical display evidence.  IdleAssumed is the bounded Vulkan fallback
// cleanup contract and is never a presentation-release proof.
enum class ERHIShutdownAssurance
{
    Unknown,
    Proven,
    IdleAssumed,
    Forced,
    DeviceLost
};

} // namespace Stoner::RHI
