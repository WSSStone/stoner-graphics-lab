#pragma once

#include "ProductionImageAcceptance.h"

struct FProductionCapabilitySignature
{
    Stoner::Core::uint32 RegistryVersion = 0;
    Stoner::Core::FString BackendImplementation;
    Stoner::Core::FString CpuArchitecture;
    Stoner::Core::FString AdapterFamily;
    Stoner::Core::FString ShaderProfile;
    Stoner::Core::FString ColorFormat;
    Stoner::Core::FString DepthFormat;
    Stoner::Core::uint32 SampleCount = 0;
    Stoner::Core::FString TextureFormatFamily;

    [[nodiscard]] bool IsValid() const noexcept;
    [[nodiscard]] Stoner::Core::FString CanonicalKey() const;
};

struct FProductionImageBaseline
{
    Stoner::Core::FString BaselineId;
    Stoner::Core::FString State;
    Stoner::Core::FString WorkloadRevision;
    Stoner::Core::FString Backend;
    Stoner::Core::FString DeviceClass;
    FProductionCapabilitySignature Signature;
    Stoner::Core::uint32 Width = 0;
    Stoner::Core::uint32 Height = 0;
    EProductionColorTransfer ColorTransfer = EProductionColorTransfer::SRGB;
    Stoner::Core::FString ReferencePath;
    Stoner::Core::FString ReferenceSha256;
    FProductionFlipPolicy FlipPolicy;
    Stoner::Core::FString CalibrationEvidenceSha256;
};

class FProductionImageBaselineRegistry
{
public:
    [[nodiscard]] bool LoadDeviceClasses(
        const Stoner::Core::FString& Path,
        Stoner::Core::FString& OutFailure);
    [[nodiscard]] bool LoadBaselines(
        const Stoner::Core::FString& Root,
        Stoner::Core::FString& OutFailure);
    [[nodiscard]] bool SelectAccepted(
        const FProductionCapabilitySignature& Signature,
        const Stoner::Core::FString& WorkloadRevision,
        const Stoner::Core::FString& Backend,
        FProductionImageBaseline& OutBaseline,
        Stoner::Core::FString& OutFailure) const;

private:
    struct FDeviceClassRecord
    {
        Stoner::Core::FString DeviceClass;
        FProductionCapabilitySignature Signature;
    };
    Stoner::Core::TArray<FDeviceClassRecord> DeviceClasses;
    Stoner::Core::TArray<FProductionImageBaseline> Baselines;
};
