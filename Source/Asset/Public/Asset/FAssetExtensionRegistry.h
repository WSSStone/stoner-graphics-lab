#pragma once

#include "Asset/FAssetParticipant.h"
#include "Core/TArray.h"
#include "Core/TSharedPtr.h"

#include <memory>
#include <utility>

namespace Stoner::Asset
{

enum class EAssetExtensionKind : Core::uint8
{
    Resolver,
    Importer,
    Loader,
    Cooker
};

struct FAssetExtensionCapability
{
    EAssetExtensionKind Kind = EAssetExtensionKind::Resolver;
    FAssetParticipantId Participant;
    FAssetProducerVersion ProducerVersion;
    int Priority = 0;
    Core::TArray<Core::FString> Schemes;
    Core::TArray<Core::FString> FormatHints;
    Core::usize ProbeByteLimit = 0;
};

class IAssetExtension
{
public:
    virtual ~IAssetExtension() = default;
    [[nodiscard]] virtual FAssetExtensionCapability GetCapability() const = 0;
};

class FAssetExtensionRegistry;

class FAssetRegistrationToken
{
public:
    FAssetRegistrationToken() = default;
    ~FAssetRegistrationToken();
    FAssetRegistrationToken(FAssetRegistrationToken&& Other) noexcept;
    FAssetRegistrationToken& operator=(FAssetRegistrationToken&& Other) noexcept;
    FAssetRegistrationToken(const FAssetRegistrationToken&) = delete;
    FAssetRegistrationToken& operator=(const FAssetRegistrationToken&) = delete;

    void Reset() noexcept;
    [[nodiscard]] bool IsActive() const noexcept;

private:
    friend class FAssetExtensionRegistry;
    struct FControl;
    explicit FAssetRegistrationToken(Core::TSharedPtr<FControl> Control);
    Core::TSharedPtr<FControl> Control_;
};

class FAssetExecutionLease
{
public:
    FAssetExecutionLease() = default;

    [[nodiscard]] bool IsValid() const noexcept { return Extension_ != nullptr; }

    template <typename T>
    [[nodiscard]] Core::TSharedPtr<T> Get() const
    {
        return std::dynamic_pointer_cast<T>(Extension_);
    }

private:
    friend class FAssetExtensionRegistry;
    explicit FAssetExecutionLease(Core::TSharedPtr<IAssetExtension> Extension)
        : Extension_(std::move(Extension))
    {
    }
    Core::TSharedPtr<IAssetExtension> Extension_;
};

class FAssetExtensionRegistry
{
public:
    FAssetExtensionRegistry();
    ~FAssetExtensionRegistry();
    FAssetExtensionRegistry(const FAssetExtensionRegistry&) = delete;
    FAssetExtensionRegistry& operator=(const FAssetExtensionRegistry&) = delete;

    [[nodiscard]] EAssetResult Register(
        Core::TSharedPtr<IAssetExtension> Extension,
        FAssetRegistrationToken& OutToken);
    [[nodiscard]] Core::TArray<FAssetExtensionCapability> Snapshot(
        EAssetExtensionKind Kind) const;
    [[nodiscard]] FAssetExecutionLease Acquire(
        EAssetExtensionKind Kind,
        const FAssetParticipantId& Participant) const;

private:
    struct FImpl;
    Core::TSharedPtr<FImpl> Impl_;
};

} // namespace Stoner::Asset
