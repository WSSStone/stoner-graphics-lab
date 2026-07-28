#pragma once

#include "Core/FPlatformTypes.h"

namespace Stoner::Asset
{

enum class EAssetResult : Core::uint8
{
    Success,
    InvalidIdentity,
    InvalidUtf8,
    IdentityTooLong,
    TypeMismatch,
    NotFound,
    AccessDenied,
    MalformedSource,
    TransientFailure,
    AlreadyExists,
    Conflict,
    UnresolvedDependency,
    DependencyCycle,
    IncompleteRegistry,
    NoMatchingResolver,
    AmbiguousResolver,
    NoMatchingImporter,
    AmbiguousImporter,
    Unsupported,
    InvalidInput,
    DependencyFailure,
    ProcessingFailure,
    RegistrationInactive,
    CapacityExceeded
};

enum class EAssetStage : Core::uint8
{
    Identity,
    Registry,
    Resolve,
    Probe,
    Import,
    Load,
    Cook,
    Inspect
};

enum class EAssetDiagnosticSeverity : Core::uint8
{
    Info,
    Warning,
    Error
};

} // namespace Stoner::Asset
