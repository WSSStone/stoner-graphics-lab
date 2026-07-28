#include "Asset/FAssetExtensionRegistry.h"

#include <algorithm>
#include <atomic>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>

namespace Stoner::Asset
{
namespace
{

struct FExtensionKey
{
    EAssetExtensionKind Kind;
    FAssetParticipantId Participant;

    bool operator==(const FExtensionKey&) const = default;
};

struct FExtensionKeyHash
{
    std::size_t operator()(const FExtensionKey& Key) const noexcept
    {
        return std::hash<std::string_view>{}(Key.Participant.ToString().View()) ^
            static_cast<std::size_t>(Key.Kind);
    }
};

} // namespace

struct FAssetRegistrationToken::FControl
{
    std::atomic<bool> Active{true};
    std::function<void()> Deactivate;
};

struct FAssetExtensionRegistry::FImpl
{
    struct FRecord
    {
        FAssetExtensionCapability Capability;
        Core::TSharedPtr<IAssetExtension> Extension;
        Core::TSharedPtr<FAssetRegistrationToken::FControl> Control;
    };

    mutable std::shared_mutex Mutex;
    std::unordered_map<FExtensionKey, FRecord, FExtensionKeyHash> Records;
};

FAssetRegistrationToken::FAssetRegistrationToken(Core::TSharedPtr<FControl> Control)
    : Control_(std::move(Control))
{
}

FAssetRegistrationToken::~FAssetRegistrationToken()
{
    Reset();
}

FAssetRegistrationToken::FAssetRegistrationToken(FAssetRegistrationToken&& Other) noexcept
    : Control_(std::move(Other.Control_))
{
}

FAssetRegistrationToken& FAssetRegistrationToken::operator=(
    FAssetRegistrationToken&& Other) noexcept
{
    if (this != &Other)
    {
        Reset();
        Control_ = std::move(Other.Control_);
    }
    return *this;
}

void FAssetRegistrationToken::Reset() noexcept
{
    if (Control_ && Control_->Active.exchange(false) && Control_->Deactivate)
    {
        Control_->Deactivate();
    }
    Control_.reset();
}

bool FAssetRegistrationToken::IsActive() const noexcept
{
    return Control_ && Control_->Active.load();
}

FAssetExtensionRegistry::FAssetExtensionRegistry()
    : Impl_(Core::MakeShared<FImpl>())
{
}

FAssetExtensionRegistry::~FAssetExtensionRegistry() = default;

EAssetResult FAssetExtensionRegistry::Register(
    Core::TSharedPtr<IAssetExtension> Extension,
    FAssetRegistrationToken& OutToken)
{
    OutToken.Reset();
    if (!Extension)
    {
        return EAssetResult::InvalidInput;
    }
    const FAssetExtensionCapability Capability = Extension->GetCapability();
    if (!Capability.Participant.IsValid())
    {
        return EAssetResult::InvalidInput;
    }
    const FExtensionKey Key{Capability.Kind, Capability.Participant};
    std::unique_lock Lock(Impl_->Mutex);
    if (Impl_->Records.contains(Key))
    {
        return EAssetResult::AlreadyExists;
    }

    auto Control = Core::MakeShared<FAssetRegistrationToken::FControl>();
    Core::TWeakPtr<FImpl> WeakImpl = Impl_;
    Control->Deactivate = [WeakImpl, Key]
    {
        if (const auto State = WeakImpl.lock())
        {
            std::unique_lock DeactivateLock(State->Mutex);
            State->Records.erase(Key);
        }
    };
    Impl_->Records.emplace(
        Key,
        FImpl::FRecord{Capability, std::move(Extension), Control});
    OutToken = FAssetRegistrationToken(std::move(Control));
    return EAssetResult::Success;
}

Core::TArray<FAssetExtensionCapability> FAssetExtensionRegistry::Snapshot(
    EAssetExtensionKind Kind) const
{
    std::shared_lock Lock(Impl_->Mutex);
    Core::TArray<FAssetExtensionCapability> Result;
    for (const auto& [Key, Record] : Impl_->Records)
    {
        (void)Key;
        if (Record.Capability.Kind == Kind && Record.Control->Active.load())
        {
            Result.push_back(Record.Capability);
        }
    }
    std::sort(
        Result.begin(),
        Result.end(),
        [](const FAssetExtensionCapability& Left, const FAssetExtensionCapability& Right)
        {
            return Left.Participant < Right.Participant;
        });
    return Result;
}

FAssetExecutionLease FAssetExtensionRegistry::Acquire(
    EAssetExtensionKind Kind,
    const FAssetParticipantId& Participant) const
{
    std::shared_lock Lock(Impl_->Mutex);
    const auto Found = Impl_->Records.find({Kind, Participant});
    if (Found == Impl_->Records.end() || !Found->second.Control->Active.load())
    {
        return {};
    }
    return FAssetExecutionLease(Found->second.Extension);
}

} // namespace Stoner::Asset
