#include "Asset/FShaderNativeLibraryEvidence.h"

#include <new>
#include <stdexcept>

namespace Stoner::Asset
{
namespace
{

void U64(Core::TArray<Core::uint8>& Out, Core::uint64 Value)
{
    for (Core::uint32 Shift = 0; Shift < 64; Shift += 8)
        Out.push_back(static_cast<Core::uint8>(Value >> Shift));
}

void Text(Core::TArray<Core::uint8>& Out, const Core::FString& Value)
{
    U64(Out, Value.Len());
    Out.insert(Out.end(), Value.View().begin(), Value.View().end());
}

void Digest(Core::TArray<Core::uint8>& Out, const FAssetDigest& Value)
{
    const auto& Bytes = Value.GetBytes();
    Out.insert(Out.end(), Bytes.begin(), Bytes.end());
}

EAssetResult ComputeDigest(
    const FShaderNativeLibraryEvidence& Evidence,
    FAssetDigest& Out) noexcept
{
    Out = {};
    try
    {
        Core::TArray<Core::uint8> Bytes;
        Text(Bytes, Core::FString("stoner.shader-native-library-evidence.v1"));
        Digest(Bytes, Evidence.DerivationEvidenceDigest);
        Text(Bytes, Evidence.TargetProfile);
        Text(Bytes, Evidence.Architecture);
        Text(Bytes, Evidence.Compiler);
        Text(Bytes, Evidence.XcodeBuild);
        Text(Bytes, Evidence.Sdk);
        Text(Bytes, Evidence.DeploymentTarget);
        Text(Bytes, Evidence.LanguageVersion);
        Digest(Bytes, Evidence.ArgumentDigest);
        Digest(Bytes, Evidence.LibraryDigest);
        U64(Bytes, Evidence.SizeBytes);
        Text(Bytes, Evidence.Finalizer.ToString());
        Text(Bytes, Evidence.FinalizerVersion.ToString());
        Out = FAssetDigest::FromBytes(Bytes);
        return EAssetResult::Success;
    }
    catch (const std::bad_alloc&)
    {
        return EAssetResult::CapacityExceeded;
    }
    catch (const std::length_error&)
    {
        return EAssetResult::CapacityExceeded;
    }
}

bool Bounded(const Core::FString& Value, Core::usize Maximum)
{
    return !Value.IsEmpty() && Value.Len() <= Maximum;
}

} // namespace

EAssetResult FShaderNativeLibraryEvidence::Validate() const noexcept
{
    if (!DerivationEvidenceDigest.IsAvailable() ||
        !Bounded(TargetProfile, 256) ||
        (Architecture != Core::FString("arm64") &&
         Architecture != Core::FString("x86_64")) ||
        !Bounded(Compiler, 4096) || !Bounded(XcodeBuild, 4096) ||
        !Bounded(Sdk, 256) || DeploymentTarget != Core::FString("12.0") ||
        LanguageVersion != Core::FString("2.4") ||
        !ArgumentDigest.IsAvailable() || !LibraryDigest.IsAvailable() ||
        SizeBytes == 0 || !Finalizer.IsValid() ||
        !FinalizerVersion.IsValid() || !CanonicalDigest.IsAvailable())
        return EAssetResult::InvalidInput;
    FAssetDigest Computed;
    return ComputeDigest(*this, Computed) == EAssetResult::Success &&
            Computed == CanonicalDigest
        ? EAssetResult::Success
        : EAssetResult::InvalidInput;
}

EAssetResult FinalizeShaderNativeLibraryEvidence(
    FShaderNativeLibraryEvidence& Evidence) noexcept
{
    const EAssetResult Result = ComputeDigest(Evidence, Evidence.CanonicalDigest);
    return Result == EAssetResult::Success ? Evidence.Validate() : Result;
}

} // namespace Stoner::Asset
